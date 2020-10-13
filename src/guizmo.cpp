#include "guizmo.h"
#include "packer_resource_list.h"
#include "shader.h"
#include <GL/gl3w.h>
#include <glm/gtc/type_ptr.hpp>
#include <tracy/Tracy.hpp>
#include <vector>

namespace Guizmo {

// static Shader colorShader;
static u32 VAO, VBO;

struct LineData {
	glm::vec3 a;
	glm::vec3 colorA;
	glm::vec3 b;
	glm::vec3 colorB;
};

struct TriangleData {
	glm::vec3 a;
	glm::vec3 colorA;
	glm::vec3 b;
	glm::vec3 colorB;
	glm::vec3 c;
	glm::vec3 colorC;
};

static std::vector< LineData >     lineDrawList;
static std::vector< TriangleData > triangleDrawList;

void Init() {
	glGenVertexArrays( 1, &VAO );
	glGenBuffers( 1, &VBO );
	glBindVertexArray( VAO );

	glBindBuffer( GL_ARRAY_BUFFER, VBO );
	glBufferData( GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW );

	// position attribute
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( float ), ( void * )0 );
	glEnableVertexAttribArray( 0 );
	// color attribute
	glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof( float ), ( void * )( 3 * sizeof( float ) ) );
	glEnableVertexAttribArray( 1 );
}

void NewFrame() {
	lineDrawList.clear();
	triangleDrawList.clear();
}

void Line( glm::vec3 a, glm::vec3 b, glm::vec3 color ) {
	LineData data = { a, color, b, color };
	lineDrawList.push_back( data );
}

void Triangle( glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color ) {
	TriangleData data = { a, color, b, color, c, color };
	triangleDrawList.push_back( data );
}

void Rectangle( glm::vec3 position, float width, float height, glm::vec3 color ) {
	Triangle( position, glm::vec3( position.x + width, position.y, position.z + height ),
	          glm::vec3( position.x + width, position.y, position.z ), color );
	Triangle( position, glm::vec3( position.x, position.y, position.z + height ),
	          glm::vec3( position.x + width, position.y, position.z + height ), color );
}

void LinesAroundCube( glm::vec3 cubeCenter, glm::vec3 cubeSize, glm::vec3 color ) {
	float x = cubeCenter.x;
	float y = cubeCenter.y;
	float z = cubeCenter.z;

	float sx = cubeSize.x / 2.0f;
	float sy = cubeSize.y / 2.0f;
	float sz = cubeSize.z / 2.0f;

	Guizmo::Line( glm::vec3( x - sx, y - sy, z - sz ), glm::vec3( x + sx, y - sy, z - sz ), color );
	Guizmo::Line( glm::vec3( x - sx, y - sy, z - sz ), glm::vec3( x - sx, y - sy, z + sz ), color );
	Guizmo::Line( glm::vec3( x - sx, y - sy, z + sz ), glm::vec3( x + sx, y - sy, z + sz ), color );
	Guizmo::Line( glm::vec3( x + sx, y - sy, z - sz ), glm::vec3( x + sx, y - sy, z + sz ), color );

	Guizmo::Line( glm::vec3( x - sx, y + sy, z - sz ), glm::vec3( x + sx, y + sy, z - sz ), color );
	Guizmo::Line( glm::vec3( x - sx, y + sy, z - sz ), glm::vec3( x - sx, y + sy, z + sz ), color );
	Guizmo::Line( glm::vec3( x - sx, y + sy, z + sz ), glm::vec3( x + sx, y + sy, z + sz ), color );
	Guizmo::Line( glm::vec3( x + sx, y + sy, z - sz ), glm::vec3( x + sx, y + sy, z + sz ), color );

	Guizmo::Line( glm::vec3( x - sx, y - sy, z - sz ), glm::vec3( x - sx, y + sy, z - sz ), color );
	Guizmo::Line( glm::vec3( x + sx, y - sy, z - sz ), glm::vec3( x + sx, y + sy, z - sz ), color );
	Guizmo::Line( glm::vec3( x - sx, y - sy, z + sz ), glm::vec3( x - sx, y + sy, z + sz ), color );
	Guizmo::Line( glm::vec3( x + sx, y - sy, z + sz ), glm::vec3( x + sx, y + sy, z + sz ), color );
}

void LinesAroundCube( glm::vec3 cubePosition ) {
	float     x = cubePosition.x;
	float     y = cubePosition.y;
	float     z = cubePosition.z;
	float     o = 0.001f;
	glm::vec3 ov( -o, -o, -o );

	Guizmo::Line( glm::vec3( x, y, z ) + ov, glm::vec3( x + 1 + 2 * o, y, z ) + ov, Guizmo::colWhite );
	Guizmo::Line( glm::vec3( x, y, z ) + ov, glm::vec3( x, y, z + 1 + 2 * o ) + ov, Guizmo::colWhite );
	Guizmo::Line( glm::vec3( x, y, z + 1 + 2 * o ) + ov, glm::vec3( x + 1 + 2 * o, y, z + 1 + 2 * o ) + ov,
	              Guizmo::colWhite );
	Guizmo::Line( glm::vec3( x + 1 + 2 * o, y, z ) + ov, glm::vec3( x + 1 + 2 * o, y, z + 1 + 2 * o ) + ov,
	              Guizmo::colWhite );

	Guizmo::Line( glm::vec3( x, y + 1 + 2 * o, z ) + ov, glm::vec3( x + 1 + 2 * o, y + 1 + 2 * o, z ) + ov,
	              Guizmo::colWhite );
	Guizmo::Line( glm::vec3( x, y + 1 + 2 * o, z ) + ov, glm::vec3( x, y + 1 + 2 * o, z + 1 + 2 * o ) + ov,
	              Guizmo::colWhite );
	Guizmo::Line( glm::vec3( x, y + 1 + 2 * o, z + 1 + 2 * o ) + ov,
	              glm::vec3( x + 1 + 2 * o, y + 1 + 2 * o, z + 1 + 2 * o ) + ov, Guizmo::colWhite );
	Guizmo::Line( glm::vec3( x + 1 + 2 * o, y + 1 + 2 * o, z ) + ov,
	              glm::vec3( x + 1 + 2 * o, y + 1 + 2 * o, z + 1 + 2 * o ) + ov, Guizmo::colWhite );

	Guizmo::Line( glm::vec3( x, y, z ) + ov, glm::vec3( x, y + 1 + 2 * o, z ) + ov, Guizmo::colWhite );
	Guizmo::Line( glm::vec3( x + 1 + 2 * o, y, z ) + ov, glm::vec3( x + 1 + 2 * o, y + 1 + 2 * o, z ) + ov,
	              Guizmo::colWhite );
	Guizmo::Line( glm::vec3( x, y, z + 1 + 2 * o ) + ov, glm::vec3( x, y + 1 + 2 * o, z + 1 + 2 * o ) + ov,
	              Guizmo::colWhite );
	Guizmo::Line( glm::vec3( x + 1 + 2 * o, y, z + 1 + 2 * o ) + ov,
	              glm::vec3( x + 1 + 2 * o, y + 1 + 2 * o, z + 1 + 2 * o ) + ov, Guizmo::colWhite );
}

void Draw() {
	ZoneScoped;

	g_shaderAtlas.colorShader.Use();

	if ( lineDrawList.size() > 0 ) {
		glBindVertexArray( VAO );
		glBindBuffer( GL_ARRAY_BUFFER, VBO );
		glBufferData( GL_ARRAY_BUFFER, lineDrawList.size() * sizeof( LineData ), lineDrawList.data(), GL_STATIC_DRAW );
		glLineWidth( 1.0f );
		for ( int i = 0; i < lineDrawList.size(); i++ ) {
			glDrawArrays( GL_LINE_STRIP, i * 2, 2 );
		}
		glLineWidth( 1.0f );
	}

	if ( triangleDrawList.size() > 0 ) {
		glDisable( GL_DEPTH_TEST );
		glBindVertexArray( VAO );
		glBindBuffer( GL_ARRAY_BUFFER, VBO );
		glBufferData( GL_ARRAY_BUFFER, triangleDrawList.size() * sizeof( TriangleData ), triangleDrawList.data(),
		              GL_STATIC_DRAW );
		glDrawArrays( GL_TRIANGLES, 0, triangleDrawList.size() * 3 );
		glEnable( GL_DEPTH_TEST );
	}
	glBindVertexArray( 0 );
}

} // namespace Guizmo