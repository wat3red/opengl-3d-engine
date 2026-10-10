#include "scene_serializer.h"
#include "renderer.h"
#include "editor.h"
#include "asset_manager.h"
#include "skybox.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <cctype>

namespace {

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct JsonValue {
	JsonType type = JsonType::Null;
	bool boolVal = false;
	double numVal = 0.0;
	std::string strVal;
	std::vector<JsonValue> arrVal;
	std::map<std::string, JsonValue> objVal;

	bool Has(const std::string& key) const {
		return type == JsonType::Object && objVal.find(key) != objVal.end();
	}

	const JsonValue& Get(const std::string& key) const {
		static JsonValue nullVal;
		auto it = objVal.find(key);
		return (it != objVal.end()) ? it->second : nullVal;
	}

	float AsFloat(float defaultVal = 0.0f) const {
		return (type == JsonType::Number) ? static_cast<float>(numVal) : defaultVal;
	}

	int AsInt(int defaultVal = 0) const {
		return (type == JsonType::Number) ? static_cast<int>(numVal) : defaultVal;
	}

	bool AsBool(bool defaultVal = false) const {
		return (type == JsonType::Bool) ? boolVal : defaultVal;
	}

	std::string AsString(const std::string& defaultVal = "") const {
		return (type == JsonType::String) ? strVal : defaultVal;
	}

	glm::vec3 AsVec3(const glm::vec3& defaultVal = glm::vec3(0.0f)) const {
		if (type == JsonType::Array && arrVal.size() >= 3) {
			return glm::vec3(arrVal[0].AsFloat(), arrVal[1].AsFloat(), arrVal[2].AsFloat());
		}
		return defaultVal;
	}
};

class SimpleJsonParser {
public:
	static JsonValue Parse(const std::string& text) {
		size_t pos = 0;
		SkipWhitespace(text, pos);
		return ParseValue(text, pos);
	}

private:
	static void SkipWhitespace(const std::string& text, size_t& pos) {
		while (pos < text.size() && (std::isspace(static_cast<unsigned char>(text[pos])) || text[pos] == '\r')) {
			++pos;
		}
	}

	static JsonValue ParseValue(const std::string& text, size_t& pos) {
		SkipWhitespace(text, pos);
		if (pos >= text.size()) return JsonValue{};

		char c = text[pos];
		if (c == '{') return ParseObject(text, pos);
		if (c == '[') return ParseArray(text, pos);
		if (c == '"') return ParseString(text, pos);
		if (c == 't' || c == 'f') return ParseBool(text, pos);
		if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return ParseNumber(text, pos);

		return JsonValue{};
	}

	static JsonValue ParseObject(const std::string& text, size_t& pos) {
		JsonValue val;
		val.type = JsonType::Object;
		++pos; // skip '{'

		while (pos < text.size()) {
			SkipWhitespace(text, pos);
			if (pos < text.size() && text[pos] == '}') {
				++pos;
				break;
			}

			JsonValue key = ParseString(text, pos);
			SkipWhitespace(text, pos);
			if (pos < text.size() && text[pos] == ':') {
				++pos;
			}
			JsonValue value = ParseValue(text, pos);
			val.objVal[key.strVal] = value;

			SkipWhitespace(text, pos);
			if (pos < text.size() && text[pos] == ',') {
				++pos;
			}
		}
		return val;
	}

	static JsonValue ParseArray(const std::string& text, size_t& pos) {
		JsonValue val;
		val.type = JsonType::Array;
		++pos; // skip '['

		while (pos < text.size()) {
			SkipWhitespace(text, pos);
			if (pos < text.size() && text[pos] == ']') {
				++pos;
				break;
			}

			val.arrVal.push_back(ParseValue(text, pos));

			SkipWhitespace(text, pos);
			if (pos < text.size() && text[pos] == ',') {
				++pos;
			}
		}
		return val;
	}

	static JsonValue ParseString(const std::string& text, size_t& pos) {
		JsonValue val;
		val.type = JsonType::String;
		if (pos >= text.size() || text[pos] != '"') return val;
		++pos; // skip opening quote

		std::string str;
		while (pos < text.size() && text[pos] != '"') {
			if (text[pos] == '\\' && pos + 1 < text.size()) {
				++pos;
			}
			str += text[pos++];
		}
		if (pos < text.size() && text[pos] == '"') {
			++pos;
		}
		val.strVal = str;
		return val;
	}

	static JsonValue ParseBool(const std::string& text, size_t& pos) {
		JsonValue val;
		val.type = JsonType::Bool;
		if (text.compare(pos, 4, "true") == 0) {
			val.boolVal = true;
			pos += 4;
		} else if (text.compare(pos, 5, "false") == 0) {
			val.boolVal = false;
			pos += 5;
		}
		return val;
	}

	static JsonValue ParseNumber(const std::string& text, size_t& pos) {
		JsonValue val;
		val.type = JsonType::Number;
		size_t start = pos;
		if (pos < text.size() && text[pos] == '-') ++pos;
		while (pos < text.size() && (std::isdigit(static_cast<unsigned char>(text[pos])) || text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E' || text[pos] == '+' || text[pos] == '-')) {
			++pos;
		}
		try {
			val.numVal = std::stod(text.substr(start, pos - start));
		} catch (...) {
			val.numVal = 0.0;
		}
		return val;
	}
};

} // anonymous namespace

SceneSerializer::SceneSerializer(Scene& scene, Renderer* renderer, Editor* editor)
	: m_Scene(scene), m_Renderer(renderer), m_Editor(editor)
{
}

bool SceneSerializer::Serialize(const std::string& filepath)
{
	std::ofstream out(filepath);
	if (!out.is_open())
		return false;

	out << "{\n";
	out << "\t\"Scene\": \"IgnisScene\",\n";

	// Skybox
	auto skybox = m_Scene.GetSkybox();
	out << "\t\"Skybox\": {\n";
	out << "\t\t\"Active\": " << (skybox && skybox->IsActive() ? "true" : "false") << ",\n";
	std::string skyboxPath = "";
	if (skybox && skybox->GetDomeModel()) {
		skyboxPath = "res/models/skybox/source/basic_skybox_3d.fbx";
	}
	out << "\t\t\"Path\": \"" << skyboxPath << "\"\n";
	out << "\t},\n";

	// Environment Settings
	out << "\t\"Environment\": {\n";
	if (m_Editor)
	{
		out << "\t\t\"Exposure\": " << m_Editor->GetExposure() << ",\n";
		out << "\t\t\"ToneMappingOperator\": " << m_Editor->GetToneMappingOperator() << ",\n";
		out << "\t\t\"Gamma\": " << m_Editor->GetGamma() << ",\n";
		out << "\t\t\"BloomEnabled\": " << (m_Editor->GetBloomEnabled() ? "true" : "false") << ",\n";
		out << "\t\t\"BloomIntensity\": " << m_Editor->GetBloomIntensity() << ",\n";
		out << "\t\t\"BloomThreshold\": " << m_Editor->GetBloomThreshold() << ",\n";
		out << "\t\t\"BloomRadius\": " << m_Editor->GetBloomFilterRadius() << ",\n";
	}
	if (m_Renderer)
	{
		const auto& fog = m_Renderer->GetFogSettings();
		out << "\t\t\"FogEnabled\": " << (fog.enabled ? "true" : "false") << ",\n";
		out << "\t\t\"FogColor\": [" << fog.color.r << ", " << fog.color.g << ", " << fog.color.b << "],\n";
		out << "\t\t\"FogDensity\": " << fog.density << ",\n";
		out << "\t\t\"FogStart\": " << fog.start << ",\n";
		out << "\t\t\"FogEnd\": " << fog.end << ",\n";
		out << "\t\t\"FogHeightFalloff\": " << fog.heightFalloff << "\n";
	}
	else
	{
		out << "\t\t\"FogEnabled\": false\n";
	}
	out << "\t},\n";

	// Entities
	out << "\t\"Entities\": [\n";
	const auto& entities = m_Scene.GetEntities();
	for (size_t i = 0; i < entities.size(); ++i)
	{
		const auto& entity = entities[i];
		if (!entity) continue;

		out << "\t\t{\n";
		out << "\t\t\t\"Name\": \"" << entity->GetName() << "\",\n";
		out << "\t\t\t\"ModelPath\": \"" << entity->GetModelPath() << "\",\n";
		out << "\t\t\t\"Active\": " << (entity->IsActive() ? "true" : "false") << ",\n";
		out << "\t\t\t\"CastsShadow\": " << (entity->CastsShadow() ? "true" : "false") << ",\n";
		glm::vec3 pos = entity->GetPosition();
		glm::vec3 rot = entity->GetRotation();
		glm::vec3 scale = entity->GetScale();
		out << "\t\t\t\"Position\": [" << pos.x << ", " << pos.y << ", " << pos.z << "],\n";
		out << "\t\t\t\"Rotation\": [" << rot.x << ", " << rot.y << ", " << rot.z << "],\n";
		out << "\t\t\t\"Scale\": [" << scale.x << ", " << scale.y << ", " << scale.z << "]\n";
		out << "\t\t}" << (i + 1 < entities.size() ? "," : "") << "\n";
	}
	out << "\t],\n";

	// Lights
	out << "\t\"Lights\": [\n";
	const auto& lights = m_Scene.GetLights();
	for (size_t i = 0; i < lights.size(); ++i)
	{
		const auto& light = lights[i];
		if (!light) continue;

		out << "\t\t{\n";
		out << "\t\t\t\"Name\": \"" << light->GetName() << "\",\n";
		std::string typeStr = "Point";
		if (light->GetType() == LightType::Directional) typeStr = "Directional";
		else if (light->GetType() == LightType::Spotlight) typeStr = "Spotlight";
		out << "\t\t\t\"Type\": \"" << typeStr << "\",\n";
		out << "\t\t\t\"Active\": " << (light->IsActive() ? "true" : "false") << ",\n";
		glm::vec3 col = light->GetColor();
		out << "\t\t\t\"Color\": [" << col.r << ", " << col.g << ", " << col.b << "],\n";
		out << "\t\t\t\"Intensity\": " << light->GetIntensity() << ",\n";
		glm::vec3 pos = light->GetPosition();
		out << "\t\t\t\"Position\": [" << pos.x << ", " << pos.y << ", " << pos.z << "],\n";
		glm::vec3 dir = light->GetDirection();
		out << "\t\t\t\"Direction\": [" << dir.x << ", " << dir.y << ", " << dir.z << "],\n";
		out << "\t\t\t\"Range\": " << light->GetRange() << ",\n";
		out << "\t\t\t\"InnerCutoff\": " << light->GetInnerCutoff() << ",\n";
		out << "\t\t\t\"OuterCutoff\": " << light->GetOuterCutoff() << "\n";
		out << "\t\t}" << (i + 1 < lights.size() ? "," : "") << "\n";
	}
	out << "\t]\n";

	out << "}\n";
	return true;
}

bool SceneSerializer::Deserialize(const std::string& filepath)
{
	std::ifstream in(filepath);
	if (!in.is_open())
		return false;

	std::stringstream ss;
	ss << in.rdbuf();
	std::string content = ss.str();

	JsonValue root = SimpleJsonParser::Parse(content);
	if (root.type != JsonType::Object)
		return false;

	m_Scene.Clear();

	// Skybox
	if (root.Has("Skybox")) {
		const auto& skyboxObj = root.Get("Skybox");
		std::string path = skyboxObj.Get("Path").AsString();
		bool active = skyboxObj.Get("Active").AsBool(true);
		if (!path.empty()) {
			auto shader = AssetManager::Get().LoadShader("skydome", "shaders/SkyDome.shader");
			if (shader) {
				auto sky = std::make_shared<Skybox>(path, shader);
				sky->SetActive(active);
				m_Scene.SetSkybox(sky);
			}
		}
	}

	// Environment
	if (root.Has("Environment")) {
		const auto& env = root.Get("Environment");
		if (m_Editor) {
			if (env.Has("Exposure")) m_Editor->GetExposure() = env.Get("Exposure").AsFloat(1.0f);
			if (env.Has("ToneMappingOperator")) m_Editor->GetToneMappingOperator() = env.Get("ToneMappingOperator").AsInt(0);
			if (env.Has("Gamma")) m_Editor->GetGamma() = env.Get("Gamma").AsFloat(2.2f);
			if (env.Has("BloomEnabled")) m_Editor->GetBloomEnabled() = env.Get("BloomEnabled").AsBool(true);
			if (env.Has("BloomIntensity")) m_Editor->GetBloomIntensity() = env.Get("BloomIntensity").AsFloat(0.8f);
			if (env.Has("BloomThreshold")) m_Editor->GetBloomThreshold() = env.Get("BloomThreshold").AsFloat(1.0f);
			if (env.Has("BloomRadius")) m_Editor->GetBloomFilterRadius() = env.Get("BloomRadius").AsFloat(0.005f);
		}
		if (m_Renderer) {
			auto& fog = m_Renderer->GetFogSettings();
			if (env.Has("FogEnabled")) fog.enabled = env.Get("FogEnabled").AsBool(false);
			if (env.Has("FogColor")) fog.color = env.Get("FogColor").AsVec3(glm::vec3(0.65f, 0.75f, 0.85f));
			if (env.Has("FogDensity")) fog.density = env.Get("FogDensity").AsFloat(0.015f);
			if (env.Has("FogStart")) fog.start = env.Get("FogStart").AsFloat(10.0f);
			if (env.Has("FogEnd")) fog.end = env.Get("FogEnd").AsFloat(120.0f);
			if (env.Has("FogHeightFalloff")) fog.heightFalloff = env.Get("FogHeightFalloff").AsFloat(0.02f);
		}
	}

	// Entities
	if (root.Has("Entities")) {
		const auto& entitiesArr = root.Get("Entities");
		for (const auto& entVal : entitiesArr.arrVal) {
			std::string name = entVal.Get("Name").AsString("Entity");
			std::string path = entVal.Get("ModelPath").AsString();
			bool active = entVal.Get("Active").AsBool(true);
			bool castsShadow = entVal.Get("CastsShadow").AsBool(true);
			glm::vec3 pos = entVal.Get("Position").AsVec3();
			glm::vec3 rot = entVal.Get("Rotation").AsVec3();
			glm::vec3 scale = entVal.Get("Scale").AsVec3(glm::vec3(1.0f));

			if (!path.empty()) {
				auto entity = std::make_shared<ModelEntity>(name, path);
				entity->SetPosition(pos);
				entity->SetRotation(rot);
				entity->SetScale(scale);
				entity->SetActive(active);
				entity->SetCastsShadow(castsShadow);
				m_Scene.AddEntity(entity);
			}
		}
	}

	// Lights
	if (root.Has("Lights")) {
		const auto& lightsArr = root.Get("Lights");
		for (const auto& lightVal : lightsArr.arrVal) {
			std::string name = lightVal.Get("Name").AsString("Light");
			std::string type = lightVal.Get("Type").AsString("Point");
			bool active = lightVal.Get("Active").AsBool(true);
			glm::vec3 color = lightVal.Get("Color").AsVec3(glm::vec3(1.0f));
			float intensity = lightVal.Get("Intensity").AsFloat(1.0f);
			glm::vec3 pos = lightVal.Get("Position").AsVec3();
			glm::vec3 dir = lightVal.Get("Direction").AsVec3(glm::vec3(0.0f, -1.0f, 0.0f));
			float range = lightVal.Get("Range").AsFloat(25.0f);
			float innerCut = lightVal.Get("InnerCutoff").AsFloat(12.5f);
			float outerCut = lightVal.Get("OuterCutoff").AsFloat(17.5f);

			if (type == "Directional") {
				auto light = std::make_shared<DirectionalLight>(name, dir, color, intensity);
				light->SetActive(active);
				m_Scene.AddLight(light);
			} else if (type == "Spotlight") {
				auto light = std::make_shared<SpotLight>(name, pos, dir, color, intensity, innerCut, outerCut);
				light->SetActive(active);
				m_Scene.AddLight(light);
			} else {
				auto light = std::make_shared<PointLight>(name, pos, color, intensity, range);
				light->SetActive(active);
				m_Scene.AddLight(light);
			}
		}
	}

	if (!m_Scene.GetEntities().empty()) {
		m_Scene.SetSelectedEntity(m_Scene.GetEntities()[0]);
	} else if (!m_Scene.GetLights().empty()) {
		m_Scene.SetSelectedEntity(m_Scene.GetLights()[0]);
	}

	return true;
}
