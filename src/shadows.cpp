#include "shadows.h"
#include "entity.h"
#include "registery.h"

void Frustrum::Update( const glm::mat4 & view, const glm::mat4 & projection ) {
	glm::mat4 matrix = projection * view;

	planes[ 0 ][ 0 ] = matrix[ 0 ][ 3 ] + matrix[ 0 ][ 0 ];
	planes[ 0 ][ 1 ] = matrix[ 1 ][ 3 ] + matrix[ 1 ][ 0 ];
	planes[ 0 ][ 2 ] = matrix[ 2 ][ 3 ] + matrix[ 2 ][ 0 ];
	planes[ 0 ][ 3 ] = matrix[ 3 ][ 3 ] + matrix[ 3 ][ 0 ];

	planes[ 1 ][ 0 ] = matrix[ 0 ][ 3 ] - matrix[ 0 ][ 0 ];
	planes[ 1 ][ 1 ] = matrix[ 1 ][ 3 ] - matrix[ 1 ][ 0 ];
	planes[ 1 ][ 2 ] = matrix[ 2 ][ 3 ] - matrix[ 2 ][ 0 ];
	planes[ 1 ][ 3 ] = matrix[ 3 ][ 3 ] - matrix[ 3 ][ 0 ];

	planes[ 2 ][ 0 ] = matrix[ 0 ][ 3 ] - matrix[ 0 ][ 1 ];
	planes[ 2 ][ 1 ] = matrix[ 1 ][ 3 ] - matrix[ 1 ][ 1 ];
	planes[ 2 ][ 2 ] = matrix[ 2 ][ 3 ] - matrix[ 2 ][ 1 ];
	planes[ 2 ][ 3 ] = matrix[ 3 ][ 3 ] - matrix[ 3 ][ 1 ];

	planes[ 3 ][ 0 ] = matrix[ 0 ][ 3 ] + matrix[ 0 ][ 1 ];
	planes[ 3 ][ 1 ] = matrix[ 1 ][ 3 ] + matrix[ 1 ][ 1 ];
	planes[ 3 ][ 2 ] = matrix[ 2 ][ 3 ] + matrix[ 2 ][ 1 ];
	planes[ 3 ][ 3 ] = matrix[ 3 ][ 3 ] + matrix[ 3 ][ 1 ];

	planes[ 4 ][ 0 ] = matrix[ 0 ][ 3 ] + matrix[ 0 ][ 2 ];
	planes[ 4 ][ 1 ] = matrix[ 1 ][ 3 ] + matrix[ 1 ][ 2 ];
	planes[ 4 ][ 2 ] = matrix[ 2 ][ 3 ] + matrix[ 2 ][ 2 ];
	planes[ 4 ][ 3 ] = matrix[ 3 ][ 3 ] + matrix[ 3 ][ 2 ];

	planes[ 5 ][ 0 ] = matrix[ 0 ][ 3 ] - matrix[ 0 ][ 2 ];
	planes[ 5 ][ 1 ] = matrix[ 1 ][ 3 ] - matrix[ 1 ][ 2 ];
	planes[ 5 ][ 2 ] = matrix[ 2 ][ 3 ] - matrix[ 2 ][ 2 ];
	planes[ 5 ][ 3 ] = matrix[ 3 ][ 3 ] - matrix[ 3 ][ 2 ];
}

bool Frustrum::IsCubeIn( const Aabb & aabb ) const {
	glm::vec4 points[] = { { aabb.min.x, aabb.min.y, aabb.min.z, 1.0f }, { aabb.max.x, aabb.min.y, aabb.min.z, 1.0f },
	                       { aabb.max.x, aabb.max.y, aabb.min.z, 1.0f }, { aabb.min.x, aabb.max.y, aabb.min.z, 1.0f },

	                       { aabb.min.x, aabb.min.y, aabb.max.z, 1.0f }, { aabb.max.x, aabb.min.y, aabb.max.z, 1.0f },
	                       { aabb.max.x, aabb.max.y, aabb.max.z, 1.0f }, { aabb.min.x, aabb.max.y, aabb.max.z, 1.0f } };

	// for each plane...
	for ( int i = 0; i < 6; ++i ) {
		bool inside = false;

		for ( int j = 0; j < 8; ++j ) {
			if ( glm::dot( points[ j ], planes[ i ] ) > 0 ) {
				inside = true;
				break;
			}
		}

		if ( !inside )
			return false;
	}

	return true;
}

glm::mat4 ComputeShadowCameraViewProj( const Registery & reg, const Frustrum & cameraFrustum, const glm::vec3 & lightDirection ) {
	auto minOfVector = []( glm::vec3 & res, const glm::vec3 & b ) {
		res.x = MIN( res.x, b.x );
		res.y = MIN( res.y, b.y );
		res.z = MIN( res.z, b.z );
	};
	auto maxOfVector = []( glm::vec3 & res, const glm::vec3 & b ) {
		res.x = MAX( res.x, b.x );
		res.y = MAX( res.y, b.y );
		res.z = MAX( res.z, b.z );
	};
	Aabb visibleObjectsBox;
	visibleObjectsBox.min = { FLT_MAX, FLT_MAX, FLT_MAX };
	visibleObjectsBox.max = { FLT_MIN, FLT_MIN, FLT_MIN };
	for ( const auto & [ entity, renderModel ] : reg.IterateOver< CpntRenderModel >() ) {
		const auto & transform = reg.GetComponent< CpntTransform >( entity );
		Aabb         bounds = ComputeMeshAabb( *renderModel.model, transform );
		if ( cameraFrustum.IsCubeIn( bounds ) ) {
			minOfVector( visibleObjectsBox.min, bounds.min );
			maxOfVector( visibleObjectsBox.max, bounds.max );
		}
	}
	ImGui::Text( "Min bounds: %f %f %f", visibleObjectsBox.min.x, visibleObjectsBox.min.y, visibleObjectsBox.min.z );
	ImGui::Text( "Max bounds: %f %f %f", visibleObjectsBox.max.x, visibleObjectsBox.max.y, visibleObjectsBox.max.z );

	glm::vec3 shadowCameraPosition = ( visibleObjectsBox.min + visibleObjectsBox.max ) / 2.0f;
	glm::mat4 shadowCameraView =
	    glm::lookAt( shadowCameraPosition, shadowCameraPosition + lightDirection, glm::vec3( 0.0f, 1.0f, 0.0f ) );
	glm::vec3 boundsShadowViewSpace[ 8 ];
	glm::vec3 boundsMin{ FLT_MAX, FLT_MAX, FLT_MAX };
	glm::vec3 boundsMax{ FLT_MIN, FLT_MIN, FLT_MIN };
	Aabb      visibleObjectsBoxViewSpace = {
        glm::vec3( shadowCameraView * glm::vec4( visibleObjectsBox.min, 1.0f ) ),
        glm::vec3( shadowCameraView * glm::vec4( visibleObjectsBox.max, 1.0f ) ),
    };
	minOfVector( boundsMin, visibleObjectsBoxViewSpace.min );
	minOfVector( boundsMin, visibleObjectsBoxViewSpace.max );
	maxOfVector( boundsMax, visibleObjectsBoxViewSpace.min );
	maxOfVector( boundsMax, visibleObjectsBoxViewSpace.max );
	glm::mat4 shadowCameraProj =
	    glm::ortho( boundsMin.x, boundsMax.x, boundsMin.y, boundsMax.y, boundsMin.z, boundsMax.z );
	return shadowCameraProj * shadowCameraView;
}
