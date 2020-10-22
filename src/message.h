#pragma once

#include "entity.h"
#include "ngLib/ngcontainers.h"

enum MessageType {
	MESSAGE_ENTITY_DELETED = 0,
	MESSAGE_ENTITY_CREATED,
	MESSAGE_CPNT_ATTACHED,
	MESSAGE_PATHFINDING_DESTINATION_REACHED,
	MESSAGE_PATHFINDING_MOVED_CELL,
	MESSAGE_SERVICE_PROVIDED,
	MESSAGE_INVENTORY_TRANSACTION,
	MessageType_COUNT, // leave this a the end
};

static_assert( MessageType_COUNT < 64,
               "if there are more than 64 message types, we can't use a Bitfield64 anymore to track which event a "
               "system listens to" );

constexpr u64 messagePayloadMaxSize = 0x100;
using MessagePayload = u8[ messagePayloadMaxSize ];

struct Message {
	MessageType    type;
	MessagePayload payload;
	Entity         recipient;
	Entity         sender;
};

template < typename T > Message & FillMessagePayload( Message & msg, const T & payload ) {
	static_assert( sizeof( T ) < messagePayloadMaxSize );
	memcpy( msg.payload, &payload, sizeof( T ) );
	return msg;
}

template < typename T > T & CastPayloadAs( MessagePayload & payload ) {
	static_assert( sizeof( T ) < messagePayloadMaxSize );
	return ( ( T * )payload )[ 0 ];
}

template < typename T > const T & CastPayloadAs( const MessagePayload & payload ) {
	static_assert( sizeof( T ) < messagePayloadMaxSize );
	return ( ( const T * )payload )[ 0 ];
}

void        PostMsg( Message msg );
inline void PostMsg( MessageType type, Entity recipient, Entity sender ) {
	Message msg{};
	msg.type = type;
	msg.recipient = recipient;
	msg.sender = sender;
	PostMsg( msg );
}
template < typename T > void PostMsg( MessageType type, const T & payload, Entity recipient, Entity sender ) {
	Message msg{};
	msg.type = type;
	msg.recipient = recipient;
	msg.sender = sender;
	FillMessagePayload< T >( msg, payload );
	PostMsg( msg );
}
