#pragma once

#include <span>

class VertexBuffer {
private:
	unsigned int m_RendererID = 0;
public:
	VertexBuffer(const void* data, unsigned int size);

	template<typename T>
	VertexBuffer(std::span<const T> data)
		: VertexBuffer(data.data(), static_cast<unsigned int>(data.size_bytes())) {}

	~VertexBuffer();

	void Bind() const;
	void Unbind() const;
};
