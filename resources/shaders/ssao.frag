#version 420 core
out float FragColor;

in vec2 fragTexCoord;

uniform sampler2D gPosition;
uniform sampler2D gViewPosition;
uniform sampler2D gNormal;
uniform sampler2D gViewNormal;
uniform sampler2D ssaoNoise;

#define SSAO_KERNEL_SIZE 32
uniform vec3 samples[SSAO_KERNEL_SIZE];
uniform vec2 noiseScale;
uniform float radius = 0.5;
uniform float bias = 0.025;

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
    if ( texture(gPosition, fragTexCoord).y <= 0.1 ) {
        FragColor = 1.0;
        return;
    }
    // get input for SSAO algorithm
    vec3 fragPos = texture(gViewPosition, fragTexCoord).xyz;
    vec3 normal = normalize(texture(gViewNormal, fragTexCoord).rgb);
    vec3 randomVec = normalize(texture(ssaoNoise, fragTexCoord * noiseScale).xyz);
    // create TBN change-of-basis matrix: from tangent-space to view-space
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    // iterate over the sample kernel and calculate occlusion factor
    float occlusion = 0.0;
    float position_depth = fragPos.z;
    for(int i = 0; i < SSAO_KERNEL_SIZE; ++i)
    {
        // get sample position
        vec4 samplePosition = vec4(fragPos + TBN * samples[i] * radius, 1.0);
        
        // project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = projection * samplePosition ; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        
        // get sample depth
        float sampleDepth = texture(gViewPosition, offset.xy).z;
        
        // range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(position_depth - sampleDepth));
        occlusion += (sampleDepth >= samplePosition.z + bias ? 1.0 : 0.0) * rangeCheck;           
    }
    occlusion = 1.0 - (occlusion / SSAO_KERNEL_SIZE);
    
    FragColor = occlusion;
}