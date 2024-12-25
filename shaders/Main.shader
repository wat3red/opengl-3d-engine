//Main.cpp
#shader vertex
#version 330 core
// Positions/Coordinates
layout (location = 0) in vec3 aPos;
// Normals (not necessarily normalized)
layout (location = 1) in vec3 aNormal;
// Texture Coordinates
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0); // For directional/spot light shadows
    gl_Position = projection * view * vec4(FragPos, 1.0);
}

#shader fragment
    #version 330 core
out vec4 FragColor;

struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    samplerCube shadowMap; // Cubemap shadow texture
    float farPlane;        // Far plane for depth comparison
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

#define NR_POINT_LIGHTS 2

// Inputs from the vertex shader
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 FragPosLightSpace;

// Uniforms
uniform vec3 viewPos;
uniform DirLight dirLight;
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform SpotLight spotLight;
uniform Material material;

// Shadow maps
uniform sampler2D dirSpotShadowMap;

// Function prototypes
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
float CalculateDirSpotShadow(vec4 fragPosLightSpace, sampler2D shadowMap);
float CalculatePointShadow(vec3 fragPos, vec3 lightPos, samplerCube shadowMap, float farPlane);

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // Accumulate lighting
    vec3 result = CalcDirLight(dirLight, norm, viewDir);

    for (int i = 0; i < NR_POINT_LIGHTS; i++) {
        result += CalcPointLight(pointLights[i], norm, FragPos, viewDir);
    }

    result += CalcSpotLight(spotLight, norm, FragPos, viewDir);

    FragColor = vec4(result, 1.0);
}

// Calculate directional light with shadows
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    vec3 ambient = light.ambient * vec3(texture(material.diffuse, TexCoords));
    vec3 diffuse = light.diffuse * diff * vec3(texture(material.diffuse, TexCoords));
    vec3 specular = light.specular * spec * vec3(texture(material.specular, TexCoords));

    // Shadow calculation
    float shadow = CalculateDirSpotShadow(FragPosLightSpace, dirSpotShadowMap);

    return ambient + (1.0 - shadow) * (diffuse + specular);
}

// Calculate point light with shadows
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    vec3 ambient = light.ambient * vec3(texture(material.diffuse, TexCoords));
    vec3 diffuse = light.diffuse * diff * vec3(texture(material.diffuse, TexCoords));
    vec3 specular = light.specular * spec * vec3(texture(material.specular, TexCoords));

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    // Shadow calculation
    float shadow = CalculatePointShadow(fragPos, light.position, light.shadowMap, light.farPlane);
    return ambient + (1.0 - shadow) * (diffuse + specular);
}

// Calculate spotlight with shadows
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    vec3 ambient = light.ambient * vec3(texture(material.diffuse, TexCoords));
    vec3 diffuse = light.diffuse * diff * vec3(texture(material.diffuse, TexCoords));
    vec3 specular = light.specular * spec * vec3(texture(material.specular, TexCoords));

    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    // Shadow calculation
    float shadow = CalculateDirSpotShadow(FragPosLightSpace, dirSpotShadowMap);
    return ambient + (1.0 - shadow) * (diffuse + specular);
}

// Directional/spotlight shadow calculation
float CalculateDirSpotShadow(vec4 fragPosLightSpace, sampler2D shadowMap)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // Discard shadows outside the shadow map range
    if (projCoords.z > 1.0)
        return 0.0;

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    float currentDepth = projCoords.z;

    // Compute distance from camera
    float fragDistance = length(viewPos - FragPos); // Camera-space distance

    // Threshold for switching sampling techniques
    float samplingThreshold = 50.0; // Adjust based on scene scale

    if (fragDistance < samplingThreshold)
    {
        // Apply Poisson Disk Sampling for close objects
        vec2 poissonDisk[16] = vec2[](
            vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
            vec2(-0.09418410, -0.92938870), vec2(0.34495938, 0.29387760),
            vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
            vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
            vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
            vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
            vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
            vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
        );

        mat2 rotationMatrix = mat2(
            cos(gl_FragCoord.x * 0.1), -sin(gl_FragCoord.x * 0.1),
            sin(gl_FragCoord.x * 0.1),  cos(gl_FragCoord.x * 0.1)
        );

        for (int i = 0; i < 16; ++i)
        {
            vec2 offset = rotationMatrix * poissonDisk[i] * texelSize;
            float closestDepth = texture(shadowMap, projCoords.xy + offset).r;

            shadow += currentDepth > closestDepth + 0.005 ? 1.0 : 0.0;
        }

        shadow /= 16.0; // Average the results
    }
    else
    {
        // Simple single-sample shadowing for distant objects
        float closestDepth = texture(shadowMap, projCoords.xy).r;
        shadow = currentDepth > closestDepth + 0.005 ? 1.0 : 0.0;
    }

    return shadow;
}

// Point light shadow calculation
float CalculatePointShadow(vec3 fragPos, vec3 lightPos, samplerCube shadowMap, float farPlane)
{
    vec3 lightToFrag = fragPos - lightPos;
    float currentDepth = length(lightToFrag) / farPlane;

    // Random sampling
    float shadow = 0.0;
    int samples = 20; // Higher samples for closer objects
    float diskRadius = 0.05;

    for (int i = 0; i < samples; ++i) {
        vec3 randomOffset = normalize(vec3(
            fract(sin(dot(vec3(gl_FragCoord.xy, i), vec3(12.9898, 78.233, 54.53))) * 43758.5453),
            fract(cos(dot(vec3(gl_FragCoord.yx, i), vec3(42.535, 93.425, 12.34))) * 23432.1234),
            fract(sin(dot(vec3(gl_FragCoord.xy, i + 42), vec3(91.934, 45.123, 34.534))) * 52342.12)
        )) * diskRadius;

        float closestDepth = texture(shadowMap, lightToFrag + randomOffset).r;
        shadow += currentDepth > closestDepth + 0.005 ? 1.0 : 0.0;
    }

    return shadow / float(samples);
}
