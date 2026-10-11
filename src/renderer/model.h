#pragma once

#include "mesh.h"

#include <assimp/scene.h>

class Model {
public:
	Model(const std::string& path, bool gamma = false, bool flipUVs = false);
	~Model();

	struct AABB {
		glm::vec3 min;
		glm::vec3 max;
	};

	struct OBB {
		glm::vec3 center;
		glm::vec3 extents;
		glm::mat3 orientation;
	};

	std::vector<Texture> textures_loaded;
	std::wstring directory;
	bool gammaCorrection;

	const AABB& GetAABB() const { return aabb; }
	const OBB& GetOBB() const { return obb; }
	const std::vector<Mesh>& GetMeshes() const { return meshes; }
	std::vector<Mesh>& GetMeshes() { return meshes; }
	size_t GetMeshCount() const { return meshes.size(); }

	void Draw(Shader& shader, const MaterialProperties* materialProps = nullptr);
private:
	void loadModel(const std::string& path, bool flipUVs = false);
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene);
	std::vector<Texture> loadMaterialTextures(const aiScene* scene, aiMaterial* mat, aiTextureType type, const std::string& typeName);

	void CalculateBoundingVolumes();

	std::vector<Mesh> meshes;
	AABB aabb;
	OBB obb;
};
