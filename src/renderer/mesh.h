#pragma once
#ifndef MESH_H
#define MESH_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "shader.h"

#include <string>
#include <vector>
using namespace std;

#define MAX_BONE_INFLUENCE 4

struct Vertex {
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TexCoords;
	glm::vec3 Tangent;
	glm::vec3 Bitangent;
	int m_BoneIDs[MAX_BONE_INFLUENCE];
	float m_Weights[MAX_BONE_INFLUENCE];
};

struct Texture {
	unsigned int id;
	string type;
	string path;
};

class Mesh {
public:
	vector<Vertex>       vertices;
	vector<unsigned int> indices;
	vector<Texture>      textures;
	unsigned int VAO;

	Mesh(vector<Vertex> vertices, vector<unsigned int> indices, vector<Texture> textures)
	{
		this->vertices = vertices;
		this->indices = indices;
		this->textures = textures;

		setupMesh();
	}

	~Mesh()
	{
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
	}

	Mesh(Mesh&& other) noexcept {
		vertices = std::move(other.vertices);
		indices = std::move(other.indices);
		textures = std::move(other.textures);
		VAO = other.VAO;
		VBO = other.VBO;
		EBO = other.EBO;
		other.VAO = 0;
		other.VBO = 0;
		other.EBO = 0;
	}

	Mesh& operator=(Mesh&& other) noexcept {
		if (this != &other) {
			vertices = std::move(other.vertices);
			indices = std::move(other.indices);
			textures = std::move(other.textures);
			VAO = other.VAO;
			VBO = other.VBO;
			EBO = other.EBO;
			other.VAO = 0;
			other.VBO = 0;
			other.EBO = 0;
		}
		return *this;
	}

	Mesh(const Mesh&) = delete;

	void Draw(Shader& shader)
	{
		// Bind appropriate textures only if the shader uses material uniforms (skip for shadow/depth passes)
		if (glGetUniformLocation(shader.RendererID(), "material.diffuse") != -1)
		{
			unsigned int diffuseNr = 1;
			unsigned int specularNr = 1;
			unsigned int normalNr = 1;
			unsigned int heightNr = 1;
			unsigned int shadowNr = 1;
			if (textures.empty())
			{
				static unsigned int defaultGreyTex = 0;
				if (defaultGreyTex == 0)
				{
					unsigned int grey = 0xFFB4B4B4;
					glGenTextures(1, &defaultGreyTex);
					glBindTexture(GL_TEXTURE_2D, defaultGreyTex);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &grey);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				}
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, defaultGreyTex);
				shader.SetUniform1i("material.diffuse", 0);
				if (glGetUniformLocation(shader.RendererID(), "material.specular") != -1)
					shader.SetUniform1i("material.specular", 0);
			}
			else
			{
				for (unsigned int i = 0; i < textures.size(); i++)
				{
					glActiveTexture(GL_TEXTURE0 + i);
					string name = textures[i].type;
					if (name == "material.diffuse")
					{
						if (diffuseNr == 1)
							shader.SetUniform1i("material.diffuse", i);
						else
							shader.SetUniform1i("material.diffuse" + std::to_string(diffuseNr), i);
						diffuseNr++;
					}
					else if (name == "material.specular")
					{
						if (specularNr == 1)
							shader.SetUniform1i("material.specular", i);
						else
							shader.SetUniform1i("material.specular" + std::to_string(specularNr), i);
						specularNr++;
					}
					else if (name == "material.normal")
						shader.SetUniform1i(name + std::to_string(normalNr++), i);
					else if (name == "material.height")
						shader.SetUniform1i(name + std::to_string(heightNr++), i);
					else if (name == "material.shadow")
						shader.SetUniform1i(name + std::to_string(shadowNr++), i);

					glBindTexture(GL_TEXTURE_2D, textures[i].id);
				}
			}
		}

		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		glActiveTexture(GL_TEXTURE0);
	}

private:
	unsigned int VBO, EBO;

	void setupMesh()
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));

		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));

		glEnableVertexAttribArray(5);
		glVertexAttribIPointer(5, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));

		glEnableVertexAttribArray(6);
		glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));
		glBindVertexArray(0);
	}
};
#endif