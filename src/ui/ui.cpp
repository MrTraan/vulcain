#include "ui.h"
#include <glm/glm.hpp>

ui::UIContext ctx;

void ui::DrawUI() {
	ctx.camera.position = glm::vec3( 0, 0, 0 );
	ctx.camera.view;
	ctx.camera.proj;
}

