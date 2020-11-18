#pragma once
#include "../entity.h"
#include "../system.h"

struct CpntStorageHouse {
	CpntStorageHouse() {
		for ( u32 i = 0; i < MAX_RESOURCES; i++ ) {
			displayedResources[i] = INVALID_ENTITY;
		}
	}
	static constexpr u32 MAX_RESOURCES = 8;
	Entity displayedResources[ MAX_RESOURCES ];
};

struct SystemStorageHouse : public System<CpntStorageHouse> {
	virtual void OnCpntAttached( Entity e, CpntStorageHouse & t ) override;
	virtual void OnCpntRemoved( Entity e, CpntStorageHouse & t ) override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
	virtual void Update( Registery & reg, Duration ticks ) override;
	virtual void DebugDraw() override;
};
