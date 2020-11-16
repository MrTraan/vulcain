#version 410 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
out vec3 ourColor;

layout (std140) uniform Matrices {
	mat4 projection;
	mat4 view;
	mat4 viewProj;
	vec4 cameraPosition;
	vec4 cameraFront;
};

void main()
{
    gl_Position = viewProj * vec4(aPos, 1.0);
    ourColor = aColor;
}
