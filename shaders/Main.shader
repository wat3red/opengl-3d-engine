#shader vertex
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec3 FragPos;
out vec2 TexCoords;
out vec3 GeometricNormal;
out mat3 TBN;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    TexCoords = aTexCoords;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 N = normalize(normalMatrix * aNormal);
    GeometricNormal = N;

    if (length(aTangent) > 0.001)
    {
        vec3 T = normalize(normalMatrix * aTangent);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T);
        if (length(aBitangent) > 0.001)
        {
            vec3 loadedB = normalize(normalMatrix * aBitangent);
            if (dot(cross(N, T), loadedB) < 0.0)
            {
                B = -B;
            }
        }
        TBN = mat3(T, B, N);
    }
    else
    {
        vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
        vec3 T = normalize(cross(up, N));
        vec3 B = cross(N, T);
        TBN = mat3(T, B, N);
    }

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}

#shader fragment
#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec2 TexCoords;
in vec3 GeometricNormal;
in mat3 TBN;

// Material PBR textures
uniform sampler2D albedoMap;            // Texture slot 0
uniform sampler2D metallicRoughnessMap; // Texture slot 1 (G = Roughness, B = Metallic)
uniform sampler2D normalMap;            // Texture slot 2
uniform sampler2D aoMap;                // Texture slot 3

uniform int hasNormalMap;

uniform vec4 baseColorFactor;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform float aoStrength;

// Lights
struct DirLight {
    vec3 direction;
    vec3 color;
    float intensity;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    samplerCube shadowMap;
    float farPlane;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float range;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
};

#define NR_POINT_LIGHTS 4
#define NR_SPOT_LIGHTS 4

uniform DirLight dirLight;
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform SpotLight spotLights[NR_SPOT_LIGHTS];

uniform vec3 viewPos;
uniform mat4 view;

// Shadows
layout (std140) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[16];
};
uniform float cascadePlaneDistances[16];
uniform int cascadeCount;
uniform sampler2DArray shadowMap; // Cascaded shadow maps (Texture slot 10)
uniform float shadowBias;

uniform float alphaThreshold;
uniform int debugCascades;

// Atmospheric Fog Uniforms
uniform int fogEnabled;
uniform vec3 fogColor;
uniform float fogDensity;
uniform float fogStart;
uniform float fogEnd;
uniform float fogHeightFalloff;

const float PI = 3.14159265359;

// ---------------- Cook-Torrance GGX PBR Functions -----------------

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / max(denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.000001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx2 * ggx1;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalcPBRLight(vec3 L, vec3 radiance, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, vec3 F0)
{
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    if (NdotL <= 0.0)
        return vec3(0.0);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(NdotV, 0.001) * max(NdotL, 0.001);
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// ---------------- Shadow Calculations -----------------

float CalculateCascadeShadow(vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir)
{
    vec4 fragPosViewSpace = view * vec4(fragPosWorldSpace, 1.0);
    float depthValue = abs(fragPosViewSpace.z);

    int layer = -1;
    for (int i = 0; i < cascadeCount; ++i)
    {
        if (depthValue < cascadePlaneDistances[i])
        {
            layer = i;
            break;
        }
    }
    if (layer == -1)
    {
        return 0.0;
    }

    vec4 fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorldSpace, 1.0);
    if (abs(fragPosLightSpace.w) < 0.0001)
    {
        return 0.0;
    }
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
    {
        return 0.0;
    }

    float cosTheta = max(dot(normal, lightDir), 0.0);
    float currentBias = max(shadowBias * (1.0 - cosTheta), shadowBias * 0.15);

    float shadow = 0.0;
    int samples = 16;
    vec3 texelSize = 1.0 / vec3(textureSize(shadowMap, 0));

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

    for (int i = 0; i < samples; ++i)
    {
        vec2 randomOffset = poissonDisk[i % 16] * texelSize.xy;
        float pcfDepth = texture(shadowMap, vec3(projCoords.xy + randomOffset, layer)).r;
        shadow += (projCoords.z - currentBias > pcfDepth) ? 1.0 : 0.0;
    }

    shadow /= float(samples);
    return shadow;
}

float CalculatePointShadow(vec3 fragPos, vec3 lightPos, vec3 normal, samplerCube shadowMap, float farPlane)
{
    vec3 lightToFrag = fragPos - lightPos;
    float currentDepth = length(lightToFrag);

    // If fragment is at or beyond the light's farPlane, it receives no shadow from this light
    if (currentDepth >= farPlane)
    {
        return 0.0;
    }

    float currentDepthNorm = currentDepth / farPlane;

    vec3 L = normalize(lightPos - fragPos);
    float cosTheta = max(dot(normal, L), 0.0);
    // Slope-scaled adaptive bias to eliminate shadow acne on angled surfaces
    float bias = max(0.005 * (1.0 - cosTheta), 0.0015);

    float shadow = 0.0;
    int samples = 20;
    float diskRadius = (1.0 + (currentDepth / farPlane)) / 50.0;

    const vec3 sampleOffsetDirections[20] = vec3[](
       vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
       vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
       vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
       vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
       vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
    );

    for (int i = 0; i < samples; ++i)
    {
        float closestDepth = texture(shadowMap, lightToFrag + sampleOffsetDirections[i] * diskRadius).r;
        if (currentDepthNorm - bias > closestDepth)
        {
            shadow += 1.0;
        }
    }

    return shadow / float(samples);
}

// ---------------- Main Shader -----------------

void main()
{
    vec4 albedoTex = texture(albedoMap, TexCoords);
    vec4 baseColor = albedoTex * baseColorFactor;

    if (alphaThreshold > 0.0 && baseColor.a < alphaThreshold)
    {
        discard;
    }

    // Convert sRGB albedo to linear space for physically accurate lighting
    vec3 albedo = pow(baseColor.rgb, vec3(2.2));

    // Normal Mapping
    vec3 N;
    if (hasNormalMap == 1 && length(TBN[0]) > 0.1)
    {
        vec3 tangentNormal = texture(normalMap, TexCoords).xyz * 2.0 - 1.0;
        N = normalize(TBN * tangentNormal);
    }
    else
    {
        N = normalize(GeometricNormal);
    }

    if (length(N) < 0.001)
    {
        N = vec3(0.0, 1.0, 0.0);
    }

    if (!gl_FrontFacing)
    {
        N = -N;
    }

    vec3 V = normalize(viewPos - FragPos);

    // glTF 2.0: Green = Roughness, Blue = Metallic
    vec4 mrSample = texture(metallicRoughnessMap, TexCoords);
    float roughness = clamp(mrSample.g * roughnessFactor, 0.04, 1.0);
    float metallic = clamp(mrSample.b * metallicFactor, 0.0, 1.0);

    float aoSample = texture(aoMap, TexCoords).r;
    float ao = mix(1.0, aoSample, aoStrength);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    // Directional Light
    if (dirLight.intensity > 0.0001)
    {
        vec3 L = normalize(-dirLight.direction);
        if (dot(N, L) > 0.0)
        {
            float shadow = CalculateCascadeShadow(FragPos, N, L);
            vec3 radiance = dirLight.color * dirLight.intensity;
            Lo += (1.0 - shadow) * CalcPBRLight(L, radiance, N, V, albedo, roughness, metallic, F0);
        }
    }

    // Point Lights
    for (int i = 0; i < NR_POINT_LIGHTS; ++i)
    {
        if (pointLights[i].intensity <= 0.0001)
            continue;

        vec3 lightVec = pointLights[i].position - FragPos;
        float distance = length(lightVec);
        if (distance >= pointLights[i].farPlane)
            continue;

        vec3 L = normalize(lightVec);
        if (dot(N, L) <= 0.0)
            continue;

        // Physically based smooth windowing (Karis / Unreal model) to eliminate sharp circular cutoff
        float distRatio = distance / pointLights[i].farPlane;
        float window = clamp(1.0 - distRatio * distRatio, 0.0, 1.0);
        window = window * window;

        float attenuation = 1.0 / (pointLights[i].constant + pointLights[i].linear * distance + pointLights[i].quadratic * (distance * distance));
        vec3 radiance = pointLights[i].color * pointLights[i].intensity * attenuation * window;

        float shadow = CalculatePointShadow(FragPos, pointLights[i].position, N, pointLights[i].shadowMap, pointLights[i].farPlane);
        Lo += (1.0 - shadow) * CalcPBRLight(L, radiance, N, V, albedo, roughness, metallic, F0);
    }

    // Spot Lights
    for (int i = 0; i < NR_SPOT_LIGHTS; ++i)
    {
        if (spotLights[i].intensity <= 0.0001)
            continue;

        vec3 lightVec = spotLights[i].position - FragPos;
        float distance = length(lightVec);
        float maxRange = max(spotLights[i].range, 1.0);
        if (distance >= maxRange)
            continue;

        vec3 L = normalize(lightVec);
        if (dot(N, L) <= 0.0)
            continue;

        float dirLen = length(spotLights[i].direction);
        if (dirLen < 0.001)
            continue;

        vec3 spotDir = -spotLights[i].direction / dirLen;
        float theta = dot(L, spotDir);
        if (theta <= spotLights[i].outerCutOff)
            continue;

        // Smooth cubic Hermite penumbra falloff across the cone boundary
        float epsilon = max(spotLights[i].cutOff - spotLights[i].outerCutOff, 0.0001);
        float t = clamp((theta - spotLights[i].outerCutOff) / epsilon, 0.0, 1.0);
        float spotIntensity = t * t * (3.0 - 2.0 * t);

        // Physically based smooth windowing (Karis / Unreal model) to eliminate sharp cutoff at range limit
        float distRatio = clamp(distance / maxRange, 0.0, 1.0);
        float window = clamp(1.0 - distRatio * distRatio, 0.0, 1.0);
        window = window * window;

        float denom = max(spotLights[i].constant + spotLights[i].linear * distance + spotLights[i].quadratic * (distance * distance), 0.01);
        float attenuation = 1.0 / denom;
        vec3 radiance = spotLights[i].color * spotLights[i].intensity * attenuation * spotIntensity * window;

        Lo += CalcPBRLight(L, radiance, N, V, albedo, roughness, metallic, F0);
    }

    // Ambient Lighting (soft ambient scaled by AO)
    vec3 ambient = (dirLight.color * dirLight.intensity * 0.05 + vec3(0.03)) * albedo * ao;

    vec3 color = ambient + Lo;

    if (fogEnabled == 1)
    {
        float dist = length(viewPos - FragPos);
        float distFactor = clamp((dist - fogStart) / max(fogEnd - fogStart, 0.001), 0.0, 1.0);
        float expDistFactor = 1.0 - exp(-pow(dist * fogDensity, 2.0));
        float combinedDist = mix(distFactor, expDistFactor, 0.5);

        float heightFactor = clamp(exp(-max(FragPos.y, 0.0) * fogHeightFalloff), 0.05, 1.0);
        float fogFactor = clamp(combinedDist * heightFactor, 0.0, 1.0);

        vec3 viewDir = normalize(FragPos - viewPos);
        float sunScatter = max(dot(viewDir, -dirLight.direction), 0.0);
        vec3 inscatterColor = mix(fogColor, dirLight.color * max(dirLight.intensity, 1.0), pow(sunScatter, 8.0) * 0.4);

        color = mix(color, inscatterColor, fogFactor);
    }

    if (debugCascades == 1)
    {
        vec4 fragPosViewSpace = view * vec4(FragPos, 1.0);
        float depthValue = abs(fragPosViewSpace.z);
        int layer = -1;
        for (int i = 0; i < cascadeCount; ++i)
        {
            if (depthValue < cascadePlaneDistances[i])
            {
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
    }
    else
    {
        FragColor = vec4(color, baseColor.a);
    }
}