#pragma once

#include "io.h"
#include "packer.h"
#include "window.h"

constexpr float FIXED_TIMESTEP = 1.0f / 30.0f;

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
};

extern Game * theGame;
