#include "shader.h"
#include "game.h"
#include "packer_resource_list.h"

ShaderAtlas g_shaderAtlas;

static int CheckCompileErrors( unsigned int shader ) {
	int  success;
	char infoLog[ 1024 ];

	glGetShaderiv( shader, GL_COMPILE_STATUS, &success );
	if ( !success ) {
		glGetShaderInfoLog( shader, 1024, NULL, infoLog );
		ng::Errorf( "Shader compilation failed: %s\n", infoLog );
		return 1;
	}
	return 0;
}

static int CheckLinkErrors( unsigned int shader ) {
	int  success;
	char infoLog[ 1024 ];

	glGetProgramiv( shader, GL_LINK_STATUS, &success );
	if ( !success ) {
		glGetProgramInfoLog( shader, 1024, NULL, infoLog );
		ng::Errorf( "Shader link failed: %s\n", infoLog );
		return 1;
	}
	return 0;
}

Shader CompileShaderFromCode( const char * vertexCode, int vertexSize, const char * fragmentCode, int fragmentSize ) {
	Shader shader;

	u32 vertex = glCreateShader( GL_VERTEX_SHADER );
	glShaderSource( vertex, 1, &vertexCode, &vertexSize );
	glCompileShader( vertex );
	if ( CheckCompileErrors( vertex ) != 0 ) {
		return shader;
	}

	u32 fragment = glCreateShader( GL_FRAGMENT_SHADER );
	glShaderSource( fragment, 1, &fragmentCode, &fragmentSize );
	glCompileShader( fragment );
	if ( CheckCompileErrors( fragment ) != 0 ) {
		return shader;
	}

	shader.ID = glCreateProgram();
	glAttachShader( shader.ID, vertex );
	glAttachShader( shader.ID, fragment );
	glLinkProgram( shader.ID );
	CheckLinkErrors( shader.ID );

	glDeleteShader( vertex );
	glDeleteShader( fragment );

	return shader;
}

Shader CompileShaderFromResource( const PackerResourceID & vertexID, const PackerResourceID & fragID ) {
	auto vertex = theGame->package.GrabResource( vertexID );
	auto frag = theGame->package.GrabResource( fragID );
	ng_assert( vertex->type == PackerResource::Type::VERTEX_SHADER );
	ng_assert( frag->type == PackerResource::Type::FRAGMENT_SHADER );

	return CompileShaderFromCode( ( char * )theGame->package.GrabResourceData( *vertex ), ( int )vertex->size,
	                              ( char * )theGame->package.GrabResourceData( *frag ), ( int )frag->size );
}

void ShaderAtlas::CompileAllShaders() {
	defaultShader =
	    CompileShaderFromResource( PackerResources::SHADERS_DEFAULT_VERT, PackerResources::SHADERS_DEFAULT_FRAG );
	colorShader =
	    CompileShaderFromResource( PackerResources::SHADERS_COLORED_VERT, PackerResources::SHADERS_COLORED_FRAG );
	instancedShader =
	    CompileShaderFromResource( PackerResources::SHADERS_INSTANCED_VERT, PackerResources::SHADERS_DEFAULT_FRAG );
}

void ShaderAtlas::FreeShaders() {
	glDeleteShader( defaultShader.ID );
	glDeleteShader( colorShader.ID );
	glDeleteShader( instancedShader.ID );
}
