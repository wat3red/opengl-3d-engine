#pragma once

#include <glm/glm.hpp>
#include <string>
#include <memory>

class Shader;
class Texture2D;

enum class MaterialPreset {
	Custom = 0,
	Default,
	Metal,
	Gold,
	Copper,
	PlasticRough,
	GlassSmooth,
	EmissiveCore,
	ConcreteStone
};

struct MaterialProperties {
	glm::vec4 AlbedoColor = glm::vec4(1.0f);
	float Metallic = 0.0f;
	float Roughness = 0.5f;
	float AO = 1.0f;
	glm::vec3 EmissionColor = glm::vec3(0.0f);
	float EmissionIntensity = 0.0f;
	glm::vec2 Tiling = glm::vec2(1.0f);
	glm::vec2 Offset = glm::vec2(0.0f);

	std::string AlbedoMapPath = "";
	std::string NormalMapPath = "";
	std::string MetallicRoughnessMapPath = "";
	std::string AOMapPath = "";

	std::shared_ptr<Texture2D> AlbedoMap = nullptr;
	std::shared_ptr<Texture2D> NormalMap = nullptr;
	std::shared_ptr<Texture2D> MetallicRoughnessMap = nullptr;
	std::shared_ptr<Texture2D> AOMap = nullptr;

	bool HasCustomAlbedo() const { return AlbedoMap != nullptr; }
	bool HasCustomNormal() const { return NormalMap != nullptr; }
	bool HasCustomMR() const { return MetallicRoughnessMap != nullptr; }
	bool HasCustomAO() const { return AOMap != nullptr; }

	void LoadTextures();

	static MaterialProperties CreatePreset(MaterialPreset preset);
	static MaterialProperties CreateDefault();
	static MaterialProperties CreateMetal();
	static MaterialProperties CreateGold();
	static MaterialProperties CreateCopper();
	static MaterialProperties CreatePlastic();
	static MaterialProperties CreateGlass();
	static MaterialProperties CreateEmissive();
	static MaterialProperties CreateConcrete();
};

class Material {
public:
	Material(const std::string& name = "New Material", const MaterialProperties& properties = MaterialProperties{});
	~Material() = default;

	const std::string& GetName() const { return m_Name; }
	void SetName(const std::string& name) { m_Name = name; }

	MaterialProperties& GetProperties() { return m_Properties; }
	const MaterialProperties& GetProperties() const { return m_Properties; }
	void SetProperties(const MaterialProperties& props) { m_Properties = props; }

	void SetPreset(MaterialPreset preset);

private:
	std::string m_Name;
	MaterialProperties m_Properties;
};
