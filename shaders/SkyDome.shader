#shader vertex
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    TexCoords = aTexCoords;
    // Strip camera translation so the dome stays infinitely centered around the viewer
    mat4 rotView = mat4(mat3(view));
    vec4 pos = projection * rotView * model * vec4(aPos, 1.0) ;
    // Force depth to the maximum far plane (z / w = 1.0)
    gl_Position = vec4(pos.x, pos.y, pos.w, pos.w);
}

#shader fragment
#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D albedoMap;
uniform int fogEnabled;
uniform vec3 fogColor;

void main()
{
    vec4 col = texture(albedoMap, TexCoords);
    if (fogEnabled == 1)
    {
        // For dome spherical texture coordinates, TexCoords.y corresponds to vertical height
        if (TexCoords.y < 0.45)
        {
            float horizonBlend = clamp((0.45 - TexCoords.y) / 0.45, 0.0, 1.0);
            col.rgb = mix(col.rgb, fogColor, horizonBlend * 0.85);
        }
    }
    FragColor = col;
}
