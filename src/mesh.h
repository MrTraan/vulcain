#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "ngLib/types.h"

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;
	glm::vec3 tangent;
	glm::vec3 bitangent;
};

struct Texture {
	u32 id;
};

struct Mesh {
	std::vector<Vertex> vertices;
	std::vector<u32> indices;
	std::vector<Texture> textures;

	u32 vao;
	u32 vbo;
	u32 ebo;
};

void AllocateMeshGLBuffers( Mesh & mesh );