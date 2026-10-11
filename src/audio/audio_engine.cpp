#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"
#include "audio_engine.h"

#include <print>
#include <filesystem>

Sound::Sound()
	: m_Sound(std::make_unique<ma_sound>())
{
}

Sound::~Sound()
{
	Unload();
}

bool Sound::Load(const std::string& filepath, bool spatial, bool loop)
{
	Unload();

	if (filepath.empty())
		return false;

	std::error_code ec;
	if (!std::filesystem::exists(filepath, ec))
	{
		std::println(stderr, "[Audio] Error: Sound file '{}' not found on disk.", filepath);
		return false;
	}

	auto* engine = AudioEngine::Get().GetEngineHandle();
	if (!engine)
	{
		std::println(stderr, "[Audio] Error: AudioEngine handle is null when loading '{}'.", filepath);
		return false;
	}

	ma_uint32 flags = 0;
	if (!spatial)
	{
		flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
	}

	ma_result result = ma_sound_init_from_file(engine, filepath.c_str(), flags, nullptr, nullptr, m_Sound.get());
	if (result != MA_SUCCESS)
	{
		std::println(stderr, "[Audio] Failed to load sound '{}' (error code: {}).", filepath, static_cast<int>(result));
		return false;
	}

	m_Path = filepath;
	m_Spatial = spatial;
	m_Loop = loop;
	m_Loaded = true;

	ma_sound_set_looping(m_Sound.get(), loop ? MA_TRUE : MA_FALSE);
	ma_sound_set_spatialization_enabled(m_Sound.get(), spatial ? MA_TRUE : MA_FALSE);
	ma_sound_set_min_distance(m_Sound.get(), m_MinDistance);
	ma_sound_set_max_distance(m_Sound.get(), m_MaxDistance);
	ma_sound_set_rolloff(m_Sound.get(), m_RollOff);
	ma_sound_set_volume(m_Sound.get(), m_Volume);
	ma_sound_set_pitch(m_Sound.get(), m_Pitch);

	return true;
}

void Sound::Unload()
{
	if (m_Loaded && m_Sound)
	{
		ma_sound_stop(m_Sound.get());
		ma_sound_uninit(m_Sound.get());
		m_Loaded = false;
	}
}

void Sound::Play()
{
	if (!m_Loaded || !m_Sound) return;

	if (ma_sound_at_end(m_Sound.get()))
	{
		ma_sound_seek_to_second(m_Sound.get(), 0.0f);
	}
	ma_sound_start(m_Sound.get());
}

void Sound::Pause()
{
	if (!m_Loaded || !m_Sound) return;
	ma_sound_stop(m_Sound.get());
}

void Sound::Stop()
{
	if (!m_Loaded || !m_Sound) return;
	ma_sound_stop(m_Sound.get());
	ma_sound_seek_to_second(m_Sound.get(), 0.0f);
}

bool Sound::IsPlaying() const
{
	if (!m_Loaded || !m_Sound) return false;
	return ma_sound_is_playing(m_Sound.get()) == MA_TRUE;
}

void Sound::SetPosition(const glm::vec3& position)
{
	if (!m_Loaded || !m_Sound) return;
	ma_sound_set_position(m_Sound.get(), position.x, position.y, position.z);
}

void Sound::SetVelocity(const glm::vec3& velocity)
{
	if (!m_Loaded || !m_Sound) return;
	ma_sound_set_velocity(m_Sound.get(), velocity.x, velocity.y, velocity.z);
}

void Sound::SetDirection(const glm::vec3& forward)
{
	if (!m_Loaded || !m_Sound) return;
	ma_sound_set_direction(m_Sound.get(), forward.x, forward.y, forward.z);
}

void Sound::SetVolume(float volume)
{
	m_Volume = volume;
	if (m_Loaded && m_Sound)
	{
		ma_sound_set_volume(m_Sound.get(), volume);
	}
}

void Sound::SetPitch(float pitch)
{
	m_Pitch = pitch;
	if (m_Loaded && m_Sound)
	{
		ma_sound_set_pitch(m_Sound.get(), pitch);
	}
}

void Sound::SetLooping(bool loop)
{
	m_Loop = loop;
	if (m_Loaded && m_Sound)
	{
		ma_sound_set_looping(m_Sound.get(), loop ? MA_TRUE : MA_FALSE);
	}
}

void Sound::SetSpatial(bool spatial)
{
	m_Spatial = spatial;
	if (m_Loaded && m_Sound)
	{
		ma_sound_set_spatialization_enabled(m_Sound.get(), spatial ? MA_TRUE : MA_FALSE);
	}
}

void Sound::SetAttenuation(float minDistance, float maxDistance, float rollOff)
{
	m_MinDistance = minDistance;
	m_MaxDistance = maxDistance;
	m_RollOff = rollOff;
	if (m_Loaded && m_Sound)
	{
		ma_sound_set_min_distance(m_Sound.get(), minDistance);
		ma_sound_set_max_distance(m_Sound.get(), maxDistance);
		ma_sound_set_rolloff(m_Sound.get(), rollOff);
	}
}

float Sound::GetCursorInSeconds() const
{
	if (!m_Loaded || !m_Sound) return 0.0f;
	float cursor = 0.0f;
	ma_sound_get_cursor_in_seconds(m_Sound.get(), &cursor);
	return cursor;
}

void Sound::SetCursorInSeconds(float seconds)
{
	if (!m_Loaded || !m_Sound) return;
	ma_sound_seek_to_second(m_Sound.get(), seconds);
}

float Sound::GetLengthInSeconds() const
{
	if (!m_Loaded || !m_Sound) return 0.0f;
	float length = 0.0f;
	ma_sound_get_length_in_seconds(m_Sound.get(), &length);
	return length;
}

// ----------------- AudioEngine Implementation -----------------

AudioEngine& AudioEngine::Get()
{
	static AudioEngine s_Instance;
	return s_Instance;
}

AudioEngine::AudioEngine()
	: m_Engine(std::make_unique<ma_engine>())
{
}

AudioEngine::~AudioEngine()
{
	Shutdown();
}

bool AudioEngine::Initialize()
{
	if (m_Initialized)
		return true;

	ma_result result = ma_engine_init(nullptr, m_Engine.get());
	if (result != MA_SUCCESS)
	{
		std::println(stderr, "[AudioEngine] Failed to initialize miniaudio engine (error: {}).", static_cast<int>(result));
		return false;
	}

	m_Initialized = true;
	std::println("[AudioEngine] Miniaudio 3D positional engine initialized successfully.");
	return true;
}

void AudioEngine::Shutdown()
{
	if (m_Initialized && m_Engine)
	{
		ma_engine_uninit(m_Engine.get());
		m_Initialized = false;
		std::println("[AudioEngine] Shutdown cleanly.");
	}
}

void AudioEngine::SetMasterVolume(float volume)
{
	m_MasterVolume = volume;
	if (m_Initialized && m_Engine)
	{
		ma_engine_set_volume(m_Engine.get(), volume);
	}
}

void AudioEngine::SetListenerPosition(const glm::vec3& position)
{
	m_ListenerPosition = position;
	if (m_Initialized && m_Engine)
	{
		ma_engine_listener_set_position(m_Engine.get(), 0, position.x, position.y, position.z);
	}
}

void AudioEngine::SetListenerDirection(const glm::vec3& forward, const glm::vec3& up)
{
	if (m_Initialized && m_Engine)
	{
		ma_engine_listener_set_direction(m_Engine.get(), 0, forward.x, forward.y, forward.z);
		ma_engine_listener_set_world_up(m_Engine.get(), 0, up.x, up.y, up.z);
	}
}

void AudioEngine::SetListenerVelocity(const glm::vec3& velocity)
{
	if (m_Initialized && m_Engine)
	{
		ma_engine_listener_set_velocity(m_Engine.get(), 0, velocity.x, velocity.y, velocity.z);
	}
}

void AudioEngine::Play2D(const std::string& filepath, float volume, float pitch)
{
	if (!m_Initialized || !m_Engine) return;
	auto sound = CreateSound(filepath, false, false);
	if (sound)
	{
		sound->SetVolume(volume);
		sound->SetPitch(pitch);
		sound->Play();
	}
}

void AudioEngine::Play3D(const std::string& filepath, const glm::vec3& position, float volume, float pitch, float minDistance, float maxDistance)
{
	if (!m_Initialized || !m_Engine) return;
	auto sound = CreateSound(filepath, true, false);
	if (sound)
	{
		sound->SetPosition(position);
		sound->SetVolume(volume);
		sound->SetPitch(pitch);
		sound->SetAttenuation(minDistance, maxDistance);
		sound->Play();
	}
}

std::shared_ptr<Sound> AudioEngine::CreateSound(const std::string& filepath, bool spatial, bool loop)
{
	auto sound = std::make_shared<Sound>();
	if (sound->Load(filepath, spatial, loop))
	{
		return sound;
	}
	return nullptr;
}
