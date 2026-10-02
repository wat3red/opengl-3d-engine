// asset_manager.cpp
#include "asset_manager.h"
#include <print>

AssetManager& AssetManager::Get()
{
	static AssetManager s_Instance;
	return s_Instance;
}

std::shared_ptr<Shader> AssetManager::LoadShader(const std::string& name, const std::string& filepath)
{
	if (auto it = m_Shaders.find(name); it != m_Shaders.end())
	{
		std::println("[AssetManager] Returning cached shader: {}", name);
		return it->second;
	}

	std::println("[AssetManager] Loading shader: {} from {}", name, filepath);
	auto shader = std::make_shared<Shader>(filepath);
	m_Shaders[name] = shader;
	return shader;
}

std::shared_ptr<Shader> AssetManager::GetShader(const std::string& name)
{
	if (auto it = m_Shaders.find(name); it != m_Shaders.end())
		return it->second;

	std::println(stderr, "[AssetManager] Warning: Shader '{}' not found in cache!", name);
	return nullptr;
}

bool AssetManager::HasShader(const std::string& name) const
{
	return m_Shaders.contains(name);
}

std::shared_ptr<Model> AssetManager::LoadModel(const std::string& filepath, bool flipUVs)
{
	if (auto it = m_Models.find(filepath); it != m_Models.end())
	{
		std::println("[AssetManager] Returning cached model: {}", filepath);
		return it->second;
	}

	std::println("[AssetManager] Loading model from: {}", filepath);
	auto model = std::make_shared<Model>(filepath, false, flipUVs);
	m_Models[filepath] = model;
	return model;
}

std::shared_ptr<Model> AssetManager::GetModel(const std::string& filepath)
{
	if (auto it = m_Models.find(filepath); it != m_Models.end())
		return it->second;

	std::println(stderr, "[AssetManager] Warning: Model '{}' not found in cache!", filepath);
	return nullptr;
}

bool AssetManager::HasModel(const std::string& filepath) const
{
	return m_Models.contains(filepath);
}

std::shared_ptr<TextureCube> AssetManager::LoadCubemap(const std::string& name, const std::vector<std::string>& facePaths)
{
	if (auto it = m_Cubemaps.find(name); it != m_Cubemaps.end())
	{
		std::println("[AssetManager] Returning cached cubemap: {}", name);
		return it->second;
	}

	std::println("[AssetManager] Loading cubemap: {}", name);
	auto cubemap = std::make_shared<TextureCube>(facePaths);
	m_Cubemaps[name] = cubemap;
	return cubemap;
}

std::shared_ptr<TextureCube> AssetManager::GetCubemap(const std::string& name)
{
	if (auto it = m_Cubemaps.find(name); it != m_Cubemaps.end())
		return it->second;

	std::println(stderr, "[AssetManager] Warning: Cubemap '{}' not found in cache!", name);
	return nullptr;
}

bool AssetManager::HasCubemap(const std::string& name) const
{
	return m_Cubemaps.contains(name);
}

void AssetManager::Clear()
{
	m_Shaders.clear();
	m_Models.clear();
	m_Cubemaps.clear();
}
