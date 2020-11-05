#pragma once

#include "../entity.h"
#include "../game_time.h"
#include "../registery.h"
#include "../system.h"

struct CpntWoodshop {
	Entity   worker = INVALID_ENTITY;
	Entity   deliveryGuy = INVALID_ENTITY;
	Duration timeBetweenWorkerMissions = DurationFromSeconds( 5 );
	Duration timeSinceLastWorkerSpawned = 0;
};

struct SystemWoodshop : public System< CpntWoodshop > {
	void Update( Registery & reg, Duration ticks ) override;
	void HandleMessage( Registery & reg, const Message & msg ) override;
	void OnCpntAttached( Entity e, CpntWoodshop & t ) override;
	void OnCpntRemoved( Entity e, CpntWoodshop & t ) override;
};

struct CpntWoodworker {
	Duration timeToChopOneTree = DurationFromSeconds( 5 );
	Duration choppingSince = 0;
	enum class Destination {
		TO_TREE,
		TO_WOODSHOP,
	};
	Entity      woodshop = INVALID_ENTITY;
	Destination currentDestination = Destination::TO_TREE;
};

struct SystemWoodworker : public System< CpntWoodworker > {
	void Update( Registery & reg, Duration ticks ) override;
	void OnCpntAttached( Entity e, CpntWoodworker & t ) override;
	void HandleMessage( Registery & reg, const Message & msg ) override;
};