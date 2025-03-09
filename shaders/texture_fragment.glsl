#version 330 core

in vec2 texCoord;

uniform sampler2D ourTexture;
out vec4 FragColor;


void main()
{
    vec4 c = texture(ourTexture, texCoord);

    // if(c.a <= 0.1)
    // c.a = 0.1;
    // c.r = 1.0f;
    
    FragColor = c;
}