#version 420 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
out vec3 ourColor;

#if OPENGL_COMPATIBILITY_VERSION
layout (std140) uniform Matrices {
#else
layout (std140, binding = 0) uniform Matrices {
#endif
	mat4 projection;
	mat4 view;
	mat4 viewProj;
	mat4 shadowViewProj;
	vec4 cameraPosition;
	vec4 cameraFront;
};

void main()
{
    gl_Position = viewProj * vec4(aPos, 1.0);
    ourColor = aColor;
}
