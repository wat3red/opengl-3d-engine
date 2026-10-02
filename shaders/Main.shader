//Main.cpp
#shader vertex
#version 330 core

layout (location = 0) in vec3 aPos; // Positions/Coordinates
layout (location = 1) in vec3 aNormal; // Normals (not necessarily normalized)
layout (location = 2) in vec2 aTexCoords; // Texture Coordinates

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = transpose(inverse(mat3(model))) * aNormal;

    TexCoords = aTexCoords;
    
    gl_Position = projection * view * model * vec4(aPos, 1.0);

}

#shader fragment
#version 330 core
#line 35 "Main.shader"
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

// -------------------------------- Uniforms ---------------------------------

uniform float shadowBias;

uniform DirLight dirLight;
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform SpotLight spotLight;

uniform Material material;

uniform vec3 viewPos;
uniform mat4 view;

// Shadow maps
layout (std140) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[16];
};
uniform float cascadePlaneDistances[16];
uniform int cascadeCount;   // number of frusta
uniform sampler2DArray shadowMap; // Cascaded shadow maps

// Alpha testing threshold
uniform float alphaThreshold;

// Debugging toggle
uniform int debugCascades;
// -------------------------------- Function Prototypes ---------------------------------

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, float alpha);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float alpha);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float alpha);
float CalculateCascadeShadow(vec3 fragPosWorldSpace);
float CalculatePointShadow(vec3 fragPos, vec3 lightPos, samplerCube shadowMap, float farPlane);

// Main Function
void main()
{
    // Alternatively, if alpha is in the diffuse texture's alpha channel:
    float alpha = texture(material.diffuse, TexCoords).a;

    // Optional alpha testing
    if (alphaThreshold > 0.0 && alpha < alphaThreshold) {
        discard;  // Discard fragments below the alpha threshold
    }

    vec3 norm = normalize(Normal);
    if (!gl_FrontFacing) norm = -norm;
    vec3 viewDir = normalize(viewPos - FragPos);

    // Accumulate lighting
    vec3 result = vec3(0.0);

    for (int i = 0; i < NR_POINT_LIGHTS; i++) {
        result += CalcPointLight(pointLights[i], norm, FragPos, viewDir, alpha);
    }

    result += CalcSpotLight(spotLight, norm, FragPos, viewDir, alpha);

    result += CalcDirLight(dirLight, norm, viewDir, alpha);
    
    if (debugCascades == 1) {
        vec4 fragPosViewSpace = view * vec4(FragPos, 1.0);
        float depthValue = abs(fragPosViewSpace.z);
        int layer = -1;
        for (int i = 0; i < cascadeCount; ++i) {
            if (depthValue < cascadePlaneDistances[i]) {
                layer = i;
                break;
            }
        }
        if (layer == -1) layer = cascadeCount;
        vec3 debugColor = vec3(1.0);
        if (layer == 0) debugColor = vec3(1.0, 0.0, 0.0);
        else if (layer == 1) debugColor = vec3(0.0, 1.0, 0.0);
        else if (layer == 2) debugColor = vec3(0.0, 0.0, 1.0);
        else if (layer == 3) debugColor = vec3(1.0, 0.0, 1.0);
        else debugColor = vec3(1.0, 1.0, 0.0);
        FragColor = vec4(debugColor, 1.0);
    } else {
        FragColor = vec4(result, 1.0);
    }
}

// Directional Light Calculation with Cascade Shadows
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, float alpha)
{
    float lightLen = length(light.direction);
    if (lightLen < 0.0001) return vec3(0.0);
    vec3 lightDir = -light.direction / lightLen;
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    vec4 diffuseTex = texture(material.diffuse, TexCoords);
    vec4 specularTex = texture(material.specular, TexCoords);

    vec3 ambient = light.ambient * diffuseTex.rgb;
    vec3 diffuse = light.diffuse * diff * diffuseTex.rgb;
    vec3 specular = light.specular * spec * specularTex.rgb;

    // Cascade shadow calculation
    float shadow = CalculateCascadeShadow(FragPos);

    return ambient + (1.0 - shadow) * (diffuse + specular);
}

float CalculateCascadeShadow(vec3 fragPosWorldSpace) {
    vec4 fragPosViewSpace = view * vec4(fragPosWorldSpace, 1.0);
    float depthValue = abs(fragPosViewSpace.z);

    int layer = -1;
    for (int i = 0; i < cascadeCount; ++i) {
        if (depthValue < cascadePlaneDistances[i]) {
            layer = i;
            break;
        }
    }
    if (layer == -1) {
        layer = cascadeCount; // Default to last cascade
    }

    vec4 fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorldSpace, 1.0);
    if (abs(fragPosLightSpace.w) < 0.0001) {
        return 0.0;
    }
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0; // Outside light's frustum
    }

    float shadow = 0.0;
    int samples = 16; // Number of samples for multisampling
    vec3 texelSize = 1.0 / textureSize(shadowMap, 0);

    // Multisampling: sample surrounding texels with random offsets
    for (int i = 0; i < samples; ++i) {
       // vec3 randomOffset = normalize(vec3(
        //    fract(sin(dot(vec3(gl_FragCoord.xy, i), vec3(12.9898, 78.233, 54.53))) * 43758.5453),
        //    fract(cos(dot(vec3(gl_FragCoord.yx, i), vec3(42.535, 93.425, 12.34))) * 23432.1234),
        //    fract(sin(dot(vec3(gl_FragCoord.xy, i + 42), vec3(91.934, 45.123, 34.534))) * 52342.12)
       // )) * texelSize;

        const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
);

        vec2 randomOffset = poissonDisk[i % 16] * texelSize.xy;


        float pcfDepth = texture(shadowMap, vec3(projCoords.xy + randomOffset, layer)).r;
        shadow += (projCoords.z > pcfDepth + shadowBias) ? 1.0 : 0.0;
        
        //float pcfDepth = texture(shadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize.xy, layer)).r;
        //shadow += (projCoords.z > pcfDepth + shadowBias) ? 1.0 : 0.0;
    }

    shadow /= float(samples);

    return shadow;
}


// Point Light Calculation (unchanged)
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float alpha)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    vec4 diffuseTex = texture(material.diffuse, TexCoords);
    vec4 specularTex = texture(material.specular, TexCoords);

    vec3 ambient = light.ambient * diffuseTex.rgb;
    vec3 diffuse = light.diffuse * diff * diffuseTex.rgb;
    vec3 specular = light.specular * spec * specularTex.rgb;

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    float shadow = CalculatePointShadow(fragPos, light.position, light.shadowMap, light.farPlane);
    return ambient + (1.0 - shadow) * (diffuse + specular);
}

// Spot Light Calculation (unchanged)
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float alpha)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    float dirLen = length(light.direction);
    if (dirLen < 0.0001) return vec3(0.0);
    float theta = dot(lightDir, -light.direction / dirLen);
    float epsilon = max(light.cutOff - light.outerCutOff, 0.0001);
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    vec4 diffuseTex = texture(material.diffuse, TexCoords);
    vec4 specularTex = texture(material.specular, TexCoords);

    vec3 ambient = light.ambient * diffuseTex.rgb;
    vec3 diffuse = light.diffuse * diff * diffuseTex.rgb;
    vec3 specular = light.specular * spec * specularTex.rgb;

    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    float shadow = CalculateCascadeShadow(FragPos);

    return ambient + (1.0 - shadow) * (diffuse + specular);
}

// Point Light Shadow Calculation (unchanged)
float CalculatePointShadow(vec3 fragPos, vec3 lightPos, samplerCube shadowMap, float farPlane)
{
    vec3 lightToFrag = fragPos - lightPos;
    float currentDepth = length(lightToFrag) / farPlane;

    float shadow = 0.0;
    int samples = 20;
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