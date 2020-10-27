#include "system.h"
#include "game_time.h"
#include "registery.h"
#include <chrono>
#include <tracy/Tracy.hpp>
#include "pathfinding_job.h"

std::atomic_bool jobsShouldRun = true;

SystemManager::~SystemManager() {
	jobsShouldRun.store( false );
	for ( std::thread * job : jobs ) {
		job->join();
		delete job;
	}
	for ( auto [ type, system ] : systems ) {
		delete system;
	}
}

void SystemManager::Update( Registery & reg, Duration ticks ) {
	ZoneScoped;

	for ( auto [ type, system ] : systems ) {
		system->Update( reg, ticks );
	}

	// Flush messages
	bool messageQueuesAreEmpty = false;
	while ( !messageQueuesAreEmpty ) {
		for ( auto [ type, system ] : systems ) {
			Message msg{};
			while ( system->messageQueue.try_dequeue( msg ) ) {
				if ( system->globalListenerMask.Test( ( u32 )msg.type ) ) {
					system->HandleMessage( reg, msg );
				} else {
					for ( ng::Tuple< Entity, ng::Bitfield64 > tuple : system->eventListenerMask ) {
						if ( tuple.First() == msg.recipient && tuple.Second().Test( ( u32 )msg.type ) ) {
							system->HandleMessage( reg, msg );
						}
					}
				}
			}
		}
		messageQueuesAreEmpty = true;
		for ( auto [ type, system ] : systems ) {
			messageQueuesAreEmpty &= system->messageQueue.size_approx() == 0;
		}
	}

	// Flush delete queue
	Entity id = INVALID_ENTITY;
	while ( reg.markedForDeleteEntityIds.try_dequeue( id ) ) {
		if ( reg.DestroyEntity( id ) ) {
			// Remove listeners listening to the destroyed entity
			for ( auto [ type, system ] : systems ) {
				for ( int64 i = ( int64 )system->eventListenerMask.Size() - 1; i >= 0; i-- ) {
					const auto & tuple = system->eventListenerMask[ i ];
					if ( tuple.First() == id ) {
						system->eventListenerMask.DeleteIndexFast( i );
					}
				}
			}
		}
	}
}

void SystemParallelTask( ISystem * system ) {
	using namespace std::chrono_literals;
	while ( jobsShouldRun.load() == true ) {
		system->ParallelJob();
		std::this_thread::sleep_for( 16ms );
	}
}

void SystemManager::StartJobs() {
	std::thread * astarThread = new std::thread( SystemParallelTask, &( GetSystem< SystemPathfinding >() ) );
	jobs.PushBack( astarThread );
}
