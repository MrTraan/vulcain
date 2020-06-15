#version 450 core
layout (location = 0) in uint x;
layout (location = 1) in uint z;
layout (location = 2) in uint y;

layout (std140, binding = 0) uniform Matrices {
	mat4 projection;
	mat4 view;
	mat4 viewProj;
	mat4 lightSpaceViewProj;
	vec3 lightPosition;
};
uniform vec3 chunkWorldPosition;

void main()
{
	vec3 position = vec3(float(x) / 10.0, float(y) / 10.0, float(z) / 10.0 );
	vec4 worldPosition = vec4(position + chunkWorldPosition, 1.0 );
    gl_Position = lightSpaceViewProj * worldPosition;
} 
