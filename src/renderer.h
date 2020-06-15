#pragma once

#include <GL/gl3w.h>
#include <glm/glm.hpp>
#include "ngLib/types.h"

constexpr u32 viewProjUBOIndex = 0u;

struct ViewProjUBOData {
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 viewProj;
};

void InitRenderer();
void ShutdownRenderer();

void CreateUBOBuffers();
void FillViewProjUBO( const ViewProjUBOData * data );
