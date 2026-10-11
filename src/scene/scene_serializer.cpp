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

	glm::vec2 AsVec2(const glm::vec2& defaultVal = glm::vec2(0.0f)) const {
		if (type == JsonType::Array && arrVal.size() >= 2) {
			return glm::vec2(arrVal[0].AsFloat(), arrVal[1].AsFloat());
		}
		return defaultVal;
	}

	glm::vec3 AsVec3(const glm::vec3& defaultVal = glm::vec3(0.0f)) const {
		if (type == JsonType::Array && arrVal.size() >= 3) {
			return glm::vec3(arrVal[0].AsFloat(), arrVal[1].AsFloat(), arrVal[2].AsFloat());
		}
		return defaultVal;
	}

	glm::vec4 AsVec4(const glm::vec4& defaultVal = glm::vec4(0.0f)) const {
		if (type == JsonType::Array && arrVal.size() >= 4) {
			return glm::vec4(arrVal[0].AsFloat(), arrVal[1].AsFloat(), arrVal[2].AsFloat(), arrVal[3].AsFloat());
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

		if (entity.HasComponent<CameraComponent>())
		{
			const auto& cc = entity.GetComponent<CameraComponent>();
			out << ",\n";
			out << "\t\t\t\"Camera\": {\n";
			out << "\t\t\t\t\"Primary\": " << (cc.Primary ? "true" : "false") << ",\n";
			out << "\t\t\t\t\"Fov\": " << cc.CameraInstance.Zoom << "\n";
			out << "\t\t\t}";
		}

		if (entity.HasComponent<BoxColliderComponent>())
		{
			const auto& bc = entity.GetComponent<BoxColliderComponent>();
			out << ",\n";
			out << "\t\t\t\"BoxCollider\": {\n";
			out << "\t\t\t\t\"Size\": [" << bc.Size.x << ", " << bc.Size.y << ", " << bc.Size.z << "],\n";
			out << "\t\t\t\t\"Center\": [" << bc.Center.x << ", " << bc.Center.y << ", " << bc.Center.z << "],\n";
			out << "\t\t\t\t\"IsTrigger\": " << (bc.IsTrigger ? "true" : "false") << ",\n";
			out << "\t\t\t\t\"Layer\": " << bc.Layer << ",\n";
			out << "\t\t\t\t\"Mask\": " << bc.Mask << "\n";
			out << "\t\t\t}";
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			const auto& sc = entity.GetComponent<SphereColliderComponent>();
			out << ",\n";
			out << "\t\t\t\"SphereCollider\": {\n";
			out << "\t\t\t\t\"Radius\": " << sc.Radius << ",\n";
			out << "\t\t\t\t\"Center\": [" << sc.Center.x << ", " << sc.Center.y << ", " << sc.Center.z << "],\n";
			out << "\t\t\t\t\"IsTrigger\": " << (sc.IsTrigger ? "true" : "false") << ",\n";
			out << "\t\t\t\t\"Layer\": " << sc.Layer << ",\n";
			out << "\t\t\t\t\"Mask\": " << sc.Mask << "\n";
			out << "\t\t\t}";
		}

		if (entity.HasComponent<CapsuleColliderComponent>())
		{
			const auto& cc = entity.GetComponent<CapsuleColliderComponent>();
			out << ",\n";
			out << "\t\t\t\"CapsuleCollider\": {\n";
			out << "\t\t\t\t\"Radius\": " << cc.Radius << ",\n";
			out << "\t\t\t\t\"Height\": " << cc.Height << ",\n";
			out << "\t\t\t\t\"Center\": [" << cc.Center.x << ", " << cc.Center.y << ", " << cc.Center.z << "],\n";
			out << "\t\t\t\t\"IsTrigger\": " << (cc.IsTrigger ? "true" : "false") << ",\n";
			out << "\t\t\t\t\"Layer\": " << cc.Layer << ",\n";
			out << "\t\t\t\t\"Mask\": " << cc.Mask << "\n";
			out << "\t\t\t}";
		}

		if (entity.HasComponent<MeshColliderComponent>())
		{
			const auto& mc = entity.GetComponent<MeshColliderComponent>();
			out << ",\n";
			out << "\t\t\t\"MeshCollider\": {\n";
			out << "\t\t\t\t\"IsTrigger\": " << (mc.IsTrigger ? "true" : "false") << ",\n";
			out << "\t\t\t\t\"Layer\": " << mc.Layer << ",\n";
			out << "\t\t\t\t\"Mask\": " << mc.Mask << "\n";
			out << "\t\t\t}";
		}

		if (entity.HasComponent<CharacterControllerComponent>())
		{
			const auto& ccc = entity.GetComponent<CharacterControllerComponent>();
			out << ",\n";
			out << "\t\t\t\"CharacterController\": {\n";
			out << "\t\t\t\t\"Radius\": " << ccc.Radius << ",\n";
			out << "\t\t\t\t\"Height\": " << ccc.Height << ",\n";
			out << "\t\t\t\t\"StepOffset\": " << ccc.StepOffset << ",\n";
			out << "\t\t\t\t\"SlopeLimit\": " << ccc.SlopeLimit << ",\n";
			out << "\t\t\t\t\"SkinWidth\": " << ccc.SkinWidth << ",\n";
			out << "\t\t\t\t\"Gravity\": " << ccc.Gravity << ",\n";
			out << "\t\t\t\t\"Center\": [" << ccc.Center.x << ", " << ccc.Center.y << ", " << ccc.Center.z << "],\n";
			out << "\t\t\t\t\"Layer\": " << ccc.Layer << ",\n";
			out << "\t\t\t\t\"CollisionMask\": " << ccc.CollisionMask << "\n";
			out << "\t\t\t}";
		}

		if (entity.HasComponent<NativeScriptComponent>())
		{
			const auto& nsc = entity.GetComponent<NativeScriptComponent>();
			out << ",\n";
			out << "\t\t\t\"NativeScript\": {\n";
			out << "\t\t\t\t\"ScriptName\": \"" << nsc.ScriptName << "\"\n";
			out << "\t\t\t}";
		}

		if (entity.HasComponent<MaterialComponent>())
		{
			const auto& mat = entity.GetComponent<MaterialComponent>();
			out << ",\n";
			out << "\t\t\t\"Material\": {\n";
			out << "\t\t\t\t\"Name\": \"" << mat.MaterialName << "\",\n";
			out << "\t\t\t\t\"AlbedoColor\": [" << mat.Properties.AlbedoColor.r << ", " << mat.Properties.AlbedoColor.g << ", " << mat.Properties.AlbedoColor.b << ", " << mat.Properties.AlbedoColor.a << "],\n";
			out << "\t\t\t\t\"Metallic\": " << mat.Properties.Metallic << ",\n";
			out << "\t\t\t\t\"Roughness\": " << mat.Properties.Roughness << ",\n";
			out << "\t\t\t\t\"AO\": " << mat.Properties.AO << ",\n";
			out << "\t\t\t\t\"EmissionColor\": [" << mat.Properties.EmissionColor.r << ", " << mat.Properties.EmissionColor.g << ", " << mat.Properties.EmissionColor.b << "],\n";
			out << "\t\t\t\t\"EmissionIntensity\": " << mat.Properties.EmissionIntensity << ",\n";
			out << "\t\t\t\t\"Tiling\": [" << mat.Properties.Tiling.x << ", " << mat.Properties.Tiling.y << "],\n";
			out << "\t\t\t\t\"Offset\": [" << mat.Properties.Offset.x << ", " << mat.Properties.Offset.y << "],\n";
			out << "\t\t\t\t\"AlbedoMap\": \"" << mat.Properties.AlbedoMapPath << "\",\n";
			out << "\t\t\t\t\"NormalMap\": \"" << mat.Properties.NormalMapPath << "\",\n";
			out << "\t\t\t\t\"MetallicRoughnessMap\": \"" << mat.Properties.MetallicRoughnessMapPath << "\",\n";
			out << "\t\t\t\t\"AOMap\": \"" << mat.Properties.AOMapPath << "\"\n";
			out << "\t\t\t}";
		}

		if (entity.HasComponent<AudioSourceComponent>())
		{
			const auto& asc = entity.GetComponent<AudioSourceComponent>();
			out << ",\n";
			out << "\t\t\t\"AudioSource\": {\n";
			out << "\t\t\t\t\"SoundPath\": \"" << asc.SoundPath << "\",\n";
			out << "\t\t\t\t\"Volume\": " << asc.Volume << ",\n";
			out << "\t\t\t\t\"Pitch\": " << asc.Pitch << ",\n";
			out << "\t\t\t\t\"Loop\": " << (asc.Loop ? "true" : "false") << ",\n";
			out << "\t\t\t\t\"PlayOnAwake\": " << (asc.PlayOnAwake ? "true" : "false") << ",\n";
			out << "\t\t\t\t\"Spatial\": " << (asc.Spatial ? "true" : "false") << ",\n";
			out << "\t\t\t\t\"MinDistance\": " << asc.MinDistance << ",\n";
			out << "\t\t\t\t\"MaxDistance\": " << asc.MaxDistance << ",\n";
			out << "\t\t\t\t\"RollOff\": " << asc.RollOff << "\n";
			out << "\t\t\t}";
		}

		if (entity.HasComponent<AudioListenerComponent>())
		{
			const auto& alc = entity.GetComponent<AudioListenerComponent>();
			out << ",\n";
			out << "\t\t\t\"AudioListener\": {\n";
			out << "\t\t\t\t\"Primary\": " << (alc.Primary ? "true" : "false") << ",\n";
			out << "\t\t\t\t\"Volume\": " << alc.Volume << "\n";
			out << "\t\t\t}";
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

			if (entVal.Has("Camera")) {
				const auto& camVal = entVal.Get("Camera");
				CameraComponent cc;
				cc.Primary = camVal.Get("Primary").AsBool(true);
				if (camVal.Has("Fov")) {
					cc.CameraInstance.Zoom = camVal.Get("Fov").AsFloat(60.0f);
				}
				entity.AddComponent<CameraComponent>(cc);
			}

			if (entVal.Has("BoxCollider")) {
				const auto& bcVal = entVal.Get("BoxCollider");
				BoxColliderComponent bc;
				bc.Size = bcVal.Get("Size").AsVec3(glm::vec3(1.0f));
				bc.Center = bcVal.Get("Center").AsVec3(glm::vec3(0.0f));
				bc.IsTrigger = bcVal.Get("IsTrigger").AsBool(false);
				if (bcVal.Has("Layer")) bc.Layer = static_cast<uint32_t>(bcVal.Get("Layer").AsInt(1));
				if (bcVal.Has("Mask")) bc.Mask = static_cast<uint32_t>(bcVal.Get("Mask").AsInt(-1));
				entity.AddComponent<BoxColliderComponent>(bc);
			}

			if (entVal.Has("SphereCollider")) {
				const auto& scVal = entVal.Get("SphereCollider");
				SphereColliderComponent sc;
				sc.Radius = scVal.Get("Radius").AsFloat(0.5f);
				sc.Center = scVal.Get("Center").AsVec3(glm::vec3(0.0f));
				sc.IsTrigger = scVal.Get("IsTrigger").AsBool(false);
				if (scVal.Has("Layer")) sc.Layer = static_cast<uint32_t>(scVal.Get("Layer").AsInt(1));
				if (scVal.Has("Mask")) sc.Mask = static_cast<uint32_t>(scVal.Get("Mask").AsInt(-1));
				entity.AddComponent<SphereColliderComponent>(sc);
			}

			if (entVal.Has("CapsuleCollider")) {
				const auto& ccVal = entVal.Get("CapsuleCollider");
				CapsuleColliderComponent cc;
				cc.Radius = ccVal.Get("Radius").AsFloat(0.5f);
				cc.Height = ccVal.Get("Height").AsFloat(1.0f);
				cc.Center = ccVal.Get("Center").AsVec3(glm::vec3(0.0f));
				cc.IsTrigger = ccVal.Get("IsTrigger").AsBool(false);
				if (ccVal.Has("Layer")) cc.Layer = static_cast<uint32_t>(ccVal.Get("Layer").AsInt(1));
				if (ccVal.Has("Mask")) cc.Mask = static_cast<uint32_t>(ccVal.Get("Mask").AsInt(-1));
				entity.AddComponent<CapsuleColliderComponent>(cc);
			}

			if (entVal.Has("MeshCollider")) {
				const auto& mcVal = entVal.Get("MeshCollider");
				MeshColliderComponent mc;
				mc.IsTrigger = mcVal.Get("IsTrigger").AsBool(false);
				if (mcVal.Has("Layer")) mc.Layer = static_cast<uint32_t>(mcVal.Get("Layer").AsInt(1));
				if (mcVal.Has("Mask")) mc.Mask = static_cast<uint32_t>(mcVal.Get("Mask").AsInt(-1));
				entity.AddComponent<MeshColliderComponent>(mc);
			}

			if (entVal.Has("CharacterController")) {
				const auto& cccVal = entVal.Get("CharacterController");
				CharacterControllerComponent ccc;
				ccc.Radius = cccVal.Get("Radius").AsFloat(0.4f);
				ccc.Height = cccVal.Get("Height").AsFloat(1.8f);
				ccc.StepOffset = cccVal.Get("StepOffset").AsFloat(0.35f);
				ccc.SlopeLimit = cccVal.Get("SlopeLimit").AsFloat(45.0f);
				ccc.SkinWidth = cccVal.Get("SkinWidth").AsFloat(0.04f);
				ccc.Gravity = cccVal.Get("Gravity").AsFloat(-20.0f);
				ccc.Center = cccVal.Get("Center").AsVec3(glm::vec3(0.0f, 0.9f, 0.0f));
				if (cccVal.Has("Layer")) ccc.Layer = static_cast<uint32_t>(cccVal.Get("Layer").AsInt(4));
				if (cccVal.Has("CollisionMask")) ccc.CollisionMask = static_cast<uint32_t>(cccVal.Get("CollisionMask").AsInt(3));
				entity.AddComponent<CharacterControllerComponent>(ccc);
			}

			if (entVal.Has("NativeScript")) {
				const auto& nsVal = entVal.Get("NativeScript");
				NativeScriptComponent nsc;
				nsc.ScriptName = nsVal.Get("ScriptName").AsString("NativeScript");
				entity.AddComponent<NativeScriptComponent>(nsc);
			}

			if (entVal.Has("Material")) {
				const auto& mVal = entVal.Get("Material");
				MaterialComponent mat;
				mat.MaterialName = mVal.Get("Name").AsString("Default");
				if (mVal.Has("AlbedoColor")) mat.Properties.AlbedoColor = mVal.Get("AlbedoColor").AsVec4(glm::vec4(1.0f));
				if (mVal.Has("Metallic")) mat.Properties.Metallic = mVal.Get("Metallic").AsFloat(0.0f);
				if (mVal.Has("Roughness")) mat.Properties.Roughness = mVal.Get("Roughness").AsFloat(0.5f);
				if (mVal.Has("AO")) mat.Properties.AO = mVal.Get("AO").AsFloat(1.0f);
				if (mVal.Has("EmissionColor")) mat.Properties.EmissionColor = mVal.Get("EmissionColor").AsVec3(glm::vec3(0.0f));
				if (mVal.Has("EmissionIntensity")) mat.Properties.EmissionIntensity = mVal.Get("EmissionIntensity").AsFloat(0.0f);
				if (mVal.Has("Tiling")) mat.Properties.Tiling = mVal.Get("Tiling").AsVec2(glm::vec2(1.0f));
				if (mVal.Has("Offset")) mat.Properties.Offset = mVal.Get("Offset").AsVec2(glm::vec2(0.0f));
				if (mVal.Has("AlbedoMap")) mat.Properties.AlbedoMapPath = mVal.Get("AlbedoMap").AsString("");
				if (mVal.Has("NormalMap")) mat.Properties.NormalMapPath = mVal.Get("NormalMap").AsString("");
				if (mVal.Has("MetallicRoughnessMap")) mat.Properties.MetallicRoughnessMapPath = mVal.Get("MetallicRoughnessMap").AsString("");
				if (mVal.Has("AOMap")) mat.Properties.AOMapPath = mVal.Get("AOMap").AsString("");

				mat.Properties.LoadTextures();
				entity.AddComponent<MaterialComponent>(mat);
			}

			if (entVal.Has("AudioSource")) {
				const auto& aVal = entVal.Get("AudioSource");
				AudioSourceComponent asc;
				asc.SoundPath = aVal.Get("SoundPath").AsString("");
				asc.Volume = aVal.Get("Volume").AsFloat(1.0f);
				asc.Pitch = aVal.Get("Pitch").AsFloat(1.0f);
				asc.Loop = aVal.Get("Loop").AsBool(false);
				asc.PlayOnAwake = aVal.Get("PlayOnAwake").AsBool(true);
				asc.Spatial = aVal.Get("Spatial").AsBool(true);
				asc.MinDistance = aVal.Get("MinDistance").AsFloat(1.0f);
				asc.MaxDistance = aVal.Get("MaxDistance").AsFloat(50.0f);
				asc.RollOff = aVal.Get("RollOff").AsFloat(1.0f);

				if (!asc.SoundPath.empty()) {
					asc.LoadSound(asc.SoundPath);
				}
				entity.AddComponent<AudioSourceComponent>(asc);
			}

			if (entVal.Has("AudioListener")) {
				const auto& alVal = entVal.Get("AudioListener");
				AudioListenerComponent alc;
				alc.Primary = alVal.Get("Primary").AsBool(true);
				alc.Volume = alVal.Get("Volume").AsFloat(1.0f);
				entity.AddComponent<AudioListenerComponent>(alc);
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
