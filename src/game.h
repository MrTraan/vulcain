#pragma once

#include "io.h"
#include "map.h"
#include "navigation.h"
#include "packer.h"
#include "window.h"

constexpr float FIXED_TIMESTEP = 1.0f / 30.0f;

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

	State         state;
	IO            io;
	Window        window;
	PackerPackage package;
	Map           map;
	RoadNetwork   roadNetwork;
};

extern Game * theGame;
