#version 420 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoord;

out vec2 fragTexCoord;

void main()
{
    fragTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 1.0);
}