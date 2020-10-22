#include "buildings/building.h"
#include "collider.h"
#include "game.h"
#include "mesh.h"
#include "registery.h"

const char * GameResourceToString( GameResource resource ) {
	switch ( resource ) {
	case GameResource::WHEAT:
		return "Wheat";
	default:
		ng_assert( false );
		return nullptr;
	}
}

bool IsCellInsideBuilding( const CpntBuilding & building, Cell cell ) {
	return ( cell.x >= building.cell.x && cell.z >= building.cell.z && cell.x < building.cell.x + building.tileSizeX &&
	         cell.z < building.cell.z + building.tileSizeZ );
}

bool IsBuildingInsideArea( const CpntBuilding & building, const Area & area ) {
	switch ( area.shape ) {
	case Area::Shape::RECTANGLE: {
		// Check if the rectangle forming the building bounds and the area overlaps
		if ( building.cell.x >= area.center.x + area.sizeX || area.center.x >= building.cell.x + building.tileSizeX ) {
			return false; // One rectangle is on the left of the other
		}
		if ( building.cell.z >= area.center.z + area.sizeZ || area.center.z >= building.cell.z + building.tileSizeZ ) {
			return false; // One rectangle is on top of the other
		}
		return true;
	}
	default:
		ng_assert_msg( false, "Unimplemented shape in IsBuildingInsideArea" );
		return false;
	}
}

bool IsCellAdjacentToBuilding( const CpntBuilding & building, Cell cell, const Map & map ) {
	if ( IsCellInsideBuilding( building, cell ) ) {
		return false;
	}
	return ( IsCellInsideBuilding( building, Cell( cell.x + 1, cell.z ) ) ||
	         IsCellInsideBuilding( building, Cell( cell.x - 1, cell.z ) ) ||
	         IsCellInsideBuilding( building, Cell( cell.x, cell.z + 1 ) ) ||
	         IsCellInsideBuilding( building, Cell( cell.x, cell.z - 1 ) ) );
}

void SystemHousing::Update( Registery & reg, Duration ticks ) {
	totalPopulation = 0;
	for ( auto & [ e, housing ] : reg.IterateOver< CpntHousing >() ) {
		// Check if population should grow in house
		if ( housing.numCurrentlyLiving < housing.maxHabitants ) {
			housing.numCurrentlyLiving++;
		}
		totalPopulation += housing.numCurrentlyLiving;
	}
}

void SystemHousing::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_SERVICE_PROVIDED: {
		GameService   service = CastPayloadAs< GameService >( msg.payload );
		CpntHousing * housing = reg.TryGetComponent< CpntHousing >( msg.recipient );
		if ( housing != nullptr ) {
			housing->lastServiceAccess[ ( u32 )service ] = theGame->clock;
		}
		break;
	}
	default:
		ng_assert( false, "Message type %d can't be handled by this system\n", msg.type );
	}
}

Entity LookForClosestBuildingKind( Registery &                reg,
                                   BuildingKind               kind,
                                   const CpntBuilding &       origin,
                                   u32                        maxDistance,
                                   ng::DynamicArray< Cell > & outPath ) {
	Entity closestStorage = INVALID_ENTITY;
	u32    closestStorageDistance = maxDistance;

	for ( auto & [ e, building ] : reg.IterateOver< CpntBuilding >() ) {
		if ( building.kind != kind ) {
			continue;
		}
		u32  distance = 0;
		bool pathFound = FindPathBetweenBuildings( origin, reg.GetComponent< CpntBuilding >( e ), theGame->map,
		                                           theGame->roadNetwork, outPath, closestStorageDistance, &distance );
		if ( pathFound && distance < closestStorageDistance ) {
			closestStorage = e;
			closestStorageDistance = distance;
		}
	}
	return closestStorage;
}

Entity LookForStorageContainingOneOfResourceList( Registery &                reg,
                                                  const CpntBuilding &       origin,
                                                  GameResource *             resourceList,
                                                  u32                        resourceListSize,
                                                  u32                        maxDistance,
                                                  ng::DynamicArray< Cell > & outPath ) {
	Entity closestStorage = INVALID_ENTITY;
	u32    closestStorageDistance = maxDistance;

	for ( auto & [ e, building ] : reg.IterateOver< CpntBuilding >() ) {
		if ( building.kind != BuildingKind::STORAGE_HOUSE ) {
			continue;
		}
		const CpntResourceInventory & inventory = reg.GetComponent< CpntResourceInventory >( e );
		bool                          hasAtLeastOneResourceInList = false;
		for ( u32 i = 0; i < resourceListSize; i++ ) {
			if ( inventory.GetResourceAmount( resourceList[ i ] ) > 0 ) {
				hasAtLeastOneResourceInList = true;
				break;
			}
		}

		if ( !hasAtLeastOneResourceInList ) {
			continue;
		}

		u32  distance = 0;
		bool pathFound = FindPathBetweenBuildings( origin, reg.GetComponent< CpntBuilding >( e ), theGame->map,
		                                           theGame->roadNetwork, outPath, closestStorageDistance, &distance );
		if ( pathFound && distance < closestStorageDistance ) {
			closestStorage = e;
			closestStorageDistance = distance;
		}
	}
	return closestStorage;
}

Entity LookForStorageAcceptingResource( Registery &                reg,
                                        const CpntBuilding &       origin,
                                        GameResource               resource,
                                        u32                        maxDistance,
                                        ng::DynamicArray< Cell > & outPath ) {
	Entity closestStorage = INVALID_ENTITY;
	u32    closestStorageDistance = maxDistance;

	for ( auto & [ e, building ] : reg.IterateOver< CpntBuilding >() ) {
		if ( building.kind != BuildingKind::STORAGE_HOUSE ) {
			continue;
		}
		const CpntResourceInventory & inventory = reg.GetComponent< CpntResourceInventory >( e );
		if ( inventory.GetResourceAmount( resource ) < inventory.GetResourceCapacity( resource ) ) {
			u32  distance = 0;
			bool pathFound =
			    FindPathBetweenBuildings( origin, reg.GetComponent< CpntBuilding >( e ), theGame->map,
			                              theGame->roadNetwork, outPath, closestStorageDistance, &distance );
			if ( pathFound && distance < closestStorageDistance ) {
				closestStorage = e;
				closestStorageDistance = distance;
			}
		}
	}
	return closestStorage;
}

void SystemBuildingProducing::Update( Registery & reg, Duration ticks ) {
	for ( auto & [ e, producer ] : reg.IterateOver< CpntBuildingProducing >() ) {
		producer.timeSinceLastProduction += ticks;
		if ( producer.timeSinceLastProduction >= producer.timeToProduceBatch ) {
			const CpntBuilding & cpntBuilding = reg.GetComponent< CpntBuilding >( e );

			// Find a path to store house
			thread_local ng::DynamicArray< Cell > path( 32 );
			Entity                                closestStorage =
			    LookForStorageAcceptingResource( reg, cpntBuilding, producer.resource, ULONG_MAX, path );

			if ( closestStorage == INVALID_ENTITY ) {
				ImGui::Text( "A producing building has no connection to a storage, stall for now" );
				producer.timeSinceLastProduction = producer.timeToProduceBatch;
			} else {
				// Produce a batch
				producer.timeSinceLastProduction -= producer.timeToProduceBatch;
				// Spawn a dummy who will move the batch the nearest storage house
				Entity carrier = reg.CreateEntity();
				reg.AssignComponent< CpntRenderModel >( carrier, g_modelAtlas.cubeMesh );
				CpntTransform & transform = reg.AssignComponent< CpntTransform >( carrier );
				CpntNavAgent &  navAgent = reg.AssignComponent< CpntNavAgent >( carrier );
				navAgent.pathfindingNextSteps = path;
				auto & inventory = reg.AssignComponent< CpntResourceInventory >( carrier );
				inventory.SetResourceMaxCapacity( producer.resource, producer.batchSize );
				inventory.StoreRessource( producer.resource, producer.batchSize );

				transform.SetTranslation( GetPointInMiddleOfCell( navAgent.pathfindingNextSteps.Last() ) );
				ListenTo( MESSAGE_PATHFINDING_DESTINATION_REACHED, carrier );
			}
		}
	}
}

void SystemBuildingProducing::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_PATHFINDING_DESTINATION_REACHED: {
		ng_assert( msg.sender == msg.recipient );
		ng::Printf( "Entity %lu has arrived!\n", msg.recipient );

		// Look for a storage house next to receiver
		CpntTransform & transform = reg.GetComponent< CpntTransform >( msg.recipient );
		Cell            position = GetCellForPoint( transform.GetTranslation() );
		Entity          closestStorage = INVALID_ENTITY;
		for ( auto & [ e, building ] : reg.IterateOver< CpntBuilding >() ) {
			if ( building.kind == BuildingKind::STORAGE_HOUSE ) {
				if ( IsCellAdjacentToBuilding( building, position, theGame->map ) ) {
					closestStorage = e;
					break;
				}
			}
		}
		if ( closestStorage == INVALID_ENTITY ) {
			// TODO: Handle that the storage has been removed
			ng::Errorf( "An agent was directed to a storage that disappeared in the meantime" );
		} else {
			// Store what we have on us in the storage
			CpntResourceInventory & carrier = reg.GetComponent< CpntResourceInventory >( msg.recipient );
			for ( u32 i = 0; i < ( u32 )GameResource::NUM_RESOURCES; i++ ) {
				u32 amount = carrier.GetResourceAmount( ( GameResource )i );
				PostMsg< TransactionMessagePayload >( MESSAGE_INVENTORY_TRANSACTION,
				                                      TransactionMessagePayload{ ( GameResource )i, amount, false },
				                                      closestStorage, msg.recipient );
			}
		}
		reg.MarkForDelete( msg.recipient );
		break;
	}
	default:
		ng_assert( false, "Message type %d can't be handled by this system\n", msg.type );
	}
}

u32 CpntResourceInventory::StoreRessource( GameResource resource, u32 amount ) {
	StorageCapacity & resourceStorage = storage[ ( int )resource ];
	if ( amount + resourceStorage.currentAmount > resourceStorage.max ) {
		u32 consumed = resourceStorage.max - resourceStorage.currentAmount;
		resourceStorage.currentAmount += consumed;
		return consumed;
	} else {
		resourceStorage.currentAmount += amount;
		return amount;
	}
}

u32 CpntResourceInventory::RemoveResource( GameResource resource, u32 amount ) {
	StorageCapacity & resourceStorage = storage[ ( int )resource ];
	if ( amount > resourceStorage.currentAmount ) {
		u32 deleted = resourceStorage.currentAmount;
		resourceStorage.currentAmount = 0;
		return deleted;
	} else {
		resourceStorage.currentAmount -= amount;
		return amount;
	}
}

bool CpntResourceInventory::IsEmpty() const {
	for ( const auto & capacity : storage ) {
		if ( capacity.currentAmount > 0 ) {
			return false;
		}
	}
	return true;
}

void SystemMarket::Update( Registery & reg, Duration ticks ) {
	for ( auto & [ marketEntity, market ] : reg.IterateOver< CpntMarket >() ) {
		CpntResourceInventory & marketInventory = reg.GetComponent< CpntResourceInventory >( marketEntity );
		CpntBuilding &          marketBuilding = reg.GetComponent< CpntBuilding >( marketEntity );
		if ( market.wanderer == INVALID_ENTITY ) {
			if ( market.timeSinceLastWandererSpawn < market.durationBetweenWandererSpawns ) {
				market.timeSinceLastWandererSpawn += ticks;
			} else if ( marketInventory.IsEmpty() == false ) {
				// Let's check if we have a path for the wanderer
				ng::DynamicArray< Cell > path( market.wandererCellRange );
				Cell                     startingCell = GetAnyRoadConnectedToBuilding( marketBuilding, theGame->map );
				if ( startingCell == INVALID_CELL ) {
					// This building is not connected to a road
					continue;
				}
				bool ok = CreateWandererRoutine( startingCell, theGame->map, theGame->roadNetwork, path,
				                                 market.wandererCellRange );
				if ( ok ) {
					// Let's spawn a wanderer
					Entity wanderer = reg.CreateEntity();
					market.wanderer = wanderer;
					reg.AssignComponent< CpntRenderModel >( wanderer, g_modelAtlas.cubeMesh );
					CpntNavAgent & navAgent = reg.AssignComponent< CpntNavAgent >( wanderer );
					navAgent.pathfindingNextSteps = path;
					CpntTransform & transform = reg.AssignComponent< CpntTransform >( wanderer );
					transform.SetTranslation( GetPointInMiddleOfCell( navAgent.pathfindingNextSteps.Last() ) );
					reg.AssignComponent< CpntSeller >( wanderer );
					auto & inventory = reg.AssignComponent< CpntResourceInventory >( wanderer );
					inventory.SetResourceMaxCapacity( GameResource::WHEAT, 100 );
					u32 amountStored = inventory.StoreRessource(
					    GameResource::WHEAT, marketInventory.GetResourceAmount( GameResource::WHEAT ) );
					marketInventory.RemoveResource( GameResource::WHEAT, amountStored );
					ListenTo( MESSAGE_PATHFINDING_DESTINATION_REACHED, wanderer );
				}
			}
		}

		u32          numResourcesToFetch = 0;
		GameResource resourcesToFetch[ ( int )GameResource::NUM_RESOURCES ];

		// Let's check if we are low on resources
		for ( u32 i = 0; i < ( u32 )GameResource::NUM_RESOURCES; i++ ) {
			auto const & capacity = marketInventory.storage[ i ];
			// if we have less than 1/4 of the max capacity, try to get new resources
			if ( capacity.max > 0 && capacity.currentAmount < capacity.max / 4 ) {
				resourcesToFetch[ numResourcesToFetch++ ] = ( GameResource )i;
			}
		}

		if ( numResourcesToFetch > 0 && market.fetcher == INVALID_ENTITY && market.wanderer == INVALID_ENTITY ) {
			ng::DynamicArray< Cell > path( 32 );
			Entity                   closestStorage = LookForStorageContainingOneOfResourceList(
                reg, marketBuilding, resourcesToFetch, numResourcesToFetch, market.fetcherCellRange, path );
			if ( closestStorage != INVALID_ENTITY ) {
				// we found a storage containing what we are looking for nearby
				// Let's spawn a fetcher
				Entity fetcher = reg.CreateEntity();
				market.fetcher = fetcher;
				reg.AssignComponent< CpntRenderModel >( fetcher, g_modelAtlas.cubeMesh );
				CpntNavAgent &  navAgent = reg.AssignComponent< CpntNavAgent >( fetcher, path );
				CpntTransform & transform = reg.AssignComponent< CpntTransform >( fetcher );
				transform.SetTranslation( GetPointInMiddleOfCell( navAgent.pathfindingNextSteps.Last() ) );
				auto & inventory = reg.AssignComponent< CpntResourceInventory >( fetcher );
				for ( u32 i = 0; i < ( u32 )GameResource::NUM_RESOURCES; i++ ) {
					// Set maximum storage of fetcher to the amount of resource we need
					auto const & marketCapacity = marketInventory.storage[ i ];
					inventory.SetResourceMaxCapacity( ( GameResource )i,
					                                  marketCapacity.max - marketCapacity.currentAmount );
				}
				CpntResourceFetcher & fetcherCpnt = reg.AssignComponent< CpntResourceFetcher >( fetcher );
				fetcherCpnt.parent = marketEntity;
				fetcherCpnt.target = closestStorage;
				ng::Printf( "We spawned a fetcher\n" );
				ListenTo( MESSAGE_ENTITY_DELETED, fetcher );
			}
		}
	}
}

void SystemMarket::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_PATHFINDING_DESTINATION_REACHED: {
		for ( auto & [ marketEntity, market ] : reg.IterateOver< CpntMarket >() ) {
			if ( market.wanderer == msg.sender ) {
				ng::Printf( "A wanderer has arrived\n" );
				// Let's get back resources that were not distributed
				auto & wandererStorage = reg.GetComponent< CpntResourceInventory >( msg.sender );
				auto & marketStorage = reg.GetComponent< CpntResourceInventory >( marketEntity );
				for ( u32 i = 0; i < ( u32 )GameResource::NUM_RESOURCES; i++ ) {
					PostMsg< TransactionMessagePayload >(
					    MESSAGE_INVENTORY_TRANSACTION,
					    TransactionMessagePayload{ ( GameResource )i,
					                               wandererStorage.GetResourceCapacity( ( GameResource )i ), false },
					    marketEntity, msg.sender );
				}
				market.wanderer = INVALID_ENTITY;
				market.timeSinceLastWandererSpawn = 0;
			}
		}
		reg.MarkForDelete( msg.recipient );
		break;
	}
	case MESSAGE_ENTITY_DELETED: {
		for ( auto & [ marketEntity, market ] : reg.IterateOver< CpntMarket >() ) {
			if ( market.fetcher == msg.recipient ) {
				ng::Printf( "A fetcher has arrived\n" );
				market.fetcher = INVALID_ENTITY;
			}
		}
		break;
	}
	default:
		ng_assert( false, "Message type %d can't be handled by this system\n", msg.type );
	}
}

void SystemSeller::Update( Registery & reg, Duration ticks ) {
	for ( auto & [ e, seller ] : reg.IterateOver< CpntSeller >() ) {
		auto & transform = reg.GetComponent< CpntTransform >( e );
		auto & sellerInventory = reg.GetComponent< CpntResourceInventory >( e );
		Cell   currentCell = GetCellForPoint( transform.GetTranslation() );
		if ( currentCell != seller.lastCellDistributed ) {
			seller.lastCellDistributed = currentCell;

			// Four cardinal directions
			ng::StaticArray< Cell, 4 > neighbors;
			for ( u32 i = 0; i < 4; i++ ) {
				Cell neighbor = GetCellAfterMovement( currentCell, ( CardinalDirection )i );
				if ( theGame->map.GetTile( neighbor ) == MapTile::BLOCKED ) {
					neighbors.PushBack( neighbor );
				}
			}

			// Look for houses adjacent to the seller
			for ( auto & [ houseEntity, house ] : reg.IterateOver< CpntHousing >() ) {
				auto const & building = reg.GetComponent< CpntBuilding >( houseEntity );
				for ( const Cell & neighborCell : neighbors ) {
					if ( IsCellInsideBuilding( building, neighborCell ) ) {
						// distribute resources
						auto & houseInventory = reg.GetComponent< CpntResourceInventory >( houseEntity );
						for ( u32 i = 0; i < ( u32 )GameResource::NUM_RESOURCES; i++ ) {
							auto & capacity = sellerInventory.storage[ i ];
							if ( capacity.currentAmount > 0 ) {
								PostMsg< TransactionMessagePayload >(
								    MESSAGE_INVENTORY_TRANSACTION,
								    TransactionMessagePayload{ ( GameResource )i, capacity.currentAmount, true },
								    houseEntity, e );
							}
						}
						break; // no need to look for other cells, we found the building
					}
				}
			}
		}
	}
}

void SystemServiceWanderer::Update( Registery & reg, Duration ticks ) {
	for ( auto & [ e, wanderer ] : reg.IterateOver< CpntServiceWanderer >() ) {
		auto & transform = reg.GetComponent< CpntTransform >( e );
		Cell   currentCell = GetCellForPoint( transform.GetTranslation() );
		if ( currentCell != wanderer.lastCellDistributed ) {
			wanderer.lastCellDistributed = currentCell;

			// Four cardinal directions
			ng::StaticArray< Cell, 4 > neighbors;
			for ( u32 i = 0; i < 4; i++ ) {
				Cell neighbor = GetCellAfterMovement( currentCell, ( CardinalDirection )i );
				if ( theGame->map.GetTile( neighbor ) == MapTile::BLOCKED ) {
					neighbors.PushBack( neighbor );
				}
			}

			// Look for houses adjacent to the wanderer
			for ( auto & [ houseEntity, house ] : reg.IterateOver< CpntHousing >() ) {
				auto const & building = reg.GetComponent< CpntBuilding >( houseEntity );
				for ( const Cell & neighborCell : neighbors ) {
					if ( IsCellInsideBuilding( building, neighborCell ) ) {
						// provide service
						PostMsg< GameService >( MESSAGE_SERVICE_PROVIDED, wanderer.service, houseEntity, e );
						break; // no need to look for other cells, we found the building
					}
				}
			}
		}
	}
}

void SystemServiceBuilding::Update( Registery & reg, Duration ticks ) {
	for ( auto & [ e, serviceBuilding ] : reg.IterateOver< CpntServiceBuilding >() ) {
		if ( serviceBuilding.wanderer == INVALID_ENTITY ) {
			if ( serviceBuilding.timeSinceLastWandererSpawn < serviceBuilding.durationBetweenWandererSpawns ) {
				serviceBuilding.timeSinceLastWandererSpawn += ticks;
			} else {
				// Let's check if we have a path for the wanderer
				ng::DynamicArray< Cell > path( serviceBuilding.wandererCellRange );
				Cell                     startingCell =
				    GetAnyRoadConnectedToBuilding( reg.GetComponent< CpntBuilding >( e ), theGame->map );
				if ( startingCell == INVALID_CELL ) {
					// This building is not connected to a road
					continue;
				}
				bool ok = CreateWandererRoutine( startingCell, theGame->map, theGame->roadNetwork, path,
				                                 serviceBuilding.wandererCellRange );
				if ( ok ) {
					// Let's spawn a wanderer
					Entity wanderer = reg.CreateEntity();
					serviceBuilding.wanderer = wanderer;
					reg.AssignComponent< CpntRenderModel >( wanderer, g_modelAtlas.cubeMesh );
					CpntNavAgent & navAgent = reg.AssignComponent< CpntNavAgent >( wanderer );
					navAgent.pathfindingNextSteps = path;
					CpntTransform & transform = reg.AssignComponent< CpntTransform >( wanderer );
					transform.SetTranslation( GetPointInMiddleOfCell( navAgent.pathfindingNextSteps.Last() ) );
					auto & serviceWanderer = reg.AssignComponent< CpntServiceWanderer >( wanderer );
					serviceWanderer.service = serviceBuilding.service;
					ListenTo( MESSAGE_PATHFINDING_DESTINATION_REACHED, wanderer );
				}
			}
		}
	}
}

void SystemServiceBuilding::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_PATHFINDING_DESTINATION_REACHED: {
		ng::Printf( "A service wanderer has arrived\n" );
		for ( auto & [ serviceBuildingEntity, serviceBuilding ] : reg.IterateOver< CpntServiceBuilding >() ) {
			if ( serviceBuilding.wanderer == msg.sender ) {
				serviceBuilding.wanderer = INVALID_ENTITY;
				serviceBuilding.timeSinceLastWandererSpawn = 0;
				reg.MarkForDelete( msg.recipient );
			}
		}
		break;
	}
	default:
		ng_assert( false, "Message type %d can't be handled by this system\n", msg.type );
	}
}

void SystemResourceInventory::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_INVENTORY_TRANSACTION: {
		CpntResourceInventory &           giver = reg.GetComponent< CpntResourceInventory >( msg.sender );
		CpntResourceInventory &           recipient = reg.GetComponent< CpntResourceInventory >( msg.recipient );
		const TransactionMessagePayload & payload = CastPayloadAs< TransactionMessagePayload >( msg.payload );

		u32 amountAvailable = giver.RemoveResource( payload.resource, payload.quantity );
		u32 amountConsumed = recipient.StoreRessource( payload.resource, amountAvailable );
		u32 amountToGiveBack = amountAvailable - amountConsumed;
		if ( amountToGiveBack > 0 && payload.acceptPayback ) {
			// Send back the amount we couldn't store
			giver.StoreRessource( payload.resource, amountToGiveBack );
		}
		break;
	}
	default:
		ng_assert( false, "Message type %d can't be handled by this system\n", msg.type );
	}
}

void SystemFetcher::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_CPNT_ATTACHED:
		if ( CastPayloadAs< CpntTypeHash >( msg.payload ) == HashComponent< CpntResourceFetcher >() ) {
			// A fetcher has spawned, let's track when it gets to destination
			ListenTo( MESSAGE_PATHFINDING_DESTINATION_REACHED, msg.recipient );
		}
		break;
	case MESSAGE_PATHFINDING_DESTINATION_REACHED: {
		Entity                fetcher = msg.recipient;
		CpntResourceFetcher * cpntFetcher = reg.TryGetComponent< CpntResourceFetcher >( fetcher );
		ng_assert( cpntFetcher != nullptr );
		if ( cpntFetcher != nullptr ) {
			if ( cpntFetcher->direction == CpntResourceFetcher::CurrentDirection::TO_TARGET ) {
				// We arrived at our target
				CpntResourceInventory & fetcherInventory = reg.GetComponent< CpntResourceInventory >( fetcher );
				Cell fetcherCell = GetCellForPoint( reg.GetComponent< CpntTransform >( fetcher ).GetTranslation() );
				CpntBuilding &          targetBuilding = reg.GetComponent< CpntBuilding >( cpntFetcher->target );
				CpntResourceInventory & targetInventory =
				    reg.GetComponent< CpntResourceInventory >( cpntFetcher->target );
				if ( IsCellAdjacentToBuilding( targetBuilding, fetcherCell, theGame->map ) ) {
					// Let's fill our inventory
					for ( u32 i = 0; i < ( u32 )GameResource::NUM_RESOURCES; i++ ) {
						u32 capacity = fetcherInventory.GetResourceCapacity( ( GameResource )i );
						if ( capacity > 0 ) {
							PostTransactionMessage( ( GameResource )i, capacity, true, fetcher, cpntFetcher->target );
						}
					}
					// Now let's go back to our parent
					cpntFetcher->direction = CpntResourceFetcher::CurrentDirection::TO_PARENT;
					CpntBuilding * parentBuilding = reg.TryGetComponent< CpntBuilding >( cpntFetcher->parent );
					if ( parentBuilding == nullptr ) {
						ng::Errorf( "A fetcher arrived at destination, but there is market to go back anymore! :(\n" );
						reg.MarkForDelete( fetcher );
					} else {
						CpntNavAgent & fetcherNavAgent = reg.GetComponent< CpntNavAgent >( fetcher );
						bool           ok =
						    FindPathFromCellToBuilding( fetcherCell, *parentBuilding, theGame->map,
						                                theGame->roadNetwork, fetcherNavAgent.pathfindingNextSteps );
						if ( !ok ) {
							ng::Errorf( "A fetcher arrived at destination, but there is no road to go back to the "
							            "market! :(\n" );
							reg.MarkForDelete( fetcher );
						}
					}
				} else {
					ng::Errorf( "A fetcher arrived at destination, but there is not storage there! :(\n" );
					reg.MarkForDelete( fetcher );
				}
			} else if ( cpntFetcher->direction == CpntResourceFetcher::CurrentDirection::TO_PARENT ) {
				// We are back to the market
				// Let's empty our inventory
				ng::Printf( "a fetcher is supposed to be back at the market" );
				Entity                fetcher = msg.recipient;
				CpntResourceFetcher * cpntFetcher = reg.TryGetComponent< CpntResourceFetcher >( fetcher );
				ng_assert( cpntFetcher != nullptr );
				if ( cpntFetcher != nullptr ) {
					CpntResourceInventory & fetcherInventory = reg.GetComponent< CpntResourceInventory >( fetcher );
					for ( u32 i = 0; i < ( u32 )GameResource::NUM_RESOURCES; i++ ) {
						u32 amount = fetcherInventory.GetResourceAmount( ( GameResource )i );
						if ( amount > 0 ) {
							PostTransactionMessage( ( GameResource )i, amount, false, cpntFetcher->parent, fetcher );
						}
					}
				}
				reg.MarkForDelete( fetcher );
			}
			break;
		}
	}
	}
}
