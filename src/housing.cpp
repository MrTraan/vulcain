#include <imgui/imgui.h>

#include "game.h"
#include "housing.h"
#include "mesh.h"

void SystemHousing::Update( Registery & reg, float dt ) {
	totalPopulation = 0;
	for ( auto & [ e, housing ] : reg.IterateOver< CpntHousing >() ) {
		// Check if population should grow in house
		if ( housing.numCurrentlyLiving < housing.maxHabitants ) {
			housing.numCurrentlyLiving++;
		}

		totalPopulation += housing.numCurrentlyLiving;
	}
}

void SystemHousing::DebugDraw() { ImGui::Text( "Total population: %lu\n", totalPopulation ); }

static Cell GetClosestRoadPoint( const CpntBuilding & building, const Map & map ) {
	// TODO: This is really bad
	for ( u32 x = building.cell.x - 1; x < building.cell.x + building.tileSizeX + 1; x++ ) {
		for ( u32 z = building.cell.z - 1; z < building.cell.z + building.tileSizeZ + 1; z++ ) {
			if ( x == building.cell.x - 1 || x == building.cell.x + building.tileSizeX || z == building.cell.z - 1 ||
			     z == building.cell.z + building.tileSizeZ ) {
				if ( map.GetTile( x, z ) == MapTile::ROAD ) {
					return Cell( x, z );
				}
			}
		}
	}
	ng_assert( false );
	return Cell( 0, 0 );
}

bool CellIsInsideBuilding( const CpntBuilding & building, Cell cell ) {
	return ( cell.x >= building.cell.x && cell.z >= building.cell.z && cell.x < building.cell.x + building.tileSizeX &&
	         cell.z < building.cell.z + building.tileSizeZ );
}

void SystemBuildingProducing::Update( Registery & reg, float dt ) {
	for ( auto & [ e, producer ] : reg.IterateOver< CpntBuildingProducing >() ) {
		producer.timeSinceLastProduction += ng::DurationInSeconds( dt );
		if ( producer.timeSinceLastProduction >= producer.timeToProduceBatch ) {
			// Produce a batch
			producer.timeSinceLastProduction -= producer.timeToProduceBatch;
			// Spawn a dummy who will move the batch the nearest storage house
			Entity carrier = reg.CreateEntity();
			reg.AssignComponent< CpntRenderModel >( carrier, g_modelAtlas.cubeMesh );
			CpntTransform & transform = reg.AssignComponent< CpntTransform >( carrier );
			CpntNavAgent &  navAgent = reg.AssignComponent< CpntNavAgent >( carrier );

			const CpntBuilding & cpntBuilding = reg.GetComponent< CpntBuilding >( e );
			Cell                 roadPoint = GetClosestRoadPoint( cpntBuilding, theGame->map );
			transform.SetTranslation( GetPointInMiddleOfCell( roadPoint ) );

			Entity closestStorage = INVALID_ENTITY_ID;
			// TODO: We should find the closestStorage, not just any storage
			// TODO: we should handle not finding a storage
			for ( auto & [ e, storage ] : reg.IterateOver< CpntBuildingStorage >() ) {
				closestStorage = e;
				break;
			}
			ng_assert( closestStorage != INVALID_ENTITY_ID );

			Cell storageRoadPoint =
			    GetClosestRoadPoint( reg.GetComponent< CpntBuilding >( closestStorage ), theGame->map );
			bool pathFound = AStar( roadPoint, storageRoadPoint, ASTAR_FORBID_DIAGONALS, theGame->map,
			                        navAgent.pathfindingNextSteps );
			ng_assert( pathFound == true );
		}
	}
}

void SystemBuildingProducing::DebugDraw() { ImGui::Text( "Hello from building producing!" ); }
