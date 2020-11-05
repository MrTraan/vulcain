#include "pathfinding_job.h"
#include "game.h"

constexpr bool movementIsAStar( MovementAllowed movement ) {
	return ( movement == ASTAR_ALLOW_DIAGONALS || movement == ASTAR_FORBID_DIAGONALS );
}

Entity SystemPathfinding::CopyPath( pathfindingID id, ng::DynamicArray< Cell > & out ) {
	Entity targetEntity = INVALID_ENTITY;
	entriesMutex.lock();
	for ( const Entry & entry : entries ) {
		if ( entry.id == id ) {
			out.Append( entry.path );
			targetEntity = entry.targetEntity;
			break;
		}
	}
	entriesMutex.unlock();
	return targetEntity;
}

Entity SystemPathfinding::CopyAndDeletePath( pathfindingID id, ng::DynamicArray< Cell > & out ) {
	Entity targetEntity = CopyPath( id, out );
	deleteQueue.enqueue( id );
	return targetEntity;
}

static bool FindPathToCellType(
    Cell start, MapTile type, MovementAllowed movementAllowed, const Map & map, ng::DynamicArray< Cell > & path ) {
	ng_assert( movementIsAStar( movementAllowed ) );
	// This is the only case where we will not be looking for the shortest path, but the closest destination
	bool pathFound = false;
	u32  maxDistance = std::max( {
        start.x,
        map.sizeX - start.x,
        start.z,
        map.sizeZ - start.z,
    } );
	for ( int64 r = 1; r <= maxDistance && !pathFound; r++ ) {
		int64 x = start.x;
		int64 y = start.z;
		for ( int64 j = y - r; j <= y + r; j++ ) {
			for ( int64 i = x; ( ( i - x ) * ( i - x ) + ( j - y ) * ( j - y ) ) <= ( r * r ); i-- ) {
				if ( ABS( y - j ) + ABS( x - i ) == r ) {
					// in the circle
					if ( map.IsValidTile( i, j ) && map.GetTile( i, j ) == type ) {
						if ( map.IsTileAStarNavigable( type ) ) {
							// If the tile type we look for is accessible, we try to go exactly there
							path.Clear();
							pathFound = AStar( start, Cell( i, j ), movementAllowed, theGame->map, path );
							if ( pathFound )
								goto FIND_PATH_TO_CELL_TYPE_DONE;
						} else {
							// Otherwise, we try to reach a neighbor
							ng::StaticArray< Cell, 4 > neighbors;
							GetNeighborsOfCell( Cell( i, j ), map, neighbors );
							for ( Cell & neighbor : neighbors ) {
								path.Clear();
								pathFound = AStar( start, neighbor, movementAllowed, theGame->map, path );
								if ( pathFound )
									goto FIND_PATH_TO_CELL_TYPE_DONE;
							}
						}
					}
				}
			}
			for ( int64 i = x + 1; ( i - x ) * ( i - x ) + ( j - y ) * ( j - y ) <= r * r; i++ ) {
				if ( ABS( y - j ) + ABS( x - i ) == r ) {
					// in the circle
					if ( map.IsValidTile( i, j ) && map.GetTile( i, j ) == type ) {
						if ( map.IsTileAStarNavigable( type ) ) {
							// If the tile type we look for is accessible, we try to go exactly there
							path.Clear();
							pathFound = AStar( start, Cell( i, j ), movementAllowed, theGame->map, path );
							if ( pathFound )
								goto FIND_PATH_TO_CELL_TYPE_DONE;
						} else {
							// Otherwise, we try to reach a neighbor
							ng::StaticArray< Cell, 4 > neighbors;
							GetNeighborsOfCell( Cell( i, j ), map, neighbors );
							for ( Cell & neighbor : neighbors ) {
								path.Clear();
								pathFound = AStar( start, neighbor, movementAllowed, theGame->map, path );
								if ( pathFound )
									goto FIND_PATH_TO_CELL_TYPE_DONE;
							}
						}
					}
				}
			}
		}
	}
FIND_PATH_TO_CELL_TYPE_DONE:
	return pathFound;
}

void SystemPathfinding::ParallelJob() {
	pathfindingID pathToDelete;
	while ( deleteQueue.try_dequeue( pathToDelete ) ) {
		entriesMutex.lock();
		for ( auto node = entries.head; node != nullptr; node = node->next ) {
			if ( node->data.id == pathToDelete ) {
				entries.DeleteNode( node );
				break;
			}
		}
		entriesMutex.unlock();
	}

	static pathfindingID nextID = 0;
	PathfindingTask      task;
	while ( taskQueue.try_dequeue( task ) ) {
		entriesMutex.lock();
		Entry & entry = entries.Alloc();
		entry.id = nextID++;
		entriesMutex.unlock();
		bool pathFound = false;
		if ( task.type == PathfindingTask::Type::FROM_CELL_TO_CELL ) {
			if ( movementIsAStar( task.movementAllowed ) ) {
				pathFound = AStar( task.start.cell, task.goal.cell, task.movementAllowed, theGame->map, entry.path );
			} else {
				pathFound = theGame->roadNetwork.FindPath( task.start.cell, task.goal.cell, theGame->map, entry.path );
			}
		} else if ( task.type == PathfindingTask::Type::FROM_CELL_TO_BUILDING ) {
			if ( movementIsAStar( task.movementAllowed ) ) {
				for ( Cell cell : task.goal.building.AdjacentCells( theGame->map ) ) {
					entry.path.Clear();
					pathFound = AStar( task.start.cell, cell, task.movementAllowed, theGame->map, entry.path );
					if ( pathFound ) {
						break;
					}
				}
			} else {
				pathFound = FindPathFromCellToBuilding( task.start.cell, task.goal.building, theGame->map,
				                                        theGame->roadNetwork, entry.path );
			}
		} else if ( task.type == PathfindingTask::Type::FROM_CELL_TO_TILE_TYPE ) {
			pathFound = FindPathToCellType( task.start.cell, task.goal.tileType, task.movementAllowed, theGame->map,
			                                entry.path );
		} else if ( task.type == PathfindingTask::Type::FROM_BUILDING_TO_TILE_TYPE ) {
			for ( Cell cell : task.start.building.AdjacentCells( theGame->map ) ) {
				pathFound =
				    FindPathToCellType( cell, task.goal.tileType, task.movementAllowed, theGame->map, entry.path );
				if ( pathFound ) {
					break;
				}
			}
		} else if ( task.type == PathfindingTask::Type::FROM_CELL_TO_RESOURCE_STORAGE_WITH_CAPACITY ||
		            task.type == PathfindingTask::Type::FROM_BUILDING_TO_RESOURCE_STORAGE_WITH_CAPACITY ||
		            task.type == PathfindingTask::Type::FROM_CELL_TO_RESOURCE_STORAGE_WITH_STOCK ||
		            task.type == PathfindingTask::Type::FROM_BUILDING_TO_RESOURCE_STORAGE_WITH_STOCK ) {
			ng_assert_msg( task.movementAllowed == ROAD_NETWORK_AND_ROAD_BLOCK, "other methods are not handled yet" );

			// TODO: This is not thread safe at all ! CpntBuildings might get created or deleted or changed during
			// iteration
			Entity closestStorage = INVALID_ENTITY;
			u32    closestStorageDistance = UINT32_MAX;

			for ( auto & [ e, building ] : theGame->registery->IterateOver< CpntBuilding >() ) {
				if ( building.kind != BuildingKind::STORAGE_HOUSE ) {
					continue;
				}
				const CpntResourceInventory & inventory =
				    theGame->registery->GetComponent< CpntResourceInventory >( e );
				if ( ( ( task.type == PathfindingTask::Type::FROM_CELL_TO_RESOURCE_STORAGE_WITH_CAPACITY ||
				         task.type == PathfindingTask::Type::FROM_BUILDING_TO_RESOURCE_STORAGE_WITH_CAPACITY ) &&
				       inventory.GetResourceCapacity( task.goal.resourceType ) > 0 ) ||
				     ( ( task.type == PathfindingTask::Type::FROM_CELL_TO_RESOURCE_STORAGE_WITH_STOCK ||
				         task.type == PathfindingTask::Type::FROM_BUILDING_TO_RESOURCE_STORAGE_WITH_STOCK ) &&
				       inventory.GetResourceAmount( task.goal.resourceType ) > 0 ) ) {
					u32 distance = 0;

					bool subPathFound = false;
					if ( task.type == PathfindingTask::Type::FROM_CELL_TO_RESOURCE_STORAGE_WITH_CAPACITY ||
					     task.type == PathfindingTask::Type::FROM_CELL_TO_RESOURCE_STORAGE_WITH_STOCK ) {
						subPathFound =
						    FindPathFromCellToBuilding( task.start.cell, building, theGame->map, theGame->roadNetwork,
						                                entry.path, closestStorageDistance, &distance );
					} else {
						subPathFound =
						    FindPathBetweenBuildings( task.start.building, building, theGame->map, theGame->roadNetwork,
						                              entry.path, closestStorageDistance, &distance );
					}
					if ( subPathFound && distance < closestStorageDistance ) {
						closestStorage = e;
						closestStorageDistance = distance;
						pathFound = true;
						entry.targetEntity = closestStorage;
					}
				}
			}
		} else {
			ng_assert( false );
		}
		if ( !pathFound ) {
			deleteQueue.enqueue( entry.id );
		}
		PostMsg< PathfindingTaskResponse >( MESSAGE_PATHFINDING_RESPONSE,
		                                    PathfindingTaskResponse{ pathFound, entry.id }, task.requester,
		                                    INVALID_ENTITY );
	}
}

void SystemPathfinding::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_PATHFINDING_REQUEST: {
		PathfindingTask task = CastPayloadAs< PathfindingTask >( msg.payload );
		bool            ok = taskQueue.enqueue( task );
		ng_assert( ok );
		break;
	}
	case MESSAGE_PATHFINDING_DELETE_ENTRY: {
		pathfindingID id = CastPayloadAs< pathfindingID >( msg.payload );
		bool          ok = deleteQueue.enqueue( id );
		ng_assert( ok );
		break;
	}
	}
}
