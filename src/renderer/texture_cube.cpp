#include "texture_cube.h"
#include <GL/glew.h>
#include <std_image/stb_image.h>
#include <print>
#include <utility>

TextureCube::TextureCube(const std::vector<std::string>& facePaths)
	: m_RendererID(0)
{
	if (facePaths.size() < 6)
	{
		std::println(stderr, "ERROR::TEXTURE_CUBE:: Exactly 6 face paths are required to construct a cubemap, got {}",
		             facePaths.size());
		return;
	}

	glGenTextures(1, &m_RendererID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

	// Cubemaps in OpenGL standard layout should not be flipped vertically
	stbi_set_flip_vertically_on_load(false);

	int width, height, channels;
	for (unsigned int i = 0; i < 6; ++i)
	{
		unsigned char* data = stbi_load(facePaths[i].c_str(), &width, &height, &channels, 0);
		if (data)
		{
			GLenum format = GL_RGB;
			if (channels == 1)
				format = GL_RED;
			else if (channels == 3)
				format = GL_RGB;
			else if (channels == 4)
				format = GL_RGBA;

			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

			stbi_image_free(data);
		}
		else
		{
			std::println(stderr, "ERROR::TEXTURE_CUBE:: Failed to load cubemap face: {}", facePaths[i]);
		}
	}

	stbi_set_flip_vertically_on_load(true);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

TextureCube::~TextureCube()
{
	if (m_RendererID != 0)
	{
		glDeleteTextures(1, &m_RendererID);
		m_RendererID = 0;
	}
}

TextureCube::TextureCube(TextureCube&& other) noexcept
	: m_RendererID(std::exchange(other.m_RendererID, 0))
{
}

TextureCube& TextureCube::operator=(TextureCube&& other) noexcept
{
	if (this != &other)
	{
		if (m_RendererID != 0)
		{
			glDeleteTextures(1, &m_RendererID);
		}
		m_RendererID = std::exchange(other.m_RendererID, 0);
	}
	return *this;
}

void TextureCube::Bind(unsigned int slot) const
{
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
}

void TextureCube::Unbind() const
{
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}
