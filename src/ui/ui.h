#pragma once

#include "renderer.h"

namespace ui {
	struct UIContext {
		Camera camera;
	};

    void DrawUI();

	bool Button(const char * text);
};