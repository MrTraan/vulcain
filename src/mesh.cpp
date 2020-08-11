#include <GL/gl3w.h>
#include <stb_image.h>

#include "Game.h"
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
	u8 data[ 4 * 4 * 4 ];
	for ( int i = 0; i < 4 * 4 * 4; i += 4 ) {
		data[ i + 0 ] = 0xff;
		data[ i + 1 ] = 0x00;
		data[ i + 2 ] = 0xff;
		data[ i + 3 ] = 0xff;
	}
	return CreateTextureFromData( data, 4, 4, 4 );
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

void FreeMeshGLBuffers( Mesh & mesh ) {
	glDeleteBuffers( 1, &mesh.ebo );
	glDeleteBuffers( 1, &mesh.vbo );
	glDeleteVertexArrays( 1, &mesh.vao );
}

static bool SetupModelFromResource( Model & model, PackerResourceID resourceID ) {
	bool success = ImportObjFile( resourceID, model );
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
	success &= SetupModelFromResource( *houseMesh, PackerResources::HOUSE_OBJ );
	success &= SetupModelFromResource( *farmMesh, PackerResources::FARM_OBJ );
	success &= SetupModelFromResource( *cubeMesh, PackerResources::CUBE_OBJ );
	success &= SetupModelFromResource( *roadMesh, PackerResources::ROAD_OBJ );
	success &= SetupModelFromResource( *storeHouseMesh, PackerResources::STOREHOUSE_OBJ );

	return success;
}

void ModelAtlas::FreeAllModels() {
	FreeModelBuffers( *houseMesh );
	FreeModelBuffers( *farmMesh);
	FreeModelBuffers( *cubeMesh );
	FreeModelBuffers( *roadMesh );
	FreeModelBuffers( *storeHouseMesh );

	delete houseMesh;
	delete farmMesh;
	delete cubeMesh;
	delete roadMesh;
	delete storeHouseMesh;
}

