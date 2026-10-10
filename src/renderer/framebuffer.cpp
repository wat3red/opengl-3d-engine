#include "framebuffer.h"
#include <print>

Framebuffer::Framebuffer(const FramebufferSpecification& specification)
	: m_Specification(specification)
{
	Invalidate();
}

Framebuffer::~Framebuffer()
{
	if (m_RendererID != 0)
	{
		glDeleteFramebuffers(1, &m_RendererID);
		glDeleteTextures(1, &m_ColorAttachment);
		glDeleteRenderbuffers(1, &m_DepthAttachment);
	}
}

void Framebuffer::Invalidate()
{
	if (m_RendererID != 0)
	{
		glDeleteFramebuffers(1, &m_RendererID);
		glDeleteTextures(1, &m_ColorAttachment);
		glDeleteRenderbuffers(1, &m_DepthAttachment);
		m_RendererID = 0;
		m_ColorAttachment = 0;
		m_DepthAttachment = 0;
	}

	if (m_Specification.Width <= 0 || m_Specification.Height <= 0)
		return;

	glGenFramebuffers(1, &m_RendererID);
	glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

	glGenTextures(1, &m_ColorAttachment);
	glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);

	GLenum internalFormat = GL_RGBA8;
	GLenum type = GL_UNSIGNED_BYTE;
	if (m_Specification.ColorFormat == FramebufferTextureFormat::RGBA16F)
	{
		internalFormat = GL_RGBA16F;
		type = GL_FLOAT;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Specification.Width, m_Specification.Height, 0, GL_RGBA, type, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

	glGenRenderbuffers(1, &m_DepthAttachment);
	glBindRenderbuffer(GL_RENDERBUFFER, m_DepthAttachment);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_Specification.Width, m_Specification.Height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_DepthAttachment);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::println(stderr, "ERROR::FRAMEBUFFER:: Framebuffer is not complete!");
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Resize(int width, int height)
{
	if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
		return;

	if (m_Specification.Width == width && m_Specification.Height == height)
		return;

	m_Specification.Width = width;
	m_Specification.Height = height;
	Invalidate();
}

void Framebuffer::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
	glViewport(0, 0, m_Specification.Width, m_Specification.Height);
}

void Framebuffer::Unbind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
