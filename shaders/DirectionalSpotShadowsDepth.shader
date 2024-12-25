#shader vertex
#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix; // Correction: Ensure the uniform matches the C++ code
uniform mat4 model;

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0); // Use lightSpaceMatrix
}

#shader fragment
#version 330 core

void main()
{
    // Empty fragment shader for depth-only rendering
}
