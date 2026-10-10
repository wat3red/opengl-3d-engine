#pragma once

#include <GL/glew.h>

enum class FramebufferTextureFormat {
	None = 0,
	RGBA8,
	RGBA16F,
	Depth24Stencil8
};

struct FramebufferSpecification {
	int Width = 0;
	int Height = 0;
	FramebufferTextureFormat ColorFormat = FramebufferTextureFormat::RGBA8;
	int Samples = 1;
	bool SwapChainTarget = false;
};

class Framebuffer {
public:
	Framebuffer(const FramebufferSpecification& specification);
	~Framebuffer();

	void Invalidate();
	void Resize(int width, int height);

	void Bind() const;
	void Unbind() const;

	unsigned int GetRendererID() const { return m_RendererID; }
	unsigned int GetColorAttachmentRendererID() const { return m_ColorAttachment; }
	const FramebufferSpecification& GetSpecification() const { return m_Specification; }

	int GetWidth() const { return m_Specification.Width; }
	int GetHeight() const { return m_Specification.Height; }

private:
	unsigned int m_RendererID = 0;
	unsigned int m_ColorAttachment = 0;
	unsigned int m_DepthAttachment = 0;
	FramebufferSpecification m_Specification;
};
