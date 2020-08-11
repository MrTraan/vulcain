#pragma once

#include "entity.h"
#include <vector>

enum AStarMovementAllowed {
	ASTAR_ALLOW_DIAGONALS,
	ASTAR_FORBID_DIAGONALS,
	ASTAR_FREE_MOVEMENT,
};

constexpr float CELL_SIZE = 1.0f;

struct Cell {
	Cell() = default;
	Cell( u32 x, u32 z ) : x( x ), z( z ) {}
	u32 x = 0;
	u32 z = 0;

	bool operator==( const Cell & rhs ) const { return x == rhs.x && z == rhs.z; }
};

glm::vec3 GetPointInMiddleOfCell( Cell cell );
Cell      GetCellForPoint( glm::vec3 point );

enum class MapTile {
	EMPTY,
	ROAD,
	BLOCKED,
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

		tiles = new MapTile[ sizeX * sizeZ ];
		for ( u32 x = 0; x < sizeX; x++ ) {
			for ( u32 z = 0; z < sizeZ; z++ ) {
				SetTile( x, z, MapTile::EMPTY );
			}
		}
	}

	MapTile GetTile( Cell coord ) const { return tiles[ coord.x * sizeZ + coord.z ]; }
	MapTile GetTile( u32 x, u32 z ) const { return tiles[ x * sizeZ + z ]; }
	void    SetTile( Cell coord, MapTile type ) { tiles[ coord.x * sizeZ + coord.z ] = type; }
	void    SetTile( u32 x, u32 z, MapTile type ) { tiles[ x * sizeZ + z ] = type; }

	bool IsTileWalkable( Cell coord ) const { return GetTile( coord ) == MapTile::ROAD; }
	int  GetTileWeight( Cell coord ) const { return 1; }

	u32       sizeX = 0;
	u32       sizeZ = 0;
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

bool AStar( Cell start, Cell goal, AStarMovementAllowed movement, const Map & map, std::vector< Cell > & outPath );
