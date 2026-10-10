// DirectionalShadowsDepth.shader
#shader vertex
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 model;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = model * vec4(aPos, 1.0);
}

#shader geometry
#version 410 core

layout(triangles, invocations = 5) in;
layout(triangle_strip, max_vertices = 3) out;

in vec2 TexCoords[];
out vec2 FragTexCoords;

layout (std140) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[16];
};

void main()
{          
	for (int i = 0; i < 3; ++i)
	{
		gl_Position = lightSpaceMatrices[gl_InvocationID] * gl_in[i].gl_Position;
		gl_Layer = gl_InvocationID;
		FragTexCoords = TexCoords[i];
		EmitVertex();
	}
	EndPrimitive();
}  

#shader fragment
#version 330 core

in vec2 FragTexCoords;

uniform sampler2D albedoMap;
uniform float alphaThreshold;

void main()
{
    if (alphaThreshold > 0.0) {
        if (texture(albedoMap, FragTexCoords).a < alphaThreshold)
            discard;
    }
}