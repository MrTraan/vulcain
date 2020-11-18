#version 410 core
out vec4 FragColor;

in vec2 fragTexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssao;
uniform sampler2DShadow shadowMap;

layout (std140) uniform Matrices {
    mat4 projection;
    mat4 view;
    mat4 viewProj;
    mat4 shadowViewProj;
    vec4 cameraPosition;
    vec4 cameraFront;
};

layout (std140) uniform Light {
	vec4 light_direction;
    vec4 light_ambient;
    vec4 light_diffuse;
    vec4 light_specular;
};

uniform float curvature_ridge;
uniform float curvature_valley;

#ifndef CURVATURE_OFFSET
#define CURVATURE_OFFSET 1
#endif

float curvature_soft_clamp(float curvature, float control)
{
	if (curvature < 0.5 / control)
		return curvature * (1.0 - curvature * control);
	return 0.25 / control;
}

float calculate_curvature(ivec2 texel, float ridge, float valley)
{
	vec3 normal_up    = (viewProj * texelFetchOffset(gNormal, texel, 0, ivec2(0,  CURVATURE_OFFSET))).rgb;
	vec3 normal_down  = (viewProj * texelFetchOffset(gNormal, texel, 0, ivec2(0, -CURVATURE_OFFSET))).rgb;
	vec3 normal_left  = (viewProj * texelFetchOffset(gNormal, texel, 0, ivec2(-CURVATURE_OFFSET, 0))).rgb;
	vec3 normal_right = (viewProj * texelFetchOffset(gNormal, texel, 0, ivec2( CURVATURE_OFFSET, 0))).rgb;

	float normal_diff = ((normal_up.g - normal_down.g) + (normal_right.r - normal_left.r));

//	FragColor = vec4( fragDiffuse, 1.0 );
	if (normal_diff < 0.0)
		return -2.0 * curvature_soft_clamp(-normal_diff, valley);

	return 2.0 * curvature_soft_clamp(normal_diff, ridge);
}

float ShadowCalculation( float dotLightNormal, vec4 fragPosLightSpace ) {
	vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projCoords = projCoords * 0.5 + 0.5;
	float bias = 0.005;
	return texture(shadowMap, vec3(projCoords.xy , projCoords.z - bias ) );
}

void main() {
    // retrieve data from gbuffer
    vec3 fragPosition = texture( gPosition, fragTexCoord ).rgb;
    vec3 fragNormal = texture( gNormal, fragTexCoord ).rgb;
    vec3 fragDiffuse = texture( gAlbedoSpec, fragTexCoord ).rgb;
    float fragSpecular = texture( gAlbedoSpec, fragTexCoord ).a;
	float ambiantOcclusion = texture( ssao, fragTexCoord ).r;

    float dotLightNormal = -dot( fragNormal, vec3( light_direction ) );
	vec3 diffuse = max( dotLightNormal, 0.0 ) * fragDiffuse * vec3(light_diffuse);

	// specular
	vec3  viewDir = normalize( vec3( cameraPosition ) - fragPosition );
	vec3  reflectDir = reflect( vec3(light_direction), fragNormal );
	float spec = pow( max( dot( viewDir, reflectDir ), 0.0 ), 0.8 );
	vec3  specular = vec3( light_specular )* ( spec * fragSpecular );

	// Curvature
	ivec2 texel = ivec2(gl_FragCoord.xy);
	float curvature = calculate_curvature(texel, curvature_ridge, curvature_valley);
	fragDiffuse *= curvature + 1.0;

	// ambiant 
	vec3 ambiant = vec3( light_ambient.r * fragDiffuse * ambiantOcclusion );

	// shadow
	float shadow = ShadowCalculation(dotLightNormal, shadowViewProj * vec4(fragPosition, 1.0) );

	vec3 lighting = ambiant + (shadow * diffuse) + specular;

	FragColor = vec4( lighting, 1.0 );
}
