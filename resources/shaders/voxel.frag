#version 450 core
out vec4 FragColor;  

in float texIndex;
in float texX;
in float texY;
in vec3 normal;
in vec4 fragPosLightSpace;

layout (std140, binding = 0) uniform Matrices {
	mat4 projection;
	mat4 view;
	mat4 viewProj;
	mat4 lightSpaceViewProj;
	vec3 lightPosition;
	vec3 lightDir;
};

uniform sampler2DArray ourTexture;
uniform sampler2DShadow shadowMap;

float ShadowCalculation(float dotLightNormal) {
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;
	if ( projCoords.z > 1.0 ) {
		return 0.0;
	}
	float currentDepth = projCoords.z;
	float bias = max(0.005 * ( 1.0 - dotLightNormal), 0.0005);
	bias = 0.005;

	float shadow = 0.0;
	vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
	for(int x = -1; x <= 1; ++x) {
		for(int y = -1; y <= 1; ++y) {
			shadow += texture(shadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, currentDepth - bias ) );
		}    
	}
	shadow /= 9.0;
	return shadow;
}

void main()
{
	vec3 lightColor = vec3(1.0, 1.0, 1.0);

	float	ambientStrength = 0.2f;
	vec3	ambient = ambientStrength * lightColor;

	float dotLightNormal = -dot(normal, lightDir);
	float	diff = max(dotLightNormal, 0.0f);
	vec3	diffuse = diff * lightColor;

	float shadow = ShadowCalculation(dotLightNormal);
	vec3 lighting = (ambient + ( shadow) * diffuse ) * lightColor;

	FragColor = texture(ourTexture, vec3(texX, texY, texIndex)) * vec4(lighting, 1.0);
}
