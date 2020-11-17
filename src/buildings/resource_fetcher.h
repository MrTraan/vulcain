#pragma once
#include "../entity.h"
#include "../system.h"
#include "building.h"

Entity CreateResourceFetcher( Registery & reg, GameResource resourceToFetch, u32 maxAmount, Entity parent );

struct CpntResourceFetcher {
	GameResource resourceToFetch;

	Entity parent = INVALID_ENTITY;
	Entity target = INVALID_ENTITY;

	enum class CurrentDirection {
		TO_PARENT,
		TO_TARGET,
	};

	CurrentDirection direction = CurrentDirection::TO_TARGET;
};


struct SystemFetcher : public System< CpntResourceFetcher > {
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
	virtual void OnCpntAttached( Entity e, CpntResourceFetcher & t ) override;
};

