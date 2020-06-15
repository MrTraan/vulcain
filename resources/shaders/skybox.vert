#version 420 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

layout (std140, binding = 0) uniform Matrices {
	mat4 projection;
	mat4 view;
	mat4 viewProj;
};

void main()
{
    TexCoords = aPos;
	mat4 noTranslateView = mat4(mat3(view));
    gl_Position = projection * noTranslateView * vec4(aPos, 1.0);
}
