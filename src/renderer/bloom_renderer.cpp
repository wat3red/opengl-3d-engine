#include "bloom_renderer.h"
#include "primitive.h"
#include <print>

BloomRenderer::BloomRenderer()
{
}

BloomRenderer::~BloomRenderer()
{
	Shutdown();
}

void BloomRenderer::Initialize(int width, int height, int mipChainLength)
{
	m_MipChainLength = mipChainLength;

	if (m_FBO == 0)
	{
		glGenFramebuffers(1, &m_FBO);
	}

	m_DownsampleShader = Shader("shaders/BloomDownsample.shader");
	m_UpsampleShader = Shader("shaders/BloomUpsample.shader");

	Resize(width, height);
}

void BloomRenderer::Shutdown()
{
	for (auto& mip : m_MipChain)
	{
		if (mip.texture != 0)
		{
			glDeleteTextures(1, &mip.texture);
			mip.texture = 0;
		}
	}
	m_MipChain.clear();

	if (m_FBO != 0)
	{
		glDeleteFramebuffers(1, &m_FBO);
		m_FBO = 0;
	}
}

void BloomRenderer::Resize(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;

	if (width == m_SrcViewportSize.x && height == m_SrcViewportSize.y && !m_MipChain.empty())
		return;

	m_SrcViewportSize = { width, height };

	// Clean up existing textures
	for (auto& mip : m_MipChain)
	{
		if (mip.texture != 0)
		{
			glDeleteTextures(1, &mip.texture);
		}
	}
	m_MipChain.clear();

	glm::vec2 currentSize = glm::vec2(static_cast<float>(width), static_cast<float>(height));
	glm::ivec2 currentIntSize = glm::ivec2(width, height);

	for (int i = 0; i < m_MipChainLength; ++i)
	{
		currentSize *= 0.5f;
		currentIntSize /= 2;

		if (currentIntSize.x < 2 || currentIntSize.y < 2)
			break;

		BloomMip mip;
		mip.size = currentSize;
		mip.intSize = currentIntSize;

		glGenTextures(1, &mip.texture);
		glBindTexture(GL_TEXTURE_2D, mip.texture);
		// R11F_G11F_B10F is optimal for HDR bloom (compact 32-bit float, no alpha)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, mip.intSize.x, mip.intSize.y, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		m_MipChain.push_back(mip);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_MipChain[0].texture, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BloomRenderer::RenderBloom(unsigned int srcTexture, float threshold, float filterRadius)
{
	if (m_MipChain.empty() || m_FBO == 0 || srcTexture == 0)
		return;

	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	glDisable(GL_DEPTH_TEST);

	// 1. Downsample pass: successively downsample and filter
	m_DownsampleShader.Bind();
	m_DownsampleShader.SetUniform1i("srcTexture", 0);
	m_DownsampleShader.SetUniform1f("threshold", threshold);

	glActiveTexture(GL_TEXTURE0);

	for (size_t i = 0; i < m_MipChain.size(); ++i)
	{
		const auto& mip = m_MipChain[i];
		glViewport(0, 0, mip.intSize.x, mip.intSize.y);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);

		if (i == 0)
		{
			glBindTexture(GL_TEXTURE_2D, srcTexture);
			m_DownsampleShader.SetUniform2f("srcResolution", glm::vec2(static_cast<float>(m_SrcViewportSize.x), static_cast<float>(m_SrcViewportSize.y)));
			m_DownsampleShader.SetUniform1i("mipLevel", 0);
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, m_MipChain[i - 1].texture);
			m_DownsampleShader.SetUniform2f("srcResolution", m_MipChain[i - 1].size);
			m_DownsampleShader.SetUniform1i("mipLevel", static_cast<int>(i));
		}

		Primitive::GetFullScreenQuad()->Draw(m_DownsampleShader);
	}

	m_DownsampleShader.Unbind();

	// 2. Upsample pass with additive blending
	m_UpsampleShader.Bind();
	m_UpsampleShader.SetUniform1i("srcTexture", 0);
	m_UpsampleShader.SetUniform1f("filterRadius", filterRadius);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glBlendEquation(GL_FUNC_ADD);

	for (int i = static_cast<int>(m_MipChain.size()) - 1; i > 0; --i)
	{
		const auto& currentMip = m_MipChain[i];
		const auto& nextMip = m_MipChain[i - 1];

		glViewport(0, 0, nextMip.intSize.x, nextMip.intSize.y);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, nextMip.texture, 0);

		glBindTexture(GL_TEXTURE_2D, currentMip.texture);

		Primitive::GetFullScreenQuad()->Draw(m_UpsampleShader);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_BLEND);
	m_UpsampleShader.Unbind();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}
