#pragma once

#include "entity.h"
#include "navigation.h"
#include "ngLib/ngtime.h"

#include <map>

struct Area {
	enum class Shape {
		RECTANGLE,
	};
	Cell  center;
	int   sizeX;
	int   sizeZ;
	Shape shape = Shape::RECTANGLE;
};

enum class BuildingKind {
	ROAD_BLOCK,
	HOUSE,
	FARM,
	STORAGE_HOUSE,
};

struct CpntBuilding {
	BuildingKind kind;
	Cell         cell;
	u32          tileSizeX;
	u32          tileSizeZ;
};

enum class GameResource {
	WHEAT,
};

struct CpntBuildingProducing {
	GameResource resource;
	ng::Duration timeToProduceBatch;
	ng::Duration timeSinceLastProduction;
	u32          batchSize = 1;
};

struct CpntResourceCarrier {
	GameResource resource;
	u32          amount;
};

struct CpntBuildingStorage {
	std::map< GameResource, u32 > storage;

	bool StoreRessource( GameResource resource, u32 amount );
};

struct CpntHousing {
	CpntHousing() {}
	CpntHousing( u32 _tier ) : tier( _tier ) {}
	u32 maxHabitants = 0;
	u32 numCurrentlyLiving = 0;
	u32 tier = 0;
};

struct SystemHousing : public ISystem {
	virtual void Update( Registery & reg, float dt ) override;

	u32 totalPopulation = 0;
};

struct SystemBuildingProducing : public ISystem {
	virtual void Update( Registery & reg, float dt ) override;
};

const char * GameResourceToString( GameResource resource );
bool         IsCellInsideBuilding( const CpntBuilding & building, Cell cell );
bool         IsBuildingInsideArea( const CpntBuilding & building, const Area & area );
bool         IsCellAdjacentToBuilding( const CpntBuilding & building, Cell cell, const Map & map );