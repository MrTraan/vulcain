#pragma once
#include "../entity.h"
#include "../system.h"

struct CpntResourceInventory;

struct CpntDeliveryGuy {
	static constexpr Duration durationBetweenTwoPathfindingTry = DurationFromSeconds( 1 );
	Entity                    buildingSpawner = INVALID_ENTITY;
	Entity                    targetEntity = INVALID_ENTITY;
	bool                      isStuck = false;
	TimePoint                 lastPathfindingTry{};
};

struct SystemDeliveryGuy : public System< CpntDeliveryGuy > {
	virtual void OnCpntAttached( Entity e, CpntDeliveryGuy & t ) override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
	virtual void Update ( Registery & reg, Duration ticks ) override;
};

Entity CreateDeliveryGuy( Registery & reg, Entity spawner, const CpntResourceInventory & inventory );
