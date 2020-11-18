#pragma once

#include <glm/glm.hpp>
#include <map>
#include <vector>

#include "entity.h"
#include "ngLib/ngcontainers.h"
#include "ngLib/types.h"
#include "packer.h"
#include "shader.h"
#include "system.h"

struct Aabb {
	glm::vec3 min;
	glm::vec3 max;
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;
};

struct Texture {
	u32  id;
	bool hasTransparency = false;

	static Texture DefaultWhiteTexture();
};

struct Material {
	enum RenderingMode {
		MODE_OPAQUE,
		MODE_TRANSPARENT,
	};
	RenderingMode mode = MODE_OPAQUE;
	Texture       diffuseTexture;
	glm::vec3     ambiant{ 1.0f, 1.0f, 1.0f };
	glm::vec3     diffuse{ 0.8f, 0.8f, 0.8f };
	glm::vec3     specular{ 0.5f, 0.5f, 0.5f };
	float         shininess = 9.0f;
};

struct Mesh {
	std::vector< Vertex > vertices;
	std::vector< u32 >    indices;
	Material *            material;
	glm::mat4             transformation{ 1.0f };

	u32 vao;
	u32 vbo;
	u32 ebo;
};

struct Model {
	std::map< std::string, Material > materials;
	std::vector< Mesh >               meshes;
	glm::vec3                         size;
	Aabb                              bounds;
	glm::i32vec3                      roundedSize;
};

struct ModelAtlas {
	bool LoadAllModels();
	void FreeAllModels();

	std::map< PackerResourceID, Model * > atlas;

	const Model * GetModel( PackerResourceID id ) { return atlas.at( id ); }
};

extern ModelAtlas g_modelAtlas;

struct CpntRenderModel {
	CpntRenderModel() = default;
	CpntRenderModel( const Model * model ) : model( model ) {}
	const Model * model = nullptr;
};

struct SystemRenderModel : System< CpntRenderModel > {};

struct InstancedModelBatch {
	const Model * model;
	struct Instance {
		glm::mat4 transform;
		Entity    id;
	};
	ng::DynamicArray< Instance > instances;
	bool                         dirty = false;
	u32                          arrayBuffer;

	void AddInstance( Entity e, const glm::mat4 & transform );
	void AddInstanceAtPosition( const glm::vec3 & position );
	bool RemoveInstance( Entity e );
	bool RemoveInstancesWithPosition( const glm::vec3 & position );
	void Init( const Model * model );
	void UpdateArrayBuffer();
	void Render( Shader & shader );
};

Texture CreateTextureFromResource( const PackerResource & resource );
Texture CreateTextureFromData( const u8 * data, int width, int height, int channels );
Texture CreatePlaceholderPinkTexture();
Texture CreateDefaultWhiteTexture();
void    AllocateMeshGLBuffers( Mesh & mesh );
Aabb    ComputeMeshAabb( const Model & model, const CpntTransform & parentTransform );
void    DrawModel( const Model & model, const CpntTransform & transform, Shader shader, bool bindMaterial = true );
void    ComputeModelSize( Model & model );

void CreateTexturedPlane(
    float sizeX, float sizeZ, float textureTiling, const PackerResource & textureResource, Model & modelOut );
