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

struct MaterialFactors {
	glm::vec4 baseColorFactor = glm::vec4(1.0f);
	float metallicFactor = 1.0f;
	float roughnessFactor = 1.0f;
	float aoStrength = 1.0f;
};

class Mesh {
public:
	vector<Vertex>       vertices;
	vector<unsigned int> indices;
	vector<Texture>      textures;
	MaterialFactors      factors;
	unsigned int VAO;

	Mesh(vector<Vertex> vertices, vector<unsigned int> indices, vector<Texture> textures, MaterialFactors factors = {})
		: vertices(std::move(vertices)), indices(std::move(indices)), textures(std::move(textures)), factors(factors)
	{
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
		factors = other.factors;
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
			factors = other.factors;
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
		bool isPBR = (glGetUniformLocation(shader.RendererID(), "normalMap") != -1);
		bool hasAlbedoUniform = (glGetUniformLocation(shader.RendererID(), "albedoMap") != -1);

		if (isPBR || hasAlbedoUniform)
		{
			static unsigned int defaultAlbedo = 0;
			static unsigned int defaultMR = 0;
			static unsigned int defaultNormal = 0;
			static unsigned int defaultAO = 0;

			if (defaultAlbedo == 0)
			{
				unsigned int white = 0xFFFFFFFF;
				glGenTextures(1, &defaultAlbedo);
				glBindTexture(GL_TEXTURE_2D, defaultAlbedo);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

				unsigned char mrData[4] = { 255, 128, 0, 255 }; // R=unused, G=roughness(0.5), B=metallic(0.0)
				glGenTextures(1, &defaultMR);
				glBindTexture(GL_TEXTURE_2D, defaultMR);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, mrData);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

				unsigned char normalData[4] = { 128, 128, 255, 255 }; // Flat tangent space normal (0, 0, 1)
				glGenTextures(1, &defaultNormal);
				glBindTexture(GL_TEXTURE_2D, defaultNormal);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, normalData);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

				unsigned int aoWhite = 0xFFFFFFFF;
				glGenTextures(1, &defaultAO);
				glBindTexture(GL_TEXTURE_2D, defaultAO);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &aoWhite);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}

			// Bind default albedo to slot 0
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, defaultAlbedo);

			if (isPBR)
			{
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, defaultMR);
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, defaultNormal);
				glActiveTexture(GL_TEXTURE3);
				glBindTexture(GL_TEXTURE_2D, defaultAO);
			}

			int hasAlbedo = 0;
			int hasNormal = 0;

			for (const auto& tex : textures)
			{
				if ((tex.type == "texture_albedo" || tex.type == "material.diffuse") && !hasAlbedo)
				{
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, tex.id);
					hasAlbedo = 1;
				}
				else if (isPBR && (tex.type == "texture_metallicRoughness" || tex.type == "material.specular"))
				{
					glActiveTexture(GL_TEXTURE1);
					glBindTexture(GL_TEXTURE_2D, tex.id);
				}
				else if (isPBR && (tex.type == "texture_normal" || tex.type == "material.normal") && !hasNormal)
				{
					glActiveTexture(GL_TEXTURE2);
					glBindTexture(GL_TEXTURE_2D, tex.id);
					hasNormal = 1;
				}
				else if (isPBR && (tex.type == "texture_ao" || tex.type == "material.ambient"))
				{
					glActiveTexture(GL_TEXTURE3);
					glBindTexture(GL_TEXTURE_2D, tex.id);
				}
			}

			shader.SetUniform1i("albedoMap", 0);

			if (isPBR)
			{
				shader.SetUniform1i("metallicRoughnessMap", 1);
				shader.SetUniform1i("normalMap", 2);
				shader.SetUniform1i("aoMap", 3);
				shader.SetUniform1i("hasNormalMap", hasNormal);

				shader.SetUniform4f("baseColorFactor", factors.baseColorFactor);
				shader.SetUniform1f("metallicFactor", factors.metallicFactor);
				shader.SetUniform1f("roughnessFactor", factors.roughnessFactor);
				shader.SetUniform1f("aoStrength", factors.aoStrength);
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