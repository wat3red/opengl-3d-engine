// asset_manager.h
#pragma once

#include "shader.h"
#include "model.h"
#include <string>
#include <memory>
#include <unordered_map>

class AssetManager {
public:
	static AssetManager& Get();

	std::shared_ptr<Shader> LoadShader(const std::string& name, const std::string& filepath);
	std::shared_ptr<Shader> GetShader(const std::string& name);
	bool HasShader(const std::string& name) const;

	std::shared_ptr<Model> LoadModel(const std::string& filepath, bool flipUVs = false);
	std::shared_ptr<Model> GetModel(const std::string& filepath);
	bool HasModel(const std::string& filepath) const;

	void Clear();

private:
	AssetManager() = default;
	~AssetManager() = default;

	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;

	std::unordered_map<std::string, std::shared_ptr<Shader>> m_Shaders;
	std::unordered_map<std::string, std::shared_ptr<Model>> m_Models;
};
