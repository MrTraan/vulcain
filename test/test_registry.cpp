#include "../src/buildings/building.h"
#include "../src/game.h"
#include "../src/registery.h"
#include <catch.hpp>

TEST_CASE( "Removed entities will be reallocated", "[bump entity version]" ) {
	theGame = new Game();
	theGame->registery = new Registery( &theGame->systemManager );
	theGame->systemManager.CreateSystem< SystemBuilding >();
	theGame->map.AllocateGrid(200, 200);
	Registery & reg = *theGame->registery;

	ng::DynamicArray< Entity > entitiesAlive;
	for ( u32 i = 0; i < INITIAL_ENTITY_ALLOC / 4; i++ ) {
		Entity a = reg.CreateEntity();
		auto & buildingA = reg.AssignComponent< CpntBuilding >( a );
		buildingA.kind = BuildingKind::MARKET;
		Entity b = reg.CreateEntity();
		auto & buildingB = reg.AssignComponent< CpntBuilding >( b );
		buildingB.kind = BuildingKind::FOUNTAIN;
		Entity c = reg.CreateEntity();
		auto & buildingC = reg.AssignComponent< CpntBuilding >( c );
		buildingC.kind = BuildingKind::HOUSE;
		Entity d = reg.CreateEntity();
		auto & buildingD = reg.AssignComponent< CpntBuilding >( d );
		buildingD.kind = BuildingKind::ROAD_BLOCK;

		theGame->systemManager.Update( reg, 1 );

		reg.MarkForDelete( b );
		reg.MarkForDelete( d );
		entitiesAlive.PushBack( a );
		entitiesAlive.PushBack( c );
	}

	Entity last = reg.CreateEntity();
	REQUIRE( last.id == 1 );
	REQUIRE( last.version == 1 );
	reg.MarkForDelete(last);
	
	for ( u32 i = 0; i < INITIAL_ENTITY_ALLOC / 4; i++ ) {
		Entity a = reg.CreateEntity();
		auto & buildingA = reg.AssignComponent< CpntBuilding >( a );
		buildingA.kind = BuildingKind::STORAGE_HOUSE;
		entitiesAlive.PushBack( a );
		theGame->systemManager.Update( reg, 1 );
	}
}