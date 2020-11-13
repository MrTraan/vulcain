#include "debug_dump.h"
#include "../game.h"
#include "resource_fetcher.h"

void SystemDebugDump::OnCpntAttached( Entity e, CpntDebugDump & t ) { ListenTo( MESSAGE_INVENTORY_UPDATE, e ); }

void SystemDebugDump::OnCpntRemoved( Entity e, CpntDebugDump & t ) {
	if ( t.fetcher != INVALID_ENTITY ) {
		theGame->registery->MarkForDelete( t.fetcher );
	}
}

void SystemDebugDump::Update( Registery & reg, Duration ticks ) {
	for ( auto [ e, dump ] : reg.IterateOver< CpntDebugDump >() ) {
		if ( dump.fetcher == INVALID_ENTITY ) {
			dump.fetcher = CreateResourceFetcher( reg, dump.resourceToDump, 100, e );
			ListenTo(MESSAGE_ENTITY_DELETED, dump.fetcher);
		}
	}
}

void SystemDebugDump::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_INVENTORY_UPDATE: {
		// when the inventory gets updated, let's just empty it
		auto & inventory = reg.GetComponent< CpntResourceInventory >( msg.recipient );
		ForEveryGameResource( resource ) {
			inventory.RemoveResource( resource, inventory.GetResourceAmount( resource ) );
		}
		break;
	}
	case MESSAGE_ENTITY_DELETED: {
		for ( auto [ e, dump ] : reg.IterateOver< CpntDebugDump >() ) {
			if ( dump.fetcher == msg.recipient ) {
				dump.fetcher = INVALID_ENTITY;
			}
		}
		break;
	}
	}
}
