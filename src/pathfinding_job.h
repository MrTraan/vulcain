#pragma once
#include "buildings/building.h"
#include "entity.h"
#include "navigation.h"
#include "ngLib/ngcontainers.h"
#include "system.h"
#include <concurrentqueue.h>
#include <mutex>

using pathfindingID = u32;

struct PathfindingTask {
	enum class Type {
		FROM_CELL_TO_CELL,
		FROM_CELL_TO_BUILDING,
		FROM_CELL_TO_TILE_TYPE,
		FROM_BUILDING_TO_BUILDING,
		FROM_BUILDING_TO_CELL,
		FROM_BUILDING_TO_TILE_TYPE,
		FROM_CELL_TO_RESOURCE_STORAGE_WITH_CAPACITY,
		FROM_BUILDING_TO_RESOURCE_STORAGE_WITH_CAPACITY,
		FROM_CELL_TO_RESOURCE_STORAGE_WITH_STOCK,
		FROM_BUILDING_TO_RESOURCE_STORAGE_WITH_STOCK,
	};
	union Coordinate {
		Cell         cell;
		CpntBuilding building;
		MapTile      tileType;
		GameResource resourceType;
	};
	Entity          requester;
	Type            type;
	Coordinate      start{ INVALID_CELL };
	Coordinate      goal{ INVALID_CELL };
	MovementAllowed movementAllowed;
};

struct PathfindingTaskResponse {
	bool          ok;
	pathfindingID id;
};

struct CpntPathfinding {};

struct SystemPathfinding : public System< CpntPathfinding > {
	SystemPathfinding() {
		ListenToGlobal( MESSAGE_PATHFINDING_REQUEST );
		ListenToGlobal( MESSAGE_PATHFINDING_DELETE_ENTRY );
	}
	struct Entry {
		pathfindingID            id;
		ng::DynamicArray< Cell > path;
		// targetEntity will only be filled on certain tasks
		// I don't know if it's a good idea, but it avoid a lot of recomputation when looking for a path to a storage
		// house
		Entity targetEntity = INVALID_ENTITY;
	};
	std::mutex              entriesMutex;
	ng::LinkedList< Entry > entries;

	// CopyPath and CopyAndDeletePath returns the target entity (reminder: will be INVALID_ENTITY most of the time)
	Entity CopyPath( pathfindingID id, ng::DynamicArray< Cell > & out );
	Entity CopyAndDeletePath( pathfindingID id, ng::DynamicArray< Cell > & out );

	moodycamel::ConcurrentQueue< PathfindingTask > taskQueue;
	moodycamel::ConcurrentQueue< pathfindingID >   deleteQueue;

	virtual void ParallelJob() override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
};
