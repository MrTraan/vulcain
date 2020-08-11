#pragma once

#include <glm/glm.hpp>

struct CpntTransform;

struct Ray {
	glm::vec3 origin;
	glm::vec3 direction;
};

struct CpntBoxCollider {
	CpntBoxCollider() = default;
	CpntBoxCollider( const glm::vec3 & inCenter, const glm::vec3 & inSize ) : center( inCenter ), size( inSize ) {
		glm::vec3 foo = inCenter;
		foo.x += 3;
	}

	glm::vec3 center = { 0.0f, 0.0f, 0.0f };
	glm::vec3 size = { 1.0f, 1.0f, 1.0f };
};

bool RayCollidesWithBox( const Ray &             ray,
                         const CpntBoxCollider & collider,
                         const CpntTransform &   transform,
                         float *                 outDistance = nullptr );