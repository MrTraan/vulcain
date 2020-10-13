#pragma once

#include "entity.h"
#include "game_time.h"
#include "navigation.h"
#include "service.h"

#include <concurrentqueue.h>
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
	MARKET,
	FOUNTAIN,
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
	Duration     timeToProduceBatch = 0;
	Duration     timeSinceLastProduction = 0;
	u32          batchSize = 1;
};

struct CpntMarket {
	u32      wandererCellRange = 128;
	Entity   wanderer = INVALID_ENTITY_ID;
	Duration timeSinceLastWandererSpawn = 0;
	Duration durationBetweenWandererSpawns = DurationFromSeconds( 5 );
};

struct CpntServiceBuilding {
	GameService service;
	u32         wandererCellRange = 128;
	Entity      wanderer = INVALID_ENTITY_ID;
	Duration    timeSinceLastWandererSpawn = 0;
	Duration    durationBetweenWandererSpawns = DurationFromSeconds( 5 );
};

struct CpntResourceCarrier {
	static constexpr u32 MAX_NUM_RESOURCES_CARRIED = 16u;
	// Carriers carries at most 16 tuple of resources and amounts
	ng::StaticArray< ng::Tuple< GameResource, u32 >, MAX_NUM_RESOURCES_CARRIED > resources;
};

struct CpntSeller {
	// Seller should also have a CpntResourceCarrier
	Cell lastCellDistributed = INVALID_CELL;
};

struct CpntServiceWanderer {
	Cell        lastCellDistributed = INVALID_CELL;
	GameService service;
};

struct CpntResourceInventory {
	struct StorageCapacity {
		u32 currentAmount;
		u32 max;
	};
	std::map< GameResource, StorageCapacity > storage;

	// Returns the amount "consumed"
	u32  StoreRessource( GameResource resource, u32 amount );
	void AccecptNewResource( GameResource resource, u32 capacity ) {
		storage[ resource ].currentAmount = 0;
		storage[ resource ].max = capacity;
	}
	bool AccecptsResource( GameResource resource ) const { return storage.contains( resource ); }
	bool IsEmpty() const;
};

struct MsgServiceProvided {
	GameService service;
	Entity      target;
};

struct CpntHousing {
	CpntHousing() {}
	CpntHousing( u32 _tier ) : tier( _tier ) {}
	u32 maxHabitants = 0;
	u32 numCurrentlyLiving = 0;
	u32 tier = 0;

	TimePoint lastServiceAccess[ ( int )GameService::NUM_SERVICES ] = {};
	bool      isServiceRequired[ ( int )GameService::NUM_SERVICES ] = {};
};

struct SystemHousing : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;

	u32                                               totalPopulation = 0;
	moodycamel::ConcurrentQueue< MsgServiceProvided > serviceMessages;

	void NotifyServiceFulfilled( GameService service, Entity target ) {
		serviceMessages.enqueue( { service, target } );
	}
};

struct SystemBuildingProducing : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;
};

struct SystemMarket : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;
};

struct SystemSeller : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;
};

struct SystemServiceBuilding : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;
};

struct SystemServiceWanderer : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;
};

const char * GameResourceToString( GameResource resource );
bool         IsCellInsideBuilding( const CpntBuilding & building, Cell cell );
bool         IsBuildingInsideArea( const CpntBuilding & building, const Area & area );
bool         IsCellAdjacentToBuilding( const CpntBuilding & building, Cell cell, const Map & map );