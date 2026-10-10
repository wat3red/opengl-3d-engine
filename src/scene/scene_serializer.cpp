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

	// Entities with Components
	out << "\t\"Entities\": [\n";
	auto view = m_Scene.GetRegistry().view<TagComponent>();
	size_t entityIdx = 0;
	size_t totalEntities = view.size();
	for (auto entityHandle : view)
	{
		Entity entity = { entityHandle, &m_Scene };
		const auto& tag = entity.GetComponent<TagComponent>();

		out << "\t\t{\n";
		out << "\t\t\t\"Name\": \"" << tag.Tag << "\",\n";
		out << "\t\t\t\"Active\": " << (tag.Active ? "true" : "false");

		if (entity.HasComponent<TransformComponent>())
		{
			const auto& tc = entity.GetComponent<TransformComponent>();
			out << ",\n";
			out << "\t\t\t\"Position\": [" << tc.Position.x << ", " << tc.Position.y << ", " << tc.Position.z << "],\n";
			out << "\t\t\t\"Rotation\": [" << tc.Rotation.x << ", " << tc.Rotation.y << ", " << tc.Rotation.z << "],\n";
			out << "\t\t\t\"Scale\": [" << tc.Scale.x << ", " << tc.Scale.y << ", " << tc.Scale.z << "]";
		}

		if (entity.HasComponent<MeshComponent>())
		{
			const auto& mc = entity.GetComponent<MeshComponent>();
			out << ",\n";
			out << "\t\t\t\"ModelPath\": \"" << mc.ModelPath << "\",\n";
			out << "\t\t\t\"CastsShadow\": " << (mc.CastsShadow ? "true" : "false") << ",\n";
			out << "\t\t\t\"FlipUVs\": " << (mc.FlipUVs ? "true" : "false");
		}

		if (entity.HasComponent<LightComponent>())
		{
			const auto& lc = entity.GetComponent<LightComponent>();
			out << ",\n";
			std::string typeStr = "Point";
			if (lc.Type == LightType::Directional) typeStr = "Directional";
			else if (lc.Type == LightType::Spotlight) typeStr = "Spotlight";
			out << "\t\t\t\"LightType\": \"" << typeStr << "\",\n";
			out << "\t\t\t\"Color\": [" << lc.Color.r << ", " << lc.Color.g << ", " << lc.Color.b << "],\n";
			out << "\t\t\t\"Intensity\": " << lc.Intensity << ",\n";
			out << "\t\t\t\"Direction\": [" << lc.Direction.x << ", " << lc.Direction.y << ", " << lc.Direction.z << "],\n";
			out << "\t\t\t\"Range\": " << lc.Range << ",\n";
			out << "\t\t\t\"InnerCutoff\": " << lc.InnerCutoff << ",\n";
			out << "\t\t\t\"OuterCutoff\": " << lc.OuterCutoff;
		}

		out << "\n\t\t}" << (entityIdx + 1 < totalEntities ? "," : "") << "\n";
		entityIdx++;
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
			bool active = entVal.Get("Active").AsBool(true);

			Entity entity = m_Scene.CreateEntity(name);
			entity.SetActive(active);

			if (entVal.Has("Position")) {
				entity.SetPosition(entVal.Get("Position").AsVec3());
			}
			if (entVal.Has("Rotation")) {
				entity.SetRotation(entVal.Get("Rotation").AsVec3());
			}
			if (entVal.Has("Scale")) {
				entity.SetScale(entVal.Get("Scale").AsVec3(glm::vec3(1.0f)));
			}

			if (entVal.Has("ModelPath")) {
				std::string path = entVal.Get("ModelPath").AsString();
				if (!path.empty()) {
					bool flip = entVal.Get("FlipUVs").AsBool(false);
					bool castsShadow = entVal.Get("CastsShadow").AsBool(true);
					auto& mesh = entity.AddComponent<MeshComponent>(path, flip, castsShadow);
					if (mesh.ModelAsset) {
						entity.AddComponent<RigidbodyComponent>(mesh.ModelAsset.get());
					}
				}
			}

			if (entVal.Has("LightType") || entVal.Has("Type")) {
				std::string type = entVal.Has("LightType") ? entVal.Get("LightType").AsString() : entVal.Get("Type").AsString();
				glm::vec3 color = entVal.Get("Color").AsVec3(glm::vec3(1.0f));
				float intensity = entVal.Get("Intensity").AsFloat(1.0f);
				glm::vec3 dir = entVal.Get("Direction").AsVec3(glm::vec3(0.0f, -1.0f, 0.0f));
				float range = entVal.Get("Range").AsFloat(25.0f);
				float inner = entVal.Get("InnerCutoff").AsFloat(12.5f);
				float outer = entVal.Get("OuterCutoff").AsFloat(17.5f);

				LightType lt = LightType::Point;
				if (type == "Directional") lt = LightType::Directional;
				else if (type == "Spotlight") lt = LightType::Spotlight;

				LightComponent lc;
				lc.Type = lt;
				lc.Color = color;
				lc.Intensity = intensity;
				lc.Direction = dir;
				lc.Range = range;
				lc.InnerCutoff = inner;
				lc.OuterCutoff = outer;
				entity.AddComponent<LightComponent>(lc);
			}
		}
	}

	// Backwards compatibility: Legacy "Lights" list in older scene saves
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
				auto light = m_Scene.CreateDirectionalLight(name, dir, color, intensity);
				light.SetActive(active);
			} else if (type == "Spotlight") {
				auto light = m_Scene.CreateSpotLight(name, pos, dir, color, intensity, innerCut, outerCut, range);
				light.SetActive(active);
			} else {
				auto light = m_Scene.CreatePointLight(name, pos, color, intensity, range);
				light.SetActive(active);
			}
		}
	}

	auto allEnts = m_Scene.GetRegistry().view<TagComponent>();
	if (!allEnts.empty()) {
		m_Scene.SetSelectedEntity({ allEnts.front(), &m_Scene });
	}

	return true;
}
