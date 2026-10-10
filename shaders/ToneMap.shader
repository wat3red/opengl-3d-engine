#shader vertex
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}

#shader fragment
#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D hdrBuffer;
uniform float exposure;
uniform int toneMappingOperator;
uniform float gamma;

vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 Reinhard(vec3 x)
{
    return x / (x + vec3(1.0));
}

void main()
{
    vec3 hdrColor = texture(hdrBuffer, TexCoords).rgb;

    vec3 mapped = hdrColor * max(exposure, 0.001);

    if (toneMappingOperator == 0)
    {
        mapped = ACESFilm(mapped);
    }
    else if (toneMappingOperator == 1)
    {
        mapped = Reinhard(mapped);
    }
    else
    {
        mapped = clamp(mapped, 0.0, 1.0);
    }

    mapped = pow(mapped, vec3(1.0 / max(gamma, 0.001)));

    FragColor = vec4(mapped, 1.0);
}
