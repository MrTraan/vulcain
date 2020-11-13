#pragma once

#include "ngLib/ngcontainers.h"
#include "ngLib/types.h"
#include <GL/gl3w.h>
#include <glm/glm.hpp>

constexpr u32 viewProjUBOIndex = 0u;
constexpr u32 lightUBOIndex = 1u;

struct Registery;
struct Model;
struct CpntTransform;
struct InstancedModelBatch;

struct ViewProjUBOData {
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 viewProj;
	glm::mat4 shadowViewProj;
	glm::vec4 cameraPosition;
	glm::vec4 cameraFront;
};

struct LightUBOData {
	glm::vec4 direction;
	glm::vec4 ambient;
	glm::vec4 diffuse;
	glm::vec4 specular;
};

struct Camera {
	glm::vec3 position;
	glm::vec3 front;
	glm::mat4 proj;
	glm::mat4 view;
};

struct Framebuffer {
	int width;
	int height;
	u32 framebufferID;
	u32 renderbufferID;
	u32 textureID;

	void Allocate( int width, int height );
	void Destroy();

	void Clear() { glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ); }

	void Bind() {
		glViewport( 0, 0, width, height );
		glBindFramebuffer( GL_FRAMEBUFFER, framebufferID );
	}
};

struct GBuffer {
	int width;
	int height;
	u32 framebufferID;
	u32 renderbufferID;
	u32 positionTexture;
	u32 normalTexture;
	u32 colorSpecularTexture;

	void Allocate( int width, int height );
	void Destroy();

	void Clear() { glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ); }

	void Bind() {
		glViewport( 0, 0, width, height );
		glBindRenderbuffer( GL_RENDERBUFFER, renderbufferID );
		glBindFramebuffer( GL_FRAMEBUFFER, framebufferID );
	}
};

struct Renderer {
	u32 viewProjUBO;
	u32 lightUBO;

	u32 fullScreenQuadVAO;
	u32 fullScreenQuadVBO;

	GBuffer gbuffer;

	u32                           SSAOFbo;
	u32                           SSAOColorBuffer;
	u32                           SSAOBlurFbo;
	u32                           SSAOBlurColorBuffer;
	u32                           SSAONoiseTexture;
	ng::DynamicArray< glm::vec3 > SSAOKernel;

	u32 shadowFramebufferID;
	u32 shadowDepthMap;

	Framebuffer										finalRender;

	void InitRenderer( int width, int height );
	void ShutdownRenderer();

	void SetResolution( int width, int height );

	void GeometryPass( const Registery &     reg,
	                   Model *               additionnalModels,
	                   CpntTransform *       additionnalModelsTransform,
	                   u32                   numAdditionnalModels,
	                   InstancedModelBatch * instancedModels,
	                   u32                   numInstancesModels );
	void LigthningPass();
	void PostProcessPass();
	void DebugPass();

	void FillViewProjUBO( const ViewProjUBOData * data );
	void FillLightUBO( const LightUBOData * data );

	void DebugDraw();
};
