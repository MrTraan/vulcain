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
	if ( GetTile( x, z ) == MapTile::ROAD ) {
		theGame->roadNetwork.RemoveRoadCellFromNetwork( Cell( x, z ), *this );
	}
	if ( type == MapTile::ROAD ) {
		theGame->roadNetwork.AddRoadCellToNetwork( Cell( x, z ), *this );
	}
	tiles[ x * sizeZ + z ] = type;
}
