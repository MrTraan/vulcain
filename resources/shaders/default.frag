#version 420 core
out vec4 FragColor;  

in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

uniform sampler2D diffuseTexture;

struct Light {
	vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform Light light;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
}; 
uniform Material material;

layout (std140, binding = 0) uniform Matrices {
	mat4 projection;
	mat4 view;
	mat4 viewProj;
	vec4 viewPosition;
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
	float dotLightNormal = -dot( normal, light.direction );
	float diff = max( dotLightNormal, 0.0f );
	vec3  diffuse = light.diffuse * ( diff * material.diffuse );

	// specular
	vec3  viewDir = normalize( vec3( viewPosition ) - fragPosition );
	vec3  reflectDir = reflect( light.direction, normal );
	float spec = pow( max( dot( viewDir, reflectDir ), 0.0 ), material.shininess );
	vec3  specular = light.specular * ( spec * material.specular );

	vec3 lighting = light.ambient + diffuse + specular;

	FragColor = textureColor * vec4( lighting, 1.0 );
}
