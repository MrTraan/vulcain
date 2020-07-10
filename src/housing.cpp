#include <imgui/imgui.h>

#include "housing.h"

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

void SystemBuildingProducing::Update( Registery & reg, float dt ) {
	for ( auto & [ e, producer ] : reg.IterateOver< CpntBuildingProducing >() ) {
		producer.timeSinceLastProduction += ng::DurationInSeconds( dt );
		if ( producer.timeSinceLastProduction >= producer.timeToProduceBatch ) {
			// Produce a batch
			if ( producer.currentlyStoring + producer.batchSize <= producer.maxStorageSize ) {
				producer.currentlyStoring += producer.batchSize;
				producer.timeSinceLastProduction -= producer.timeToProduceBatch;
			} else {
				// Stuck because storage is full
				producer.timeSinceLastProduction = producer.timeToProduceBatch;
			}
		}
	}
}

void SystemBuildingProducing::DebugDraw() { ImGui::Text( "Hello from building producing!" ); }
