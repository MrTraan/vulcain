#include <GL/gl3w.h>
#include <stb_image.h>

#include "game.h"
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

Aabb ComputeMeshAabb( const Model & model, const CpntTransform & parentTransform ) {
	auto minWorldSpace = parentTransform.Transform( model.bounds.min );
	auto maxWorldSpace = parentTransform.Transform( model.bounds.max );
	Aabb bounds = {
	    {
	        MIN( minWorldSpace.x, maxWorldSpace.x ),
	        MIN( minWorldSpace.y, maxWorldSpace.y ),
	        MIN( minWorldSpace.z, maxWorldSpace.z ),
	    },
	    {
	        MAX( minWorldSpace.x, maxWorldSpace.x ),
	        MAX( minWorldSpace.y, maxWorldSpace.y ),
	        MAX( minWorldSpace.z, maxWorldSpace.z ),
	    },
	};
	return bounds;
}

void DrawModel( const Model &         model,
                const CpntTransform & parentTransform,
                Shader                shader,
                bool                  bindMaterial /*= true */ ) {
	for ( const Mesh & mesh : model.meshes ) {
		glm::mat4 transform = parentTransform.GetMatrix() * mesh.transformation;
		shader.SetMatrix( "modelTransform", transform );
		if ( bindMaterial ) {
			glm::mat3 normalMatrix( glm::transpose( glm::inverse( transform ) ) );
			shader.SetMatrix3( "normalTransform", normalMatrix );
			shader.SetVector( "material.ambient", mesh.material->ambiant );
			shader.SetVector( "material.diffuse", mesh.material->diffuse );
			shader.SetVector( "material.specular", mesh.material->specular );
			shader.SetFloat( "material.shininess", mesh.material->shininess );
		}

		glBindVertexArray( mesh.vao );
		if ( bindMaterial ) {
			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_2D, mesh.material->diffuseTexture.id );
		}
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
	bool successAll = true;
	for ( auto & res : theGame->package.resourceList ) {
		if ( res.type == PackerResource::Type::OBJ || res.type == PackerResource::Type::COLLADA ) {
			Model * model = new Model();
			bool    success = SetupModelFromResource( *model, res.id );
			ng_assert( success );
			successAll &= success;
			atlas[ res.id ] = model;
		}
	}
	return successAll;
}

void ModelAtlas::FreeAllModels() {
	for ( auto & [ id, model ] : atlas ) {
		if ( model != nullptr ) {
			FreeModelBuffers( *model );
			delete model;
		}
	}
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
	model.bounds.min = glm::vec3( minX, minY, minZ );
	model.bounds.max = glm::vec3( maxX, maxY, maxZ );
	model.size = model.bounds.min - model.bounds.max;

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

void InstancedModelBatch::AddInstance( Entity e, const glm::mat4 & transform ) {
	Instance & instance = instances.AllocateOne();
	instance.transform = transform;
	instance.id = e;
	dirty = true;
}

void InstancedModelBatch::AddInstanceAtPosition( const glm::vec3 & position ) {
	Instance & instance = instances.AllocateOne();
	instance.transform = glm::translate( glm::mat4( 1.0f ), position );
	instance.id = INVALID_ENTITY;
	dirty = true;
}

bool InstancedModelBatch::RemoveInstance( Entity e ) {
	bool modified = false;
	for ( int64 i = ( int64 )instances.Size() - 1; i >= 0; i-- ) {
		if ( instances[ i ].id == e ) {
			instances.DeleteIndexFast( i );
			modified = true;
		}
	}
	if ( modified ) {
		dirty = true;
		return true;
	}
	return false;
}

bool InstancedModelBatch::RemoveInstancesWithPosition( const glm::vec3 & position ) {
	glm::mat4 transform( 1.0f );
	transform = glm::translate( transform, position );
	bool modified = false;
	for ( int64 i = ( int64 )instances.Size() - 1; i >= 0; i-- ) {
		if ( instances[ i ].transform == transform ) {
			instances.DeleteIndexFast( i );
			modified = true;
		}
	}
	if ( modified ) {
		dirty = true;
		return true;
	}
	return false;
}

void InstancedModelBatch::Init( const Model * model ) {
	this->model = model;
	glGenBuffers( 1, &arrayBuffer );
	UpdateArrayBuffer();

	for ( unsigned int i = 0; i < model->meshes.size(); i++ ) {
		unsigned int VAO = model->meshes[ i ].vao;
		glBindVertexArray( VAO );
		glEnableVertexAttribArray( 3 );
		constexpr u64 stride = sizeof( Instance );
		constexpr u64 baseOffset = offsetof( Instance, transform );
		glVertexAttribPointer( 3, 4, GL_FLOAT, GL_FALSE, stride, ( void * )( baseOffset + 0 * sizeof( glm::vec4 ) ) );
		glEnableVertexAttribArray( 4 );
		glVertexAttribPointer( 4, 4, GL_FLOAT, GL_FALSE, stride, ( void * )( baseOffset + 1 * sizeof( glm::vec4 ) ) );
		glEnableVertexAttribArray( 5 );
		glVertexAttribPointer( 5, 4, GL_FLOAT, GL_FALSE, stride, ( void * )( baseOffset + 2 * sizeof( glm::vec4 ) ) );
		glEnableVertexAttribArray( 6 );
		glVertexAttribPointer( 6, 4, GL_FLOAT, GL_FALSE, stride, ( void * )( baseOffset + 3 * sizeof( glm::vec4 ) ) );
		glVertexAttribDivisor( 3, 1 );
		glVertexAttribDivisor( 4, 1 );
		glVertexAttribDivisor( 5, 1 );
		glVertexAttribDivisor( 6, 1 );

		glBindVertexArray( 0 );
	}
}

void InstancedModelBatch::UpdateArrayBuffer() {
	glBindBuffer( GL_ARRAY_BUFFER, arrayBuffer );
	glBufferData( GL_ARRAY_BUFFER, instances.Size() * sizeof( Instance ), instances.data, GL_STATIC_DRAW );
}

void InstancedModelBatch::Render( Shader & shader ) {
	ZoneScoped;
	if ( dirty ) {
		UpdateArrayBuffer();
		dirty = false;
	}
	shader.Use();
	for ( const Mesh & mesh : model->meshes ) {
		shader.SetMatrix( "baseTransform", mesh.transformation );
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
		                         instances.Size() );
		glDisable( GL_BLEND );
		glBindVertexArray( 0 );
	}
}
