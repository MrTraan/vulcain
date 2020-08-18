#include "renderer.h"
#include "ngLib/nglib.h"

u32 viewProjUBO;

void InitRenderer() { CreateUBOBuffers(); }

void ShutdownRenderer() { glDeleteBuffers( 1, &viewProjUBO ); }

void CreateUBOBuffers() {
	glGenBuffers( 1, &viewProjUBO );
	glBindBuffer( GL_UNIFORM_BUFFER, viewProjUBO );
	glBufferData( GL_UNIFORM_BUFFER, sizeof( ViewProjUBOData ), nullptr, GL_DYNAMIC_DRAW );
	glBindBuffer( GL_UNIFORM_BUFFER, 0 );

	glBindBufferRange( GL_UNIFORM_BUFFER, viewProjUBOIndex, viewProjUBO, 0, sizeof( ViewProjUBOData ) );
}

void FillViewProjUBO( const ViewProjUBOData * data ) {
	glBindBuffer( GL_UNIFORM_BUFFER, viewProjUBO );
	GLvoid * p = glMapBuffer( GL_UNIFORM_BUFFER, GL_WRITE_ONLY );
	memcpy( p, data, sizeof( ViewProjUBOData ) );
	glUnmapBuffer( GL_UNIFORM_BUFFER );
	glBindBuffer( GL_UNIFORM_BUFFER, 0 );
}

void Framebuffer::Allocate( int width, int height ) {
	this->width = width;
	this->height = height;

	glGenFramebuffers( 1, &framebufferID );

	glGenTextures( 1, &textureID );
	glBindTexture( GL_TEXTURE_2D, textureID );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	glBindFramebuffer( GL_FRAMEBUFFER, framebufferID );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0 );

	glGenRenderbuffers( 1, &renderbufferID );
	glBindRenderbuffer( GL_RENDERBUFFER, renderbufferID );

	glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbufferID );

	ng_assert( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glBindRenderbuffer( GL_RENDERBUFFER, 0 );
}

void Framebuffer::Destroy() {
	glDeleteRenderbuffers( 1, &renderbufferID );
	glDeleteTextures( 1, &textureID );
	glDeleteFramebuffers( 1, &framebufferID );
}

void Camera::Bind() {
	ViewProjUBOData uboData{};
	uboData.view = view;
	uboData.projection = proj;
	uboData.viewProj = proj * view;
	uboData.viewPosition = glm::vec4( position, 1.0f );

	FillViewProjUBO( &uboData );
}
