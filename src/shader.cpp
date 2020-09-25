#include "shader.h"
#include "game.h"
#include "packer_resource_list.h"
#include <map>

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
	std::map< PackerResourceID, u32 > shadersCompiled;
	for ( auto & res : theGame->package.resourceList ) {
		if ( res.type == PackerResource::Type::VERTEX_SHADER ) {
			u32    vertex = glCreateShader( GL_VERTEX_SHADER );
			char * vertexCode = ( char * )theGame->package.GrabResourceData( res );
			int    vertexSize = ( int )res.size;
			glShaderSource( vertex, 1, &vertexCode, &vertexSize );
			glCompileShader( vertex );
			int ret = CheckCompileErrors( vertex );
			if ( ret == 0 ) {
				shadersCompiled.emplace( res.id, vertex );
			}
		}
		if ( res.type == PackerResource::Type::FRAGMENT_SHADER ) {
			u32    fragment = glCreateShader( GL_FRAGMENT_SHADER );
			char * fragmentCode = ( char * )theGame->package.GrabResourceData( res );
			int    fragmentSize = ( int )res.size;
			glShaderSource( fragment, 1, &fragmentCode, &fragmentSize );
			glCompileShader( fragment );
			int ret = CheckCompileErrors( fragment );
			if ( ret == 0 ) {
				shadersCompiled.emplace( res.id, fragment );
			}
		}
	}

	auto LinkProgram = [ & ]( const PackerResourceID & vertexID, const PackerResourceID & fragID ) -> Shader {
		Shader shader;
		shader.ID = glCreateProgram();
		if ( shadersCompiled.contains( vertexID ) ) {
			glAttachShader( shader.ID, shadersCompiled.at( vertexID ) );
		}
		if ( shadersCompiled.contains( fragID ) ) {
			glAttachShader( shader.ID, shadersCompiled.at( fragID ) );
		}
		glLinkProgram( shader.ID );
		CheckLinkErrors( shader.ID );
		return shader;
	};

	defaultShader = LinkProgram( PackerResources::SHADERS_DEFAULT_VERT, PackerResources::SHADERS_DEFAULT_FRAG );
	colorShader = LinkProgram( PackerResources::SHADERS_COLORED_VERT, PackerResources::SHADERS_COLORED_FRAG );
	instancedShader = LinkProgram( PackerResources::SHADERS_INSTANCED_VERT, PackerResources::SHADERS_DEFAULT_FRAG );
	deferredShader = LinkProgram( PackerResources::SHADERS_DEFAULT_VERT, PackerResources::SHADERS_DEFERRED_FRAG );
	instancedDeferredShader =
	    LinkProgram( PackerResources::SHADERS_INSTANCED_VERT, PackerResources::SHADERS_DEFERRED_FRAG );
	postProcessShader =
	    LinkProgram( PackerResources::SHADERS_PASSTHROUGH_VERT, PackerResources::SHADERS_POSTPROCESS_FRAG );
	ssaoShader = LinkProgram( PackerResources::SHADERS_PASSTHROUGH_VERT, PackerResources::SHADERS_SSAO_FRAG );
	ssaoBlurShader = LinkProgram( PackerResources::SHADERS_PASSTHROUGH_VERT, PackerResources::SHADERS_SSAO_BLUR_FRAG );

	for ( auto & [ id, shader ] : shadersCompiled ) {
		glDeleteShader( shader );
	}
}

void ShaderAtlas::FreeShaders() {
	glDeleteProgram( defaultShader.ID );
	glDeleteProgram( colorShader.ID );
	glDeleteProgram( instancedShader.ID );
	glDeleteProgram( deferredShader.ID );
	glDeleteProgram( instancedDeferredShader.ID );
	glDeleteProgram( postProcessShader.ID );
	glDeleteProgram( ssaoShader.ID );
	glDeleteProgram( ssaoBlurShader.ID );
}
