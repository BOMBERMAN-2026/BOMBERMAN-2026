#version 330
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 iModelRow0;
layout (location = 4) in vec4 iModelRow1;
layout (location = 5) in vec4 iModelRow2;
layout (location = 6) in vec4 iModelRow3;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform int useInstancing;

out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;
out vec4 FragPosLightSpace;

void main()
{
    mat4 instanceModel = mat4(iModelRow0, iModelRow1, iModelRow2, iModelRow3);
    mat4 effectiveModel = (useInstancing != 0) ? instanceModel : model;

    vec4 worldPos = effectiveModel * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    Normal = mat3(transpose(inverse(effectiveModel))) * aNormal;
    gl_Position = projection * view * worldPos;
    TexCoord = aTexCoord;
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
}
