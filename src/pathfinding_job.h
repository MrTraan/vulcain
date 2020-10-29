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
		FROM_BUILDING_TO_BUILDING,
		FROM_BUILDING_TO_CELL,
	};
	Entity               requester;
	Type                 type;
	Cell                 startCell = INVALID_CELL;
	CpntBuilding         startBuilding{};
	Cell                 goalCell = INVALID_CELL;
	CpntBuilding         goalBuilding{};
	AStarMovementAllowed movementAllowed;
};

struct PathfindingTaskResponse {
	bool          ok;
	pathfindingID id;
};

struct CpntPathfinding {};

struct SystemPathfinding : public System<CpntPathfinding> {
	SystemPathfinding() {
		ListenToGlobal( MESSAGE_PATHFINDING_REQUEST );
		ListenToGlobal( MESSAGE_PATHFINDING_DELETE_ENTRY );
	}
	struct Entry {
		pathfindingID            id;
		ng::DynamicArray< Cell > path;
	};
	std::mutex              entriesMutex;
	ng::LinkedList< Entry > entries;

	void CopyPath( pathfindingID id, ng::DynamicArray< Cell > & out );
	void CopyAndDeletePath( pathfindingID id, ng::DynamicArray< Cell > & out );

	moodycamel::ConcurrentQueue< PathfindingTask > taskQueue;
	moodycamel::ConcurrentQueue< pathfindingID >   deleteQueue;

	virtual void ParallelJob() override;
	virtual void HandleMessage( Registery & reg, const Message & msg ) override;
};
