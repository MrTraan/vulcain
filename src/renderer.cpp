#include "renderer.h"
#include "environment/trees.h"
#include "game.h"
#include "mesh.h"
#include "ngLib/nglib.h"
#include "registery.h"
#include <random>
#include <tracy/Tracy.hpp>
#include <tracy/TracyOpenGL.hpp>

int SHADOW_MAP_WIDTH = 2048;
int SHADOW_MAP_HEIGHT = 2048;

constexpr int ssaoKernelSize = 32;

ng::DynamicArray< glm::vec3 > CreateSSAOKernel();
u32                           CreateSSAONoiseTexture();

void Renderer::InitRenderer( int width, int height ) {
	gbuffer.Allocate( width, height );

	{
		// Create UBO buffers
		glGenBuffers( 1, &viewProjUBO );
		glGenBuffers( 1, &lightUBO );
		glBindBuffer( GL_UNIFORM_BUFFER, viewProjUBO );
		glBufferData( GL_UNIFORM_BUFFER, sizeof( ViewProjUBOData ), nullptr, GL_STATIC_DRAW );
		glBindBuffer( GL_UNIFORM_BUFFER, lightUBO );
		glBufferData( GL_UNIFORM_BUFFER, sizeof( LightUBOData ), nullptr, GL_STATIC_DRAW );
		glBindBuffer( GL_UNIFORM_BUFFER, 0 );

		for ( auto & shader : g_shaderAtlas.shaders ) {
			glUniformBlockBinding( shader.ID, glGetUniformBlockIndex( shader.ID, "Matrices" ), viewProjUBOIndex );
			glUniformBlockBinding( shader.ID, glGetUniformBlockIndex( shader.ID, "Light" ), lightUBOIndex );
		}

		glBindBufferRange( GL_UNIFORM_BUFFER, viewProjUBOIndex, viewProjUBO, 0, sizeof( ViewProjUBOData ) );
		glBindBufferRange( GL_UNIFORM_BUFFER, lightUBOIndex, lightUBO, 0, sizeof( LightUBOData ) );
	}

	// Create full screen quad in NDC
	float quadVertices[] = {
	    // 3 floats position, 2 floats texture coords
	    -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
	    1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
	};
	// setup plane VAO
	glGenVertexArrays( 1, &fullScreenQuadVAO );
	glGenBuffers( 1, &fullScreenQuadVBO );
	glBindVertexArray( fullScreenQuadVAO );
	glBindBuffer( GL_ARRAY_BUFFER, fullScreenQuadVBO );
	glBufferData( GL_ARRAY_BUFFER, sizeof( quadVertices ), &quadVertices, GL_STATIC_DRAW );
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof( float ), ( void * )0 );
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof( float ), ( void * )( 3 * sizeof( float ) ) );

	g_shaderAtlas.postProcessShader.Use();
	g_shaderAtlas.postProcessShader.SetInt( "gPosition", 0 );
	g_shaderAtlas.postProcessShader.SetInt( "gNormal", 1 );
	g_shaderAtlas.postProcessShader.SetInt( "gAlbedoSpec", 2 );
	g_shaderAtlas.postProcessShader.SetInt( "ssao", 3 );
	g_shaderAtlas.postProcessShader.SetInt( "shadowMap", 4 );

	g_shaderAtlas.ssaoShader.Use();
	g_shaderAtlas.ssaoShader.SetInt( "gPosition", 0 );
	g_shaderAtlas.ssaoShader.SetInt( "gViewPosition", 1 );
	g_shaderAtlas.ssaoShader.SetInt( "gNormal", 2 );
	g_shaderAtlas.ssaoShader.SetInt( "gViewNormal", 3 );
	g_shaderAtlas.ssaoShader.SetInt( "ssaoNoise", 4 );

	glGenFramebuffers( 1, &SSAOFbo );
	glBindFramebuffer( GL_FRAMEBUFFER, SSAOFbo );
	// SSAO color buffer
	glGenTextures( 1, &SSAOColorBuffer );
	glBindTexture( GL_TEXTURE_2D, SSAOColorBuffer );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, NULL );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, SSAOColorBuffer, 0 );
	if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		ng::Errorf( "SSAO Framebuffer not complete!\n" );
	}
	SSAOKernel = CreateSSAOKernel();
	SSAONoiseTexture = CreateSSAONoiseTexture();

	glGenFramebuffers( 1, &SSAOBlurFbo );
	glBindFramebuffer( GL_FRAMEBUFFER, SSAOBlurFbo );
	glGenTextures( 1, &SSAOBlurColorBuffer );
	glBindTexture( GL_TEXTURE_2D, SSAOBlurColorBuffer );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, NULL );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, SSAOBlurColorBuffer, 0 );
	if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		ng::Errorf( "SSAO blur Framebuffer not complete!\n" );
	}

	glGenFramebuffers( 1, &shadowFramebufferID );

	glGenTextures( 1, &shadowDepthMap );
	glBindTexture( GL_TEXTURE_2D, shadowDepthMap );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT, 0, GL_DEPTH_COMPONENT,
	              GL_FLOAT, NULL );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
	constexpr float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor );

	glBindFramebuffer( GL_FRAMEBUFFER, shadowFramebufferID );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthMap, 0 );
	glDrawBuffer( GL_NONE );
	glReadBuffer( GL_NONE );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void Renderer::ShutdownRenderer() {
	glDeleteBuffers( 1, &viewProjUBO );
	glDeleteBuffers( 1, &lightUBO );

	glDeleteBuffers( 1, &fullScreenQuadVBO );
	glDeleteVertexArrays( 1, &fullScreenQuadVAO );

	gbuffer.Destroy();
	glDeleteFramebuffers( 1, &SSAOFbo );
	glDeleteTextures( 1, &SSAOColorBuffer );
	glDeleteTextures( 1, &SSAONoiseTexture );
	glDeleteFramebuffers( 1, &SSAOBlurFbo );
	glDeleteTextures( 1, &SSAOBlurColorBuffer );

	glDeleteFramebuffers( 1, &shadowFramebufferID );
	glDeleteTextures( 1, &shadowDepthMap );
}

void Renderer::SetResolution( int width, int height ) {}

void Renderer::GeometryPass( const Registery &     reg,
                             Model *               additionnalModels,
                             CpntTransform *       additionnalModelsTransform,
                             u32                   numAdditionnalModels,
                             InstancedModelBatch * instancedModels,
                             u32                   numInstancesModels ) {
	ZoneScoped;
	TracyGpuZone( "GeometryPass" );
	gbuffer.Bind();
	gbuffer.Clear();

	g_shaderAtlas.deferredShader.Use();
	for ( u32 i = 0; i < numAdditionnalModels; i++ ) {
		DrawModel( additionnalModels[ i ], additionnalModelsTransform[ i ], g_shaderAtlas.deferredShader );
	}

	// For now, let's disable depth test while drawing roads. This will avoid dealing with z-fighting with the road
	glDisable( GL_DEPTH_TEST );
	for ( u32 i = 0; i < numInstancesModels; i++ ) {
		instancedModels[ i ].Render( g_shaderAtlas.instancedDeferredShader );
	}
	glEnable( GL_DEPTH_TEST );

	theGame->systemManager.GetSystem< SystemTree >().instances.Render( g_shaderAtlas.instancedDeferredShader );

	g_shaderAtlas.deferredShader.Use();
	for ( auto const & [ e, renderModel ] : reg.IterateOver< CpntRenderModel >() ) {
		if ( renderModel.model != nullptr ) {
			DrawModel( *renderModel.model, reg.GetComponent< CpntTransform >( e ), g_shaderAtlas.deferredShader );
		}
	}

	glBindRenderbuffer( GL_RENDERBUFFER, 0 );
	glBindFramebuffer( GL_FRAMEBUFFER, shadowFramebufferID );
	glClear( GL_DEPTH_BUFFER_BIT );
	glViewport( 0, 0, SHADOW_MAP_WIDTH, SHADOW_MAP_HEIGHT );
	g_shaderAtlas.shadowPassShader.Use();
	for ( auto const & [ e, renderModel ] : reg.IterateOver< CpntRenderModel >() ) {
		if ( renderModel.model != nullptr ) {
			DrawModel( *renderModel.model, reg.GetComponent< CpntTransform >( e ), g_shaderAtlas.shadowPassShader,
			           false );
		}
	}
	theGame->systemManager.GetSystem< SystemTree >().instances.Render( g_shaderAtlas.shadowPassInstancedShader );

	g_shaderAtlas.deferredShader.Use();
}

void Renderer::LigthningPass() {
	ZoneScoped;
	TracyGpuZone( "LightningPass" );
	glViewport( 0, 0, gbuffer.width, gbuffer.height );
	glBindFramebuffer( GL_FRAMEBUFFER, SSAOFbo );
	glClear( GL_COLOR_BUFFER_BIT );

	g_shaderAtlas.ssaoShader.Use();
	g_shaderAtlas.ssaoShader.SetVectorArray( "samples", SSAOKernel.data, SSAOKernel.Size() );
	g_shaderAtlas.ssaoShader.SetVector2( "noiseScale", glm::vec2( gbuffer.width / 4.0f, gbuffer.height / 4.0f ) );
	static float radius = 0.5f;
	ImGui::SliderFloat( "radius", &radius, 0.0f, 4.0f );
	static float bias = 0.025f;
	ImGui::SliderFloat( "bias", &bias, 0.0f, 4.0f );
	g_shaderAtlas.ssaoShader.SetFloat( "radius", radius );
	g_shaderAtlas.ssaoShader.SetFloat( "bias", bias );

	std::pair< int, int > textures[] = {
		{ GL_TEXTURE0, gbuffer.positionTexture },
		{ GL_TEXTURE1, gbuffer.viewPositionTexture },
		{ GL_TEXTURE2, gbuffer.normalTexture },
		{ GL_TEXTURE3, gbuffer.viewNormalTexture },
		{ GL_TEXTURE4, SSAONoiseTexture },
	};

	for ( auto & [ textureInd, texure ] : textures ) {
		glActiveTexture( textureInd );
		glBindTexture( GL_TEXTURE_2D, texure );
	}

	glBindVertexArray( fullScreenQuadVAO );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

	glBindFramebuffer( GL_FRAMEBUFFER, SSAOBlurFbo );
	glClear( GL_COLOR_BUFFER_BIT );
	g_shaderAtlas.ssaoBlurShader.Use();
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, SSAOColorBuffer );
	glBindVertexArray( fullScreenQuadVAO );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	glBindVertexArray( 0 );
}

void Renderer::PostProcessPass() {
	ZoneScoped;
	TracyGpuZone( "PostProcessPass" );

	theGame->window.BindDefaultFramebuffer();
	theGame->window.Clear();

	g_shaderAtlas.postProcessShader.Use();
	static float curvatureRidge = 0.0f;
	static float curvatureValley = 0.0f;
	ImGui::SliderFloat( "curvature ridge", &curvatureRidge, 0.0f, 2.0f );
	ImGui::SliderFloat( "curvature valley", &curvatureValley, 0.0f, 2.0f );
	g_shaderAtlas.postProcessShader.SetFloat( "curvature_ridge", 0.5f / MAX( curvatureRidge * curvatureRidge, 1e-4 ) );
	g_shaderAtlas.postProcessShader.SetFloat( "curvature_valley",
	                                          0.7f / MAX( curvatureValley * curvatureValley, 1e-4 ) );

	std::pair< int, int > textures[] = {
		{ GL_TEXTURE0, gbuffer.positionTexture },
		{ GL_TEXTURE1, gbuffer.normalTexture },
		{ GL_TEXTURE2, gbuffer.colorSpecularTexture },
		{ GL_TEXTURE3, SSAOBlurColorBuffer },
		{ GL_TEXTURE4, shadowDepthMap },
	};

	for ( auto & [ textureInd, texure ] : textures ) {
		glActiveTexture( textureInd );
		glBindTexture( GL_TEXTURE_2D, texure );
	}

	glBindVertexArray( fullScreenQuadVAO );
	glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	glBindVertexArray( 0 );
}

void Renderer::DebugPass() {}

void Renderer::FillViewProjUBO( const ViewProjUBOData * data ) {
	ZoneScoped;
	glBindBuffer( GL_UNIFORM_BUFFER, viewProjUBO );
	glBufferSubData( GL_UNIFORM_BUFFER, 0, sizeof( ViewProjUBOData ), data );
	glBindBuffer( GL_UNIFORM_BUFFER, 0 );
}

void Renderer::FillLightUBO( const LightUBOData * data ) {
	ZoneScoped;
	glBindBuffer( GL_UNIFORM_BUFFER, lightUBO );
	glBufferSubData( GL_UNIFORM_BUFFER, 0, sizeof( LightUBOData ), data );
	glBindBuffer( GL_UNIFORM_BUFFER, 0 );
}

void Renderer::DebugDraw() {
	static float height = 400.0f;
	ImGui::SliderFloat( "texture size", &height, 1.0f, 1920.0f );
	float width = height * 16.0f / 9.0f;
	ImGui::Text( "Albedo" );
	ImGui::Image( ( ImTextureID )gbuffer.colorSpecularTexture, ImVec2( width / 2.0f, height / 2.0f ), ImVec2( 0, 1 ),
	              ImVec2( 1, 0 ) );
	ImGui::Text( "Position" );
	ImGui::Image( ( ImTextureID )gbuffer.positionTexture, ImVec2( width / 2.0f, height / 2.0f ), ImVec2( 0, 1 ),
	              ImVec2( 1, 0 ) );
	ImGui::Text( "Normal" );
	ImGui::Image( ( ImTextureID )gbuffer.normalTexture, ImVec2( width / 2.0f, height / 2.0f ), ImVec2( 0, 1 ),
	              ImVec2( 1, 0 ) );

	ImGui::Text( "SSAO" );
	ImGui::Image( ( ImTextureID )SSAOColorBuffer, ImVec2( width / 2.0f, height / 2.0f ), ImVec2( 0, 1 ),
	              ImVec2( 1, 0 ) );

	ImGui::Text( "SSAO blurred" );
	ImGui::Image( ( ImTextureID )SSAOBlurColorBuffer, ImVec2( width / 2.0f, height / 2.0f ), ImVec2( 0, 1 ),
	              ImVec2( 1, 0 ) );

	ImGui::Text( "Shadow map" );
	ImGui::Image( ( ImTextureID )shadowDepthMap, ImVec2( width / 2.0f, height / 2.0f ), ImVec2( 0, 1 ),
	              ImVec2( 1, 0 ) );
}

void GBuffer::Allocate( int width, int height ) {
	this->width = width;
	this->height = height;
	glGenFramebuffers( 1, &framebufferID );
	glBindFramebuffer( GL_FRAMEBUFFER, framebufferID );

	std::tuple< int, u32*, int > textures[] = {
		{ GL_COLOR_ATTACHMENT0, &positionTexture, GL_RGBA32F },
		{ GL_COLOR_ATTACHMENT1, &viewPositionTexture, GL_RGBA32F },
		{ GL_COLOR_ATTACHMENT2, &normalTexture, GL_RGBA32F },
		{ GL_COLOR_ATTACHMENT3, &viewNormalTexture, GL_RGBA32F },
		{ GL_COLOR_ATTACHMENT4, &colorSpecularTexture, GL_RGBA }
	};

	for ( auto & [ glColorAttachment, texture, format ] : textures ) {
		glGenTextures( 1, texture );
		glBindTexture( GL_TEXTURE_2D, *texture );
		glTexImage2D( GL_TEXTURE_2D, 0, format, width, height, 0, GL_RGBA, GL_FLOAT, NULL );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glFramebufferTexture2D( GL_FRAMEBUFFER, glColorAttachment, GL_TEXTURE_2D, *texture, 0 );
	}

	u32 attachments[ 5 ] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4 };
	glDrawBuffers( 5, attachments );

	// Depth render buffer
	glGenRenderbuffers( 1, &renderbufferID );
	glBindRenderbuffer( GL_RENDERBUFFER, renderbufferID );

	glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbufferID );

	ng_assert( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glBindRenderbuffer( GL_RENDERBUFFER, 0 );
}

void GBuffer::Destroy() {
	glDeleteRenderbuffers( 1, &renderbufferID );
	glDeleteTextures( 1, &positionTexture );
	glDeleteTextures( 1, &viewPositionTexture );
	glDeleteTextures( 1, &normalTexture );
	glDeleteTextures( 1, &viewNormalTexture );
	glDeleteTextures( 1, &colorSpecularTexture );
	glDeleteFramebuffers( 1, &framebufferID );
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

static float lerp( float a, float b, float f ) { return a + f * ( b - a ); }

ng::DynamicArray< glm::vec3 > CreateSSAOKernel() {
	std::uniform_real_distribution< float > randomFloats( 0.0, 1.0 ); // random floats between [0.0, 1.0]
	std::default_random_engine              generator;
	ng::DynamicArray< glm::vec3 >           ssaoKernel( ssaoKernelSize );
	for ( unsigned int i = 0; i < ssaoKernelSize; ++i ) {
		glm::vec3 sample( randomFloats( generator ) * 2.0 - 1.0, randomFloats( generator ) * 2.0 - 1.0,
		                  randomFloats( generator ) );
		sample = glm::normalize( sample );
		sample *= randomFloats( generator );
		float scale = ( float )i / ( float )ssaoKernelSize;

		// scale samples s.t. they're more aligned to center of kernel
		scale = lerp( 0.1f, 1.0f, scale * scale );
		sample *= scale;
		ssaoKernel.PushBack( sample );
	}
	return ssaoKernel;
}

u32 CreateSSAONoiseTexture() {
	std::uniform_real_distribution< float > randomFloats( 0.0, 1.0 ); // random floats between [0.0, 1.0]
	std::default_random_engine              generator;
	ng::DynamicArray< glm::vec3 >           ssaoNoise;
	for ( unsigned int i = 0; i < 16; i++ ) {
		glm::vec3 noise( randomFloats( generator ) * 2.0 - 1.0, randomFloats( generator ) * 2.0 - 1.0, 0.0f );
		ssaoNoise.PushBack( noise );
	}

	u32 noiseTexture;
	glGenTextures( 1, &noiseTexture );
	glBindTexture( GL_TEXTURE_2D, noiseTexture );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[ 0 ] );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	return noiseTexture;
}
