#pragma once

#include <string>
#include <memory>
#include <algorithm>
#include <limits>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "model.h"
#include "material.h"
#include "asset_manager.h"
#include "shader.h"
#include "camera.h"
#include "physics/rigidbody.h"
#include "audio_engine.h"

class Scene;
class Entity;

// Forward declaration of LightType enum (or defined here/in light.h)
#ifndef LIGHT_TYPE_ENUM_DEFINED
#define LIGHT_TYPE_ENUM_DEFINED
enum class LightType {
	Directional,
	Point,
	Spotlight
};

inline constexpr LightType DIRECTIONAL = LightType::Directional;
inline constexpr LightType POINT = LightType::Point;
inline constexpr LightType SPOTLIGHT = LightType::Spotlight;
#endif

// Tag / Metadata Component
struct TagComponent {
	std::string Tag = "Entity";
	bool Active = true;

	TagComponent() = default;
	TagComponent(const std::string& tag, bool active = true)
		: Tag(tag), Active(active) {}
};

// Transform Component (Translation, Euler Rotation in degrees, Scale)
struct TransformComponent {
	glm::vec3 Position{ 0.0f };
	glm::vec3 Rotation{ 0.0f }; // Euler angles in degrees
	glm::vec3 Scale{ 1.0f };

	TransformComponent() = default;
	TransformComponent(const glm::vec3& position, const glm::vec3& rotation = glm::vec3(0.0f), const glm::vec3& scale = glm::vec3(1.0f))
		: Position(position), Rotation(rotation), Scale(scale) {}

	[[nodiscard]] glm::mat4 GetMatrix() const
	{
		glm::mat4 matrix = glm::mat4(1.0f);
		matrix = glm::translate(matrix, Position);
		matrix = glm::rotate(matrix, glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		matrix = glm::rotate(matrix, glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		matrix = glm::rotate(matrix, glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		matrix = glm::scale(matrix, Scale);
		return matrix;
	}

	bool operator==(const TransformComponent&) const = default;
};

// Mesh / 3D Model Component
struct MeshComponent {
	std::shared_ptr<Model> ModelAsset = nullptr;
	std::string ModelPath = "";
	bool CastsShadow = true;
	bool FlipUVs = false;

	MeshComponent() = default;
	MeshComponent(const std::string& path, bool flipUVs = false, bool castsShadow = true)
		: ModelPath(path), FlipUVs(flipUVs), CastsShadow(castsShadow)
	{
		LoadModel(path, flipUVs);
	}

	void LoadModel(const std::string& path, bool flipUVs = false)
	{
		ModelPath = path;
		FlipUVs = flipUVs;
		if (!path.empty())
		{
			ModelAsset = AssetManager::Get().LoadModel(path, flipUVs);
		}
		else
		{
			ModelAsset = nullptr;
		}
	}

	void Draw(Shader& shader, const glm::mat4& transform, const MaterialProperties* materialProps = nullptr)
	{
		if (ModelAsset)
		{
			shader.SetUniformMat4f("model", transform);
			ModelAsset->Draw(shader, materialProps);
		}
	}

	void GetWorldAABB(const glm::mat4& transform, glm::vec3& outMin, glm::vec3& outMax) const
	{
		if (!ModelAsset) {
			outMin = glm::vec3(transform[3]);
			outMax = glm::vec3(transform[3]);
			return;
		}

		const auto& aabb = ModelAsset->GetAABB();
		glm::vec3 corners[8] = {
			glm::vec3(aabb.min.x, aabb.min.y, aabb.min.z),
			glm::vec3(aabb.max.x, aabb.min.y, aabb.min.z),
			glm::vec3(aabb.min.x, aabb.max.y, aabb.min.z),
			glm::vec3(aabb.max.x, aabb.max.y, aabb.min.z),
			glm::vec3(aabb.min.x, aabb.min.y, aabb.max.z),
			glm::vec3(aabb.max.x, aabb.min.y, aabb.max.z),
			glm::vec3(aabb.min.x, aabb.max.y, aabb.max.z),
			glm::vec3(aabb.max.x, aabb.max.y, aabb.max.z)
		};

		outMin = glm::vec3(std::numeric_limits<float>::max());
		outMax = glm::vec3(std::numeric_limits<float>::lowest());

		for (int i = 0; i < 8; ++i) {
			glm::vec3 transformed = glm::vec3(transform * glm::vec4(corners[i], 1.0f));
			outMin = glm::min(outMin, transformed);
			outMax = glm::max(outMax, transformed);
		}
	}
};

// PBR Material Component
struct MaterialComponent {
	std::string MaterialName = "Default";
	MaterialProperties Properties;

	MaterialComponent() = default;
	explicit MaterialComponent(const std::string& name) : MaterialName(name) {}
	explicit MaterialComponent(MaterialPreset preset)
	{
		Properties = MaterialProperties::CreatePreset(preset);
		switch (preset)
		{
		case MaterialPreset::Metal: MaterialName = "Metal"; break;
		case MaterialPreset::Gold: MaterialName = "Gold"; break;
		case MaterialPreset::Copper: MaterialName = "Copper"; break;
		case MaterialPreset::PlasticRough: MaterialName = "Plastic Rough"; break;
		case MaterialPreset::GlassSmooth: MaterialName = "Glass Smooth"; break;
		case MaterialPreset::EmissiveCore: MaterialName = "Emissive Core"; break;
		case MaterialPreset::ConcreteStone: MaterialName = "Concrete"; break;
		default: MaterialName = "Default"; break;
		}
	}

	void SetPreset(MaterialPreset preset)
	{
		Properties = MaterialProperties::CreatePreset(preset);
	}
};

// Light Component (Directional, Point, Spot)
struct LightComponent {
	LightType Type = LightType::Point;
	glm::vec3 Color{ 1.0f };
	float Intensity = 1.0f;

	// For Directional & Spot lights:
	glm::vec3 Direction{ 0.0f, -1.0f, 0.0f };

	// For Point & Spot lights:
	float Range = 25.0f;
	float Constant = 1.0f;
	float Linear = 0.09f;
	float Quadratic = 0.032f;

	// For Spot lights:
	float InnerCutoff = 12.5f; // degrees
	float OuterCutoff = 17.5f; // degrees

	bool CastsShadow = true;

	LightComponent() = default;

	static LightComponent CreateDirectional(const glm::vec3& direction, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f)
	{
		LightComponent lc;
		lc.Type = LightType::Directional;
		lc.Direction = glm::normalize(direction);
		lc.Color = color;
		lc.Intensity = intensity;
		return lc;
	}

	static LightComponent CreatePoint(const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f, float range = 25.0f)
	{
		LightComponent lc;
		lc.Type = LightType::Point;
		lc.Color = color;
		lc.Intensity = intensity;
		lc.Range = range;
		return lc;
	}

	static LightComponent CreateSpot(const glm::vec3& direction, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f,
		float innerCutoff = 12.5f, float outerCutoff = 17.5f, float range = 35.0f)
	{
		LightComponent lc;
		lc.Type = LightType::Spotlight;
		lc.Direction = glm::normalize(direction);
		lc.Color = color;
		lc.Intensity = intensity;
		lc.InnerCutoff = innerCutoff;
		lc.OuterCutoff = outerCutoff;
		lc.Range = range;
		return lc;
	}
};

// Rigidbody Component
struct RigidbodyComponent {
	std::unique_ptr<Rigidbody> Body = nullptr;
	float Mass = 1.0f;
	glm::vec3 Velocity{ 0.0f };
	bool UseGravity = true;

	RigidbodyComponent() = default;
	explicit RigidbodyComponent(Model* model)
	{
		if (model)
			Body = std::make_unique<Rigidbody>(model);
	}
};

// Camera Component
struct CameraComponent {
	Camera CameraInstance;
	bool Primary = true;

	CameraComponent() = default;
	explicit CameraComponent(bool primary)
		: Primary(primary) {}
	explicit CameraComponent(const Camera& cam, bool primary = true)
		: CameraInstance(cam), Primary(primary) {}
};

class ScriptableEntity;

// Native Script Component
struct NativeScriptComponent {
	ScriptableEntity* Instance = nullptr;
	std::string ScriptName = "Script";

	std::function<ScriptableEntity*()> InstantiateScript = nullptr;
	std::function<void(NativeScriptComponent*)> DestroyScript = nullptr;

	template<typename T>
	void Bind(const std::string& name = "")
	{
		ScriptName = name.empty() ? "NativeScript" : name;
		InstantiateScript = []() -> ScriptableEntity* { return static_cast<ScriptableEntity*>(new T()); };
		DestroyScript = [](NativeScriptComponent* nsc) {
			delete nsc->Instance;
			nsc->Instance = nullptr;
		};
	}

	bool Bind(const std::string& name);
};

// Collider Components
struct BoxColliderComponent {
	glm::vec3 Size{ 1.0f, 1.0f, 1.0f };
	glm::vec3 Center{ 0.0f, 0.0f, 0.0f };
	bool IsTrigger = false;
	uint32_t Layer = 1; // Physics::Layer_Default
	uint32_t Mask = 0xFFFFFFFF; // Physics::Layer_All

	BoxColliderComponent() = default;
	BoxColliderComponent(const glm::vec3& size, const glm::vec3& center = glm::vec3(0.0f), bool isTrigger = false, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF)
		: Size(size), Center(center), IsTrigger(isTrigger), Layer(layer), Mask(mask) {}
};

struct SphereColliderComponent {
	float Radius = 0.5f;
	glm::vec3 Center{ 0.0f, 0.0f, 0.0f };
	bool IsTrigger = false;
	uint32_t Layer = 1;
	uint32_t Mask = 0xFFFFFFFF;

	SphereColliderComponent() = default;
	SphereColliderComponent(float radius, const glm::vec3& center = glm::vec3(0.0f), bool isTrigger = false, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF)
		: Radius(radius), Center(center), IsTrigger(isTrigger), Layer(layer), Mask(mask) {}
};

struct CapsuleColliderComponent {
	float Radius = 0.5f;
	float Height = 1.0f;
	glm::vec3 Center{ 0.0f, 0.0f, 0.0f };
	bool IsTrigger = false;
	uint32_t Layer = 1;
	uint32_t Mask = 0xFFFFFFFF;

	CapsuleColliderComponent() = default;
	CapsuleColliderComponent(float radius, float height, const glm::vec3& center = glm::vec3(0.0f), bool isTrigger = false, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF)
		: Radius(radius), Height(height), Center(center), IsTrigger(isTrigger), Layer(layer), Mask(mask) {}
};

struct MeshColliderComponent {
	bool IsTrigger = false;
	uint32_t Layer = 1;
	uint32_t Mask = 0xFFFFFFFF;

	MeshColliderComponent() = default;
	explicit MeshColliderComponent(bool isTrigger, uint32_t layer = 1, uint32_t mask = 0xFFFFFFFF)
		: IsTrigger(isTrigger), Layer(layer), Mask(mask) {}
};

// Kinematic Character Controller Component
struct CharacterControllerComponent {
	float Radius = 0.4f;
	float Height = 1.8f;
	float StepOffset = 0.35f;
	float SlopeLimit = 45.0f; // degrees
	float SkinWidth = 0.04f;
	float Gravity = -20.0f;
	glm::vec3 Center{ 0.0f, 0.9f, 0.0f };

	uint32_t Layer = 4; // Physics::Layer_Player (1 << 2)
	uint32_t CollisionMask = 3; // Physics::Layer_Default (1) | Physics::Layer_Static (2)

	// Runtime state
	bool IsGrounded = false;
	glm::vec3 Velocity{ 0.0f };
	glm::vec3 GroundNormal{ 0.0f, 1.0f, 0.0f };

	CharacterControllerComponent() = default;

	glm::vec3 Move(Scene& scene, Entity entity, const glm::vec3& displacement);
	void SimpleMove(Scene& scene, Entity entity, const glm::vec3& horizontalVelocity, float deltaTime);
	void Jump(float jumpForce);
};

// 3D Audio Source Component
struct AudioSourceComponent {
	std::string SoundPath = "";
	float Volume = 1.0f;
	float Pitch = 1.0f;
	bool Loop = false;
	bool PlayOnAwake = true;
	bool Spatial = true;
	float MinDistance = 1.0f;
	float MaxDistance = 50.0f;
	float RollOff = 1.0f;

	std::shared_ptr<Sound> SoundInstance = nullptr;

	AudioSourceComponent() = default;
	explicit AudioSourceComponent(const std::string& path, bool loop = false, bool playOnAwake = true, bool spatial = true)
		: SoundPath(path), Loop(loop), PlayOnAwake(playOnAwake), Spatial(spatial)
	{
		LoadSound(path);
	}

	void LoadSound(const std::string& path)
	{
		SoundPath = path;
		if (!path.empty())
		{
			SoundInstance = AudioEngine::Get().CreateSound(path, Spatial, Loop);
			if (SoundInstance)
			{
				SoundInstance->SetVolume(Volume);
				SoundInstance->SetPitch(Pitch);
				SoundInstance->SetAttenuation(MinDistance, MaxDistance, RollOff);
			}
		}
		else
		{
			SoundInstance = nullptr;
		}
	}

	void Play()
	{
		if (!SoundInstance && !SoundPath.empty())
		{
			LoadSound(SoundPath);
		}
		if (SoundInstance)
		{
			SoundInstance->SetVolume(Volume);
			SoundInstance->SetPitch(Pitch);
			SoundInstance->SetLooping(Loop);
			SoundInstance->SetSpatial(Spatial);
			SoundInstance->SetAttenuation(MinDistance, MaxDistance, RollOff);
			SoundInstance->Play();
		}
	}

	void Pause()
	{
		if (SoundInstance)
			SoundInstance->Pause();
	}

	void Stop()
	{
		if (SoundInstance)
			SoundInstance->Stop();
	}

	[[nodiscard]] bool IsPlaying() const
	{
		return SoundInstance ? SoundInstance->IsPlaying() : false;
	}

	void UpdateTransform(const glm::vec3& worldPos)
	{
		if (SoundInstance && Spatial)
		{
			SoundInstance->SetPosition(worldPos);
		}
	}
};

// 3D Audio Listener Component
struct AudioListenerComponent {
	bool Primary = true;
	float Volume = 1.0f;

	AudioListenerComponent() = default;
	explicit AudioListenerComponent(bool primary, float volume = 1.0f)
		: Primary(primary), Volume(volume) {}
};


