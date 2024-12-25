#pragma once

#include "mesh.h"

#include <assimp/scene.h>

class Model {
public:
	Model(const std::string& path, bool gamma = false, bool flipUVs = false);

	struct AABB {
		glm::vec3 min;
		glm::vec3 max;
	};

	struct OBB {
		glm::vec3 center;
		glm::vec3 extents;
		glm::mat3 orientation;
	};

	// Model data
	std::vector<Texture> textures_loaded; // Stores all loaded textures to prevent duplication
	std::wstring directory; // Changed to std::wstring for Unicode support
	bool gammaCorrection;

	// Get bounding volumes
	const AABB& GetAABB() const { return aabb; }
	const OBB& GetOBB() const { return obb; }

	void Draw(Shader& shader);
private:
	// Loads a model with supported ASSIMP extensions from file
	void loadModel(const std::string& path, bool flipUVs = false);
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	std::vector<Texture> loadMaterialTextures(const aiScene* scene, aiMaterial* mat, aiTextureType type, const std::string& typeName);

	void CalculateBoundingVolumes();

	std::vector<Mesh> meshes;
	AABB aabb;
	OBB obb;
};
