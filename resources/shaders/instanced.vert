#version 420 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in mat4 model;

out vec3 fragPosition;
out vec3 fragViewPosition;
out vec3 fragNormal;
out vec3 fragViewNormal;
out vec2 fragTexCoord;

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

uniform mat4 baseTransform;

void main()
{
  // TODO: Could be passed as uniform to save a bit of time
	mat4 transform = model * baseTransform;
	fragPosition = vec3( transform * vec4( aPosition, 1.0 ) );
	gl_Position = viewProj * vec4( fragPosition, 1.0 );
	// TODO: view * transform could be passed as uniform to save a bit of time
	vec4 viewPos = view * transform * vec4( aPosition, 1.0 );
	fragViewPosition = viewPos.xyz;
	// TODO: Could be passed as uniform to save quite some time
	mat3 normalTransform = mat3( transpose( inverse( transform ) ) );
	fragNormal = normalTransform * aNormal;
	// TODO: Could be passed as uniform to save quite some time
	mat3 viewNormalTransform = transpose( inverse( mat3( view * transform ) ) );
  fragViewNormal = viewNormalTransform * aNormal;
	fragTexCoord = aTexCoord;
}