#include "registery.h"
#include "message.h"

void Registery::MarkForDelete( Entity e ) {
	markedForDeleteEntityIds.enqueue( e );
	PostMsg( MESSAGE_ENTITY_DELETED, e, INVALID_ENTITY );
}
