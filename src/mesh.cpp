#include <GL/gl3w.h>
#include <stb_image.h>

#include "Game.h"
#include "collada_parser.h"
#include "mesh.h"
#include "obj_parser.h"
#include "packer_resource_list.h"

ModelAtlas g_modelAtlas;

Texture CreateTextureFromResource( const PackerResource & resource ) {
	int       width;
	int       height;
	int       channels;
	stbi_uc * data = stbi_load_from_memory( theGame->package.GrabResourceData( resource ), ( int )resource.size, &width,
	                                        &height, &channels, 0 );
	Texture   texture = CreateTextureFromData( data, width, height, channels );
	stbi_image_free( data );
	return texture;
}

Texture CreateTextureFromData( const u8 * data, int width, int height, int channels ) {
	Texture texture;
	GLenum  format = GL_RGBA;
	switch ( channels ) {
	case 1:
		format = GL_RED;
		break;
	case 3:
		format = GL_RGB;
		break;
	case 4:
		format = GL_RGBA;
		break;
	default:
		ng_assert( false );
	}
	texture.hasTransparency = format == GL_RGBA ? true : false;
	glGenTextures( 1, &texture.id );
	glBindTexture( GL_TEXTURE_2D, texture.id );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, format, GL_UNSIGNED_BYTE, data );
	glGenerateMipmap( GL_TEXTURE_2D );
	return texture;
}

Texture CreatePlaceholderPinkTexture() {
	u8 data[ 4 * 4 * 3 ];
	for ( int i = 0; i < 4 * 4 * 3; i += 4 ) {
		data[ i + 0 ] = 0xff;
		data[ i + 1 ] = 0x00;
		data[ i + 2 ] = 0xff;
	}
	return CreateTextureFromData( data, 4, 4, 3 );
}

Texture CreateDefaultWhiteTexture() {
	u8 data[ 4 * 4 * 4 ];
	for ( int i = 0; i < 4 * 4 * 4; i++ ) {
		data[ i ] = 0xff;
	}
	return CreateTextureFromData( data, 4, 4, 4 );
}

void AllocateMeshGLBuffers( Mesh & mesh ) {
	glGenVertexArrays( 1, &mesh.vao );
	glGenBuffers( 1, &mesh.vbo );
	glGenBuffers( 1, &mesh.ebo );

	glBindVertexArray( mesh.vao );

	glBindBuffer( GL_ARRAY_BUFFER, mesh.vbo );
	glBufferData( GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof( Vertex ), &mesh.vertices[ 0 ], GL_STATIC_DRAW );

	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mesh.ebo );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof( u32 ), &mesh.indices[ 0 ], GL_STATIC_DRAW );

	// Set vertex attribute pointers
	// positions
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex ), ( void * )offsetof( Vertex, position ) );
	// normals
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex ), ( void * )offsetof( Vertex, normal ) );
	// texture coords
	glEnableVertexAttribArray( 2 );
	glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex ), ( void * )offsetof( Vertex, texCoords ) );

	glBindVertexArray( 0 );
}

void DrawModel( const Model & model, const CpntTransform & parentTransform, Shader shader ) {
	for ( const Mesh & mesh : model.meshes ) {
		glm::mat4 transform = parentTransform.GetMatrix() * mesh.transformation;
		shader.SetMatrix( "modelTransform", transform );
		glm::mat3 normalMatrix( glm::transpose( glm::inverse( transform ) ) );
		shader.SetMatrix3( "normalTransform", normalMatrix );
		shader.SetVector( "material.ambient", mesh.material->ambiant );
		shader.SetVector( "material.diffuse", mesh.material->diffuse );
		shader.SetVector( "material.specular", mesh.material->specular );
		shader.SetFloat( "material.shininess", mesh.material->shininess );

		glActiveTexture( GL_TEXTURE0 );
		glBindVertexArray( mesh.vao );
		glBindTexture( GL_TEXTURE_2D, mesh.material->diffuseTexture.id );
		if ( mesh.material->mode == Material::MODE_TRANSPARENT ) {
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		}
		glDrawElements( GL_TRIANGLES, ( GLsizei )mesh.indices.size(), GL_UNSIGNED_INT, nullptr );
		glDisable( GL_BLEND );
		glBindVertexArray( 0 );
	}
}

void FreeMeshGLBuffers( Mesh & mesh ) {
	glDeleteBuffers( 1, &mesh.ebo );
	glDeleteBuffers( 1, &mesh.vbo );
	glDeleteVertexArrays( 1, &mesh.vao );
}

static bool SetupModelFromResource( Model & model, PackerResourceID resourceID ) {
	bool success = false;
	auto resource = theGame->package.GrabResource( resourceID );
	if ( resource->type == PackerResource::Type::OBJ ) {
		success = ImportObjFile( resourceID, model );
	} else if ( resource->type == PackerResource::Type::COLLADA ) {
		success = ImportColladaFile( resourceID, model );
	} else {
		ng_assert( false );
	}
	if ( success == false ) {
		return success;
	}

	for ( Mesh & mesh : model.meshes ) {
		AllocateMeshGLBuffers( mesh );
	}
	return true;
}

static void FreeModelBuffers( Model & model ) {
	for ( Mesh & mesh : model.meshes ) {
		FreeMeshGLBuffers( mesh );
	}
}

bool ModelAtlas::LoadAllModels() {
	bool success = true;
	houseMesh = new Model();
	farmMesh = new Model();
	cubeMesh = new Model();
	roadMesh = new Model();
	storeHouseMesh = new Model();
	roadBlockMesh = new Model();
	marketMesh = new Model();
	fountainMesh = new Model();
	success &= SetupModelFromResource( *houseMesh, PackerResources::FUTURUSTIC_HOUSE_DAE );
	success &= SetupModelFromResource( *farmMesh, PackerResources::NICE_HOUSE_DAE );
	success &= SetupModelFromResource( *cubeMesh, PackerResources::CUBE_DAE );
	success &= SetupModelFromResource( *roadMesh, PackerResources::ROAD_OBJ );
	success &= SetupModelFromResource( *storeHouseMesh, PackerResources::STOREHOUSE_OBJ );
	success &= SetupModelFromResource( *roadBlockMesh, PackerResources::ROAD_BLOCK_DAE );
	success &= SetupModelFromResource( *marketMesh, PackerResources::MARKET_DAE );
	success &= SetupModelFromResource( *fountainMesh, PackerResources::WELL_DAE );

	return success;
}

void ModelAtlas::FreeAllModels() {
	FreeModelBuffers( *houseMesh );
	FreeModelBuffers( *farmMesh );
	FreeModelBuffers( *cubeMesh );
	FreeModelBuffers( *roadMesh );
	FreeModelBuffers( *storeHouseMesh );
	FreeModelBuffers( *roadBlockMesh );
	FreeModelBuffers( *marketMesh );
	FreeModelBuffers( *fountainMesh );

	delete houseMesh;
	delete farmMesh;
	delete cubeMesh;
	delete roadMesh;
	delete storeHouseMesh;
	delete roadBlockMesh;
	delete marketMesh;
	delete fountainMesh;
}

void ComputeModelSize( Model & model ) {
	ng_assert( model.meshes.size() > 0 );
	ng_assert( model.meshes[ 0 ].vertices.size() > 0 );
	float minX = FLT_MAX;
	float minY = FLT_MAX;
	float minZ = FLT_MAX;
	float maxX = FLT_MIN;
	float maxY = FLT_MIN;
	float maxZ = FLT_MIN;
	for ( auto const & mesh : model.meshes ) {
		for ( auto const & vertex : mesh.vertices ) {
			auto position = mesh.transformation * glm::vec4( vertex.position, 1.0f );
			if ( position.x < minX )
				minX = position.x;
			if ( position.y < minY )
				minY = position.y;
			if ( position.z < minZ )
				minZ = position.z;
			if ( position.x > maxX )
				maxX = position.x;
			if ( position.y > maxY )
				maxY = position.y;
			if ( position.z > maxZ )
				maxZ = position.z;
		}
	}
	model.minCoords = glm::vec3( minX, minY, minZ );
	model.maxCoords = glm::vec3( maxX, maxY, maxZ );
	model.size = model.maxCoords - model.minCoords;

	model.roundedSize.x = ( int )ceilf( model.size.x );
	model.roundedSize.y = ( int )ceilf( model.size.y );
	model.roundedSize.z = ( int )ceilf( model.size.z );
}

void CreateTexturedPlane(
    float sizeX, float sizeZ, float textureTiling, const PackerResource & textureResource, Model & modelOut ) {
	Material & mat = modelOut.materials[ "default" ];
	mat.diffuseTexture = CreateTextureFromResource( textureResource );

	modelOut.meshes.resize( 1 );
	Mesh & mesh = modelOut.meshes[ 0 ];
	mesh.material = &mat;

	mesh.vertices.resize( 4 );
	mesh.vertices[ 0 ] = Vertex{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } };
	mesh.vertices[ 1 ] = Vertex{ { sizeX, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { textureTiling, 0.0f } };
	mesh.vertices[ 2 ] = Vertex{ { sizeX, 0.0f, sizeZ }, { 0.0f, 1.0f, 0.0f }, { textureTiling, textureTiling } };
	mesh.vertices[ 3 ] = Vertex{ { 0.0f, 0.0f, sizeZ }, { 0.0f, 1.0f, 0.0f }, { 0.0f, textureTiling } };

	mesh.indices.resize( 6 );
	mesh.indices[ 0 ] = 0;
	mesh.indices[ 1 ] = 2;
	mesh.indices[ 2 ] = 1;
	mesh.indices[ 3 ] = 0;
	mesh.indices[ 4 ] = 3;
	mesh.indices[ 5 ] = 2;

	ComputeModelSize( modelOut );
	AllocateMeshGLBuffers( mesh );
}

Texture Texture::DefaultWhiteTexture() {
	static Texture defaultTexture = CreateDefaultWhiteTexture();
	return defaultTexture;
}

void InstancedModelBatch::AddInstanceAtPosition( const glm::vec3 & position ) {
	positions.PushBack( position );
	dirty = true;
}

bool InstancedModelBatch::RemoveInstancesWithPosition( const glm::vec3 & position ) {
	bool modified = positions.DeleteValueFast( position );
	if ( modified ) {
		dirty = true;
		return true;
	}
	return false;
}

void InstancedModelBatch::Init( Model * model ) {
	this->model = model;
	glGenBuffers( 1, &arrayBuffer );
	UpdateArrayBuffer();

	for ( unsigned int i = 0; i < model->meshes.size(); i++ ) {
		unsigned int VAO = model->meshes[ i ].vao;
		glBindVertexArray( VAO );
		glEnableVertexAttribArray( 3 );
		glVertexAttribPointer( 3, 3, GL_FLOAT, GL_FALSE, sizeof( glm::vec3 ), ( void * )0 );
		glVertexAttribDivisor( 3, 1 );

		glBindVertexArray( 0 );
	}
}

void InstancedModelBatch::UpdateArrayBuffer() {
	glBindBuffer( GL_ARRAY_BUFFER, arrayBuffer );
	glBufferData( GL_ARRAY_BUFFER, positions.Size() * sizeof( glm::vec3 ), positions.data, GL_STATIC_DRAW );
}

void InstancedModelBatch::Render( Shader & shader ) {
	ZoneScoped;
	if ( dirty ) {
		UpdateArrayBuffer();
		dirty = false;
	}
	shader.Use();
	for ( const Mesh & mesh : model->meshes ) {
		glm::mat4 transform( 1.0f );
		glm::mat3 normalMatrix( glm::transpose( glm::inverse( transform ) ) );
		shader.SetMatrix3( "normalTransform", normalMatrix );
		shader.SetVector( "material.ambient", mesh.material->ambiant );
		shader.SetVector( "material.diffuse", mesh.material->diffuse );
		shader.SetVector( "material.specular", mesh.material->specular );
		shader.SetFloat( "material.shininess", mesh.material->shininess );

		glActiveTexture( GL_TEXTURE0 );
		glBindVertexArray( mesh.vao );
		glBindTexture( GL_TEXTURE_2D, mesh.material->diffuseTexture.id );
		if ( mesh.material->mode == Material::MODE_TRANSPARENT ) {
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		}
		glDrawElementsInstanced( GL_TRIANGLES, ( GLsizei )mesh.indices.size(), GL_UNSIGNED_INT, nullptr,
		                         positions.Size() );
		glDisable( GL_BLEND );
		glBindVertexArray( 0 );
	}
}
