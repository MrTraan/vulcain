#version 420 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in mat4 model;

out vec3 fragPosition;
out vec3 fragNormal;
out vec2 fragTexCoord;

#if OPENGL_COMPATIBILITY_VERSION
uniform Matrices {
#else
layout (std140, binding = 0) uniform Matrices {
#endif
	mat4 projection;
	mat4 view;
	mat4 viewProj;
	vec4 cameraPosition;
	vec4 cameraFront;
};

uniform mat4 baseTransform;

void main()
{
	mat4 transform = model * baseTransform;
	mat3 normalTransform = mat3(transpose(inverse(transform)));
	fragPosition = vec3( transform * vec4( aPosition, 1.0 ) );
    gl_Position = viewProj * vec4( fragPosition, 1.0 );
	fragTexCoord = aTexCoord;
	fragNormal = normalTransform * aNormal;
} 
