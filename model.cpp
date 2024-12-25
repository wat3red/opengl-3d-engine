#pragma once

#include "mesh.h"
#include "model.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <std_image/stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <codecvt>

// Helper function to convert std::string to std::wstring
std::wstring stringToWString(const std::string& str) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.from_bytes(str);
}

// Helper function to convert std::wstring to UTF-8 std::string
std::string wstringToUtf8(const std::wstring& wstr) {
	std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
	return converter.to_bytes(wstr);
}

// Function to load textures from file (external) with Unicode path support
unsigned int TextureFromFile(const std::wstring& path, const std::wstring& directory, bool gamma = false) {
	std::wstring filename = directory + L'/' + path;
	std::cout << "Loading texture from file: " << wstringToUtf8(filename) << std::endl;

	std::string filenameUtf8 = wstringToUtf8(filename);

	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char* data = stbi_load(filenameUtf8.c_str(), &width, &height, &nrComponents, 0);
	if (data) {
		GLenum format = (nrComponents == 3) ? GL_RGB : GL_RGBA;
		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
		std::cout << "Successfully loaded external texture with ID: " << textureID << std::endl;
	}
	else {
		std::cerr << "Failed to load texture from file: " << filenameUtf8 << std::endl;
		stbi_image_free(data);
	}

	return textureID;
}

// Function to load textures from memory
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

		std::cout << "Successfully loaded embedded texture with ID: " << textureID << std::endl;
	}
	else
		std::cerr << "Failed to load embedded texture" << std::endl;


	return textureID;
}


Model::Model(const std::string& path, bool gamma, bool flipUVs) : gammaCorrection(gamma) {
		std::cout << "Loading model: " << path << std::endl;
		loadModel(path, flipUVs);
		CalculateBoundingVolumes();
}

void Model::Draw(Shader& shader) {
	for (unsigned int i = 0; i < meshes.size(); i++) {
		meshes[i].Draw(shader);
	}
}

// Loads a model with supported ASSIMP extensions from file
void Model::loadModel(const std::string& path, bool flipUVs) {
	Assimp::Importer importer;
	std::cout << "Starting Assimp import..." << std::endl;

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
		std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
		return;
	}

	// Convert directory to wstring for Unicode compatibility
	directory = stringToWString(path.substr(0, path.find_last_of('/')));
	std::cout << "Model directory set to: " << wstringToUtf8(directory) << std::endl;

	std::cout << "Processing root node..." << std::endl;
	processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene) {
	std::cout << "Processing node: " << node->mName.C_Str() << std::endl;
	std::cout << "Number of meshes in this node: " << node->mNumMeshes << std::endl;
	std::cout << "Number of child nodes: " << node->mNumChildren << std::endl;

	for (unsigned int i = 0; i < node->mNumMeshes; i++) {
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(processMesh(mesh, scene));
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++) {
		processNode(node->mChildren[i], scene);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
	//std::cout << "Processing mesh: " << mesh->mName.C_Str() << std::endl;
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<Texture> textures;

	// Process vertices
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

	// Process indices
	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);

	}

	// Process material
	aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
	std::vector<Texture> diffuseMaps = loadMaterialTextures(scene, material, aiTextureType_DIFFUSE, "material.diffuse");
	std::vector<Texture> specularMaps = loadMaterialTextures(scene, material, aiTextureType_SPECULAR, "material.specular");

	textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
	textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

	return Mesh(vertices, indices, textures);
}

std::vector<Texture> Model::loadMaterialTextures(const aiScene* scene, aiMaterial* mat, aiTextureType type, const std::string& typeName) {
	std::vector<Texture> textures;
	unsigned int textureCount = mat->GetTextureCount(type);
	std::cout << "Loading " << textureCount << " " << typeName << " textures." << std::endl;

	for (unsigned int i = 0; i < textureCount; i++) {
		aiString str;
		mat->GetTexture(type, i, &str);

		if (str.C_Str()[0] == '*') {
			int textureIndex = atoi(str.C_Str() + 1);
			if (!scene->mTextures) continue;
			const aiTexture* texture = scene->mTextures[textureIndex];
			if (texture) {
				std::cout << "Found embedded texture: " << str.C_Str() << " (Index: " << textureIndex << ")" << std::endl;
				Texture embeddedTexture;
				embeddedTexture.id = TextureFromMemory(texture, gammaCorrection);
				embeddedTexture.type = typeName;
				embeddedTexture.path = str.C_Str();
				textures.push_back(embeddedTexture);
				textures_loaded.push_back(embeddedTexture);
			}
			else
				std::cerr << "Error: Embedded texture " << str.C_Str() << " not found." << std::endl;

		}
		else {
			// Convert the path to std::wstring and use the new TextureFromFile
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
				std::cout << "Loading external texture: " << str.C_Str() << std::endl;
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

	// Calculate AABB by finding min and max vertices across all meshes
	for (const auto& mesh : meshes) {
		for (const auto& vertex : mesh.vertices) {
			minCoords = glm::min(minCoords, vertex.Position);
			maxCoords = glm::max(maxCoords, vertex.Position);
		}
	}

	// Define AABB min and max from computed extents
	aabb.min = minCoords;
	aabb.max = maxCoords;

	// Define OBB based on the AABB, center it, and set orientation to identity
	obb.center = (aabb.min + aabb.max) * 0.5f;
	obb.extents = (aabb.max - aabb.min) * 0.5f;
	obb.orientation = glm::mat3(1.0f);  // Identity matrix for orientation
}
