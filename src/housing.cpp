#include <imgui/imgui.h>

#include "game.h"
#include "housing.h"
#include "mesh.h"
#include "registery.h"

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

bool IsCellConnectedToBuildingByRoad( Cell cell, const CpntBuilding & building, const Map & map ) {
	if ( map.GetTile( cell ) != MapTile::ROAD ) {
		return false;
	}
	return ( IsCellInsideBuilding( building, Cell( cell.x + 1, cell.z ) ) ||
	         IsCellInsideBuilding( building, Cell( cell.x - 1, cell.z ) ) ||
	         IsCellInsideBuilding( building, Cell( cell.x, cell.z + 1 ) ) ||
	         IsCellInsideBuilding( building, Cell( cell.x, cell.z - 1 ) ) );
}

bool IsCellInsideBuilding( const CpntBuilding & building, Cell cell ) {
	return ( cell.x >= building.cell.x && cell.z >= building.cell.z && cell.x < building.cell.x + building.tileSizeX &&
	         cell.z < building.cell.z + building.tileSizeZ );
}

void OnAgentArrived( Registery & reg, Entity sender, Entity receiver ) {
	ng_assert( sender == receiver );
	ng::Printf( "Entity %lu has arrived!\n", receiver );

	// Look for a storage house next to receiver
	CpntTransform & transform = reg.GetComponent< CpntTransform >( receiver );
	Cell            position = GetCellForPoint( transform.GetTranslation() );
	Entity          closestStorage = INVALID_ENTITY_ID;
	for ( auto & [ e, storage ] : reg.IterateOver< CpntBuildingStorage >() ) {
		CpntBuilding & building = reg.GetComponent< CpntBuilding >( e );
		if ( IsCellConnectedToBuildingByRoad( position, building, theGame->map ) ) {
			closestStorage = e;
		}
		break;
	}
	if ( closestStorage == INVALID_ENTITY_ID ) {
		// TODO: Handle that the storage has been removed
		ng::Errorf( "An agent was directed to a storage that disappeared in the meantime" );
	} else {
		// Store what we have on us in the storage
		CpntBuildingStorage & storage = reg.GetComponent< CpntBuildingStorage >( closestStorage );
		CpntResourceCarrier & carrier = reg.GetComponent< CpntResourceCarrier >( receiver );
		storage.StoreRessource( carrier.resource, carrier.amount );
	}

	reg.MarkForDelete( receiver );
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
			CpntTransform &       transform = reg.AssignComponent< CpntTransform >( carrier );
			CpntNavAgent &        navAgent = reg.AssignComponent< CpntNavAgent >( carrier );
			CpntResourceCarrier & resourceCarrier = reg.AssignComponent< CpntResourceCarrier >( carrier );
			resourceCarrier.amount = producer.batchSize;
			resourceCarrier.resource = producer.resource;

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
			reg.messageBroker.AddListener( carrier, carrier, OnAgentArrived, MESSAGE_PATHFINDING_DESTINATION_REACHED );
		}
	}
}

void SystemBuildingProducing::DebugDraw() { ImGui::Text( "Hello from building producing!" ); }

bool CpntBuildingStorage::StoreRessource( GameResource resource, u32 amount ) {
	// TODO: We could refuse some resources, or be full
	if ( storage.contains( resource ) ) {
		storage[ resource ] += amount;
	} else {
		storage[ resource ] = amount;
	}
	return true;
}
