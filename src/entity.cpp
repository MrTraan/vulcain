#include "entity.h"
#include "registery.h"
#include "../lib/tracy/Tracy.hpp"

void SystemManager::Update( Registery & reg, Duration ticks ) {
	ZoneScoped;
	reg.FlushDeleteQueue();
	for ( auto [ type, system ] : systems ) {
		system->Update( reg, ticks );
	}
	reg.FlushDeleteQueue();
}
