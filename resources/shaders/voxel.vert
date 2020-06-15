#version 450 core
layout (location = 0) in uint x;
layout (location = 1) in uint z;
layout (location = 2) in uint y;
layout (location = 3) in uint aTexIndex;
layout (location = 4) in uint aTexX;
layout (location = 5) in uint aTexY;
layout (location = 6) in uint direction;

layout (std140, binding = 0) uniform Matrices {
	mat4 projection;
	mat4 view;
	mat4 viewProj;
	mat4 lightSpaceViewProj;
	vec3 lightPosition;
	vec3 lightDir;
};
uniform vec3 chunkWorldPosition;

out float texIndex;
out float texX;
out float texY;
out vec3 normal;
out vec4 fragPosLightSpace;

const vec3 directionToNormal[6] = vec3[6](
	vec3(0, 0, -1),
	vec3(0, 0, 1),
	vec3(1, 0, 0),
	vec3(-1, 0, 0),
	vec3(0, 1, 0),
	vec3(0, -1, 0)
);

void main()
{
	vec3 position = vec3(float(x) / 10.0, float(y) / 10.0, float(z) / 10.0 );
	vec4 worldPosition = vec4(position + chunkWorldPosition, 1.0 );
	fragPosLightSpace = lightSpaceViewProj * worldPosition;
    gl_Position = viewProj * worldPosition;
	texIndex = float(aTexIndex);
	texX = float(aTexX);
	texY = float(aTexY);
	normal = directionToNormal[ direction ];
} 
