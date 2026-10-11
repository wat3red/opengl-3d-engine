#include "scene.h"
#include "light.h"
#include "scriptable_entity.h"
#include <unordered_map>

Scene::Scene()
	: m_SelectedEntity{}, m_Skybox(nullptr)
{
}

Scene::~Scene()
{
	OnRuntimeStop();
	Clear();
}

Entity Scene::CreateEntity(const std::string& name)
{
	entt::entity handle = m_Registry.create();
	Entity entity = { handle, this };
	entity.AddComponent<TagComponent>(name.empty() ? "Entity" : name);
	entity.AddComponent<TransformComponent>();
	return entity;
}

void Scene::DestroyEntity(Entity entity)
{
	if (entity == m_SelectedEntity)
	{
		m_SelectedEntity = {};
	}
	m_Registry.destroy(entity.GetHandle());
}

void Scene::OnRuntimeStart()
{
	// 1. Instantiate and start native scripts
	auto view = m_Registry.view<NativeScriptComponent>();
	for (auto [entityHandle, nsc] : view.each())
	{
		if (!nsc.Instance)
		{
			if (!nsc.InstantiateScript && !nsc.ScriptName.empty())
			{
				nsc.Bind(nsc.ScriptName);
			}

			if (nsc.InstantiateScript)
			{
				nsc.Instance = nsc.InstantiateScript();
				nsc.Instance->m_Entity = Entity{ entityHandle, this };
				nsc.Instance->OnCreate();
			}
		}
	}

	// 2. Start Audio Sources with PlayOnAwake
	{
		auto audioView = m_Registry.view<TagComponent, AudioSourceComponent>();
		for (auto [entityHandle, tag, audio] : audioView.each())
		{
			if (tag.Active && audio.PlayOnAwake)
			{
				if (m_Registry.all_of<TransformComponent>(entityHandle))
				{
					audio.UpdateTransform(m_Registry.get<TransformComponent>(entityHandle).Position);
				}
				audio.Play();
			}
		}
	}
}

void Scene::OnUpdateRuntime(float deltaTime)
{
	// 1. Update Scripts
	{
		auto view = m_Registry.view<TagComponent, NativeScriptComponent>();
		for (auto [entityHandle, tag, nsc] : view.each())
		{
			if (tag.Active && nsc.Instance)
			{
				nsc.Instance->OnUpdate(deltaTime);
			}
		}
	}

	// 2. Update Character Controllers (simulate gravity and grounding for unscripted controllers)
	{
		auto view = m_Registry.view<TagComponent, TransformComponent, CharacterControllerComponent>();
		for (auto [entityHandle, tag, transform, cc] : view.each())
		{
			if (tag.Active && deltaTime > 0.0f)
			{
				bool hasScript = m_Registry.all_of<NativeScriptComponent>(entityHandle);
				if (!hasScript)
				{
					cc.SimpleMove(*this, Entity{ entityHandle, this }, glm::vec3(0.0f), deltaTime);
				}
			}
		}
	}

	// 3. Update Physics Rigidbody
	{
		auto view = m_Registry.view<TagComponent, TransformComponent, RigidbodyComponent>();
		for (auto [entityHandle, tag, transform, rb] : view.each())
		{
			if (tag.Active && rb.Body && deltaTime > 0.0f)
			{
				if (rb.UseGravity)
				{
					rb.Body->ApplyForce(glm::vec3(0.0f, -9.81f * rb.Mass, 0.0f));
				}
				rb.Body->Update(deltaTime);
				if (glm::length(rb.Body->GetVelocity()) > 0.0001f || rb.UseGravity)
				{
					transform.Position = rb.Body->GetPosition();
				}
			}
		}
	}

	// 4. Update Audio Listener and Spatial Sources
	{
		Entity listenerEntity = {};
		auto listenerView = m_Registry.view<TagComponent, AudioListenerComponent, TransformComponent>();
		for (auto [entityHandle, tag, listener, tc] : listenerView.each())
		{
			if (tag.Active && listener.Primary)
			{
				listenerEntity = Entity{ entityHandle, this };
				break;
			}
		}

		if (listenerEntity)
		{
			auto& tc = listenerEntity.GetTransform();
			glm::vec3 forward = DirectionFromEuler(tc.Rotation);
			AudioEngine::Get().SetListenerPosition(tc.Position);
			AudioEngine::Get().SetListenerDirection(forward, glm::vec3(0.0f, 1.0f, 0.0f));
		}
		else
		{
			Entity primaryCam = GetPrimaryCameraEntity();
			if (primaryCam && primaryCam.HasComponent<TransformComponent>())
			{
				auto& tc = primaryCam.GetTransform();
				glm::vec3 forward = DirectionFromEuler(tc.Rotation);
				AudioEngine::Get().SetListenerPosition(tc.Position);
				AudioEngine::Get().SetListenerDirection(forward, glm::vec3(0.0f, 1.0f, 0.0f));
			}
		}

		// Update 3D Audio Sources positions
		auto audioView = m_Registry.view<TagComponent, TransformComponent, AudioSourceComponent>();
		for (auto [entityHandle, tag, tc, audio] : audioView.each())
		{
			if (tag.Active && audio.Spatial)
			{
				audio.UpdateTransform(tc.Position);
			}
		}
	}
}

void Scene::OnRuntimeGUI()
{
	auto view = m_Registry.view<TagComponent, NativeScriptComponent>();
	for (auto [entityHandle, tag, nsc] : view.each())
	{
		if (tag.Active && nsc.Instance)
		{
			nsc.Instance->OnGUI();
		}
	}
}

void Scene::OnRuntimeStop()
{
	// 1. Destroy running script instances
	auto view = m_Registry.view<NativeScriptComponent>();
	for (auto [entityHandle, nsc] : view.each())
	{
		if (nsc.Instance)
		{
			nsc.Instance->OnDestroy();
			if (nsc.DestroyScript)
			{
				nsc.DestroyScript(&nsc);
			}
			nsc.Instance = nullptr;
		}
	}

	// 2. Stop all audio playback
	auto audioView = m_Registry.view<AudioSourceComponent>();
	for (auto [entityHandle, audio] : audioView.each())
	{
		audio.Stop();
	}
}

void Scene::OnUpdateEditor(float deltaTime)
{
	// Editor simulation update (no gameplay scripts run)
}

void Scene::Update(float deltaTime)
{
	OnUpdateRuntime(deltaTime);
}



void Scene::DuplicateFrom(const Scene& other)
{
	Clear();
	m_Skybox = other.m_Skybox;

	std::unordered_map<entt::entity, Entity> entMap;

	auto view = other.m_Registry.view<TagComponent>();
	for (auto srcHandle : view)
	{
		const auto& tag = other.m_Registry.get<TagComponent>(srcHandle);
		Entity dstEntity = CreateEntity(tag.Tag);
		dstEntity.GetComponent<TagComponent>().Active = tag.Active;
		entMap[srcHandle] = dstEntity;

		if (other.m_Registry.all_of<TransformComponent>(srcHandle))
		{
			dstEntity.GetComponent<TransformComponent>() = other.m_Registry.get<TransformComponent>(srcHandle);
		}

		if (other.m_Registry.all_of<MeshComponent>(srcHandle))
		{
			const auto& srcMesh = other.m_Registry.get<MeshComponent>(srcHandle);
			dstEntity.AddComponent<MeshComponent>(srcMesh.ModelPath, srcMesh.FlipUVs, srcMesh.CastsShadow);
		}

		if (other.m_Registry.all_of<LightComponent>(srcHandle))
		{
			dstEntity.AddComponent<LightComponent>(other.m_Registry.get<LightComponent>(srcHandle));
		}

		if (other.m_Registry.all_of<CameraComponent>(srcHandle))
		{
			dstEntity.AddComponent<CameraComponent>(other.m_Registry.get<CameraComponent>(srcHandle));
		}

		if (other.m_Registry.all_of<BoxColliderComponent>(srcHandle))
		{
			dstEntity.AddComponent<BoxColliderComponent>(other.m_Registry.get<BoxColliderComponent>(srcHandle));
		}

		if (other.m_Registry.all_of<SphereColliderComponent>(srcHandle))
		{
			dstEntity.AddComponent<SphereColliderComponent>(other.m_Registry.get<SphereColliderComponent>(srcHandle));
		}

		if (other.m_Registry.all_of<CapsuleColliderComponent>(srcHandle))
		{
			dstEntity.AddComponent<CapsuleColliderComponent>(other.m_Registry.get<CapsuleColliderComponent>(srcHandle));
		}

		if (other.m_Registry.all_of<MeshColliderComponent>(srcHandle))
		{
			dstEntity.AddComponent<MeshColliderComponent>(other.m_Registry.get<MeshColliderComponent>(srcHandle));
		}

		if (other.m_Registry.all_of<CharacterControllerComponent>(srcHandle))
		{
			dstEntity.AddComponent<CharacterControllerComponent>(other.m_Registry.get<CharacterControllerComponent>(srcHandle));
		}

		if (other.m_Registry.all_of<RigidbodyComponent>(srcHandle))
		{
			const auto& srcRb = other.m_Registry.get<RigidbodyComponent>(srcHandle);
			auto& dstRb = dstEntity.AddComponent<RigidbodyComponent>();
			dstRb.Mass = srcRb.Mass;
			dstRb.Velocity = srcRb.Velocity;
			dstRb.UseGravity = srcRb.UseGravity;
			if (dstEntity.HasComponent<MeshComponent>() && dstEntity.GetComponent<MeshComponent>().ModelAsset)
			{
				dstRb.Body = std::make_unique<Rigidbody>(dstEntity.GetComponent<MeshComponent>().ModelAsset.get());
				dstRb.Body->SetPosition(dstEntity.GetPosition());
				dstRb.Body->SetVelocity(srcRb.Velocity);
				dstRb.Body->SetMass(srcRb.Mass);
			}
		}

		if (other.m_Registry.all_of<MaterialComponent>(srcHandle))
		{
			dstEntity.AddComponent<MaterialComponent>(other.m_Registry.get<MaterialComponent>(srcHandle));
		}

		if (other.m_Registry.all_of<AudioSourceComponent>(srcHandle))
		{
			const auto& srcAudio = other.m_Registry.get<AudioSourceComponent>(srcHandle);
			auto& dstAudio = dstEntity.AddComponent<AudioSourceComponent>();
			dstAudio.SoundPath = srcAudio.SoundPath;
			dstAudio.Volume = srcAudio.Volume;
			dstAudio.Pitch = srcAudio.Pitch;
			dstAudio.Loop = srcAudio.Loop;
			dstAudio.PlayOnAwake = srcAudio.PlayOnAwake;
			dstAudio.Spatial = srcAudio.Spatial;
			dstAudio.MinDistance = srcAudio.MinDistance;
			dstAudio.MaxDistance = srcAudio.MaxDistance;
			dstAudio.RollOff = srcAudio.RollOff;
			if (!dstAudio.SoundPath.empty())
			{
				dstAudio.LoadSound(dstAudio.SoundPath);
			}
		}

		if (other.m_Registry.all_of<AudioListenerComponent>(srcHandle))
		{
			dstEntity.AddComponent<AudioListenerComponent>(other.m_Registry.get<AudioListenerComponent>(srcHandle));
		}

		if (other.m_Registry.all_of<NativeScriptComponent>(srcHandle))
		{
			const auto& srcNsc = other.m_Registry.get<NativeScriptComponent>(srcHandle);
			auto& dstNsc = dstEntity.AddComponent<NativeScriptComponent>();
			dstNsc.ScriptName = srcNsc.ScriptName;
			dstNsc.InstantiateScript = srcNsc.InstantiateScript;
			dstNsc.DestroyScript = srcNsc.DestroyScript;
			dstNsc.Instance = nullptr;
		}
	}

	if (other.m_SelectedEntity)
	{
		auto it = entMap.find(other.m_SelectedEntity.GetHandle());
		if (it != entMap.end())
		{
			m_SelectedEntity = it->second;
		}
	}
}

std::shared_ptr<Scene> Scene::Copy(const std::shared_ptr<Scene>& other)
{
	if (!other) return nullptr;
	auto newScene = std::make_shared<Scene>();
	newScene->DuplicateFrom(*other);
	return newScene;
}

void Scene::Clear()
{
	OnRuntimeStop();
	m_Registry.clear();
	m_SelectedEntity = {};
	m_Skybox.reset();
}

Entity Scene::GetSunLight()
{
	auto view = m_Registry.view<TagComponent, LightComponent>();
	for (auto [entityHandle, tag, light] : view.each())
	{
		if (tag.Active && light.Type == LightType::Directional)
		{
			return { entityHandle, this };
		}
	}
	return {};
}

Entity Scene::CreateModelEntity(const std::string& name, const std::string& modelPath, bool flipUVs)
{
	Entity entity = CreateEntity(name);
	entity.AddComponent<MeshComponent>(modelPath, flipUVs);
	if (!m_SelectedEntity)
	{
		m_SelectedEntity = entity;
	}
	return entity;
}

Entity Scene::CreateDirectionalLight(const std::string& name, const glm::vec3& direction, const glm::vec3& color, float intensity)
{
	Entity entity = CreateEntity(name);
	entity.SetRotation(EulerFromDirection(direction));
	entity.AddComponent<LightComponent>(LightComponent::CreateDirectional(direction, color, intensity));
	if (!m_SelectedEntity)
	{
		m_SelectedEntity = entity;
	}
	return entity;
}

Entity Scene::CreatePointLight(const std::string& name, const glm::vec3& position, const glm::vec3& color, float intensity, float range)
{
	Entity entity = CreateEntity(name);
	entity.SetPosition(position);
	entity.AddComponent<LightComponent>(LightComponent::CreatePoint(color, intensity, range));
	if (!m_SelectedEntity)
	{
		m_SelectedEntity = entity;
	}
	return entity;
}

Entity Scene::CreateSpotLight(const std::string& name, const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity, float innerCutoff, float outerCutoff, float range)
{
	Entity entity = CreateEntity(name);
	entity.SetPosition(position);
	entity.SetRotation(EulerFromDirection(direction));
	entity.AddComponent<LightComponent>(LightComponent::CreateSpot(direction, color, intensity, innerCutoff, outerCutoff, range));
	if (!m_SelectedEntity)
	{
		m_SelectedEntity = entity;
	}
	return entity;
}

std::vector<Entity> Scene::GetAllEntitiesWithMesh()
{
	std::vector<Entity> result;
	auto view = m_Registry.view<MeshComponent>();
	result.reserve(view.size());
	for (auto entityHandle : view)
	{
		result.emplace_back(entityHandle, this);
	}
	return result;
}

std::vector<Entity> Scene::GetAllEntitiesWithLight()
{
	std::vector<Entity> result;
	auto view = m_Registry.view<LightComponent>();
	result.reserve(view.size());
	for (auto entityHandle : view)
	{
		result.emplace_back(entityHandle, this);
	}
	return result;
}
