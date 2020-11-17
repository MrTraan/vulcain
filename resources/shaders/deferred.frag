#version 330 core
layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gViewPosition;
layout (location = 2) out vec4 gNormal;
layout (location = 3) out vec4 gViewNormal;
layout (location = 4) out vec4 gAlbedoSpec;

in vec3 fragPosition;
in vec3 fragViewPosition;
in vec3 fragNormal;
in vec3 fragViewNormal;
in vec2 fragTexCoord;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
}; 
uniform Material material;

uniform sampler2D diffuseTexture;

void main()
{    
    // store the fragment position vector in the first gbuffer texture
    gPosition = vec4(fragPosition, 1.0);
    gViewPosition = vec4(fragViewPosition, 1.0);
    // also store the per-fragment normals into the gbuffer
    gNormal = vec4(normalize(fragNormal), 1.0);
    gViewNormal = vec4(normalize(fragViewNormal), 1.0);
    // and the diffuse per-fragment color
    gAlbedoSpec.rgb = texture(diffuseTexture, fragTexCoord).rgb;
    gAlbedoSpec.rgb *= material.diffuse;
    // store specular intensity in gAlbedoSpec's alpha component
    gAlbedoSpec.a = material.shininess;
}  