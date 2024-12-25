#pragma once
#include <GL/glew.h>
#include "vertex_buffer.h"
#include <vector>

struct VertexBufferLayoutElement
{
	unsigned int count;
	unsigned int type;
	unsigned char normalized;

	static unsigned int GetSizeOfType(unsigned int type)
	{
		switch (type)
		{
		case GL_FLOAT:			return 4;
		case GL_UNSIGNED_INT:	return 4;
		case GL_UNSIGNED_BYTE:	return 1;
		}
		return 0;
	};
};

class VertexBufferLayout
{
private:

	unsigned int m_Stride;
	std::vector<VertexBufferLayoutElement> m_Elements;
public:
	template<typename T>
	void Push(unsigned int count) {
		static_assert(sizeof(T) == 0, "Unsupported type passed to Push.");
	}

	template<>
	void Push<float>(unsigned int count) {
		m_Elements.push_back({ count, GL_FLOAT, GL_FALSE });
		m_Stride += count * VertexBufferLayoutElement::GetSizeOfType(GL_FLOAT);
	}

	template<>
	void Push<unsigned int>(unsigned int count) {
		m_Elements.push_back({ count, GL_UNSIGNED_INT, GL_FALSE });
		m_Stride += count * VertexBufferLayoutElement::GetSizeOfType(GL_UNSIGNED_INT);
	}

	template<>
	void Push<unsigned char>(unsigned int count) {
		m_Elements.push_back({ count, GL_UNSIGNED_BYTE, GL_TRUE });
		m_Stride += count * VertexBufferLayoutElement::GetSizeOfType(GL_UNSIGNED_BYTE);
	}

	inline unsigned int GetStride() const { return m_Stride; }
	inline auto GetElements() const { return m_Elements; }
};
