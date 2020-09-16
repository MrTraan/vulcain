#include "placement.h"
#include "../mesh.h"

glm::i32vec2 GetBuildingSize( BuildingKind kind ) {
	// TODO: Could we get these values from a data file?
	switch ( kind ) {
	case ( BuildingKind::ROAD_BLOCK ):
		return { 1, 1 };
	case ( BuildingKind::FARM ):
		return { 3, 3 };
	case ( BuildingKind::STORAGE_HOUSE ):
		return { 3, 3 };
	case ( BuildingKind::HOUSE ):
		return { 2, 2 };
	default:
		ng_assert( false );
		return { 1, 1 };
	}
}

bool CanPlaceBuilding( const Cell cell, BuildingKind kind, const Map & map ) {
	switch ( kind ) {
	case BuildingKind::ROAD_BLOCK:
		return map.GetTile( cell ) == MapTile::ROAD;
	default: {
		auto size = GetBuildingSize( kind );
		if ( cell.x + size.x >= map.sizeX || cell.z + size.y >= map.sizeZ ) {
			return false;
		}
		for ( u32 x = cell.x; x < cell.x + size.x; x++ ) {
			for ( u32 z = cell.z; z < cell.z + size.y; z++ ) {
				if ( map.GetTile( x, z ) != MapTile::EMPTY ) {
					return false;
				}
			}
		}
		return true;
	}
	}
}

Entity FindBuildingByPosition( Registery & reg, const Cell cell ) {
	for ( auto & [ e, building ] : reg.IterateOver< CpntBuilding >() ) {
		if ( IsCellInsideBuilding( building, cell ) ) {
			return e;
		}
	}
	return INVALID_ENTITY_ID;
}

bool DeleteBuildingByPosition( Registery & reg, const Cell cell, Map & map ) {
	for ( auto & [ e, building ] : reg.IterateOver< CpntBuilding >() ) {
		if ( IsCellInsideBuilding( building, cell ) ) {
			reg.MarkForDelete( e );
			for ( u32 x = building.cell.x; x < building.cell.x + building.tileSizeX; x++ ) {
				for ( u32 z = building.cell.z; z < building.cell.z + building.tileSizeZ; z++ ) {
					map.SetTile( x, z, MapTile::EMPTY );
				}
			}
			return true;
		}
	}
	return false;
}

int DeleteBuildingsInsideArea( Registery & reg, const Area & area, Map & map ) {
	int numDeletions = 0;
	for ( auto & [ e, building ] : reg.IterateOver< CpntBuilding >() ) {
		if ( IsBuildingInsideArea( building, area ) ) {
			reg.MarkForDelete( e );
			for ( u32 x = building.cell.x; x < building.cell.x + building.tileSizeX; x++ ) {
				for ( u32 z = building.cell.z; z < building.cell.z + building.tileSizeZ; z++ ) {
					map.SetTile( x, z, MapTile::EMPTY );
				}
			}
			numDeletions++;
		}
	}
	return numDeletions;
}

const Model * GetBuildingModel( BuildingKind kind ) {
	switch ( kind ) {
	case BuildingKind::HOUSE:
		return g_modelAtlas.houseMesh;
		break;

	case BuildingKind::FARM:
		return g_modelAtlas.farmMesh;
		break;

	case BuildingKind::STORAGE_HOUSE:
		return g_modelAtlas.storeHouseMesh;
		break;

	case BuildingKind::ROAD_BLOCK:
		return g_modelAtlas.roadBlockMesh;
		return nullptr;

	default:
		ng_assert( false );
		return nullptr;
	}
}

bool PlaceBuilding( Registery & reg, const Cell cell, BuildingKind kind, Map & map ) {
	Entity e = reg.CreateEntity();

	CpntBuilding & cpntBuilding = reg.AssignComponent< CpntBuilding >( e );
	cpntBuilding.kind = kind;
	cpntBuilding.cell = cell;
	auto size = GetBuildingSize( kind );
	cpntBuilding.tileSizeX = size.x;
	cpntBuilding.tileSizeZ = size.y;

	for ( u32 x = cell.x; x < cell.x + size.x; x++ ) {
		for ( u32 z = cell.z; z < cell.z + size.y; z++ ) {
			map.SetTile( x, z, MapTile::BLOCKED );
		}
	}

	reg.AssignComponent< CpntRenderModel >( e, GetBuildingModel( kind ) );

	// Center in the middle of the tile group
	CpntTransform & transform = reg.AssignComponent< CpntTransform >( e );
	transform.SetTranslation(
	    glm::vec3( cell.x + cpntBuilding.tileSizeX / 2.0f, 0, cell.z + cpntBuilding.tileSizeZ / 2.0f ) );

	switch ( kind ) {
	case BuildingKind::HOUSE: {
		auto & housing = reg.AssignComponent< CpntHousing >( e );
		housing.maxHabitants = 20;
		housing.numCurrentlyLiving = 0;
		housing.tier = 0;
		break;
	}

	case BuildingKind::FARM: {
		auto & producer = reg.AssignComponent< CpntBuildingProducing >( e );
		producer.batchSize = 4;
		producer.timeToProduceBatch = ng::DurationInMs( 5000.0f );
		producer.resource = GameResource::WHEAT;
		break;
	}

	case BuildingKind::STORAGE_HOUSE: {
		reg.AssignComponent< CpntBuildingStorage >( e );
		break;
	}

	case BuildingKind::ROAD_BLOCK:
		// There are no specific components for a road block
		break;

	default:
		ng_assert( false );
		return false;
	}
	return true;
}
