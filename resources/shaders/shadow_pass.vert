#version 410 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

layout (std140) uniform Matrices {
    mat4 projection;
    mat4 view;
    mat4 viewProj;
    mat4 shadowViewProj;
    vec4 cameraPosition;
    vec4 cameraFront;
};

uniform mat4 modelTransform;

void main()
{
	vec4 fragPosition = modelTransform * vec4( aPosition, 1.0 );
    gl_Position = shadowViewProj * fragPosition;
} 
