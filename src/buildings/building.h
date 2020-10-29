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
	u32          workersNeeded = 0;
	u32          workersEmployed = 0;

	double GetEfficiency() const {
		if ( workersNeeded == 0 )
			return 1.0f;
		return ( double )workersEmployed / ( double )workersNeeded;
	}
	double GetInvEfficiency() const {
		if ( workersEmployed == 0 )
			return 0.0f;
		return ( double )workersNeeded / (double)workersEmployed;
	}

	// This allows to iterate over adjacent cells using a for loop
	// syntax : for ( Cell cell : building.AdjacentCells(map) ) {}
	struct AdjacentCellsIterable {
		AdjacentCellsIterable( const CpntBuilding * building, const Map * map ) : building( building ), map( map ) {}

		const CpntBuilding * building;
		const Map *          map;

		struct Iterator {
			Iterator( const CpntBuilding * building, const Map * map, int shiftX, int shiftZ )
			    : building( building ), map( map ), shiftX( shiftX ), shiftZ( shiftZ ) {}

			bool operator!=( const Iterator & other ) const { return shiftX != other.shiftX || shiftZ != other.shiftZ; }
			Cell operator*() const {
				return Cell( ( u32 )( ( int )building->cell.x + shiftX ), ( u32 )( ( int )building->cell.z + shiftZ ) );
			}
			bool IsValid() const {
				if ( !building || !map ) {
					return false;
				}
				int64 resX = ( int64 )building->cell.x + shiftX;
				int64 resZ = ( int64 )building->cell.z + shiftZ;
				return resX >= 0 && resX < map->sizeX && resZ >= 0 && resZ < map->sizeZ;
			}

			Iterator & operator++() {
				// prefix

				while ( shiftX != -1 || shiftZ != -1 ) {
					if ( shiftX == -1 ) {
						shiftZ++;
						if ( shiftZ == building->tileSizeZ ) {
							shiftX++;
						}
					} else if ( shiftZ == building->tileSizeZ ) {
						shiftX++;
						if ( shiftX == building->tileSizeX ) {
							shiftZ--;
						}
					} else if ( shiftX == building->tileSizeX ) {
						shiftZ--;
						if ( shiftZ == -1 ) {
							shiftX--;
						}
					} else {
						shiftX--;
					}
					int64 resX = ( int64 )building->cell.x + shiftX;
					int64 resZ = ( int64 )building->cell.z + shiftZ;
					if ( resX >= 0 && resX < map->sizeX && resZ >= 0 && resZ < map->sizeZ ) {
						break; // we are on a valid cell, we can break the loop
					}
				}
				return *this;
			}

			const CpntBuilding * building;
			const Map *          map;
			int                  shiftX;
			int                  shiftZ;
		};

		Iterator begin() const {
			auto start = Iterator( building, map, -1, 0 );
			while ( !start.IsValid() && start != end() ) {
				++start;
			}
			return start;
		}
		Iterator end() const { return Iterator( nullptr, nullptr, -1, -1 ); }
	};

	AdjacentCellsIterable AdjacentCells( const Map & map ) const { return AdjacentCellsIterable( this, &map ); }
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
	u32 numIncomingMigrants = 0;
	u32 tier = 0;

	TimePoint lastServiceAccess[ ( int )GameService::NUM_SERVICES ] = {};
	bool      isServiceRequired[ ( int )GameService::NUM_SERVICES ] = {};
};

struct SystemResourceInventory : public System< CpntResourceInventory > {
	SystemResourceInventory() { ListenToGlobal( MESSAGE_INVENTORY_TRANSACTION ); }
	virtual void Update( Registery & reg, Duration ticks ) override {}
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
};

struct SystemHousing : public System< CpntHousing > {
	virtual void Update( Registery & reg, Duration ticks ) override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
	virtual void OnCpntAttached( Entity e, CpntHousing & t ) override;
	virtual void OnCpntRemoved( Entity e, CpntHousing & t ) override;

	u32 totalPopulation = 0;
};

struct SystemBuilding : public System< CpntBuilding > {
	SystemBuilding() {
		ListenToGlobal( MESSAGE_WORKER_AVAILABLE );
		ListenToGlobal( MESSAGE_WORKER_REMOVED );
	}
	virtual void Update( Registery & reg, Duration ticks ) override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
	virtual void OnCpntAttached( Entity e, CpntBuilding & t ) override;
	virtual void OnCpntRemoved( Entity e, CpntBuilding & t ) override;
	virtual void DebugDraw() override;

	u32 totalUnemployed = 0;
	u32 totalEmployed = 0;
	u32 totalEmployeesNeeded = 0;
};

struct SystemBuildingProducing : public System< CpntBuildingProducing > {
	virtual void Update( Registery & reg, Duration ticks ) override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
};

struct SystemMarket : public System< CpntMarket > {
	virtual void Update( Registery & reg, Duration ticks ) override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
	virtual void OnCpntRemoved( Entity e, CpntMarket & t ) override;
};

struct SystemSeller : public System< CpntSeller > {
	virtual void Update( Registery & reg, Duration ticks ) override;
};

struct SystemServiceBuilding : public System< CpntServiceBuilding > {
	virtual void Update( Registery & reg, Duration ticks ) override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
	virtual void OnCpntRemoved( Entity e, CpntServiceBuilding & t ) override;
};

struct SystemServiceWanderer : public System< CpntServiceWanderer > {
	virtual void Update( Registery & reg, Duration ticks ) override;
};

struct SystemFetcher : public System< CpntResourceFetcher > {
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
	virtual void OnCpntAttached( Entity e, CpntResourceFetcher & t ) override;
};

struct CpntMigrant {
	CpntMigrant() = default;
	CpntMigrant( Entity targetHouse ) : targetHouse( targetHouse ) {}
	Entity targetHouse;
};

struct SystemMigrant : public System< CpntMigrant > {
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
	virtual void OnCpntAttached( Entity e, CpntMigrant & t ) override;
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
	return PostMsg< TransactionMessagePayload >( MESSAGE_INVENTORY_TRANSACTION,
	                                             TransactionMessagePayload{ resource, amount, acceptPayback },
	                                             recipient, sender );
}
