// asset_manager.cpp
#include "asset_manager.h"
#include <iostream>

AssetManager& AssetManager::Get()
{
	static AssetManager s_Instance;
	return s_Instance;
}

std::shared_ptr<Shader> AssetManager::LoadShader(const std::string& name, const std::string& filepath)
{
	auto it = m_Shaders.find(name);
	if (it != m_Shaders.end())
	{
		std::cout << "[AssetManager] Returning cached shader: " << name << std::endl;
		return it->second;
	}

	std::cout << "[AssetManager] Loading shader: " << name << " from " << filepath << std::endl;
	auto shader = std::make_shared<Shader>(filepath);
	m_Shaders[name] = shader;
	return shader;
}

std::shared_ptr<Shader> AssetManager::GetShader(const std::string& name)
{
	auto it = m_Shaders.find(name);
	if (it != m_Shaders.end())
		return it->second;

	std::cerr << "[AssetManager] Warning: Shader '" << name << "' not found in cache!" << std::endl;
	return nullptr;
}

bool AssetManager::HasShader(const std::string& name) const
{
	return m_Shaders.find(name) != m_Shaders.end();
}

std::shared_ptr<Model> AssetManager::LoadModel(const std::string& filepath, bool flipUVs)
{
	auto it = m_Models.find(filepath);
	if (it != m_Models.end())
	{
		std::cout << "[AssetManager] Returning cached model: " << filepath << std::endl;
		return it->second;
	}

	std::cout << "[AssetManager] Loading model from: " << filepath << std::endl;
	auto model = std::make_shared<Model>(filepath, false, flipUVs);
	m_Models[filepath] = model;
	return model;
}

std::shared_ptr<Model> AssetManager::GetModel(const std::string& filepath)
{
	auto it = m_Models.find(filepath);
	if (it != m_Models.end())
		return it->second;

	std::cerr << "[AssetManager] Warning: Model '" << filepath << "' not found in cache!" << std::endl;
	return nullptr;
}

bool AssetManager::HasModel(const std::string& filepath) const
{
	return m_Models.find(filepath) != m_Models.end();
}

void AssetManager::Clear()
{
	m_Shaders.clear();
	m_Models.clear();
}
