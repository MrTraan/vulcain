#version 420 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
out vec3 ourColor;

layout (std140, binding = 0) uniform Matrices {
	mat4 projection;
	mat4 view;
	mat4 viewProj;
};

void main()
{
    gl_Position = viewProj * vec4(aPos, 1.0);
    ourColor = aColor;
};
