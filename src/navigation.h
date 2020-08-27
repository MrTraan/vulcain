#pragma once

#include "entity.h"
#include <vector>

enum AStarMovementAllowed {
	ASTAR_ALLOW_DIAGONALS,
	ASTAR_FORBID_DIAGONALS,
	ASTAR_FREE_MOVEMENT,
};

enum CardinalDirection {
	SOUTH = 0, // x negative
	NORTH = 1, // x positive
	EAST = 2,  // z negative
	WEST = 3,  // z positive
};

constexpr float CELL_SIZE = 1.0f;

struct Cell {
	Cell() = default;
	constexpr Cell( u32 x, u32 z ) : x( x ), z( z ) {}
	u32 x = 0;
	u32 z = 0;

	constexpr bool operator==( const Cell & rhs ) const { return x == rhs.x && z == rhs.z; }
	constexpr bool IsValid() const { return x != ( u32 )-1 && z != ( u32 )-1; }
};

constexpr Cell INVALID_CELL{ ( u32 )-1, ( u32 )-1 };

enum class MapTile {
	EMPTY,
	ROAD,
	BLOCKED,
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
		Connection connections[ 4 ];

		// Returns a pointer to a set connection, with offset
		Connection * GetValidConnectionWithOffset( u32 offset ) {
			ng_assert( offset < NumSetConnections() );
			for ( size_t i = 0; i < 4; i++ ) {
				if ( connections[ i ].IsValid() ) {
					if ( offset == 0 ) {
						return connections + i;
					}
					offset--;
				}
			}
			return nullptr;
		}

		u32 NumSetConnections() const {
			u32 count = 0;
			for ( size_t i = 0; i < 4; i++ ) {
				if ( connections[ i ].IsValid() ) {
					count++;
				}
			}
			return count;
		}

		Connection * FindConnectionWith( const Cell & cell ) {
			ng_assert_msg( HasMultipleConnectionsWith( cell ) == false,
			               "FindConnectionWith will only return the first connection, which is unsafe if a node has "
			               "multiple connections with the same node" );
			for ( size_t i = 0; i < 4; i++ ) {
				if ( connections[ i ].connectedTo == cell ) {
					return &connections[ i ];
				}
			}
			return nullptr;
		}
		
		Connection * FindShortestConnectionWith( const Cell & cell ) {
			Connection * res = nullptr;
			for ( size_t i = 0; i < 4; i++ ) {
				if ( connections[ i ].connectedTo == cell ) {
					if ( res == nullptr) {
						res = connections + i;
					} else if (connections[i].distance < res->distance) {
						res = connections + i;
					}
				}
			}
			return res;
		}

		bool HasMultipleConnectionsWith( const Cell & cell ) {
			u32 numConnections = 0;
			for ( size_t i = 0; i < 4; i++ ) {
				if ( connections[ i ].connectedTo == cell ) {
					numConnections++;
				}
			}
			return numConnections > 1;
		}

		bool IsConnectedToItself() const {
			for ( size_t i = 0; i < 4; i++ ) {
				if ( connections[ i ].IsValid() && connections[ i ].connectedTo == position ) {
					return true;
				}
			}
			return false;
		}

		CardinalDirection GetDirectionOfConnection(const Connection * connection) const {
			int offset = (int)(connection - connections);
			ng_assert(offset >= 0 && offset < 4 );
			return (CardinalDirection)offset;
		}
	};

	std::vector< Node > nodes;

	Node * FindNodeWithPosition( Cell cell );
	Node * ResolveConnection( Connection * connection ) {
		ng_assert( connection->IsValid() );
		return FindNodeWithPosition( connection->connectedTo );
	}
	bool RemoveNodeByPosition( Cell cell );
	void AddRoadCellToNetwork( Cell cellToAdd, const Map & map );
	void RemoveRoadCellFromNetwork( Cell cellToRemove, const Map & map );
	void DissolveNode( Node & nodeToDissolve );

	// Returns true if the cell is connected to a road node
	struct NodeSearchResult {
		bool              found = false;
		Node *            node = nullptr;
		CardinalDirection directionFromStart;
		CardinalDirection directionFromEnd;
		u32               distance;
	};
	void FindNearestRoadNodes( Cell cell, const Map & map, NodeSearchResult & first, NodeSearchResult & second );

	bool FindPath( Cell start, Cell goal, const Map & map, std::vector< Cell > & outPath );

	bool CheckNetworkIntegrity();
};

struct Map {
	~Map() {
		if ( tiles != nullptr ) {
			delete[] tiles;
		}
	}

	void AllocateGrid( u32 sizeX, u32 sizeZ ) {
		this->sizeX = sizeX;
		this->sizeZ = sizeZ;

		tiles = new MapTile[ ( u64 )sizeX * sizeZ ];
		for ( u32 x = 0; x < sizeX; x++ ) {
			for ( u32 z = 0; z < sizeZ; z++ ) {
				SetTile( x, z, MapTile::EMPTY );
			}
		}
	}

	MapTile GetTile( Cell coord ) const { return tiles[ coord.x * sizeZ + coord.z ]; }
	MapTile GetTile( u32 x, u32 z ) const { return tiles[ x * sizeZ + z ]; }
	void    SetTile( Cell coord, MapTile type ) {
        if ( GetTile( coord ) == MapTile::ROAD ) {
            roadNetwork.RemoveRoadCellFromNetwork( coord, *this );
        }
        if ( type == MapTile::ROAD ) {
            roadNetwork.AddRoadCellToNetwork( coord, *this );
        }
        tiles[ coord.x * sizeZ + coord.z ] = type;
	}
	void SetTile( u32 x, u32 z, MapTile type ) {
		if ( GetTile( x, z ) == MapTile::ROAD ) {
			roadNetwork.RemoveRoadCellFromNetwork( Cell( x, z ), *this );
		}
		if ( type == MapTile::ROAD ) {
			roadNetwork.AddRoadCellToNetwork( Cell( x, z ), *this );
		}
		tiles[ x * sizeZ + z ] = type;
	}

	bool IsTileWalkable( Cell coord ) const { return GetTile( coord ) == MapTile::ROAD; }
	int  GetTileWeight( Cell coord ) const { return 1; }

	bool FindPath(  Cell start, Cell goal,  std::vector< Cell > & outPath  ) {
		return roadNetwork.FindPath( start, goal, *this, outPath );
	}

	u32         sizeX = 0;
	u32         sizeZ = 0;
	RoadNetwork roadNetwork;

  private:
	MapTile * tiles = nullptr;
};

struct CpntNavAgent {
	std::vector< Cell > pathfindingNextSteps;
	// speed is in cells per second
	float movementSpeed = 5.0f;
};

struct SystemNavAgent : public ISystem {
	virtual void Update( Registery & reg, float dt ) override;
};

bool      AStar( Cell start, Cell goal, AStarMovementAllowed movement, const Map & map, std::vector< Cell > & outPath );
glm::vec3 GetPointInMiddleOfCell( Cell cell );
glm::vec3 GetPointInCornerOfCell( Cell cell );
Cell      GetCellForPoint( glm::vec3 point );
Cell      GetCellAfterMovement( Cell start, int movementX, int movementZ );
Cell      GetCellAfterMovement( Cell start, CardinalDirection direction );
CardinalDirection GetDirectionFromCellTo( Cell from, Cell to );
CardinalDirection OppositeDirection( CardinalDirection direction );
