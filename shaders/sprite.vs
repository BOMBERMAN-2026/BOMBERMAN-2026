#version 330
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 texCoord;
out vec2 TexCoord;
uniform mat4 model;
uniform mat4 projection;
uniform vec4 uvRect; // (u0, v0, u1, v1)
uniform float flipX; // 0.0 normal, 1.0 mirror horizontally

void main()
{
    gl_Position = projection * model * vec4(pos, 1.0);
    float tx = mix(texCoord.x, 1.0 - texCoord.x, flipX);
    TexCoord = vec2(
        mix(uvRect.x, uvRect.z, tx),
        mix(uvRect.y, uvRect.w, texCoord.y)
    );
}
