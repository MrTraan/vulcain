#pragma once

#include "entity.h"
#include "game_time.h"
#include "io.h"
#include "map.h"
#include "navigation.h"
#include "packer.h"
#include "registery.h"
#include "renderer.h"
#include "window.h"

enum class MouseAction {
	SELECT,
	BUILD,
	BUILD_ROAD,
	DESTROY,
};

struct Game {
	enum class State {
		MENU,
		PLAYING,
		LOADING,
	};

	SystemManager systemManager;
	Registery     registery;
	State         state;
	IO            io;
	Window        window;
	PackerPackage package;
	Map           map;
	RoadNetwork   roadNetwork;
	Renderer      renderer;
	TimePoint     clock = 0;
	Duration      ticks = 1;
};

extern Game * theGame;
