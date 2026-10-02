#pragma once

#include <string>

class Texture2D {
private:
	unsigned int m_RendererID;
	std::string m_FilePath;
	unsigned char* m_LocalBuffer;
	int m_Width, m_Height, m_BPP;
public:
	Texture2D(const std::string& filePath);
	~Texture2D();

	void Bind(unsigned int slot = 0) const;
	void Unbind() const;

	[[nodiscard]] inline int GetWidth() const noexcept { return m_Width; }
	[[nodiscard]] inline int GetHeight() const noexcept { return m_Height; }
	[[nodiscard]] inline unsigned int GetRendererID() const noexcept { return m_RendererID; }
};
