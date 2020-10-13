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
	MsgServiceProvided serviceProvided;
	while ( serviceMessages.try_dequeue( serviceProvided ) ) {
		CpntHousing * housing = reg.TryGetComponent< CpntHousing >( serviceProvided.target );
		if ( housing != nullptr ) {
			housing->lastServiceAccess[ ( u32 )serviceProvided.service ] = theGame->clock;
		}
	}

	totalPopulation = 0;
	for ( auto & [ e, housing ] : reg.IterateOver< CpntHousing >() ) {
		// Check if population should grow in house
		if ( housing.numCurrentlyLiving < housing.maxHabitants ) {
			housing.numCurrentlyLiving++;
		}
		totalPopulation += housing.numCurrentlyLiving;
	}
}

void OnAgentArrived( Registery & reg, Entity sender, Entity receiver ) {
	ng_assert( sender == receiver );
	ng::Printf( "Entity %lu has arrived!\n", receiver );

	// Look for a storage house next to receiver
	CpntTransform & transform = reg.GetComponent< CpntTransform >( receiver );
	Cell            position = GetCellForPoint( transform.GetTranslation() );
	Entity          closestStorage = INVALID_ENTITY_ID;
	for ( auto & [ e, storage ] : reg.IterateOver< CpntResourceInventory >() ) {
		CpntBuilding & building = reg.GetComponent< CpntBuilding >( e );
		if ( IsCellAdjacentToBuilding( building, position, theGame->map ) ) {
			closestStorage = e;
			break;
		}
	}
	if ( closestStorage == INVALID_ENTITY_ID ) {
		// TODO: Handle that the storage has been removed
		ng::Errorf( "An agent was directed to a storage that disappeared in the meantime" );
	} else {
		// Store what we have on us in the storage
		CpntResourceInventory & storage = reg.GetComponent< CpntResourceInventory >( closestStorage );
		CpntResourceCarrier &   carrier = reg.GetComponent< CpntResourceCarrier >( receiver );
		for ( auto & resourceTuple : carrier.resources ) {
			storage.StoreRessource( resourceTuple.First(), resourceTuple.Second() );
		}
	}

	reg.MarkForDelete( receiver );
}

Entity LookForClosestBuildingKind( Registery &                reg,
                                   BuildingKind               kind,
                                   const CpntBuilding &       origin,
                                   ng::DynamicArray< Cell > & outPath ) {
	Entity closestStorage = INVALID_ENTITY_ID;
	u32    closestStorageDistance = ULONG_MAX;

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

void SystemBuildingProducing::Update( Registery & reg, Duration ticks ) {
	for ( auto & [ e, producer ] : reg.IterateOver< CpntBuildingProducing >() ) {
		producer.timeSinceLastProduction += ticks;
		if ( producer.timeSinceLastProduction >= producer.timeToProduceBatch ) {
			const CpntBuilding & cpntBuilding = reg.GetComponent< CpntBuilding >( e );

			// Find a path to store house
			thread_local ng::DynamicArray< Cell > path( 32 );
			Entity closestStorage = LookForClosestBuildingKind( reg, BuildingKind::STORAGE_HOUSE, cpntBuilding, path );

			if ( closestStorage == INVALID_ENTITY_ID ) {
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
				CpntResourceCarrier & resourceCarrier = reg.AssignComponent< CpntResourceCarrier >( carrier );
				resourceCarrier.resources.PushBack( { producer.resource, producer.batchSize } );

				transform.SetTranslation( GetPointInMiddleOfCell( navAgent.pathfindingNextSteps.Last() ) );
				reg.messageBroker.AddListener( carrier, carrier, OnAgentArrived,
				                               MESSAGE_PATHFINDING_DESTINATION_REACHED );
			}
		}
	}
}

u32 CpntResourceInventory::StoreRessource( GameResource resource, u32 amount ) {
	if ( !storage.contains( resource ) ) {
		return 0;
	}
	if ( amount + storage[ resource ].currentAmount > storage[ resource ].max ) {
		u32 consumed = storage[ resource ].max - storage[ resource ].currentAmount;
		storage[ resource ].currentAmount += consumed;
		return consumed;
	} else {
		storage[ resource ].currentAmount += amount;
		return amount;
	}
}

bool CpntResourceInventory::IsEmpty() const {
	if ( storage.size() == 0 ) {
		return true;
	}
	for ( const auto & [ resource, capacity ] : storage ) {
		if ( capacity.currentAmount > 0 ) {
			return false;
		}
	}
	return true;
}

void SystemMarket::Update( Registery & reg, Duration ticks ) {
	for ( auto & [ e, market ] : reg.IterateOver< CpntMarket >() ) {
		CpntResourceInventory & marketInventory = reg.GetComponent< CpntResourceInventory >( e );
		if ( market.wanderer == INVALID_ENTITY_ID ) {
			if ( market.timeSinceLastWandererSpawn < market.durationBetweenWandererSpawns ) {
				market.timeSinceLastWandererSpawn += ticks;
			} else if ( marketInventory.IsEmpty() == false ) {
				// Let's check if we have a path for the wanderer
				ng::DynamicArray< Cell > path( market.wandererCellRange );
				Cell                     startingCell =
				    GetAnyRoadConnectedToBuilding( reg.GetComponent< CpntBuilding >( e ), theGame->map );
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
					inventory.AccecptNewResource( GameResource::WHEAT, 10 );
					marketInventory.storage[ GameResource::WHEAT ].currentAmount -= inventory.StoreRessource(
					    GameResource::WHEAT, marketInventory.storage[ GameResource::WHEAT ].currentAmount );
					reg.messageBroker.AddListener(
					    e, wanderer,
					    []( Registery & reg, Entity sender, Entity receiver ) {
						    ng_assert( reg.HasComponent< CpntMarket >( receiver ) );
						    ng_assert( reg.HasComponent< CpntResourceInventory >( sender ) );

						    // Let's get back resources that were not distributed
						    auto & wandererStorage = reg.GetComponent< CpntResourceInventory >( sender );
						    auto & marketStorage = reg.GetComponent< CpntResourceInventory >( receiver );
						    for ( auto & [ resource, capacity ] : wandererStorage.storage ) {
							    marketStorage.StoreRessource( resource, capacity.currentAmount );
						    }

						    auto & market = reg.GetComponent< CpntMarket >( receiver );
						    market.wanderer = INVALID_ENTITY_ID;
						    market.timeSinceLastWandererSpawn = 0;
						    ng::Printf( "Wanderer has arrived\n" );
						    reg.MarkForDelete( sender );
					    },
					    MESSAGE_PATHFINDING_DESTINATION_REACHED );
				}
			}
		}
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
						for ( auto & [ resource, capacity ] : sellerInventory.storage ) {
							if ( capacity.currentAmount > 0 && houseInventory.AccecptsResource( resource ) ) {
								u32 amountConsumed = houseInventory.StoreRessource( resource, capacity.currentAmount );
								ng::Printf( "Distributed %d resource with id %d\n", amountConsumed, ( int )resource );
								capacity.currentAmount -= amountConsumed;
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
						theGame->systemManager.GetSystem< SystemHousing >().NotifyServiceFulfilled( wanderer.service,
						                                                                            houseEntity );
						break; // no need to look for other cells, we found the building
					}
				}
			}
		}
	}
}

void SystemServiceBuilding::Update( Registery & reg, Duration ticks ) {
	for ( auto & [ e, serviceBuilding ] : reg.IterateOver< CpntServiceBuilding >() ) {
		if ( serviceBuilding.wanderer == INVALID_ENTITY_ID ) {
			if ( serviceBuilding.timeSinceLastWandererSpawn < serviceBuilding.durationBetweenWandererSpawns ) {
				serviceBuilding.timeSinceLastWandererSpawn += ticks;
			} else {
				// Let's check if we have a path for the wanderer
				ng::DynamicArray< Cell > path( serviceBuilding.wandererCellRange );
				Cell                     startingCell =
				    GetAnyRoadConnectedToBuilding( reg.GetComponent< CpntBuilding >( e ), theGame->map );
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
					reg.messageBroker.AddListener(
					    e, wanderer,
					    []( Registery & reg, Entity sender, Entity receiver ) {
						    ng_assert( reg.HasComponent< CpntServiceBuilding >( receiver ) );

						    auto & serviceBuilding = reg.GetComponent< CpntServiceBuilding >( receiver );
						    serviceBuilding.wanderer = INVALID_ENTITY_ID;
						    serviceBuilding.timeSinceLastWandererSpawn = 0;
						    ng::Printf( "service wanderer has arrived\n" );
						    reg.MarkForDelete( sender );
					    },
					    MESSAGE_PATHFINDING_DESTINATION_REACHED );
				}
			}
		}
	}
}
