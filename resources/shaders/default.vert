#version 420 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 fragPosition;
out vec3 fragNormal;
out vec2 fragTexCoord;

layout (std140) uniform Matrices {
	mat4 projection;
	mat4 view;
	mat4 viewProj;
	vec4 cameraPosition;
	vec4 cameraFront;
};

uniform mat4 modelTransform;
uniform mat3 normalTransform;

void main()
{
	fragPosition = vec3( modelTransform * vec4( aPosition, 1.0  ) );
    gl_Position = viewProj * vec4( fragPosition, 1.0 );
	fragTexCoord = aTexCoord;
	fragNormal = normalTransform * aNormal;
} 
