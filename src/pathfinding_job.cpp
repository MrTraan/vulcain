#include "pathfinding_job.h"
#include "game.h"

void SystemPathfinding::CopyPath( pathfindingID id, ng::DynamicArray< Cell > & out ) {
	entriesMutex.lock();
	for ( const Entry & entry : entries ) {
		if ( entry.id == id ) {
			out.Append( entry.path );
			break;
		}
	}
	entriesMutex.unlock();
}

void SystemPathfinding::CopyAndDeletePath( pathfindingID id, ng::DynamicArray< Cell > & out ) {
	CopyPath( id, out );
	deleteQueue.enqueue( id );
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
			pathFound = AStar( task.startCell, task.goalCell, task.movementAllowed, theGame->map, entry.path );
		} else if ( task.type == PathfindingTask::Type::FROM_CELL_TO_BUILDING ) {
			for ( Cell cell : task.goalBuilding.AdjacentCells( theGame->map ) ) {
				entry.path.Clear();
				pathFound = AStar( task.startCell, cell, task.movementAllowed, theGame->map, entry.path );
				if ( pathFound ) {
					break;
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
