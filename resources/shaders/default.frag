#version 330 core
out vec4 FragColor;  

in vec2 TexCoord;
in float TexIndex; 

uniform sampler2DArray ourTexture;
  
void main()
{
	FragColor = texture(ourTexture, vec3(TexCoord, TexIndex));
}
