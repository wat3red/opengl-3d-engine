#pragma once
#include <GL/glew.h>
#include "vertex_buffer.h"
#include <vector>
#include <concepts>

struct VertexBufferLayoutElement
{
	unsigned int count = 0;
	unsigned int type = 0;
	unsigned char normalized = GL_FALSE;

	[[nodiscard]] static constexpr unsigned int GetSizeOfType(unsigned int type) noexcept
	{
		switch (type)
		{
		case GL_FLOAT:			return 4;
		case GL_UNSIGNED_INT:	return 4;
		case GL_UNSIGNED_BYTE:	return 1;
		default:				return 0;
		}
	}
};

template<typename T>
concept VertexLayoutSupportedType = std::same_as<T, float> || std::same_as<T, unsigned int> || std::same_as<T, unsigned char>;

class VertexBufferLayout
{
private:
	unsigned int m_Stride = 0;
	std::vector<VertexBufferLayoutElement> m_Elements;
public:
	template<typename T>
	void Push(unsigned int count) {
		static_assert(VertexLayoutSupportedType<T>, "Unsupported type passed to VertexBufferLayout::Push.");
	}

	template<>
	void Push<float>(unsigned int count) {
		m_Elements.push_back({ .count = count, .type = GL_FLOAT, .normalized = GL_FALSE });
		m_Stride += count * VertexBufferLayoutElement::GetSizeOfType(GL_FLOAT);
	}

	template<>
	void Push<unsigned int>(unsigned int count) {
		m_Elements.push_back({ .count = count, .type = GL_UNSIGNED_INT, .normalized = GL_FALSE });
		m_Stride += count * VertexBufferLayoutElement::GetSizeOfType(GL_UNSIGNED_INT);
	}

	template<>
	void Push<unsigned char>(unsigned int count) {
		m_Elements.push_back({ .count = count, .type = GL_UNSIGNED_BYTE, .normalized = GL_TRUE });
		m_Stride += count * VertexBufferLayoutElement::GetSizeOfType(GL_UNSIGNED_BYTE);
	}

	[[nodiscard]] inline unsigned int GetStride() const noexcept { return m_Stride; }
	[[nodiscard]] inline const std::vector<VertexBufferLayoutElement>& GetElements() const noexcept { return m_Elements; }
};
