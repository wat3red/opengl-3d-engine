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

uniform sampler2D srcTexture;
uniform vec2 srcResolution;
uniform int mipLevel;
uniform float threshold;

// Soft threshold / knee curve (Jimenez 2014) to avoid harsh fireflies
vec3 Prefilter(vec3 color, float thresh)
{
    float brightness = max(color.r, max(color.g, color.b));
    float knee = thresh * 0.5;
    float soft = brightness - thresh + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);
    float contribution = max(soft, brightness - thresh);
    contribution /= max(brightness, 0.00001);
    return color * max(contribution, 0.0);
}

void main()
{
    vec2 texelSize = 1.0 / srcResolution;
    float x = texelSize.x;
    float y = texelSize.y;

    // 13-tap bilinear box filter (Call of Duty: AW / Jimenez 2014)
    vec3 a = texture(srcTexture, vec2(TexCoords.x - 2.0*x, TexCoords.y + 2.0*y)).rgb;
    vec3 b = texture(srcTexture, vec2(TexCoords.x,          TexCoords.y + 2.0*y)).rgb;
    vec3 c = texture(srcTexture, vec2(TexCoords.x + 2.0*x, TexCoords.y + 2.0*y)).rgb;

    vec3 d = texture(srcTexture, vec2(TexCoords.x - 2.0*x, TexCoords.y)).rgb;
    vec3 e = texture(srcTexture, vec2(TexCoords.x,          TexCoords.y)).rgb;
    vec3 f = texture(srcTexture, vec2(TexCoords.x + 2.0*x, TexCoords.y)).rgb;

    vec3 g = texture(srcTexture, vec2(TexCoords.x - 2.0*x, TexCoords.y - 2.0*y)).rgb;
    vec3 h = texture(srcTexture, vec2(TexCoords.x,          TexCoords.y - 2.0*y)).rgb;
    vec3 i = texture(srcTexture, vec2(TexCoords.x + 2.0*x, TexCoords.y - 2.0*y)).rgb;

    vec3 j = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
    vec3 k = texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;
    vec3 l = texture(srcTexture, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
    vec3 m = texture(srcTexture, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;

    vec3 downsample = e * 0.125;
    downsample += (a + c + g + i) * 0.03125;
    downsample += (b + d + f + h) * 0.0625;
    downsample += (j + k + l + m) * 0.125;

    // Apply bright threshold only on the first downsample pass (mipLevel == 0)
    if (mipLevel == 0)
    {
        downsample = Prefilter(downsample, threshold);
    }

    FragColor = vec4(downsample, 1.0);
}
