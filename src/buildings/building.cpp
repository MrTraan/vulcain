#include "buildings/building.h"
#include "collider.h"
#include "game.h"
#include "mesh.h"
#include "registery.h"

const char * GameResourceToString( GameResource resource ) {
	switch ( resource ) {
	case GameResource::WHEAT:
		return "Wheat";
	default:
		ng_assert( false );
		return nullptr;
	}
}

bool IsCellInsideBuilding( const CpntBuilding & building, Cell cell ) {
	return ( cell.x >= building.cell.x && cell.z >= building.cell.z && cell.x < building.cell.x + building.tileSizeX &&
	         cell.z < building.cell.z + building.tileSizeZ );
}

bool IsBuildingInsideArea( const CpntBuilding & building, const Area & area ) {
	switch ( area.shape ) {
	case Area::Shape::RECTANGLE: {
		// Check if the rectangle forming the building bounds and the area overlaps
		if ( building.cell.x >= area.center.x + area.sizeX || area.center.x >= building.cell.x + building.tileSizeX ) {
			return false; // One rectangle is on the left of the other
		}
		if ( building.cell.z >= area.center.z + area.sizeZ || area.center.z >= building.cell.z + building.tileSizeZ ) {
			return false; // One rectangle is on top of the other
		}
		return true;
	}
	default:
		ng_assert_msg( false, "Unimplemented shape in IsBuildingInsideArea" );
		false;
	}
}

bool IsCellAdjacentToBuilding( const CpntBuilding & building, Cell cell, const Map & map ) {
	if ( IsCellInsideBuilding( building, cell ) ) {
		return false;
	}
	return ( IsCellInsideBuilding( building, Cell( cell.x + 1, cell.z ) ) ||
	         IsCellInsideBuilding( building, Cell( cell.x - 1, cell.z ) ) ||
	         IsCellInsideBuilding( building, Cell( cell.x, cell.z + 1 ) ) ||
	         IsCellInsideBuilding( building, Cell( cell.x, cell.z - 1 ) ) );
}

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

void OnAgentArrived( Registery & reg, Entity sender, Entity receiver ) {
	ng_assert( sender == receiver );
	ng::Printf( "Entity %lu has arrived!\n", receiver );

	// Look for a storage house next to receiver
	CpntTransform & transform = reg.GetComponent< CpntTransform >( receiver );
	Cell            position = GetCellForPoint( transform.GetTranslation() );
	Entity          closestStorage = INVALID_ENTITY_ID;
	for ( auto & [ e, storage ] : reg.IterateOver< CpntBuildingStorage >() ) {
		CpntBuilding & building = reg.GetComponent< CpntBuilding >( e );
		if ( IsCellAdjacentToBuilding( building, position, theGame->map ) ) {
			closestStorage = e;
			break;
		}
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
			Entity                   closestStorage = INVALID_ENTITY_ID;
			u32                      closestStorageDistance = ULONG_MAX;
			ng::DynamicArray< Cell > currentPath( 32 );
			for ( auto & [ e, storage ] : reg.IterateOver< CpntBuildingStorage >() ) {
				u32  distance = 0;
				bool pathFound =
				    FindPathBetweenBuildings( cpntBuilding, reg.GetComponent< CpntBuilding >( e ), theGame->map,
				                              theGame->roadNetwork, currentPath, closestStorageDistance, &distance );
				if ( pathFound && distance < closestStorageDistance ) {
					closestStorage = e;
					closestStorageDistance = distance;
				}
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

				transform.SetTranslation( GetPointInMiddleOfCell( navAgent.pathfindingNextSteps.Last() ) );
				reg.messageBroker.AddListener( carrier, carrier, OnAgentArrived,
				                               MESSAGE_PATHFINDING_DESTINATION_REACHED );
			}
		}
	}
}

bool CpntBuildingStorage::StoreRessource( GameResource resource, u32 amount ) {
	// TODO: We could refuse some resources, or be full
	if ( storage.contains( resource ) ) {
		storage[ resource ] += amount;
	} else {
		storage[ resource ] = amount;
	}
	return true;
}
