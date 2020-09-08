#include <imgui/imgui.h>

#include "collider.h"
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
	for ( u32 x = building.cell.x; x < building.cell.x + building.tileSizeX; x++ ) {
		if ( map.GetTile( x, building.cell.z - 1 ) == MapTile::ROAD ) {
			return Cell( x, building.cell.z - 1 );
		}
		if ( map.GetTile( x, building.cell.z + building.tileSizeZ ) == MapTile::ROAD ) {
			return Cell( x, building.cell.z + building.tileSizeZ );
		}
	}
	for ( u32 z = building.cell.z; z < building.cell.z + building.tileSizeZ; z++ ) {
		if ( map.GetTile( building.cell.x - 1, z ) == MapTile::ROAD ) {
			return Cell( building.cell.x - 1, z );
		}
		if ( map.GetTile( building.cell.x + building.tileSizeX, z ) == MapTile::ROAD ) {
			return Cell( building.cell.x + building.tileSizeX, z );
		}
	}

	return INVALID_CELL;
}

static bool BuildPathFromBuilding( const CpntBuilding & building, const Cell goal, std::vector< Cell > & outPath ) {
	Map & map = theGame->map;
	for ( u32 x = building.cell.x; x < building.cell.x + building.tileSizeX; x++ ) {
		if ( map.GetTile( x, building.cell.z - 1 ) == MapTile::ROAD ) {
			Cell start( x, building.cell.z - 1 );
			if ( map.FindPath( start, goal, outPath ) == true ) {
				return true;
			}
		}
		if ( map.GetTile( x, building.cell.z + building.tileSizeZ ) == MapTile::ROAD ) {
			Cell start( x, building.cell.z + building.tileSizeZ );
			if ( map.FindPath( start, goal, outPath ) == true ) {
				return true;
			}
		}
	}
	for ( u32 z = building.cell.z; z < building.cell.z + building.tileSizeZ; z++ ) {
		if ( map.GetTile( building.cell.x - 1, z ) == MapTile::ROAD ) {
			Cell start( building.cell.x - 1, z );
			if ( map.FindPath( start, goal, outPath ) == true ) {
				return true;
			}
		}
		if ( map.GetTile( building.cell.x + building.tileSizeX, z ) == MapTile::ROAD ) {
			Cell start( building.cell.x + building.tileSizeX, z );
			if ( map.FindPath( start, goal, outPath ) == true ) {
				return true;
			}
		}
	}

	return false;
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
			const CpntBuilding & cpntBuilding = reg.GetComponent< CpntBuilding >( e );

			// Find a path to store house
			Entity              closestStorage = INVALID_ENTITY_ID;
			u32                 closestStorageDistance = ULONG_MAX;
			std::vector< Cell > currentPath;
			currentPath.reserve( 32 );
			for ( auto & [ e, storage ] : reg.IterateOver< CpntBuildingStorage >() ) {
				u32  distance = 0;
				bool pathFound = theGame->map.FindPathBetweenBuildings(
				    cpntBuilding, reg.GetComponent< CpntBuilding >( e  ), currentPath );
				if ( pathFound && distance < closestStorageDistance ) {
					closestStorage = e;
					closestStorageDistance = distance;
				}
				break;
			}

			if ( closestStorageDistance == INVALID_ENTITY_ID ) {
				ImGui::Text( "A producing building has no connection to a storage, stall for now" );
				producer.timeSinceLastProduction = producer.timeToProduceBatch;
			} else {
				// Produce a batch
				producer.timeSinceLastProduction -= producer.timeToProduceBatch;
				// Spawn a dummy who will move the batch the nearest storage house
				Entity carrier = reg.CreateEntity();
				reg.AssignComponent< CpntRenderModel >( carrier, g_modelAtlas.cubeMesh );
				CpntTransform & transform = reg.AssignComponent< CpntTransform >( carrier );
				CpntNavAgent &  navAgent = reg.AssignComponent< CpntNavAgent >( carrier );
				navAgent.pathfindingNextSteps = currentPath;
				CpntResourceCarrier & resourceCarrier = reg.AssignComponent< CpntResourceCarrier >( carrier );
				resourceCarrier.amount = producer.batchSize;
				resourceCarrier.resource = producer.resource;

				transform.SetTranslation( GetPointInMiddleOfCell(
				    navAgent.pathfindingNextSteps[ navAgent.pathfindingNextSteps.size() - 1 ] ) );
				reg.messageBroker.AddListener( carrier, carrier, OnAgentArrived,
				                               MESSAGE_PATHFINDING_DESTINATION_REACHED );
			}
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
