#include "woodworking.h"
#include "../game.h"
#include "../mesh.h"
#include "../packer_resource_list.h"
#include "../pathfinding_job.h"
#include "building.h"
#include "delivery.h"

void SystemWoodshop::Update( Registery & reg, Duration ticks ) {
	for ( auto [ entity, woodshop ] : reg.IterateOver< CpntWoodshop >() ) {
		auto & building = reg.GetComponent< CpntBuilding >( entity );
		double invEfficiency = building.GetInvEfficiency();
		if ( invEfficiency == 0.0f ) {
			continue;
		}
		if ( woodshop.deliveryGuy != INVALID_ENTITY ) {
			continue;
		}
		woodshop.timeSinceLastWorkerSpawned += ticks;
		Duration timeBetweenSpawns = Duration( ( double )woodshop.timeBetweenWorkerMissions * invEfficiency );
		if ( woodshop.worker == INVALID_ENTITY && woodshop.timeSinceLastWorkerSpawned >= timeBetweenSpawns ) {
			// it's time to spawn a new worker
			Entity worker = reg.CreateEntity();
			woodshop.worker = worker;
			ListenTo( MESSAGE_ENTITY_DELETED, worker );
			reg.AssignComponent< CpntNavAgent >( worker );
			reg.AssignComponent< CpntTransform >( worker );
			reg.AssignComponent< CpntRenderModel >( worker, g_modelAtlas.GetModel( PackerResources::CUBE_DAE ) );
			auto & cpntWoodworker = reg.AssignComponent< CpntWoodworker >( worker );
			cpntWoodworker.woodshop = entity;
			cpntWoodworker.woodshop = entity;
		}
	}
}

void SystemWoodshop::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_WOODSHOP_WORKER_RETURNED: {
		auto &                woodshop = reg.GetComponent< CpntWoodshop >( msg.recipient );
		CpntResourceInventory inventory;
		inventory.SetResourceMaxCapacity( GameResource::WOOD, 1 );
		inventory.StoreRessource( GameResource::WOOD, 1 );
		woodshop.deliveryGuy = CreateDeliveryGuy( reg, msg.recipient, inventory );
		ListenTo( MESSAGE_ENTITY_DELETED, woodshop.deliveryGuy );
		break;
	}
	case MESSAGE_ENTITY_DELETED: {
		for ( auto [ entity, woodshop ] : reg.IterateOver< CpntWoodshop >() ) {
			if ( woodshop.worker == msg.recipient ) {
				woodshop.worker = INVALID_ENTITY;
				woodshop.timeSinceLastWorkerSpawned = 0;
			}
			if ( woodshop.deliveryGuy == msg.recipient ) {
				woodshop.deliveryGuy = INVALID_ENTITY;
			}
		}
		break;
	}
	default:
		break;
	}
}

void SystemWoodshop::OnCpntAttached( Entity e, CpntWoodshop & t ) { ListenTo( MESSAGE_WOODSHOP_WORKER_RETURNED, e ); }

void SystemWoodshop::OnCpntRemoved( Entity e, CpntWoodshop & t ) {
	if ( t.worker != INVALID_ENTITY ) {
		theGame->registery->MarkForDelete( t.worker );
	}
}

void SystemWoodworker::Update( Registery & reg, Duration ticks ) {
	for ( auto [ e, woodworker ] : reg.IterateOver< CpntWoodworker >() ) {
		if ( woodworker.choppingSince > 0 ) {
			woodworker.choppingSince += ticks;
			if ( woodworker.choppingSince >= woodworker.timeToChopOneTree ) {
				// Time to get back to the woodshop
				woodworker.currentDestination = CpntWoodworker::Destination::TO_WOODSHOP;
				woodworker.choppingSince = 0;
				CpntBuilding &  targetBuilding = reg.GetComponent< CpntBuilding >( woodworker.woodshop );
				PathfindingTask task{};
				task.type = PathfindingTask::Type::FROM_CELL_TO_BUILDING;
				task.start.cell = GetCellForPoint( reg.GetComponent< CpntTransform >( e ).GetTranslation() );
				task.goal.building = targetBuilding;
				task.movementAllowed = ASTAR_ALLOW_DIAGONALS;
				task.requester = e;
				PostMsg< PathfindingTask >( MESSAGE_PATHFINDING_REQUEST, task, INVALID_ENTITY, e );
				ListenTo( MESSAGE_PATHFINDING_RESPONSE, e );
			}
		}
	}
}

void SystemWoodworker::OnCpntAttached( Entity e, CpntWoodworker & t ) {
	// Let's find a path to the nearest tree
	CpntBuilding * woodshop = theGame->registery->TryGetComponent< CpntBuilding >( t.woodshop );
	if ( woodshop ) {
		PathfindingTask task{};
		task.type = PathfindingTask::Type::FROM_BUILDING_TO_TILE_TYPE;
		task.start.building = *woodshop;
		task.goal.tileType = MapTile::TREE;
		task.movementAllowed = ASTAR_ALLOW_DIAGONALS;
		task.requester = e;
		PostMsg< PathfindingTask >( MESSAGE_PATHFINDING_REQUEST, task, INVALID_ENTITY, e );
		ListenTo( MESSAGE_PATHFINDING_RESPONSE, e );
	}
}

void SystemWoodworker::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_PATHFINDING_RESPONSE: {
		auto &           payload = CastPayloadAs< PathfindingTaskResponse >( msg.payload );
		CpntWoodworker * woodworker = reg.TryGetComponent< CpntWoodworker >( msg.recipient );
		if ( woodworker != nullptr ) {
			if ( payload.ok == false ) {
				ng::Errorf( "Could not find a path for woodworker\n" );
				reg.MarkForDelete( msg.recipient );
				return;
			}
			CpntNavAgent &  navAgent = reg.GetComponent< CpntNavAgent >( msg.recipient );
			CpntTransform & transform = reg.GetComponent< CpntTransform >( msg.recipient );
			theGame->systemManager.GetSystem< SystemPathfinding >().CopyAndDeletePath( payload.id,
			                                                                           navAgent.pathfindingNextSteps );
			transform.SetTranslation( GetPointInMiddleOfCell( navAgent.pathfindingNextSteps.Last() ) );
			ListenTo( MESSAGE_NAVAGENT_DESTINATION_REACHED, msg.recipient );
		}
		break;
	}
	case MESSAGE_NAVAGENT_DESTINATION_REACHED: {
		CpntWoodworker * woodworker = reg.TryGetComponent< CpntWoodworker >( msg.recipient );
		if ( woodworker != nullptr ) {
			if ( woodworker->currentDestination == CpntWoodworker::Destination::TO_TREE ) {
				woodworker->choppingSince = 1;
			} else if ( woodworker->currentDestination == CpntWoodworker::Destination::TO_WOODSHOP ) {
				PostMsg( MESSAGE_WOODSHOP_WORKER_RETURNED, woodworker->woodshop, msg.recipient );
				reg.MarkForDelete( msg.recipient );
			}
		}
		break;
	}
	default:
		break;
	}
}
