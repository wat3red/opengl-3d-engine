#include "material.h"
#include "asset_manager.h"
#include "texture.h"

void MaterialProperties::LoadTextures()
{
	auto& assetManager = AssetManager::Get();

	if (!AlbedoMapPath.empty())
		AlbedoMap = assetManager.LoadTexture(AlbedoMapPath);
	else
		AlbedoMap = nullptr;

	if (!NormalMapPath.empty())
		NormalMap = assetManager.LoadTexture(NormalMapPath);
	else
		NormalMap = nullptr;

	if (!MetallicRoughnessMapPath.empty())
		MetallicRoughnessMap = assetManager.LoadTexture(MetallicRoughnessMapPath);
	else
		MetallicRoughnessMap = nullptr;

	if (!AOMapPath.empty())
		AOMap = assetManager.LoadTexture(AOMapPath);
	else
		AOMap = nullptr;
}

MaterialProperties MaterialProperties::CreatePreset(MaterialPreset preset)
{
	switch (preset)
	{
	case MaterialPreset::Default: return CreateDefault();
	case MaterialPreset::Metal: return CreateMetal();
	case MaterialPreset::Gold: return CreateGold();
	case MaterialPreset::Copper: return CreateCopper();
	case MaterialPreset::PlasticRough: return CreatePlastic();
	case MaterialPreset::GlassSmooth: return CreateGlass();
	case MaterialPreset::EmissiveCore: return CreateEmissive();
	case MaterialPreset::ConcreteStone: return CreateConcrete();
	default: return CreateDefault();
	}
}

MaterialProperties MaterialProperties::CreateDefault()
{
	MaterialProperties p;
	p.AlbedoColor = glm::vec4(1.0f);
	p.Metallic = 0.0f;
	p.Roughness = 0.5f;
	p.AO = 1.0f;
	p.EmissionColor = glm::vec3(0.0f);
	p.EmissionIntensity = 0.0f;
	p.Tiling = glm::vec2(1.0f);
	p.Offset = glm::vec2(0.0f);
	return p;
}

MaterialProperties MaterialProperties::CreateMetal()
{
	MaterialProperties p = CreateDefault();
	p.AlbedoColor = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);
	p.Metallic = 1.0f;
	p.Roughness = 0.15f;
	return p;
}

MaterialProperties MaterialProperties::CreateGold()
{
	MaterialProperties p = CreateDefault();
	p.AlbedoColor = glm::vec4(1.0f, 0.782f, 0.344f, 1.0f);
	p.Metallic = 1.0f;
	p.Roughness = 0.2f;
	return p;
}

MaterialProperties MaterialProperties::CreateCopper()
{
	MaterialProperties p = CreateDefault();
	p.AlbedoColor = glm::vec4(0.955f, 0.637f, 0.538f, 1.0f);
	p.Metallic = 1.0f;
	p.Roughness = 0.22f;
	return p;
}

MaterialProperties MaterialProperties::CreatePlastic()
{
	MaterialProperties p = CreateDefault();
	p.AlbedoColor = glm::vec4(0.85f, 0.25f, 0.25f, 1.0f);
	p.Metallic = 0.0f;
	p.Roughness = 0.75f;
	return p;
}

MaterialProperties MaterialProperties::CreateGlass()
{
	MaterialProperties p = CreateDefault();
	p.AlbedoColor = glm::vec4(0.92f, 0.95f, 0.98f, 1.0f);
	p.Metallic = 0.0f;
	p.Roughness = 0.05f;
	return p;
}

MaterialProperties MaterialProperties::CreateEmissive()
{
	MaterialProperties p = CreateDefault();
	p.AlbedoColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	p.Metallic = 0.0f;
	p.Roughness = 0.3f;
	p.EmissionColor = glm::vec3(0.2f, 0.75f, 1.0f);
	p.EmissionIntensity = 4.0f;
	return p;
}

MaterialProperties MaterialProperties::CreateConcrete()
{
	MaterialProperties p = CreateDefault();
	p.AlbedoColor = glm::vec4(0.55f, 0.55f, 0.53f, 1.0f);
	p.Metallic = 0.0f;
	p.Roughness = 0.9f;
	return p;
}

Material::Material(const std::string& name, const MaterialProperties& properties)
	: m_Name(name), m_Properties(properties)
{
}

void Material::SetPreset(MaterialPreset preset)
{
	m_Properties = MaterialProperties::CreatePreset(preset);
}
