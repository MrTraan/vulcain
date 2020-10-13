#pragma once

#include "entity.h"
#include <map>
#include <vector>

enum MessageType {
	MESSAGE_PATHFINDING_DESTINATION_REACHED,
	MESSAGE_PATHFINDING_MOVED_CELL,
	MessageType_COUNT, // leave this a the end
};

struct Registery;

typedef void ( *MessageHandler )( Registery & reg, Entity sender, Entity receiver );

struct MessageBroker {
	struct Listener {
		Entity         listenerId;
		Entity         listenTo;
		MessageHandler handler;
	};
	// An array of pair, first entity is the listener, second is the entity it listens to
	std::vector< Listener > listeners[ MessageType_COUNT ];

	void AddListener( Entity listener, Entity listenTo, MessageHandler handler, MessageType type ) {
		listeners[ type ].push_back( { listener, listenTo, handler } );
	}

	void RemoveListener( Entity listener, MessageType type ) {
		auto & v = listeners[ type ];
		for ( auto it = v.begin(); it != v.end(); ) {
			if ( ( *it ).listenerId == listener ) {
				it = v.erase( it );
			} else {
				it++;
			}
		}
	}

	void RemoveListener( Entity listener ) {
		for ( int i = 0; i < MessageType_COUNT; i++ ) {
			auto & v = listeners[ i ];
			for ( auto it = v.begin(); it != v.end(); ) {
				if ( ( *it ).listenerId == listener ) {
					it = v.erase( it );
				} else {
					it++;
				}
			}
		}
	}

	void BroadcastMessage( Registery & reg, Entity emitter, MessageType type ) {
		for ( auto & listener : listeners[ type ] ) {
			if ( listener.listenTo == emitter ) {
				listener.handler( reg, emitter, listener.listenerId );
			}
		}
	}
};