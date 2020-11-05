#include "map.h"
#include "game.h"
#include "navigation.h"

void Map::AllocateGrid( u32 sizeX, u32 sizeZ ) {
	this->sizeX = sizeX;
	this->sizeZ = sizeZ;

	tiles = new MapTile[ ( u64 )sizeX * sizeZ ];
	for ( u32 x = 0; x < sizeX; x++ ) {
		for ( u32 z = 0; z < sizeZ; z++ ) {
			tiles[ x * sizeZ + z ] = MapTile::EMPTY;
		}
	}
}

MapTile Map::GetTile( Cell coord ) const { return GetTile( coord.x, coord.z ); }
MapTile Map::GetTile( u32 x, u32 z ) const { return tiles[ x * sizeZ + z ]; }

void Map::SetTile( Cell coord, MapTile type ) { SetTile( coord.x, coord.z, type ); }
void Map::SetTile( u32 x, u32 z, MapTile type ) {
	if ( IsTileWalkable( x, z ) ) {
		Cell cell(x, z);
		theGame->roadNetwork.RemoveRoadCellFromNetwork( cell, *this );
		PostMsgGlobal<Cell>( MESSAGE_ROAD_CELL_REMOVED, cell );
	}
	if ( IsTileWalkable( type ) ) {
		Cell cell(x, z);
		theGame->roadNetwork.AddRoadCellToNetwork( cell, *this );
		PostMsgGlobal<Cell>( MESSAGE_ROAD_CELL_ADDED, cell );
	}
	tiles[ x * sizeZ + z ] = type;
}
