#shader vertex
#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = vec4(pos.x, pos.y, pos.w,pos.w);
}  

#shader fragment
#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;
uniform int fogEnabled;
uniform vec3 fogColor;

void main()
{    
    vec4 skyColor = texture(skybox, TexCoords);
    if (fogEnabled == 1)
    {
        vec3 norm = normalize(TexCoords);
        if (norm.y < 0.35)
        {
            float horizonBlend = clamp((0.35 - norm.y) / 0.45, 0.0, 1.0);
            skyColor.rgb = mix(skyColor.rgb, fogColor, horizonBlend * 0.85);
        }
    }
    FragColor = skyColor;
}