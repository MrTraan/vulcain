#include "system.h"
#include "game_time.h"
#include "registery.h"
#include <tracy/Tracy.hpp>

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
