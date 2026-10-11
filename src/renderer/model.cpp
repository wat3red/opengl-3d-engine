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
#include <filesystem>

namespace fs = std::filesystem;

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
	fs::path p(path);
	fs::path dir(directory);
	fs::path resolvedPath;

	std::error_code ec;
	if (fs::exists(p, ec) && !fs::is_directory(p, ec)) {
		resolvedPath = p;
	}
	else if (fs::exists(dir / p, ec) && !fs::is_directory(dir / p, ec)) {
		resolvedPath = dir / p;
	}
	else if (fs::exists(dir / p.filename(), ec) && !fs::is_directory(dir / p.filename(), ec)) {
		resolvedPath = dir / p.filename();
	}
	else if (fs::exists(dir / "textures" / p.filename(), ec) && !fs::is_directory(dir / "textures" / p.filename(), ec)) {
		resolvedPath = dir / "textures" / p.filename();
	}
	else if (fs::exists(dir / ".." / "textures" / p.filename(), ec) && !fs::is_directory(dir / ".." / "textures" / p.filename(), ec)) {
		resolvedPath = dir / ".." / "textures" / p.filename();
	}
	else if (fs::exists(dir / ".." / p.filename(), ec) && !fs::is_directory(dir / ".." / p.filename(), ec)) {
		resolvedPath = dir / ".." / p.filename();
	}
	else {
		resolvedPath = dir / p;
	}

	std::string filenameUtf8 = resolvedPath.string();
	std::println("Loading texture from file: {}", filenameUtf8);

	int width = 0, height = 0, nrComponents = 0;
	// Always request 4 channels (RGBA) so that 1-, 2-, 3-, and 4-channel textures all decode
	// to 4 bytes per pixel, preventing driver memory overreads.
	unsigned char* data = stbi_load(filenameUtf8.c_str(), &width, &height, &nrComponents, 4);
	if (!data || width <= 0 || height <= 0) {
		std::println(stderr, "Failed to load texture from file: {}", filenameUtf8);
		return 0;
	}

	unsigned int textureID = 0;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	GLenum internalFormat = gamma ? GL_SRGB8_ALPHA8 : GL_RGBA8;
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(data);
	std::println("Successfully loaded external texture with ID: {} ({}x{})", textureID, width, height);
	return textureID;
}

unsigned int TextureFromMemory(const aiTexture* texture, bool gamma = false) {
	if (!texture || !texture->pcData) {
		std::println(stderr, "Error: Invalid texture pointer or empty data");
		return 0;
	}

	int width = 0, height = 0, nrComponents = 0;
	unsigned char* data = nullptr;
	bool isStbiAllocated = false;

	if (texture->mHeight == 0) {
		// Compressed format (PNG, JPG, WebP, etc.).
		// Force desired_channels = 4 (RGBA) so that 1-channel, 2-channel (like shotgun image 4),
		// 3-channel, and 4-channel textures are all safely expanded to 4-byte RGBA pixel buffers.
		data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(texture->pcData),
		                             texture->mWidth, &width, &height, &nrComponents, 4);
		isStbiAllocated = (data != nullptr);
	}
	else {
		// Raw uncompressed pixels in Assimp aiTexel format (BGRA).
		width = texture->mWidth;
		height = texture->mHeight;
		data = reinterpret_cast<unsigned char*>(const_cast<aiTexel*>(texture->pcData));
		isStbiAllocated = false;
	}

	if (!data || width <= 0 || height <= 0) {
		std::println(stderr, "Failed to load embedded texture (hint: {})", texture->achFormatHint);
		return 0;
	}

	unsigned int textureID = 0;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	GLenum internalFormat = gamma ? GL_SRGB8_ALPHA8 : GL_RGBA8;
	GLenum dataFormat = (texture->mHeight == 0) ? GL_RGBA : GL_BGRA;

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (isStbiAllocated) {
		stbi_image_free(data);
	}

	std::println("Successfully loaded embedded texture with ID: {} ({}x{})", textureID, width, height);
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

void Model::Draw(Shader& shader, const MaterialProperties* materialProps) {
	for (unsigned int i = 0; i < meshes.size(); i++) {
		meshes[i].Draw(shader, materialProps);
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
		aiProcess_OptimizeMeshes |
		aiProcess_PreTransformVertices;

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

	// 1. Albedo / Base Color
	std::vector<Texture> albedoMaps = loadMaterialTextures(scene, material, aiTextureType_BASE_COLOR, "texture_albedo");
	if (albedoMaps.empty()) {
		albedoMaps = loadMaterialTextures(scene, material, aiTextureType_DIFFUSE, "texture_albedo");
	}

	// Sibling fallback if no albedo texture found
	if (albedoMaps.empty()) {
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
						if (otherStr.rfind(prefix, 0) == 0 && (otherMat->GetTextureCount(aiTextureType_BASE_COLOR) > 0 || otherMat->GetTextureCount(aiTextureType_DIFFUSE) > 0)) {
							std::println("[Model] Material '{}' has no albedo texture; falling back to sibling '{}'", nameStr, otherStr);
							albedoMaps = loadMaterialTextures(scene, otherMat, aiTextureType_BASE_COLOR, "texture_albedo");
							if (albedoMaps.empty()) {
								albedoMaps = loadMaterialTextures(scene, otherMat, aiTextureType_DIFFUSE, "texture_albedo");
							}
							break;
						}
					}
				}
			}
		}
	}

	// Fallback to directory scan for loose textures if model material has no texture bindings
	if (albedoMaps.empty()) {
		fs::path dir(directory);
		std::vector<fs::path> searchDirs = {
			dir / "textures",
			dir / ".." / "textures",
			dir,
			dir / ".."
		};

		static const std::vector<std::string> imageExtensions = {
			".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds", ".webp"
		};

		std::vector<fs::path> foundImages;
		for (const auto& searchDir : searchDirs) {
			std::error_code ec;
			if (fs::exists(searchDir, ec) && fs::is_directory(searchDir, ec)) {
				for (const auto& entry : fs::directory_iterator(searchDir, ec)) {
					if (entry.is_regular_file(ec)) {
						std::string ext = entry.path().extension().string();
						for (auto& c : ext) c = static_cast<char>(tolower(c));
						for (const auto& validExt : imageExtensions) {
							if (ext == validExt) {
								foundImages.push_back(entry.path());
								break;
							}
						}
					}
				}
				if (!foundImages.empty()) {
					break;
				}
			}
		}

		if (!foundImages.empty()) {
			fs::path bestAlbedo;
			for (const auto& imgPath : foundImages) {
				std::string stem = imgPath.stem().string();
				for (auto& c : stem) c = static_cast<char>(tolower(c));
				if (stem.find("albedo") != std::string::npos ||
				    stem.find("diffuse") != std::string::npos ||
				    stem.find("basecolor") != std::string::npos ||
				    stem.find("base_color") != std::string::npos ||
				    stem.find("color") != std::string::npos ||
				    stem.find("col") != std::string::npos ||
				    stem.find("sky") != std::string::npos ||
				    stem.find("landscape") != std::string::npos) {
					bestAlbedo = imgPath;
					break;
				}
			}

			if (bestAlbedo.empty()) {
				bestAlbedo = foundImages[0];
			}

			std::string albedoPathStr = bestAlbedo.string();
			bool alreadyLoaded = false;
			for (const auto& loadedTex : textures_loaded) {
				if (loadedTex.path == albedoPathStr) {
					albedoMaps.push_back(loadedTex);
					alreadyLoaded = true;
					break;
				}
			}

			if (!alreadyLoaded) {
				std::println("[Model] Auto-associated loose texture '{}' as albedo for mesh '{}'", albedoPathStr, mesh->mName.C_Str());
				Texture tex;
				tex.id = TextureFromFile(bestAlbedo.wstring(), dir.wstring());
				tex.type = "texture_albedo";
				tex.path = albedoPathStr;
				albedoMaps.push_back(tex);
				textures_loaded.push_back(tex);
			}
		}
	}

	// 2. Metallic / Roughness (glTF packs Roughness in G and Metallic in B)
	std::vector<Texture> mrMaps = loadMaterialTextures(scene, material, aiTextureType_DIFFUSE_ROUGHNESS, "texture_metallicRoughness");
	if (mrMaps.empty()) {
		mrMaps = loadMaterialTextures(scene, material, aiTextureType_METALNESS, "texture_metallicRoughness");
	}
	if (mrMaps.empty()) {
		mrMaps = loadMaterialTextures(scene, material, aiTextureType_UNKNOWN, "texture_metallicRoughness");
	}
	if (mrMaps.empty()) {
		mrMaps = loadMaterialTextures(scene, material, aiTextureType_SPECULAR, "texture_metallicRoughness");
	}

	// 3. Normal Map
	std::vector<Texture> normalMaps = loadMaterialTextures(scene, material, aiTextureType_NORMALS, "texture_normal");
	if (normalMaps.empty()) {
		normalMaps = loadMaterialTextures(scene, material, aiTextureType_HEIGHT, "texture_normal");
	}

	// 4. Ambient Occlusion Map
	std::vector<Texture> aoMaps = loadMaterialTextures(scene, material, aiTextureType_AMBIENT_OCCLUSION, "texture_ao");
	if (aoMaps.empty()) {
		aoMaps = loadMaterialTextures(scene, material, aiTextureType_LIGHTMAP, "texture_ao");
	}

	textures.insert(textures.end(), albedoMaps.begin(), albedoMaps.end());
	textures.insert(textures.end(), mrMaps.begin(), mrMaps.end());
	textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
	textures.insert(textures.end(), aoMaps.begin(), aoMaps.end());

	// 5. Material Factors
	MaterialFactors factors;
	aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
	if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS ||
	    material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS) {
		factors.baseColorFactor = glm::vec4(baseColor.r, baseColor.g, baseColor.b, baseColor.a);
	}

	float metallic = 1.0f;
	if (material->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) {
		factors.metallicFactor = metallic;
	}

	float roughness = 1.0f;
	if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
		factors.roughnessFactor = roughness;
	}

	return Mesh(vertices, indices, textures, factors);
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

			bool skip = false;
			for (unsigned int j = 0; j < textures_loaded.size(); j++) {
				if (textures_loaded[j].path == str.C_Str()) {
					textures.push_back(textures_loaded[j]);
					skip = true;
					break;
				}
			}

			if (!skip) {
				const aiTexture* texture = scene->GetEmbeddedTexture(str.C_Str());
				if (!texture && scene->mTextures && textureIndex >= 0 && (unsigned int)textureIndex < scene->mNumTextures) {
					texture = scene->mTextures[textureIndex];
				}

				if (texture) {
					std::println("Found embedded texture: {} (Index: {})", str.C_Str(), textureIndex);
					unsigned int texID = TextureFromMemory(texture, gammaCorrection);
					if (texID != 0) {
						Texture embeddedTexture;
						embeddedTexture.id = texID;
						embeddedTexture.type = typeName;
						embeddedTexture.path = str.C_Str();
						textures.push_back(embeddedTexture);
						textures_loaded.push_back(embeddedTexture);
					}
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
				unsigned int texID = TextureFromFile(texturePath, directory, gammaCorrection);
				if (texID != 0) {
					Texture texture;
					texture.id = texID;
					texture.type = typeName;
					texture.path = str.C_Str();
					textures.push_back(texture);
					textures_loaded.push_back(texture);
				}
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
