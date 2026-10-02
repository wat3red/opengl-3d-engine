#pragma once

#include <span>

class IndexBuffer {
private:
	unsigned int m_RendererID = 0;
	unsigned int m_Count = 0;
public:
	IndexBuffer(const unsigned int* data, unsigned int count);
	IndexBuffer(std::span<const unsigned int> indices);
	~IndexBuffer();

	void Bind() const;
	void Unbind() const;

	[[nodiscard]] inline unsigned int GetCount() const noexcept { return m_Count; }
};
