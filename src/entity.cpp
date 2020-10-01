#include "entity.h"
#include "registery.h"
#include "../lib/tracy/Tracy.hpp"

void SystemManager::Update( Registery & reg, float dt ) {
	ZoneScoped;
	reg.FlushDeleteQueue();
	for ( auto [ type, system ] : systems ) {
		system->Update( reg, dt );
	}
	reg.FlushDeleteQueue();
}
