#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "game_module.h"
#include "core/input.h"
#include "audio/audio_engine.h"
#include "physics/collision.h"
#include "scene/scene.h"
#include "scene/script_registry.h"
#include "scene/components.h"
#include <imgui/imgui.h>
#include <filesystem>
#include <print>
#include <algorithm>

GameModule& GameModule::Get()
{
	static GameModule s_Instance;
	return s_Instance;
}

GameModule::GameModule()
{
	SetupContext();
}

GameModule::~GameModule()
{
	Unload();
}

void GameModule::SetupContext()
{
	// Input Context
	m_Context.Input.IsKeyPressed = [](int keyCode) -> bool {
		return Input::IsKeyPressed(static_cast<KeyCode>(keyCode));
	};
	m_Context.Input.IsMouseButtonPressed = [](int mouseCode) -> bool {
		return Input::IsMouseButtonPressed(static_cast<MouseCode>(mouseCode));
	};
	m_Context.Input.GetMouseDelta = []() -> glm::vec2 {
		return Input::GetMouseDelta();
	};
	m_Context.Input.SetCursorMode = [](int mode) {
		Input::SetCursorMode(static_cast<CursorMode>(mode));
	};

	// Audio Context
	m_Context.Audio.CreateSound = [](const char* filepath, bool is3D, bool loop) -> void* {
		auto sound = AudioEngine::Get().CreateSound(filepath, is3D, loop);
		if (!sound) return nullptr;
		return new std::shared_ptr<Sound>(sound);
	};
	m_Context.Audio.DestroySound = [](void* handle) {
		if (handle) {
			auto* sp = static_cast<std::shared_ptr<Sound>*>(handle);
			delete sp;
		}
	};
	m_Context.Audio.PlaySound = [](void* handle) {
		if (handle) {
			auto* sp = static_cast<std::shared_ptr<Sound>*>(handle);
			if (sp && *sp) (*sp)->Play();
		}
	};
	m_Context.Audio.PauseSound = [](void* handle) {
		if (handle) {
			auto* sp = static_cast<std::shared_ptr<Sound>*>(handle);
			if (sp && *sp) (*sp)->Pause();
		}
	};
	m_Context.Audio.StopSound = [](void* handle) {
		if (handle) {
			auto* sp = static_cast<std::shared_ptr<Sound>*>(handle);
			if (sp && *sp) (*sp)->Stop();
		}
	};
	m_Context.Audio.SetSoundPosition = [](void* handle, const glm::vec3& position) {
		if (handle) {
			auto* sp = static_cast<std::shared_ptr<Sound>*>(handle);
			if (sp && *sp) (*sp)->SetPosition(position);
		}
	};
	m_Context.Audio.SetSoundVolume = [](void* handle, float volume) {
		if (handle) {
			auto* sp = static_cast<std::shared_ptr<Sound>*>(handle);
			if (sp && *sp) (*sp)->SetVolume(volume);
		}
	};
	m_Context.Audio.SetSoundPitch = [](void* handle, float pitch) {
		if (handle) {
			auto* sp = static_cast<std::shared_ptr<Sound>*>(handle);
			if (sp && *sp) (*sp)->SetPitch(pitch);
		}
	};
	m_Context.Audio.SetSoundLooping = [](void* handle, bool loop) {
		if (handle) {
			auto* sp = static_cast<std::shared_ptr<Sound>*>(handle);
			if (sp && *sp) (*sp)->SetLooping(loop);
		}
	};
	m_Context.Audio.IsSoundPlaying = [](void* handle) -> bool {
		if (handle) {
			auto* sp = static_cast<std::shared_ptr<Sound>*>(handle);
			return (sp && *sp) ? (*sp)->IsPlaying() : false;
		}
		return false;
	};

	// Physics Context
	m_Context.Physics.Raycast = [](void* scene, const glm::vec3& origin, const glm::vec3& dir, float maxDist, Ignis::RawRaycastHit* outHit, uint32_t layerMask, bool includeTriggers) -> bool {
		if (!scene) return false;
		auto* sc = static_cast<Scene*>(scene);
		Physics::Ray ray(origin, dir);
		Physics::RaycastHit hit;
		bool res = Physics::Raycast(*sc, ray, maxDist, hit, layerMask, includeTriggers);
		if (res && outHit) {
			outHit->Hit = hit.Hit;
			outHit->Distance = hit.Distance;
			outHit->Point = hit.Point;
			outHit->Normal = hit.Normal;
			outHit->EntityHandle = static_cast<uint32_t>(hit.HitEntity.GetHandle());
		}
		return res;
	};

	m_Context.Physics.RaycastAll = [](void* scene, const glm::vec3& origin, const glm::vec3& dir, float maxDist, Ignis::RawRaycastHit* outHits, int maxHits, uint32_t layerMask, bool includeTriggers) -> int {
		if (!scene || !outHits || maxHits <= 0) return 0;
		auto* sc = static_cast<Scene*>(scene);
		Physics::Ray ray(origin, dir);
		std::vector<Physics::RaycastHit> hits;
		if (Physics::RaycastAll(*sc, ray, maxDist, hits, layerMask, includeTriggers)) {
			int count = std::min(maxHits, static_cast<int>(hits.size()));
			for (int i = 0; i < count; i++) {
				outHits[i].Hit = hits[i].Hit;
				outHits[i].Distance = hits[i].Distance;
				outHits[i].Point = hits[i].Point;
				outHits[i].Normal = hits[i].Normal;
				outHits[i].EntityHandle = static_cast<uint32_t>(hits[i].HitEntity.GetHandle());
			}
			return count;
		}
		return 0;
	};

	m_Context.Physics.CharacterControllerSimpleMove = [](void* cc, void* scene, uint32_t entityHandle, const glm::vec3& horiz, float dt) {
		if (!cc || !scene) return;
		auto* comp = static_cast<CharacterControllerComponent*>(cc);
		auto* sc = static_cast<Scene*>(scene);
		Entity ent{ static_cast<entt::entity>(entityHandle), sc };
		comp->SimpleMove(*sc, ent, horiz, dt);
	};

	m_Context.Physics.CharacterControllerJump = [](void* cc, float force) {
		if (cc) {
			auto* comp = static_cast<CharacterControllerComponent*>(cc);
			comp->Jump(force);
		}
	};

	// ImGui context
	m_Context.ImGuiContext = ImGui::GetCurrentContext();
	m_Context.GetImGuiContext = []() -> void* {
		return ImGui::GetCurrentContext();
	};
	ImGuiMemAllocFunc allocFunc = nullptr;
	ImGuiMemFreeFunc freeFunc = nullptr;
	void* userData = nullptr;
	ImGui::GetAllocatorFunctions(&allocFunc, &freeFunc, &userData);
	m_Context.ImGuiMemAllocFunc = reinterpret_cast<void*>(allocFunc);
	m_Context.ImGuiMemFreeFunc = reinterpret_cast<void*>(freeFunc);
	m_Context.ImGuiMemUserData = userData;

	// Script registration
	m_Context.RegisterScript = [](const char* name, Ignis::ScriptCreatorFn creator) {
		if (name && creator) {
			std::string scriptName = name;
			ScriptRegistry::RegisterCustom(scriptName, [creator]() -> ScriptableEntity* {
				return creator();
			});
			std::println("[GameModule] Registered script: {}", scriptName);
		}
	};

	m_Context.UnregisterScript = [](const char* name) {
		// Handled on module unload
	};

	m_Context.LogMessage = [](int level, const char* message) {
		if (message) {
			std::println("[GameModule] {}", message);
		}
	};
}

bool GameModule::Load(const std::string& dllPath)
{
	namespace fs = std::filesystem;
	std::error_code ec;

	Unload();

	m_ModulePath = dllPath;

	// Check candidate paths: dllPath, projects/Doom/bin/Game.dll, etc.
	std::string targetPath = dllPath;
	if (!fs::exists(targetPath, ec)) {
		if (fs::exists("projects/Doom/bin/Game.dll", ec)) {
			targetPath = "projects/Doom/bin/Game.dll";
		} else if (fs::exists("bin/Game.dll", ec)) {
			targetPath = "bin/Game.dll";
		} else if (fs::exists("projects/Doom/Game.dll", ec)) {
			targetPath = "projects/Doom/Game.dll";
		}
	}

	if (!fs::exists(targetPath, ec)) {
		std::println("[GameModule] Module DLL not found at: {}", dllPath);
		return false;
	}

	// Copy to runtime file to prevent locking target file during rebuilds in Visual Studio
	m_RuntimePath = fs::path(targetPath).parent_path().string();
	if (!m_RuntimePath.empty()) m_RuntimePath += "/";
	m_RuntimePath += "Game_runtime.dll";

	fs::copy_file(targetPath, m_RuntimePath, fs::copy_options::overwrite_existing, ec);
	if (ec) {
		// Fallback to loading original file directly if copy fails
		m_RuntimePath = targetPath;
	}

	m_Module = LoadLibraryA(m_RuntimePath.c_str());
	if (!m_Module) {
		DWORD err = GetLastError();
		std::println("[GameModule] Failed to load DLL '{}'. Error code: {}", m_RuntimePath, err);
		return false;
	}

	// Refresh ImGui context pointer before initializing
	m_Context.ImGuiContext = ImGui::GetCurrentContext();

	using GetNameFn = const char* (*)();
	GetNameFn getName = reinterpret_cast<GetNameFn>(GetProcAddress(static_cast<HMODULE>(m_Module), "GetGameModuleName"));
	if (getName) {
		m_ModuleName = getName();
	} else {
		m_ModuleName = fs::path(targetPath).stem().string();
	}

	using InitFn = void (*)(Ignis::EngineContext*);
	InitFn init = reinterpret_cast<InitFn>(GetProcAddress(static_cast<HMODULE>(m_Module), "InitializeGameModule"));
	if (!init) {
		std::println("[GameModule] Missing InitializeGameModule entry point in '{}'!", m_RuntimePath);
		FreeLibrary(static_cast<HMODULE>(m_Module));
		m_Module = nullptr;
		return false;
	}

	m_SetImGuiContextFn = reinterpret_cast<SetImGuiContextFn>(GetProcAddress(static_cast<HMODULE>(m_Module), "SetGameModuleImGuiContext"));

	init(&m_Context);
	SyncImGuiContext();

	std::println("[GameModule] Successfully loaded game module '{}' from '{}'", m_ModuleName, targetPath);
	return true;
}

void GameModule::Unload()
{
	if (m_Module) {
		using ShutdownFn = void (*)();
		ShutdownFn shutdown = reinterpret_cast<ShutdownFn>(GetProcAddress(static_cast<HMODULE>(m_Module), "ShutdownGameModule"));
		if (shutdown) {
			shutdown();
		}

		FreeLibrary(static_cast<HMODULE>(m_Module));
		m_Module = nullptr;
		m_SetImGuiContextFn = nullptr;
		m_ModuleName = "None";
		std::println("[GameModule] Unloaded game module.");
	}
}

bool GameModule::Reload()
{
	std::string path = m_ModulePath;
	ScriptRegistry::Clear();
	return Load(path);
}

void GameModule::SyncImGuiContext(void* ctx)
{
	if (!ctx) ctx = ImGui::GetCurrentContext();
	m_Context.ImGuiContext = ctx;

	ImGuiMemAllocFunc allocFunc = nullptr;
	ImGuiMemFreeFunc freeFunc = nullptr;
	void* userData = nullptr;
	ImGui::GetAllocatorFunctions(&allocFunc, &freeFunc, &userData);
	m_Context.ImGuiMemAllocFunc = reinterpret_cast<void*>(allocFunc);
	m_Context.ImGuiMemFreeFunc = reinterpret_cast<void*>(freeFunc);
	m_Context.ImGuiMemUserData = userData;

	if (m_SetImGuiContextFn && ctx) {
		m_SetImGuiContextFn(ctx, m_Context.ImGuiMemAllocFunc, m_Context.ImGuiMemFreeFunc, m_Context.ImGuiMemUserData);
	}
}
