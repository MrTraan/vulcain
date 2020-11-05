#pragma once
#include "building.h"
#include "../system.h"

struct CpntDebugDump {
	GameResource resourceToDump = GameResource::WHEAT;
	Entity fetcher = INVALID_ENTITY;
};

struct SystemDebugDump : public System<CpntDebugDump> {
	void OnCpntAttached( Entity e, CpntDebugDump & t ) override;
	void OnCpntRemoved( Entity e, CpntDebugDump & t ) override;
	void Update( Registery & reg, Duration ticks ) override;
	void HandleMessage( Registery & reg, const Message & msg ) override;
};