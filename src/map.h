#pragma once

#include "ngLib/types.h"

constexpr float CELL_SIZE = 1.0f;

struct Cell {
	Cell() = default;
	constexpr Cell( u32 x, u32 z ) : x( x ), z( z ) {}
	u32 x = 0;
	u32 z = 0;

	constexpr bool operator==( const Cell & rhs ) const { return x == rhs.x && z == rhs.z; }
	constexpr bool operator<( const Cell & rhs ) const {
		u64 sum = ( ( u64 )x << 32 ) | z;
		u64 sumrhs = ( ( u64 )rhs.x << 32 ) | rhs.z;
		return sum < sumrhs;
	}
	constexpr bool IsValid() const { return x != ( u32 )-1 && z != ( u32 )-1; }
};

constexpr Cell INVALID_CELL{ ( u32 )-1, ( u32 )-1 };

enum class MapTile {
	EMPTY,
	ROAD,
	ROAD_BLOCK,
	BLOCKED,
	TREE,
};

struct Map {
	~Map() {
		if ( tiles != nullptr ) {
			delete[] tiles;
		}
	}

	void AllocateGrid( u32 sizeX, u32 sizeZ );

	MapTile GetTile( Cell coord ) const;
	MapTile GetTile( u32 x, u32 z ) const;
	void    SetTile( Cell coord, MapTile type );
	void    SetTile( u32 x, u32 z, MapTile type );

	bool IsTileWalkable( Cell cell ) const { return IsTileWalkable( GetTile( cell.x, cell.z ) ); }
	bool IsTileWalkable( u32 x, u32 z ) const {
		MapTile tile = GetTile( x, z );
		return IsTileWalkable( tile );
	}
	bool IsTileWalkable( MapTile type ) const { return type == MapTile::ROAD || type == MapTile::ROAD_BLOCK; }

	bool IsTileAStarNavigable( Cell cell ) const { return IsTileAStarNavigable( GetTile( cell ) ); }
	bool IsTileAStarNavigable( MapTile type ) const { return type == MapTile::EMPTY || type == MapTile::ROAD; }

	bool IsValidTile( int64 x, int64 z ) const { return x >= 0 && x < sizeX && z >= 0 && z < sizeZ; }

	u32 sizeX = 0;
	u32 sizeZ = 0;

  private:
	MapTile * tiles = nullptr;
};
