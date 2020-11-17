#include "resource_fetcher.h"
#include "../game.h"
#include "../mesh.h"
#include "../packer_resource_list.h"
#include "../pathfinding_job.h"
#include "building.h"

Entity CreateResourceFetcher( Registery & reg, GameResource resourceToFetch, u32 maxAmount, Entity parent ) {
	Entity e = reg.CreateEntity();
	auto & transform = reg.AssignComponent< CpntTransform >( e );
	auto & building = reg.GetComponent< CpntBuilding >( parent );
	for ( Cell cell : building.AdjacentCells( theGame->map ) ) {
		if ( theGame->map.GetTile( cell ) == MapTile::ROAD ) {
			transform.SetTranslation( GetPointInMiddleOfCell( cell ) );
			break;
		}
	}
	reg.AssignComponent< CpntRenderModel >( e, g_modelAtlas.GetModel( PackerResources::CUBE_DAE ) );
	reg.AssignComponent< CpntNavAgent >( e );
	auto & inventory = reg.AssignComponent< CpntResourceInventory >( e );
	inventory.SetResourceMaxCapacity( resourceToFetch, maxAmount );
	auto & fetcher = reg.AssignComponent< CpntResourceFetcher >( e );
	fetcher.parent = parent;
	fetcher.resourceToFetch = resourceToFetch;
	return e;
}

void SystemFetcher::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_PATHFINDING_RESPONSE: {
		PathfindingTaskResponse response = CastPayloadAs< PathfindingTaskResponse >( msg.payload );
		CpntResourceFetcher &   fetcher = reg.GetComponent< CpntResourceFetcher >( msg.recipient );
		if ( !response.ok ) {
			reg.MarkForDelete( msg.recipient );
			return;
		}
		auto & agent = reg.GetComponent< CpntNavAgent >( msg.recipient );
		auto & transform = reg.GetComponent< CpntTransform >( msg.recipient );
		fetcher.target = theGame->systemManager.GetSystem< SystemPathfinding >().CopyAndDeletePath(
		    response.id, agent.pathfindingNextSteps );
		transform.SetTranslation( GetPointInMiddleOfCell( agent.pathfindingNextSteps.Last() ) );
		break;
	}
	case MESSAGE_NAVAGENT_DESTINATION_REACHED: {
		Entity                fetcher = msg.recipient;
		CpntResourceFetcher & cpntFetcher = reg.GetComponent< CpntResourceFetcher >( fetcher );
		if ( cpntFetcher.direction == CpntResourceFetcher::CurrentDirection::TO_TARGET ) {
			// We arrived at our target
			CpntBuilding * targetBuilding = reg.TryGetComponent< CpntBuilding >( cpntFetcher.target );
			if ( targetBuilding == nullptr ) {
				// Oh no, our target building has been destroyed!
				ng::Infof( "A fetcher destination has been destroyed" );
				reg.MarkForDelete( fetcher );
				return;
			}
			CpntResourceInventory & fetcherInventory = reg.GetComponent< CpntResourceInventory >( fetcher );

			// Let's fill our inventory
			u32 capacity = fetcherInventory.GetResourceCapacity( cpntFetcher.resourceToFetch );
			PostTransactionMessage( cpntFetcher.resourceToFetch, capacity, true, fetcher, cpntFetcher.target );

			// Now let's go back to our parent
			cpntFetcher.direction = CpntResourceFetcher::CurrentDirection::TO_PARENT;
			CpntBuilding * parentBuilding = reg.TryGetComponent< CpntBuilding >( cpntFetcher.parent );
			if ( parentBuilding == nullptr ) {
				ng::Errorf( "A fetcher arrived at destination, but there is no parent to go back anymore! :(\n" );
				reg.MarkForDelete( fetcher );
			} else {
				PathfindingTask task{};
				task.type = PathfindingTask::Type::FROM_CELL_TO_BUILDING;
				task.start.cell = GetCellForTransform( reg.GetComponent< CpntTransform >( fetcher ) );
				task.goal.building = *parentBuilding;
				task.movementAllowed = ROAD_NETWORK_AND_ROAD_BLOCK;
				task.requester = fetcher;
				PostMsg< PathfindingTask >( MESSAGE_PATHFINDING_REQUEST, task, INVALID_ENTITY, fetcher );
			}
		} else if ( cpntFetcher.direction == CpntResourceFetcher::CurrentDirection::TO_PARENT ) {
			// We are back to the parent
			// Let's empty our inventory
			Entity                  fetcher = msg.recipient;
			CpntResourceFetcher &   cpntFetcher = reg.GetComponent< CpntResourceFetcher >( fetcher );
			for ( u32 i = 0; i < ( u32 )GameResource::NUM_RESOURCES; i++ ) {
				PostMsg( MESSAGE_FULL_INVENTORY_TRANSACTION, cpntFetcher.parent, fetcher );
			}
			reg.MarkForDelete( fetcher );
		}
		break;
	}
	default:
		break;
	}
}

void SystemFetcher::OnCpntAttached( Entity e, CpntResourceFetcher & t ) {
	ListenTo( MESSAGE_NAVAGENT_DESTINATION_REACHED, e );
	ListenTo( MESSAGE_PATHFINDING_RESPONSE, e );

	CpntBuilding * buildingSpawner = theGame->registery->TryGetComponent< CpntBuilding >( t.parent );
	if ( buildingSpawner == nullptr ) {
		theGame->registery->MarkForDelete( e );
		return;
	}

	PathfindingTask task{};
	task.type = PathfindingTask::Type::FROM_BUILDING_TO_RESOURCE_STORAGE_WITH_STOCK;
	task.start.building = *buildingSpawner;
	task.goal.resourceType = t.resourceToFetch;
	task.movementAllowed = ROAD_NETWORK_AND_ROAD_BLOCK;
	task.requester = e;
	PostMsg< PathfindingTask >( MESSAGE_PATHFINDING_REQUEST, task, INVALID_ENTITY, e );
}
