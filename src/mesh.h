#pragma once

#include <glm/glm.hpp>
#include <map>
#include <vector>

#include "ngLib/types.h"
#include "packer.h"

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;
};

struct Texture {
	u32 id;
};

struct Material {
	enum RenderingMode {
		MODE_OPAQUE,
		MODE_TRANSPARENT,
	};
	RenderingMode mode = MODE_OPAQUE;
	Texture       diffuseTexture;
	glm::vec3     ambiant = { 1.0f, 1.0f, 1.0f };
	glm::vec3     diffuse = { 0.8f, 0.8f, 0.8f };
	glm::vec3     specular = { 0.5f, 0.5f, 0.5f };
	float         shininess = 9.0f;
};

struct Mesh {
	std::vector< Vertex > vertices;
	std::vector< u32 >    indices;
	Material *            material;

	u32 vao;
	u32 vbo;
	u32 ebo;
};

struct CpntRenderModel {
	std::map< std::string, Material > materials;
	std::vector< Mesh >               meshes;
	glm::vec3                         size;
	glm::i32vec3                      roundedSize;
};


Texture CreateTextureFromResource( const PackerResource & resource );
Texture CreateTextureFromData( const u8 * data, int width, int height, int channels );
Texture CreatePlaceholderPinkTexture();
Texture CreateDefaultWhiteTexture();
void    AllocateMeshGLBuffers( Mesh & mesh );