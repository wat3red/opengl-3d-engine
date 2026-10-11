#pragma once

#include <string>
#include <memory>
#include <algorithm>
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "model.h"
#include "asset_manager.h"
#include "shader.h"
#include "camera.h"
#include "physics/rigidbody.h"

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

	void Draw(Shader& shader, const glm::mat4& transform)
	{
		if (ModelAsset)
		{
			shader.SetUniformMat4f("model", transform);
			ModelAsset->Draw(shader);
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
	explicit CameraComponent(const Camera& cam, bool primary = true)
		: CameraInstance(cam), Primary(primary) {}
};

class ScriptableEntity;

// Native Script Component
struct NativeScriptComponent {
	ScriptableEntity* Instance = nullptr;
	std::string ScriptName = "Script";

	ScriptableEntity* (*InstantiateScript)() = nullptr;
	void (*DestroyScript)(NativeScriptComponent*) = nullptr;

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
};

// Collider Components
struct BoxColliderComponent {
	glm::vec3 Size{ 1.0f, 1.0f, 1.0f };
	glm::vec3 Center{ 0.0f, 0.0f, 0.0f };
	bool IsTrigger = false;

	BoxColliderComponent() = default;
	BoxColliderComponent(const glm::vec3& size, const glm::vec3& center = glm::vec3(0.0f), bool isTrigger = false)
		: Size(size), Center(center), IsTrigger(isTrigger) {}
};

struct SphereColliderComponent {
	float Radius = 0.5f;
	glm::vec3 Center{ 0.0f, 0.0f, 0.0f };
	bool IsTrigger = false;

	SphereColliderComponent() = default;
	SphereColliderComponent(float radius, const glm::vec3& center = glm::vec3(0.0f), bool isTrigger = false)
		: Radius(radius), Center(center), IsTrigger(isTrigger) {}
};

struct CapsuleColliderComponent {
	float Radius = 0.5f;
	float Height = 1.0f;
	glm::vec3 Center{ 0.0f, 0.0f, 0.0f };
	bool IsTrigger = false;

	CapsuleColliderComponent() = default;
	CapsuleColliderComponent(float radius, float height, const glm::vec3& center = glm::vec3(0.0f), bool isTrigger = false)
		: Radius(radius), Height(height), Center(center), IsTrigger(isTrigger) {}
};

struct MeshColliderComponent {
	bool IsTrigger = false;

	MeshColliderComponent() = default;
	explicit MeshColliderComponent(bool isTrigger) : IsTrigger(isTrigger) {}
};

