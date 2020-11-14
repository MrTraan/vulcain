#version 420 core
out vec4 FragColor;  

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

uniform sampler2D diffuseTexture;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
}; 
uniform Material material;

layout (std140) uniform Matrices {
	mat4 projection;
	mat4 view;
	mat4 viewProj;
	vec4 cameraPosition;
	vec4 cameraFront;
};

#if OPENGL_COMPATIBILITY_VERSION
uniform Light {
#else
layout (std140, binding = 1) uniform Light {
#endif
	vec4 light_direction;
    vec4 light_ambient;
    vec4 light_diffuse;
    vec4 light_specular;
};

void main()
{
	vec4 textureColor = texture( diffuseTexture, fragTexCoord );
	textureColor = textureColor * vec4( material.diffuse, 1.0 );
	if ( textureColor.a < 0.1 ) {
		discard;
	}

	vec3 normal = normalize( fragNormal );
	// diffuse
	float dotLightNormal = -dot( normal, vec3(light_direction ) );
	float diff = max( dotLightNormal, 0.0f );
	vec3  diffuse = vec3(light_diffuse ) * ( diff * material.diffuse );

	// specular
	vec3  viewDir = normalize( vec3( cameraPosition ) - fragPosition );
	vec3  reflectDir = reflect( vec3(light_direction), normal );
	float spec = pow( max( dot( viewDir, reflectDir ), 0.0 ), material.shininess );
	vec3  specular = vec3(light_specular )* ( spec * material.specular );

	vec3 lighting = vec3(light_ambient )+ diffuse + specular;

	FragColor = textureColor * vec4( lighting, 1.0 );
}
