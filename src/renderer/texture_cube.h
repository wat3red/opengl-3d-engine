// texture_cube.h
#pragma once

#include <string>
#include <vector>

class TextureCube {
public:
	TextureCube() = default;
	TextureCube(const std::vector<std::string>& facePaths);
	~TextureCube();

	// Move-only semantics (Rule of 5)
	TextureCube(const TextureCube&) = delete;
	TextureCube& operator=(const TextureCube&) = delete;
	TextureCube(TextureCube&& other) noexcept;
	TextureCube& operator=(TextureCube&& other) noexcept;

	void Bind(unsigned int slot = 0) const;
	void Unbind() const;

	[[nodiscard]] bool IsLoaded() const noexcept { return m_RendererID != 0; }
	[[nodiscard]] unsigned int GetRendererID() const noexcept { return m_RendererID; }

private:
	unsigned int m_RendererID = 0;
};
