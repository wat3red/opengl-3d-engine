#pragma once

#include "shader.h"
#include "model.h"
#include "texture_cube.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

class AssetManager {
public:
	static AssetManager& Get();

	std::shared_ptr<Shader> LoadShader(const std::string& name, const std::string& filepath);
	[[nodiscard]] std::shared_ptr<Shader> GetShader(const std::string& name);
	[[nodiscard]] bool HasShader(const std::string& name) const;

	std::shared_ptr<Model> LoadModel(const std::string& filepath, bool flipUVs = false);
	[[nodiscard]] std::shared_ptr<Model> GetModel(const std::string& filepath);
	[[nodiscard]] bool HasModel(const std::string& filepath) const;

	std::shared_ptr<TextureCube> LoadCubemap(const std::string& name, const std::vector<std::string>& facePaths);
	[[nodiscard]] std::shared_ptr<TextureCube> GetCubemap(const std::string& name);
	[[nodiscard]] bool HasCubemap(const std::string& name) const;

	void Clear();

private:
	AssetManager() = default;
	~AssetManager() = default;

	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;

	std::unordered_map<std::string, std::shared_ptr<Shader>> m_Shaders;
	std::unordered_map<std::string, std::shared_ptr<Model>> m_Models;
	std::unordered_map<std::string, std::shared_ptr<TextureCube>> m_Cubemaps;
};
