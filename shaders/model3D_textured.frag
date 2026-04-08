#version 330
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D baseColorTex;

void main()
{
    vec4 color = texture(baseColorTex, TexCoord);
    FragColor = vec4(color.rgb, 1.0);
}
