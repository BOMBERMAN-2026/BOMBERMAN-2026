#version 330
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D baseColorTex;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform float ambientStrength;
uniform float specularStrength;
uniform float shininess;
uniform vec3 objectTintColor; // Color para tintado (ej: rojo al morir)

in vec3 FragPos;
in vec3 Normal;

void main()
{
    vec4 albedo = texture(baseColorTex, TexCoord);

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);

    float diff = max(dot(norm, lightDir), 0.0);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);

    vec3 ambient = ambientStrength * lightColor;
    vec3 diffuse = diff * lightColor;
    vec3 specular = specularStrength * spec * lightColor;

    vec3 litColor = (ambient + diffuse + specular) * albedo.rgb;
    FragColor = vec4(litColor * objectTintColor, albedo.a);
}
