#version 420 core
layout (location = 0) in vec3 aPos;   // the position variable has attribute position 0
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in float aTexIndex;

out vec2 TexCoord;
out float TexIndex;

layout (std140, binding = 0) uniform Matrices {
	mat4 projection;
	mat4 view;
	mat4 viewProj;
};
uniform mat4 model;

void main()
{
    gl_Position = viewProj * model * vec4(aPos, 1.0);
	TexCoord = aTexCoord;
	TexIndex = aTexIndex;
} 
