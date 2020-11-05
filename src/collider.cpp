#include "collider.h"
#include "entity.h"

//bool BoxCollidesWithBox( 
//	const CpntBoxCollider & colliderA, const CpntTransform & transformA,
//	const CpntBoxCollider & colliderB, const CpntTransform & transformB
//	) {
//	glm::vec3 aabbMinA = colliderA.center - ( colliderA.size / 2.0f );
//	glm::vec3 aabbMaxA = colliderA.center + ( colliderA.size / 2.0f );
//	glm::vec3 aabbMinB = colliderB.center - ( colliderB.size / 2.0f );
//	glm::vec3 aabbMaxB = colliderB.center + ( colliderB.size / 2.0f );
//
//	aabbMinA = glm::vec3( transformA.GetMatrix() * glm::vec4(aabbMinA, 1.0f) );
//	aabbMaxA = glm::vec3( transformA.GetMatrix() * glm::vec4(aabbMaxA, 1.0f) );
//	aabbMinB = glm::vec3( transformA.GetMatrix() * glm::vec4(aabbMinB, 1.0f) );
//	aabbMaxB = glm::vec3( transformA.GetMatrix() * glm::vec4(aabbMaxB, 1.0f) );
//
//}

bool RayCollidesWithBox( const Ray &             ray,
                         const CpntBoxCollider & collider,
                         const CpntTransform &   transform,
                         float *                 outDistance /*= nullptr */ ) {

	float tMin = 0.0f;
	float tMax = FLT_MAX;

	glm::vec3 aabbMin = collider.center - ( collider.size / 2.0f );
	glm::vec3 aabbMax = collider.center + ( collider.size / 2.0f );

	const glm::mat4 matrix = transform.GetMatrix();

	glm::vec3 delta = transform.GetTranslation() - ray.origin;

	glm::vec3 xaxis( matrix[ 0 ].x, matrix[ 0 ].y, matrix[ 0 ].z );

	glm::vec3 axis[ 3 ] = {
	    {
	        matrix[ 0 ].x,
	        matrix[ 0 ].y,
	        matrix[ 0 ].z,
	    },
	    {
	        matrix[ 1 ].x,
	        matrix[ 1 ].y,
	        matrix[ 1 ].z,
	    },
	    {
	        matrix[ 2 ].x,
	        matrix[ 2 ].y,
	        matrix[ 2 ].z,
	    },
	};

	for ( int i = 0; i < 3; i++ ) {
		float e = glm::dot( axis[ i ], delta );
		float f = glm::dot( ray.direction, axis[ i ] );

		ng_assert( f != 0.0f );

		float t1 = ( e + aabbMin[i] ) / f;
		float t2 = ( e + aabbMax[i] ) / f;

		if ( t1 > t2 ) {
			std::swap( t1, t2 ); // make sure t1 is the near intersection, and t2 far intersection
		}

		if ( t2 < tMax ) {
			tMax = t2;
		}
		if ( t1 > tMin ) {
			tMin = t1;
		}

		if ( tMax < tMin ) {
			return false;
		}
	}
	if ( outDistance != nullptr ) {
		*outDistance = tMin;
	}
	return true;
}
