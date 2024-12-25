#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include <std_image/stb_image.h>

#include "cubemap.h"
#include "vertex_array.h"
#include "index_buffer.h"
#include "vertex_buffer.h"
#include "shader.h"

// Function to generate cube indices
std::vector<unsigned int> generateCubeIndices() {
	std::vector<unsigned int> indices;

	// Generate indices for each face
	for (unsigned int i = 0; i < 36; ++i) {
		indices.push_back(i);
	}

	return indices;
}

float vertices[] = {
	// positions          
	-1.0f,  1.0f, -1.0f,
	-1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f, -1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,

	-1.0f, -1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f, -1.0f,  1.0f,
	-1.0f, -1.0f,  1.0f,

	-1.0f,  1.0f, -1.0f,
	 1.0f,  1.0f, -1.0f,
	 1.0f,  1.0f,  1.0f,
	 1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f,  1.0f,
	-1.0f,  1.0f, -1.0f,

	-1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	 1.0f, -1.0f, -1.0f,
	 1.0f, -1.0f, -1.0f,
	-1.0f, -1.0f,  1.0f,
	 1.0f, -1.0f,  1.0f
};

Cubemap::Cubemap(const std::vector<std::string>& facePathes) : m_RendererID(0), m_FacePathes()
{
	glGenTextures(1, &m_RendererID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(false);

	for (unsigned int i = 0; i < facePathes.size(); i++)
	{

		unsigned char* data = stbi_load(facePathes[i].c_str(), &width, &height, &nrChannels, 0);
		if (data)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
				0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
			);
			stbi_image_free(data);
		}
		else
		{
			std::cout << "Cubemap texture failed to load at path: " << facePathes[i] << std::endl;
			stbi_image_free(data);
		}
	}
	stbi_set_flip_vertically_on_load(true);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

Cubemap::~Cubemap()
{

}

void Cubemap::Render() const
{
	glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
	glDepthMask(GL_FALSE);

	// Create indices for the cube
	std::vector<unsigned int> indices = generateCubeIndices();
	IndexBuffer ib(indices.data(), indices.size());

	VertexBuffer vb(vertices, sizeof(vertices));
	VertexBufferLayout layout;
	layout.Push<float>(3);  // Position (x, y, z)

	VertexArray va;
	va.AddBuffer(vb, layout);
	va.Bind();

	glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
	glDrawArrays(GL_TRIANGLES, 0, 36);

	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS); // set depth function back to default
}

void Cubemap::Unbind() const
{

}

