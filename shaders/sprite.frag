#version 330
in vec2 TexCoord;
out vec4 color;
uniform sampler2D ourTexture;
uniform vec4 tintColor;
uniform float whiteFlash; // 0..1: flash a blanco (feedback de invulnerabilidad)

void main()
{
    vec4 texColor = texture(ourTexture, TexCoord);
    if (texColor.a < 0.08) {
        discard;
    }

    vec4 tinted = texColor * tintColor;
    vec4 flashed = vec4(1.0, 1.0, 1.0, tinted.a);
    color = mix(tinted, flashed, clamp(whiteFlash, 0.0, 1.0));
}
