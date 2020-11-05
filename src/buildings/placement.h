#pragma once

#include "buildings/building.h"
#include "registery.h"

struct Map;
struct Model;

bool          CanPlaceBuilding( const Cell cell, BuildingKind kind, const Map & map );
Entity        FindBuildingByPosition( Registery & reg, const Cell cell );
Entity        PlaceBuilding( Registery & reg, const Cell cell, BuildingKind kind, Map & map );
bool          DeleteBuildingByPosition( Registery & reg, const Cell cell, Map & map );
int           DeleteBuildingsInsideArea( Registery & reg, const Area & area, Map & map );
const Model * GetGameResourceModel( GameResource kind );
glm::i32vec2  GetBuildingSize( BuildingKind kind );
