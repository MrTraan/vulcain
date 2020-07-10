#pragma once
#include <glm/glm.hpp>

#include "entity.h"
#include "ngLib/ngtime.h"

struct CpntHousing {
	CpntHousing() {}
	CpntHousing( u32 _tier ) : tier(_tier ) {}
	u32 maxHabitants = 0;
	u32 numCurrentlyLiving = 0;
	u32 tier = 0;
};

enum class GameResource {
	WHEAT,
};

struct CpntBuildingProducing {
	GameResource resource;
	ng::Duration timeToProduceBatch;
	ng::Duration timeSinceLastProduction;
	u32 batchSize = 1;
	u32 currentlyStoring = 0;
	u32 maxStorageSize = 1;
};

struct SystemHousing : public ISystem {
	virtual void Update( Registery & reg, float dt ) override;
	virtual void DebugDraw() override;

	u32 totalPopulation = 0;
};

struct SystemBuildingProducing : public ISystem {
	virtual void Update( Registery & reg, float dt ) override;
	virtual void DebugDraw() override;
};
