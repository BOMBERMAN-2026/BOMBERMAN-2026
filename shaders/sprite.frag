#version 330
in vec2 TexCoord;
out vec4 color;
uniform sampler2D ourTexture;
uniform vec4 tintColor;

void main()
{
    vec4 texColor = texture(ourTexture, TexCoord);
    if (texColor.a < 0.08) {
        discard;
    }
    color = texColor * tintColor;
}
