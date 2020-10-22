#pragma once

#include "entity.h"
#include "game_time.h"
#include "navigation.h"
#include "service.h"

#include "../system.h"
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
	WHEAT = 0,
	NUM_RESOURCES, // keep me at the end
};

struct CpntBuildingProducing {
	GameResource resource;
	Duration     timeToProduceBatch = 0;
	Duration     timeSinceLastProduction = 0;
	u32          batchSize = 1;
};

struct CpntMarket {
	static constexpr u32 wandererCellRange = 128;
	static constexpr u32 fetcherCellRange = 128;

	Entity   wanderer = INVALID_ENTITY;
	Entity   fetcher = INVALID_ENTITY;
	Duration timeSinceLastWandererSpawn = 0;
	Duration durationBetweenWandererSpawns = DurationFromSeconds( 5 );
};

struct CpntServiceBuilding {
	GameService service;
	u32         wandererCellRange = 128;
	Entity      wanderer = INVALID_ENTITY;
	Duration    timeSinceLastWandererSpawn = 0;
	Duration    durationBetweenWandererSpawns = DurationFromSeconds( 5 );
};

struct CpntSeller {
	Cell lastCellDistributed = INVALID_CELL;
};

struct CpntResourceFetcher {
	Entity parent;
	Entity target;

	enum class CurrentDirection {
		TO_PARENT,
		TO_TARGET,
	};

	CurrentDirection direction = CurrentDirection::TO_TARGET;
};

struct CpntServiceWanderer {
	Cell        lastCellDistributed = INVALID_CELL;
	GameService service;
};

struct CpntResourceInventory {
	struct StorageCapacity {
		u32 currentAmount = 0;
		u32 max = 0;
	};
	StorageCapacity storage[ ( int )GameResource::NUM_RESOURCES ] = {};

	// Returns the amount actually stored
	u32 StoreRessource( GameResource resource, u32 amount );
	// returns the amount actually removed
	u32  RemoveResource( GameResource resource, u32 amount );
	void SetResourceMaxCapacity( GameResource resource, u32 max ) { storage[ ( int )resource ].max = max; }
	u32  GetResourceAmount( GameResource resource ) const { return storage[ ( int )resource ].currentAmount; }
	u32  GetResourceCapacity( GameResource resource ) const { return storage[ ( int )resource ].max; }
	bool IsEmpty() const;
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

struct SystemResourceInventory : public ISystem {
	SystemResourceInventory() { ListenToGlobal( MESSAGE_INVENTORY_TRANSACTION ); }
	virtual void Update( Registery & reg, Duration ticks ) override {}
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
};

struct SystemHousing : public ISystem {
	SystemHousing() { ListenToGlobal( MESSAGE_SERVICE_PROVIDED ); }
	virtual void Update( Registery & reg, Duration ticks ) override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;

	u32 totalPopulation = 0;
};

struct SystemBuildingProducing : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
};

struct SystemMarket : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
};

struct SystemSeller : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;
};

struct SystemServiceBuilding : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
};

struct SystemServiceWanderer : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;
};

struct SystemFetcher : public ISystem {
	SystemFetcher() { ListenToGlobal( MESSAGE_CPNT_ATTACHED ); }
	virtual void Update( Registery & reg, Duration ticks ) override {}
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
};

const char * GameResourceToString( GameResource resource );
bool         IsCellInsideBuilding( const CpntBuilding & building, Cell cell );
bool         IsBuildingInsideArea( const CpntBuilding & building, const Area & area );
bool         IsCellAdjacentToBuilding( const CpntBuilding & building, Cell cell, const Map & map );

struct TransactionMessagePayload {
	GameResource resource;
	u32          quantity = 0;
	bool         acceptPayback = false;
};

inline void
PostTransactionMessage( GameResource resource, u32 amount, bool acceptPayback, Entity recipient, Entity sender ) {
	return PostMsg< TransactionMessagePayload >(
	    MESSAGE_INVENTORY_TRANSACTION, TransactionMessagePayload{ resource, amount, acceptPayback }, recipient, sender );
}
