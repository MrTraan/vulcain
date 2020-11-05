#include "storage_house.h"
#include "../game.h"
#include "../mesh.h"
#include "../packer_resource_list.h"
#include "building.h"
#include "placement.h"

constexpr glm::vec3 offsetsForResources[ CpntStorageHouse::MAX_RESOURCES ] = {
    { 1.0f, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f },   { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f },  { -1.0f, 0.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 1.0f },
};

void SystemStorageHouse::OnCpntAttached( Entity e, CpntStorageHouse & t ) { ListenTo( MESSAGE_INVENTORY_UPDATE, e ); }

void SystemStorageHouse::OnCpntRemoved( Entity e, CpntStorageHouse & t ) {
	for ( u32 i = 0; i < CpntStorageHouse::MAX_RESOURCES; i++ ) {
		if ( t.displayedResources[ i ] != INVALID_ENTITY ) {
			theGame->registery->MarkForDelete( t.displayedResources[ i ] );
		}
	}
}

void SystemStorageHouse::HandleMessage( Registery & reg, const Message & msg ) {
	switch ( msg.type ) {
	case MESSAGE_INVENTORY_UPDATE: {
		auto & storage = reg.GetComponent< CpntStorageHouse >( msg.recipient );
		auto & inventory = reg.GetComponent< CpntResourceInventory >( msg.recipient );
		auto & transform = reg.GetComponent< CpntTransform >( msg.recipient );
		for ( Entity & e : storage.displayedResources ) {
			if ( e != INVALID_ENTITY ) {
				reg.MarkForDelete( e );
				e = INVALID_ENTITY;
			}
		}
		int offset = 0;
		ForEveryGameResource( resource ) {
			for ( int quantity = 0;
			      quantity < inventory.GetResourceAmount( resource ) && offset < CpntStorageHouse::MAX_RESOURCES;
			      quantity++ ) {
				Entity resourceSprite = reg.CreateEntity();
				storage.displayedResources[ offset ] = resourceSprite;
				auto & childTransform = reg.AssignComponent< CpntTransform >( resourceSprite, transform.GetMatrix() );
				childTransform.SetTranslation( childTransform.GetTranslation() + offsetsForResources[ offset ] );
				reg.AssignComponent< CpntRenderModel >( resourceSprite, GetGameResourceModel( resource ) );
				offset++;
			}
		}
		break;
	}
	}
}

void SystemStorageHouse::Update( Registery & reg, Duration ticks ) {}

void SystemStorageHouse::DebugDraw() {}
