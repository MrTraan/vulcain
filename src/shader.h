#pragma once

#include <GL/gl3w.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "packer.h"

struct Shader {
	u32 ID = 0;

	void Use() { glUseProgram( ID ); }

	void SetInt( const char * name, int value ) const { glUniform1i( glGetUniformLocation( ID, name ), value ); }
	void SetFloat( const char * name, float value ) const { glUniform1f( glGetUniformLocation( ID, name ), value ); }
	void SetBool( const char * name, bool value ) const { SetInt( name, ( int )value ); }
	void SetVector( const char * name, const glm::vec3 & v ) const {
		glUniform3f( glGetUniformLocation( ID, name ), v.x, v.y, v.z );
	}
	void SetMatrix( const char * name, const glm::mat4x4 & mat ) const {
		glUniformMatrix4fv( glGetUniformLocation( ID, name ), 1, GL_FALSE, glm::value_ptr( mat ) );
	}
	void SetMatrix3( const char * name, const glm::mat3x3 & mat ) const {
		glUniformMatrix3fv( glGetUniformLocation( ID, name ), 1, GL_FALSE, glm::value_ptr( mat ) );
	}
};

Shader CompileShaderFromCode( const char * vertexCode, int vertexSize, const char * fragmentCode, int fragmentSize );
Shader CompileShaderFromResource( const PackerResourceID & vertex, const PackerResourceID & frag );

struct ShaderAtlas {
	void CompileAllShaders();
	void FreeShaders();

	Shader defaultShader;
	Shader colorShader;
	Shader instancedShader;
};

extern ShaderAtlas g_shaderAtlas;
