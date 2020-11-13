#pragma once

#include <glm/glm.hpp>
#include "mesh.h"

struct Registery;

struct Frustrum {
	void      Update( const glm::mat4 & view, const glm::mat4 & projection );
	bool      IsCubeIn( const Aabb & aabb ) const;
	glm::vec4 planes[ 6 ];
};
glm::mat4
ComputeShadowCameraViewProj( const Registery & reg, const Frustrum & cameraFrustum, const glm::vec3 & lightDirection );
