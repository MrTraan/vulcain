#include "entity.h"
#include "registery.h"

void SystemManager::Update( Registery & reg, float dt ) {
	reg.FlushDeleteQueue();
	for ( auto [ type, system ] : systems ) {
		system->Update( reg, dt );
	}
	reg.FlushDeleteQueue();
}
