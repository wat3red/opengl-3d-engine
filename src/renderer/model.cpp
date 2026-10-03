#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <print>

#include "mesh.h"
#include "model.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <std_image/stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

std::wstring stringToWString(const std::string& str) {
	if (str.empty()) return std::wstring();
	int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
	std::wstring result(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &result[0], size);
	return result;
}

std::string wstringToUtf8(const std::wstring& wstr) {
	if (wstr.empty()) return std::string();
	int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &result[0], size, nullptr, nullptr);
	return result;
}

unsigned int TextureFromFile(const std::wstring& path, const std::wstring& directory, bool gamma = false) {
	std::wstring filename = directory + L'/' + path;
	std::println("Loading texture from file: {}", wstringToUtf8(filename));

	std::string filenameUtf8 = wstringToUtf8(filename);

	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char* data = stbi_load(filenameUtf8.c_str(), &width, &height, &nrComponents, 0);
	if (data) {
		GLenum format = GL_RGBA;
		if (nrComponents == 1) format = GL_RED;
		else if (nrComponents == 3) format = GL_RGB;
		else if (nrComponents == 4) format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
		std::println("Successfully loaded external texture with ID: {}", textureID);
	}
	else {
		std::println(stderr, "Failed to load texture from file: {}", filenameUtf8);
		stbi_image_free(data);
	}

	return textureID;
}

unsigned int TextureFromMemory(const aiTexture* texture, bool gamma = false) {
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char* data = nullptr;

	if (texture->mHeight == 0)
		data = stbi_load_from_memory((unsigned char*)texture->pcData, texture->mWidth, &width, &height, &nrComponents, 0);

	else {
		data = (unsigned char*)texture->pcData;
		width = texture->mWidth;
		height = texture->mHeight;
		nrComponents = 4;
	}

	if (data) {
		GLenum format = (nrComponents == 3) ? GL_RGB : GL_RGBA;
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (texture->mHeight == 0)
			stbi_image_free(data);

		std::println("Successfully loaded embedded texture with ID: {}", textureID);
	}
	else
		std::println(stderr, "Failed to load embedded texture");


	return textureID;
}


Model::Model(const std::string& path, bool gamma, bool flipUVs) : gammaCorrection(gamma) {
		std::println("Loading model: {}", path);
		loadModel(path, flipUVs);
		CalculateBoundingVolumes();
}

Model::~Model() {
	for (const auto& tex : textures_loaded) {
		glDeleteTextures(1, &tex.id);
	}
}

void Model::Draw(Shader& shader) {
	for (unsigned int i = 0; i < meshes.size(); i++) {
		meshes[i].Draw(shader);
	}
}

void Model::loadModel(const std::string& path, bool flipUVs) {
	Assimp::Importer importer;
	std::println("Starting Assimp import...");

	int sceneParams = aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_JoinIdenticalVertices |
		aiProcess_GlobalScale |
		aiProcess_TransformUVCoords |
		aiProcess_OptimizeGraph |
		aiProcess_OptimizeMeshes;

	const aiScene* scene = importer.ReadFile(path,
		flipUVs ? sceneParams | aiProcess_FlipUVs : sceneParams
	);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
		std::println(stderr, "ERROR::ASSIMP::{}", importer.GetErrorString());
		return;
	}

	directory = stringToWString(path.substr(0, path.find_last_of('/')));
	std::println("Model directory set to: {}", wstringToUtf8(directory));

	std::println("Processing root node...");
	processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene) {
	std::println("Processing node: {}", node->mName.C_Str());
	std::println("Number of meshes in this node: {}", node->mNumMeshes);
	std::println("Number of child nodes: {}", node->mNumChildren);

	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(processMesh(mesh, scene));
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<Texture> textures;

	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		Vertex vertex;
		vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		vertex.Normal = mesh->HasNormals() ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z) : glm::vec3(0.0f);

		if (mesh->mTextureCoords[0])
			vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		else
			vertex.TexCoords = glm::vec2(0.0f);


		if (mesh->HasTangentsAndBitangents()) {
			vertex.Tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
			vertex.Bitangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
		}
		vertices.push_back(vertex);
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);

	}

	aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
	std::vector<Texture> diffuseMaps = loadMaterialTextures(scene, material, aiTextureType_DIFFUSE, "material.diffuse");

	// If no diffuse texture assigned to this material, search for sibling materials with the same prefix (e.g. Floor001 -> Floor002)
	if (diffuseMaps.empty()) {
		aiString matName;
		if (material->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
			std::string nameStr = matName.C_Str();
			size_t lastUnderscore = nameStr.rfind('_');
			if (lastUnderscore != std::string::npos && lastUnderscore > 0) {
				std::string prefix = nameStr.substr(0, lastUnderscore);
				for (unsigned int m = 0; m < scene->mNumMaterials; ++m) {
					if (m == mesh->mMaterialIndex) continue;
					aiMaterial* otherMat = scene->mMaterials[m];
					aiString otherName;
					if (otherMat->Get(AI_MATKEY_NAME, otherName) == AI_SUCCESS) {
						std::string otherStr = otherName.C_Str();
						if (otherStr.rfind(prefix, 0) == 0 && otherMat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
							std::println("[Model] Material '{}' has no diffuse texture; falling back to sibling '{}'", nameStr, otherStr);
							diffuseMaps = loadMaterialTextures(scene, otherMat, aiTextureType_DIFFUSE, "material.diffuse");
							break;
						}
					}
				}
			}
		}
	}

	std::vector<Texture> specularMaps = loadMaterialTextures(scene, material, aiTextureType_SPECULAR, "material.specular");

	textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
	textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

	return Mesh(vertices, indices, textures);
}

std::vector<Texture> Model::loadMaterialTextures(const aiScene* scene, aiMaterial* mat, aiTextureType type, const std::string& typeName) {
	std::vector<Texture> textures;
	unsigned int textureCount = mat->GetTextureCount(type);
	std::println("Loading {} {} textures.", textureCount, typeName);

	for (unsigned int i = 0; i < textureCount; i++) {
		aiString str;
		mat->GetTexture(type, i, &str);

		if (str.C_Str()[0] == '*') {
			int textureIndex = atoi(str.C_Str() + 1);
			if (!scene->mTextures) continue;

			bool skip = false;
			for (unsigned int j = 0; j < textures_loaded.size(); j++) {
				if (textures_loaded[j].path == str.C_Str()) {
					textures.push_back(textures_loaded[j]);
					skip = true;
					break;
				}
			}

			if (!skip) {
				const aiTexture* texture = scene->mTextures[textureIndex];
				if (texture) {
					std::println("Found embedded texture: {} (Index: {})", str.C_Str(), textureIndex);
					Texture embeddedTexture;
					embeddedTexture.id = TextureFromMemory(texture, gammaCorrection);
					embeddedTexture.type = typeName;
					embeddedTexture.path = str.C_Str();
					textures.push_back(embeddedTexture);
					textures_loaded.push_back(embeddedTexture);
				}
				else
					std::println(stderr, "Error: Embedded texture {} not found.", str.C_Str());
			}
		}
		else {
			std::wstring texturePath = stringToWString(str.C_Str());
			bool skip = false;

			for (unsigned int j = 0; j < textures_loaded.size(); j++) {
				if (textures_loaded[j].path == str.C_Str()) {
					textures.push_back(textures_loaded[j]);
					skip = true;
					break;
				}
			}

			if (!skip) {
				std::println("Loading external texture: {}", str.C_Str());
				Texture texture;
				texture.id = TextureFromFile(texturePath, directory);
				texture.type = typeName;
				texture.path = str.C_Str();
				textures.push_back(texture);
				textures_loaded.push_back(texture);
			}
		}
	}
	return textures;
}

void Model::CalculateBoundingVolumes()
{
	glm::vec3 minCoords(std::numeric_limits<float>::max());
	glm::vec3 maxCoords(std::numeric_limits<float>::lowest());

	for (const auto& mesh : meshes) {
		for (const auto& vertex : mesh.vertices) {
			minCoords = glm::min(minCoords, vertex.Position);
			maxCoords = glm::max(maxCoords, vertex.Position);
		}
	}

	aabb.min = minCoords;
	aabb.max = maxCoords;

	obb.center = (aabb.min + aabb.max) * 0.5f;
	obb.extents = (aabb.max - aabb.min) * 0.5f;
	obb.orientation = glm::mat3(1.0f);
}
