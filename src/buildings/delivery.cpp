#include "delivery.h"
#include "../game.h"
#include "../mesh.h"
#include "../ngLib/nglib.h"
#include "../packer_resource_list.h"
#include "../pathfinding_job.h"
#include "building.h"

static GameResource GetNextResourceToDeliver( const CpntResourceInventory & inventory ) {
	for ( int i = 0; i < ( int )GameResource::NUM_RESOURCES; i++ ) {
		if ( inventory.GetResourceAmount( ( GameResource )i ) > 0 ) {
			return ( GameResource )i;
		}
	}
	ng_assert( false );
	return GameResource::WHEAT;
}

Entity CreateDeliveryGuy( Registery & reg, Entity spawner, const CpntResourceInventory & inventory ) {
	Entity e = reg.CreateEntity();
	auto & transform = reg.AssignComponent< CpntTransform >( e );
	auto & building = reg.GetComponent< CpntBuilding >( spawner );
	for ( Cell cell : building.AdjacentCells( theGame->map ) ) {
		if ( theGame->map.GetTile( cell ) == MapTile::ROAD ) {
			transform.SetTranslation( GetPointInMiddleOfCell( cell ) );
			break;
		}
	}
	reg.AssignComponent< CpntRenderModel >( e, g_modelAtlas.GetModel( PackerResources::CUBE_DAE ) );
	reg.AssignComponent< CpntNavAgent >( e );
	reg.AssignComponent< CpntResourceInventory >( e, inventory );
	auto & guy = reg.AssignComponent< CpntDeliveryGuy >( e );
	guy.buildingSpawner = spawner;
	return e;
}

void SystemDeliveryGuy::OnCpntAttached( Entity e, CpntDeliveryGuy & t ) {
	ListenTo( MESSAGE_PATHFINDING_RESPONSE, e );
	ListenTo( MESSAGE_NAVAGENT_DESTINATION_REACHED, e );
	ListenTo( MESSAGE_INVENTORY_TRANSACTION_COMPLETED, e );

	// We should find a path to a storage
	ng_assert( theGame->registery->HasComponent< CpntResourceInventory >( e ) );
	auto & inventory = theGame->registery->GetComponent< CpntResourceInventory >( e );
	if ( inventory.IsEmpty() ) {
		ng_assert_msg( false, "Why do we have a delivery guy with an empty inventory?" );
		theGame->registery->MarkForDelete( e );
		return;
	}

	GameResource nextResourceToDeliver = GetNextResourceToDeliver( inventory );

	CpntBuilding * buildingSpawner = theGame->registery->TryGetComponent< CpntBuilding >( t.buildingSpawner );
	if ( buildingSpawner == nullptr ) {
		theGame->registery->MarkForDelete( e );
		return;
	}

	PathfindingTask task{};
	task.type = PathfindingTask::Type::FROM_BUILDING_TO_RESOURCE_STORAGE_WITH_CAPACITY;
	task.start.building = *buildingSpawner;
	task.goal.resourceType = nextResourceToDeliver;
	task.movementAllowed = ROAD_NETWORK_AND_ROAD_BLOCK;
	task.requester = e;
	PostMsg< PathfindingTask >( MESSAGE_PATHFINDING_REQUEST, task, INVALID_ENTITY, e );
}

void SystemDeliveryGuy::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_PATHFINDING_RESPONSE: {
		PathfindingTaskResponse response = CastPayloadAs< PathfindingTaskResponse >( msg.payload );
		CpntDeliveryGuy &       guy = reg.GetComponent< CpntDeliveryGuy >( msg.recipient );
		if ( !response.ok ) {
			guy.isStuck = true;
			return;
		}
		auto & agent = reg.GetComponent< CpntNavAgent >( msg.recipient );
		auto & transform = reg.GetComponent< CpntTransform >( msg.recipient );
		guy.targetEntity = theGame->systemManager.GetSystem< SystemPathfinding >().CopyAndDeletePath(
		    response.id, agent.pathfindingNextSteps );
		transform.SetTranslation( GetPointInMiddleOfCell( agent.pathfindingNextSteps.Last() ) );
		break;
	}
	case MESSAGE_NAVAGENT_DESTINATION_REACHED: {
		CpntDeliveryGuy & guy = reg.GetComponent< CpntDeliveryGuy >( msg.recipient );
		CpntBuilding *    targetBuilding = reg.TryGetComponent< CpntBuilding >( guy.targetEntity );
		if ( targetBuilding == nullptr ) {
			// TODO: Handle that the storage has been removed
			ng::Errorf( "An agent was directed to a storage that disappeared in the meantime" );
			guy.isStuck = true;
		} else {
			// Store what we have on us in the storage
			PostMsg( MESSAGE_FULL_INVENTORY_TRANSACTION, guy.targetEntity, msg.recipient );
		}
		break;
	}
	case MESSAGE_INVENTORY_TRANSACTION_COMPLETED: {
		CpntDeliveryGuy * guy = reg.TryGetComponent< CpntDeliveryGuy >( msg.recipient );
		if ( guy ) {
			auto & inventory = theGame->registery->GetComponent< CpntResourceInventory >( msg.recipient );
			if ( inventory.IsEmpty() ) {
				// we have stored all of our inventory, we are now done!
				reg.MarkForDelete( msg.recipient );
			} else {
				// we should look for more storage
				ng::Debugf( "one storage wasnt enough!\n" );
				GameResource    nextResourceToDeliver = GetNextResourceToDeliver( inventory );
				PathfindingTask task{};
				task.type = PathfindingTask::Type::FROM_CELL_TO_RESOURCE_STORAGE_WITH_CAPACITY;
				task.start.cell =
				    GetCellForPoint( reg.GetComponent< CpntTransform >( msg.recipient ).GetTranslation() );
				task.goal.resourceType = nextResourceToDeliver;
				task.movementAllowed = ROAD_NETWORK_AND_ROAD_BLOCK;
				task.requester = msg.recipient;
				PostMsg< PathfindingTask >( MESSAGE_PATHFINDING_REQUEST, task, INVALID_ENTITY, msg.recipient );
			}
		}
		break;
	}
	}
}

void SystemDeliveryGuy::Update( Registery & reg, Duration ticks ) {
	for ( auto [ e, guy ] : reg.IterateOver< CpntDeliveryGuy >() ) {
		if ( guy.isStuck == true &&
		     theGame->clock - guy.lastPathfindingTry > CpntDeliveryGuy::durationBetweenTwoPathfindingTry ) {
			guy.isStuck = false;
			guy.lastPathfindingTry = theGame->clock;
			const CpntResourceInventory & inventory = reg.GetComponent< CpntResourceInventory >( e );
			GameResource                  nextResourceToDeliver = GetNextResourceToDeliver( inventory );
			PathfindingTask               task{};
			task.type = PathfindingTask::Type::FROM_CELL_TO_RESOURCE_STORAGE_WITH_CAPACITY;
			task.start.cell = GetCellForPoint( reg.GetComponent< CpntTransform >( e ).GetTranslation() );
			task.goal.resourceType = nextResourceToDeliver;
			task.movementAllowed = ROAD_NETWORK_AND_ROAD_BLOCK;
			task.requester = e;
			PostMsg< PathfindingTask >( MESSAGE_PATHFINDING_REQUEST, task, INVALID_ENTITY, e );
		}
	}
}
