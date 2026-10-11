#pragma once

#include <glm/glm.hpp>
#include <string>
#include <memory>

#ifdef IGNIS_BUILD_MODULE
#include "ignis/api.h"

class Sound {
public:
	Sound() = default;
	~Sound()
	{
		if (m_NativeSound && Ignis::GetContext() && Ignis::GetContext()->Audio.DestroySound)
		{
			Ignis::GetContext()->Audio.DestroySound(m_NativeSound);
			m_NativeSound = nullptr;
		}
	}

	void Play() { if (m_NativeSound && Ignis::GetContext()) Ignis::GetContext()->Audio.PlaySound(m_NativeSound); }
	void Pause() { if (m_NativeSound && Ignis::GetContext()) Ignis::GetContext()->Audio.PauseSound(m_NativeSound); }
	void Stop() { if (m_NativeSound && Ignis::GetContext()) Ignis::GetContext()->Audio.StopSound(m_NativeSound); }
	[[nodiscard]] bool IsPlaying() const { return (m_NativeSound && Ignis::GetContext()) ? Ignis::GetContext()->Audio.IsSoundPlaying(m_NativeSound) : false; }

	void SetPosition(const glm::vec3& pos) { if (m_NativeSound && Ignis::GetContext()) Ignis::GetContext()->Audio.SetSoundPosition(m_NativeSound, pos); }
	void SetVelocity(const glm::vec3&) {}
	void SetDirection(const glm::vec3&) {}
	void SetVolume(float vol) { if (m_NativeSound && Ignis::GetContext()) Ignis::GetContext()->Audio.SetSoundVolume(m_NativeSound, vol); }
	void SetPitch(float p) { if (m_NativeSound && Ignis::GetContext()) Ignis::GetContext()->Audio.SetSoundPitch(m_NativeSound, p); }
	void SetLooping(bool loop) { if (m_NativeSound && Ignis::GetContext()) Ignis::GetContext()->Audio.SetSoundLooping(m_NativeSound, loop); }
	void SetSpatial(bool) {}
	void SetAttenuation(float, float, float = 1.0f) {}

	void SetNativeHandle(void* handle) { m_NativeSound = handle; }
	void* GetNativeHandle() const { return m_NativeSound; }

private:
	void* m_NativeSound = nullptr;
};

class AudioEngine {
public:
	static AudioEngine& Get()
	{
		static AudioEngine s_Instance;
		return s_Instance;
	}

	std::shared_ptr<Sound> CreateSound(const std::string& path, bool spatial = true, bool loop = false)
	{
		if (!Ignis::GetContext() || !Ignis::GetContext()->Audio.CreateSound)
			return nullptr;
		void* handle = Ignis::GetContext()->Audio.CreateSound(path.c_str(), spatial, loop);
		if (!handle) return nullptr;
		auto s = std::make_shared<Sound>();
		s->SetNativeHandle(handle);
		return s;
	}
};
#else

struct ma_engine;
struct ma_sound;

class Sound {
public:
	Sound();
	~Sound();

	Sound(const Sound&) = delete;
	Sound& operator=(const Sound&) = delete;

	bool Load(const std::string& filepath, bool spatial = true, bool loop = false);
	void Unload();

	void Play();
	void Pause();
	void Stop();

	[[nodiscard]] bool IsPlaying() const;
	[[nodiscard]] bool IsLoaded() const { return m_Loaded; }

	void SetPosition(const glm::vec3& position);
	void SetVelocity(const glm::vec3& velocity);
	void SetDirection(const glm::vec3& forward);

	void SetVolume(float volume);
	[[nodiscard]] float GetVolume() const { return m_Volume; }

	void SetPitch(float pitch);
	[[nodiscard]] float GetPitch() const { return m_Pitch; }

	void SetLooping(bool loop);
	[[nodiscard]] bool IsLooping() const { return m_Loop; }

	void SetSpatial(bool spatial);
	[[nodiscard]] bool IsSpatial() const { return m_Spatial; }

	void SetAttenuation(float minDistance, float maxDistance, float rollOff = 1.0f);
	[[nodiscard]] float GetMinDistance() const { return m_MinDistance; }
	[[nodiscard]] float GetMaxDistance() const { return m_MaxDistance; }
	[[nodiscard]] float GetRollOff() const { return m_RollOff; }

	[[nodiscard]] float GetCursorInSeconds() const;
	void SetCursorInSeconds(float seconds);
	[[nodiscard]] float GetLengthInSeconds() const;

	[[nodiscard]] const std::string& GetPath() const { return m_Path; }

private:
	std::unique_ptr<ma_sound> m_Sound;
	std::string m_Path;
	bool m_Loaded = false;
	bool m_Spatial = true;
	bool m_Loop = false;
	float m_Volume = 1.0f;
	float m_Pitch = 1.0f;
	float m_MinDistance = 1.0f;
	float m_MaxDistance = 50.0f;
	float m_RollOff = 1.0f;
};

class AudioEngine {
public:
	static AudioEngine& Get();

	bool Initialize();
	void Shutdown();
	[[nodiscard]] bool IsInitialized() const { return m_Initialized; }

	// Master controls
	void SetMasterVolume(float volume);
	[[nodiscard]] float GetMasterVolume() const { return m_MasterVolume; }

	// 3D Audio Listener
	void SetListenerPosition(const glm::vec3& position);
	void SetListenerDirection(const glm::vec3& forward, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));
	void SetListenerVelocity(const glm::vec3& velocity);
	[[nodiscard]] glm::vec3 GetListenerPosition() const { return m_ListenerPosition; }

	// One-shot sounds (fire-and-forget)
	void Play2D(const std::string& filepath, float volume = 1.0f, float pitch = 1.0f);
	void Play3D(const std::string& filepath, const glm::vec3& position, float volume = 1.0f, float pitch = 1.0f, float minDistance = 1.0f, float maxDistance = 50.0f);

	// Dedicated Sound factory
	std::shared_ptr<Sound> CreateSound(const std::string& filepath, bool spatial = true, bool loop = false);

	// Internal engine accessor
	ma_engine* GetEngineHandle() { return m_Engine.get(); }

private:
	AudioEngine();
	~AudioEngine();

	AudioEngine(const AudioEngine&) = delete;
	AudioEngine& operator=(const AudioEngine&) = delete;

	std::unique_ptr<ma_engine> m_Engine;
	bool m_Initialized = false;
	float m_MasterVolume = 1.0f;
	glm::vec3 m_ListenerPosition{ 0.0f };
};
#endif

