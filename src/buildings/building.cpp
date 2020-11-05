#include "buildings/building.h"
#include "../packer_resource_list.h"
#include "../pathfinding_job.h"
#include "collider.h"
#include "delivery.h"
#include "game.h"
#include "mesh.h"
#include "registery.h"
#include "resource_fetcher.h"

const char * GameResourceToString( GameResource resource ) {
	switch ( resource ) {
	case GameResource::WHEAT:
		return "Wheat";
	case GameResource::WOOD:
		return "Wood";
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
		if ( housing.numCurrentlyLiving + housing.numIncomingMigrants < housing.maxHabitants ) {
			constexpr Cell migrantSpawnPosition( 0, 0 );
			housing.numIncomingMigrants++;
			Entity migrant = reg.CreateEntity();
			reg.AssignComponent< CpntRenderModel >( migrant, g_modelAtlas.GetModel( PackerResources::CUBE_DAE ) );
			auto & transform = reg.AssignComponent< CpntTransform >( migrant );
			transform.SetTranslation( GetPointInMiddleOfCell( migrantSpawnPosition ) );
			reg.AssignComponent< CpntNavAgent >( migrant );
			reg.AssignComponent< CpntMigrant >( migrant, e );

			PathfindingTask task{};
			task.requester = migrant;
			task.type = PathfindingTask::Type::FROM_CELL_TO_BUILDING;
			task.start.cell = migrantSpawnPosition;
			task.goal.building = reg.GetComponent< CpntBuilding >( e );
			task.movementAllowed = ASTAR_ALLOW_DIAGONALS;
			PostMsg< PathfindingTask >( MESSAGE_PATHFINDING_REQUEST, task, INVALID_ENTITY, migrant );

			ListenTo( MESSAGE_HOUSE_MIGRANT_ARRIVED, e );
		}
		totalPopulation += housing.numCurrentlyLiving;

		// Let's see if house can evolve
		if ( housing.tier == 0 ) {
			auto & inventory = reg.GetComponent< CpntResourceInventory >( e );
			if ( inventory.GetResourceAmount( GameResource::WHEAT ) >= 1 &&
			     IsServiceFulfilled( housing.lastServiceAccess[ ( int )GameService::WATER ], theGame->clock ) ) {
				auto & model = reg.GetComponent< CpntRenderModel >( e );
				model.model = g_modelAtlas.GetModel( PackerResources::HOUSE_DAE );
				housing.tier = 1;
				housing.maxHabitants = 8;
				inventory.SetResourceMaxCapacity( GameResource::WHEAT, 8 );
				inventory.SetResourceMaxCapacity( GameResource::WOOD, 8 );
			}
		}

		if ( ( theGame->clock - housing.lastAteAt ) * housing.numCurrentlyLiving >
		     housing.foodConsuptionSpeedPerHabitant ) {
			housing.lastAteAt = theGame->clock;
			auto & inventory = reg.GetComponent< CpntResourceInventory >( e );
			inventory.RemoveResource( GameResource::WHEAT, 1 );
		}
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
	case MESSAGE_HOUSE_MIGRANT_ARRIVED: {
		// a migrant has arrived
		CpntHousing * housing = reg.TryGetComponent< CpntHousing >( msg.recipient );
		if ( housing != nullptr ) {
			ng_assert( housing->numIncomingMigrants > 0 );
			ng_assert( housing->numCurrentlyLiving < housing->maxHabitants );
			housing->numIncomingMigrants--;
			housing->numCurrentlyLiving++;
			PostMsg( MESSAGE_WORKER_AVAILABLE, INVALID_ENTITY, INVALID_ENTITY );
		}
		break;
	}
	default:
		ng_assert_msg( false, "Message type %d can't be handled by this system\n", msg.type );
	}
}

void SystemHousing::OnCpntAttached( Entity e, CpntHousing & t ) {
	// A house has spawned, let's track when it receives a service
	ListenTo( MESSAGE_SERVICE_PROVIDED, e );
	for ( u32 i = 0; i < t.numCurrentlyLiving; i++ ) {
		PostMsg( MESSAGE_WORKER_AVAILABLE, INVALID_ENTITY, INVALID_ENTITY );
	}
}

void SystemHousing::OnCpntRemoved( Entity e, CpntHousing & t ) {
	for ( u32 i = 0; i < t.numCurrentlyLiving; i++ ) {
		PostMsg( MESSAGE_WORKER_REMOVED, INVALID_ENTITY, INVALID_ENTITY );
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
		if ( inventory.GetResourceCapacity( resource ) > 0 ) {
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
		if ( producer.deliveryGuy != INVALID_ENTITY ) {
			continue;
		}
		const CpntBuilding & cpntBuilding = reg.GetComponent< CpntBuilding >( e );
		float                efficiency = cpntBuilding.GetEfficiency();
		if ( efficiency == 0.0f ) {
			// Building has no workers
			continue;
		}
		Duration timeToProduce = Duration( ( double )producer.timeToProduceBatch / ( double )efficiency );
		producer.timeSinceLastProduction += ticks;
		if ( producer.timeSinceLastProduction >= timeToProduce ) {
			producer.timeSinceLastProduction -= timeToProduce;
			CpntResourceInventory inventory;
			inventory.SetResourceMaxCapacity( producer.resource, producer.batchSize );
			inventory.StoreRessource( producer.resource, producer.batchSize );
			producer.deliveryGuy = CreateDeliveryGuy( reg, e, inventory );
			ListenTo( MESSAGE_ENTITY_DELETED, producer.deliveryGuy );
		}
	}
}

void SystemBuildingProducing::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_ENTITY_DELETED: {
		for ( auto & [ e, producer ] : reg.IterateOver< CpntBuildingProducing >() ) {
			if ( producer.deliveryGuy == msg.recipient ) {
				producer.deliveryGuy = INVALID_ENTITY;
			}
		}
		break;
	}
	default:
		ng_assert_msg( false, "Message type %d can't be handled by this system\n", msg.type );
	}
}

u32 CpntResourceInventory::StoreRessource( GameResource resource, u32 amount ) {
	StorageCapacity & resourceStorage = storage[ ( int )resource ];
	u32               capacity = hasMaxTotalAmount
	                   ? MIN( resourceStorage.max - resourceStorage.currentAmount, maxTotalAmount - GetTotalAmount() )
	                   : resourceStorage.max - resourceStorage.currentAmount;
	if ( amount > capacity ) {
		resourceStorage.currentAmount += capacity;
		return capacity;
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
					reg.AssignComponent< CpntRenderModel >( wanderer,
					                                        g_modelAtlas.GetModel( PackerResources::CUBE_DAE ) );
					CpntNavAgent & navAgent = reg.AssignComponent< CpntNavAgent >( wanderer );
					navAgent.pathfindingNextSteps = path;
					CpntTransform & transform = reg.AssignComponent< CpntTransform >( wanderer );
					transform.SetTranslation( GetPointInMiddleOfCell( navAgent.pathfindingNextSteps.Last() ) );
					reg.AssignComponent< CpntSeller >( wanderer, marketEntity );
					ListenTo( MESSAGE_NAVAGENT_DESTINATION_REACHED, wanderer );
				}
			}
		}

		u32          numResourcesToFetch = 0;
		GameResource resourcesToFetch[ ( int )GameResource::NUM_RESOURCES ];

		// Let's check if we are low on resources
		ForEveryGameResource( resource ) {
			if ( marketInventory.GetResourceCapacity( resource ) > 0 ) {
				resourcesToFetch[ numResourcesToFetch++ ] = resource;
			}
		}

		if ( numResourcesToFetch > 0 && market.fetcher == INVALID_ENTITY && market.wanderer == INVALID_ENTITY ) {
			for ( u32 i = 0; i < numResourcesToFetch; i++ ) {
				ng::DynamicArray< Cell > path( 32 );
				Entity                   closestStorage = LookForStorageContainingOneOfResourceList(
                    reg, marketBuilding, resourcesToFetch + i, 1, market.fetcherCellRange, path );
				if ( closestStorage != INVALID_ENTITY ) {
					// we found a storage containing what we are looking for nearby
					// Let's spawn a fetcher
					market.fetcher = CreateResourceFetcher(
					    reg, resourcesToFetch[ i ], marketInventory.GetResourceCapacity( resourcesToFetch[ 0 ] ),
					    marketEntity );
					ListenTo( MESSAGE_ENTITY_DELETED, market.fetcher );
					break;
				}
			}
		}
	}
}

void SystemMarket::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_NAVAGENT_DESTINATION_REACHED: {
		for ( auto & [ marketEntity, market ] : reg.IterateOver< CpntMarket >() ) {
			if ( market.wanderer == msg.sender ) {
				// Let's get back resources that were not distributed
				PostMsg( MESSAGE_FULL_INVENTORY_TRANSACTION, marketEntity, msg.sender );
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
				market.fetcher = INVALID_ENTITY;
			}
		}
		break;
	}
	default:
		ng_assert_msg( false, "Message type %d can't be handled by this system\n", msg.type );
	}
}

void SystemMarket::OnCpntRemoved( Entity e, CpntMarket & t ) {
	if ( t.fetcher != INVALID_ENTITY ) {
		theGame->registery->MarkForDelete( t.fetcher );
	}
	if ( t.wanderer != INVALID_ENTITY ) {
		theGame->registery->MarkForDelete( t.wanderer );
	}
}

void SystemSeller::Update( Registery & reg, Duration ticks ) {
	for ( auto & [ e, seller ] : reg.IterateOver< CpntSeller >() ) {
		auto & transform = reg.GetComponent< CpntTransform >( e );
		Cell   currentCell = GetCellForPoint( transform.GetTranslation() );
		if ( currentCell != seller.lastCellDistributed ) {
			seller.lastCellDistributed = currentCell;

			// Four cardinal directions
			ng::StaticArray< Cell, 4 > neighbors;
			GetNeighborsOfCell( currentCell, theGame->map, neighbors );

			// Look for houses adjacent to the seller
			for ( auto & [ houseEntity, house ] : reg.IterateOver< CpntHousing >() ) {
				auto const & building = reg.GetComponent< CpntBuilding >( houseEntity );
				if ( IsCellAdjacentToBuilding( building, currentCell, theGame->map ) ) {
					// distribute resources
					auto & houseInventory = reg.GetComponent< CpntResourceInventory >( houseEntity );
					ForEveryGameResource( resource ) {
						if ( houseInventory.GetResourceCapacity( resource ) > 0 ) {
							PostTransactionMessage( resource, houseInventory.GetResourceCapacity( resource ), true,
							                        houseEntity, seller.market );
						}
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
			GetNeighborsOfCell( currentCell, theGame->map, neighbors );

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
			auto const & cpntBuilding = reg.GetComponent< CpntBuilding >( e );
			double       invEfficiency = cpntBuilding.GetInvEfficiency();
			if ( invEfficiency == 0.0f ) {
				continue;
			}
			Duration timeBetweenSpawn = serviceBuilding.durationBetweenWandererSpawns * invEfficiency;
			if ( serviceBuilding.timeSinceLastWandererSpawn < timeBetweenSpawn ) {
				serviceBuilding.timeSinceLastWandererSpawn += ticks;
			} else {
				// Let's check if we have a path for the wanderer
				ng::DynamicArray< Cell > path( serviceBuilding.wandererCellRange );
				Cell                     startingCell = GetAnyRoadConnectedToBuilding( cpntBuilding, theGame->map );
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
					reg.AssignComponent< CpntRenderModel >( wanderer,
					                                        g_modelAtlas.GetModel( PackerResources::CUBE_DAE ) );
					CpntNavAgent & navAgent = reg.AssignComponent< CpntNavAgent >( wanderer );
					navAgent.pathfindingNextSteps = path;
					navAgent.deleteAtDestination = true;
					CpntTransform & transform = reg.AssignComponent< CpntTransform >( wanderer );
					transform.SetTranslation( GetPointInMiddleOfCell( navAgent.pathfindingNextSteps.Last() ) );
					auto & serviceWanderer = reg.AssignComponent< CpntServiceWanderer >( wanderer );
					serviceWanderer.service = serviceBuilding.service;
					ListenTo( MESSAGE_NAVAGENT_DESTINATION_REACHED, wanderer );
				}
			}
		}
	}
}

void SystemServiceBuilding::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_NAVAGENT_DESTINATION_REACHED: {
		for ( auto & [ serviceBuildingEntity, serviceBuilding ] : reg.IterateOver< CpntServiceBuilding >() ) {
			if ( serviceBuilding.wanderer == msg.sender ) {
				serviceBuilding.wanderer = INVALID_ENTITY;
				serviceBuilding.timeSinceLastWandererSpawn = 0;
			}
		}
		break;
	}
	default:
		ng_assert_msg( false, "Message type %d can't be handled by this system\n", msg.type );
	}
}

void SystemServiceBuilding::OnCpntRemoved( Entity e, CpntServiceBuilding & t ) {
	if ( t.wanderer != INVALID_ENTITY ) {
		theGame->registery->MarkForDelete( t.wanderer );
	}
}

void SystemResourceInventory::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_INVENTORY_TRANSACTION: {
		CpntResourceInventory * giver = reg.TryGetComponent< CpntResourceInventory >( msg.sender );
		CpntResourceInventory * recipient = reg.TryGetComponent< CpntResourceInventory >( msg.recipient );
		if ( giver == nullptr ) {
			ng::Debugf( "A transaction has been aborted, the giver is invalid\n" );
			return;
		}
		if ( recipient == nullptr ) {
			ng::Debugf( "A transaction has been aborted, the recipient is invalid\n" );
			return;
		}
		const TransactionMessagePayload & payload = CastPayloadAs< TransactionMessagePayload >( msg.payload );

		u32 amountAvailable = giver->RemoveResource( payload.resource, payload.quantity );
		u32 amountConsumed = recipient->StoreRessource( payload.resource, amountAvailable );
		u32 amountToGiveBack = amountAvailable - amountConsumed;
		if ( amountToGiveBack > 0 && payload.acceptPayback ) {
			// Send back the amount we couldn't store
			giver->StoreRessource( payload.resource, amountToGiveBack );
		}
		PostMsg( MESSAGE_INVENTORY_TRANSACTION_COMPLETED, msg.sender, msg.recipient );
		if ( amountConsumed > 0 ) {
			PostMsg( MESSAGE_INVENTORY_UPDATE, msg.sender, INVALID_ENTITY );
			PostMsg( MESSAGE_INVENTORY_UPDATE, msg.recipient, INVALID_ENTITY );
		}
		break;
	}
	case MESSAGE_FULL_INVENTORY_TRANSACTION: {
		CpntResourceInventory * giver = reg.TryGetComponent< CpntResourceInventory >( msg.sender );
		CpntResourceInventory * recipient = reg.TryGetComponent< CpntResourceInventory >( msg.recipient );
		if ( giver == nullptr ) {
			ng::Debugf( "A transaction has been aborted, the giver is invalid\n" );
			return;
		}
		if ( recipient == nullptr ) {
			ng::Debugf( "A transaction has been aborted, the recipient is invalid\n" );
			return;
		}
		u32 amountConsumed = 0;
		for ( int i = 0; i < ( int )GameResource::NUM_RESOURCES; i++ ) {
			GameResource resource = ( GameResource )i;
			u32 amountAvailable = giver->RemoveResource( resource, recipient->GetResourceCapacity( resource ) );
			amountConsumed += recipient->StoreRessource( resource, amountAvailable );
		}
		PostMsg( MESSAGE_INVENTORY_TRANSACTION_COMPLETED, msg.sender, msg.recipient );
		if ( amountConsumed > 0 ) {
			PostMsg( MESSAGE_INVENTORY_UPDATE, msg.sender, INVALID_ENTITY );
			PostMsg( MESSAGE_INVENTORY_UPDATE, msg.recipient, INVALID_ENTITY );
		}
		break;
	}
	default:
		ng_assert_msg( false, "Message type %d can't be handled by this system\n", msg.type );
	}
}

void SystemMigrant::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_PATHFINDING_RESPONSE: {
		auto & payload = CastPayloadAs< PathfindingTaskResponse >( msg.payload );
		Entity migrant = msg.recipient;
		if ( payload.ok == false ) {
			ng::Errorf( "We couldn't find a path from a migrant to its new house\n" );
			reg.MarkForDelete( migrant );
		} else {
			CpntNavAgent & agent = reg.GetComponent< CpntNavAgent >( migrant );
			theGame->systemManager.GetSystem< SystemPathfinding >().CopyPath( payload.id, agent.pathfindingNextSteps );
			ListenTo( MESSAGE_NAVAGENT_DESTINATION_REACHED, migrant );
		}
		break;
	}
	case MESSAGE_NAVAGENT_DESTINATION_REACHED: {
		Entity migrant = msg.recipient;
		PostMsg( MESSAGE_HOUSE_MIGRANT_ARRIVED, reg.GetComponent< CpntMigrant >( migrant ).targetHouse, migrant );
		reg.MarkForDelete( migrant );
		break;
	}
	}
}

void SystemMigrant::OnCpntAttached( Entity e, CpntMigrant & t ) {
	// A migrant has been spawned, we should be waiting for a pathfinding to follow
	ListenTo( MESSAGE_PATHFINDING_RESPONSE, e );
}

void SystemBuilding::Update( Registery & reg, Duration ticks ) {
	totalEmployed = 0;
	totalEmployeesNeeded = 0;
	for ( auto & [ entity, building ] : reg.IterateOver< CpntBuilding >() ) {
		while ( building.workersEmployed < building.workersNeeded && totalUnemployed > 0 ) {
			totalUnemployed--;
			building.workersEmployed++;
		}
		totalEmployed += building.workersEmployed;
		totalEmployeesNeeded += building.workersNeeded - building.workersEmployed;
	}
}

void SystemBuilding::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_WORKER_AVAILABLE: {
		// we have a new worker to distribute
		bool addedSomewhere = false;
		for ( auto & [ entity, building ] : reg.IterateOver< CpntBuilding >() ) {
			if ( building.workersEmployed < building.workersNeeded ) {
				addedSomewhere = true;
				building.workersEmployed++;
				break;
			}
		}
		if ( !addedSomewhere ) {
			// we have nowhere to employ this worker, let's go to pole emploi
			totalUnemployed++;
		}
		break;
	}
	case MESSAGE_WORKER_REMOVED: {
		// We have to remove a worker somewhere
		bool removedSomewhere = false;
		for ( auto & [ entity, building ] : reg.IterateOver< CpntBuilding >() ) {
			if ( building.workersEmployed > 0 ) {
				removedSomewhere = true;
				building.workersEmployed--;
				break;
			}
		}
		if ( !removedSomewhere ) {
			ng_assert( totalUnemployed > 0 );
			// That's one less chomeur
			if ( totalUnemployed > 0 ) {
				totalUnemployed--;
			}
		}
		break;
	}
	case MESSAGE_ROAD_CELL_ADDED: {
		Cell cell = CastPayloadAs< Cell >( msg.payload );
		for ( auto & [ entity, building ] : reg.IterateOver< CpntBuilding >() ) {
			if ( building.hasRoadConnection == false && IsCellAdjacentToBuilding( building, cell, theGame->map ) ) {
				building.hasRoadConnection = true;
			}
		}
		break;
	}
	case MESSAGE_ROAD_CELL_REMOVED: {
		Cell removedCell = CastPayloadAs< Cell >( msg.payload );
		for ( auto & [ entity, building ] : reg.IterateOver< CpntBuilding >() ) {
			if ( building.hasRoadConnection == true &&
			     IsCellAdjacentToBuilding( building, removedCell, theGame->map ) ) {
				building.hasRoadConnection = false;
				// we removed a road connected to a building, but it still might has a connection with another cell
				for ( const Cell & cell : building.AdjacentCells( theGame->map ) ) {
					if ( theGame->map.GetTile( cell ) == MapTile::ROAD && cell != removedCell ) {
						building.hasRoadConnection = true;
						break;
					}
				}
			}
		}
		break;
	}
	}
}

void SystemBuilding::OnCpntAttached( Entity e, CpntBuilding & t ) {
	// Let's see if we have worker to attach
	while ( t.workersEmployed < t.workersNeeded && totalUnemployed > 0 ) {
		totalUnemployed--;
		t.workersEmployed++;
	}

	// Let's check if we have a road connection
	t.hasRoadConnection = false;
	for ( const Cell & cell : t.AdjacentCells( theGame->map ) ) {
		if ( theGame->map.GetTile( cell ) == MapTile::ROAD ) {
			t.hasRoadConnection = true;
			break;
		}
	}
}

void SystemBuilding::OnCpntRemoved( Entity e, CpntBuilding & t ) {
	// A building is removed, lets send its workforce to pole emploi
	totalUnemployed += t.workersEmployed;
}

void SystemBuilding::DebugDraw() { ImGui::Text( "%d chomeurs", totalUnemployed ); }
