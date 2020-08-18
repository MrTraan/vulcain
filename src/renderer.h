#pragma once

#include "ngLib/types.h"
#include <GL/gl3w.h>
#include <glm/glm.hpp>

constexpr u32 viewProjUBOIndex = 0u;

struct ViewProjUBOData {
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 viewProj;
	glm::vec3 viewPosition;
};

struct Camera {
	glm::vec3 position;
	glm::mat4 proj;
	glm::mat4 view;

	void Bind();
};

struct Framebuffer {
	int width;
	int height;
	u32 framebufferID;
	u32 renderbufferID;
	u32 textureID;

	void Allocate( int width, int height );
	void Destroy();

	void Clear() { glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ); }

	void Bind() {
		glViewport( 0, 0, width, height );
		glBindFramebuffer( GL_FRAMEBUFFER, framebufferID );
	}
};

void InitRenderer();
void ShutdownRenderer();

void CreateUBOBuffers();
void FillViewProjUBO( const ViewProjUBOData * data );
void BindDefaultFramebuffer();
