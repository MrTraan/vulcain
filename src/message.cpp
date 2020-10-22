#include "message.h"
#include "game.h"

void PostMsg( Message msg ) {
	for ( auto & [ hash, system ] : theGame->systemManager.systems ) {
		bool ok = system->messageQueue.enqueue( msg );
		ng_assert( ok );
	}
}
