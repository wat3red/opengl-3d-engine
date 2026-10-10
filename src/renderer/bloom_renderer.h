#pragma once

#include "shader.h"
#include <vector>
#include <glm/glm.hpp>
#include <GL/glew.h>

struct BloomMip {
	glm::vec2 size;
	glm::ivec2 intSize;
	unsigned int texture = 0;
};

class BloomRenderer {
public:
	BloomRenderer();
	~BloomRenderer();

	BloomRenderer(const BloomRenderer&) = delete;
	BloomRenderer& operator=(const BloomRenderer&) = delete;

	void Initialize(int width, int height, int mipChainLength = 6);
	void Shutdown();
	void Resize(int width, int height);

	void RenderBloom(unsigned int srcTexture, float threshold = 1.0f, float filterRadius = 0.005f);

	unsigned int GetBloomTexture() const {
		return m_MipChain.empty() ? 0 : m_MipChain[0].texture;
	}

private:
	unsigned int m_FBO = 0;
	std::vector<BloomMip> m_MipChain;
	int m_MipChainLength = 6;
	glm::ivec2 m_SrcViewportSize = { 0, 0 };

	Shader m_DownsampleShader;
	Shader m_UpsampleShader;
};
