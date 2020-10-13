#pragma once

#include "entity.h"
#include "map.h"
#include "ngLib/ngcontainers.h"

struct CpntBuilding;

enum AStarMovementAllowed {
	ASTAR_ALLOW_DIAGONALS,
	ASTAR_FORBID_DIAGONALS,
	ASTAR_FREE_MOVEMENT,
};

enum CardinalDirection {
	NORTH = 0,               // x positive
	EAST = 1,                // z negative
	SOUTH = 2,               // x negative
	WEST = 3,                // z positive
	NUM_CARDINAL_DIRECTIONS, // keep me at the end
};

struct Map;

struct RoadNetwork {
	struct Connection {
		Connection() = default;
		Connection( const Cell & connectedTo, u32 distance ) : connectedTo( connectedTo ), distance( distance ) {}
		Cell connectedTo = INVALID_CELL;
		u32  distance = 0;
		bool IsValid() const { return connectedTo != INVALID_CELL; }
		void Invalidate() {
			connectedTo = INVALID_CELL;
			distance = 0;
		}
	};
	struct Node {
		Cell       position;
		Connection connections[ NUM_CARDINAL_DIRECTIONS ];

		u32               NumSetConnections() const;
		Connection *      GetValidConnectionWithOffset( u32 offset );
		Connection *      FindConnectionWith( const Cell & cell );
		Connection *      FindShortestConnectionWith( const Cell & cell );
		bool              HasMultipleConnectionsWith( const Cell & cell );
		bool              IsConnectedToItself() const;
		CardinalDirection GetDirectionOfConnection( const Connection * connection ) const;
	};

	std::vector< Node > nodes;

	Node * FindNodeWithPosition( Cell cell );
	Node * ResolveConnection( Connection * connection );
	bool   RemoveNodeByPosition( Cell cell );
	void   AddRoadCellToNetwork( Cell cellToAdd, const Map & map );
	void   RemoveRoadCellFromNetwork( Cell cellToRemove, const Map & map );
	void   DissolveNode( Node & nodeToDissolve );

	struct NodeSearchResult {
		bool              found = false;
		Node *            node = nullptr;
		CardinalDirection directionFromStart;
		CardinalDirection directionFromEnd;
		u32               distance;
	};
	void FindNearestRoadNodes( Cell cell, const Map & map, NodeSearchResult & first, NodeSearchResult & second );

	bool FindPath( Cell                       start,
	               Cell                       goal,
	               const Map &                map,
	               ng::DynamicArray< Cell > & outPath,
	               u32 *                      outTotalDistance = nullptr,
	               u32                        maxDistance = ULONG_MAX );

	bool CheckNetworkIntegrity();
};

struct CpntNavAgent {
	ng::DynamicArray< Cell > pathfindingNextSteps;
	// speed is in cells per ticks
	float movementSpeed = ConvertPerSecondToPerTick( 5.0f );
};

struct CpntWanderer {
	u32 maxCellsCovered = 32;
};

struct SystemNavAgent : public ISystem {
	virtual void Update( Registery & reg, Duration ticks ) override;
};

bool FindPathBetweenBuildings( const CpntBuilding &       start,
                               const CpntBuilding &       goal,
                               Map &                      map,
                               RoadNetwork &              roadNetwork,
                               ng::DynamicArray< Cell > & outPath,
                               u32                        maxDistance = ULONG_MAX,
                               u32 *                      outDistance = nullptr );
bool CreateWandererRoutine(
    const Cell & start, Map & map, RoadNetwork & roadNetwork, ng::DynamicArray< Cell > & outPath, u32 maxDistance );

bool      AStar( Cell start, Cell goal, AStarMovementAllowed movement, const Map & map, std::vector< Cell > & outPath );
glm::vec3 GetPointInMiddleOfCell( Cell cell );
glm::vec3 GetPointInCornerOfCell( Cell cell );
Cell      GetCellForPoint( glm::vec3 point );
Cell      GetCellAfterMovement( Cell start, int movementX, int movementZ );
Cell      GetCellAfterMovement( Cell start, CardinalDirection direction );
Cell      GetAnyRoadConnectedToBuilding( const CpntBuilding & building, const Map & map );
CardinalDirection GetDirectionFromCellTo( Cell from, Cell to );
CardinalDirection OppositeDirection( CardinalDirection direction );
