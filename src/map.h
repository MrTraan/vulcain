#pragma once

#include "ngLib/types.h"

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

	u32 sizeX = 0;
	u32 sizeZ = 0;

  private:
	MapTile * tiles = nullptr;
};
