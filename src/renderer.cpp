#include "renderer.h"

u32 viewProjUBO;

void InitRenderer() {
	CreateUBOBuffers();
}

void ShutdownRenderer() {
	glDeleteBuffers( 1, &viewProjUBO );
}

void CreateUBOBuffers() {
	glGenBuffers( 1, &viewProjUBO );
	glBindBuffer( GL_UNIFORM_BUFFER, viewProjUBO );
	glBufferData( GL_UNIFORM_BUFFER, sizeof( ViewProjUBOData ), nullptr, GL_STATIC_DRAW );
	glBindBuffer( GL_UNIFORM_BUFFER, 0 );

	glBindBufferRange( GL_UNIFORM_BUFFER, viewProjUBOIndex, viewProjUBO, 0, sizeof( ViewProjUBOData ) );
}

void FillViewProjUBO( const ViewProjUBOData * data ) {
	glBindBuffer( GL_UNIFORM_BUFFER, viewProjUBO );
	glBufferSubData( GL_UNIFORM_BUFFER, 0, sizeof(ViewProjUBOData), data );
	glBindBuffer( GL_UNIFORM_BUFFER, 0 );
}

