#pragma once

#ifdef _WIN32
    #define IGNIS_EXPORT extern "C" __declspec(dllexport)
    #define IGNIS_API __declspec(dllexport)
#else
    #define IGNIS_EXPORT extern "C"
    #define IGNIS_API
#endif

#include <glm/glm.hpp>
#include <cstdint>

// Forward declaration of ScriptableEntity in global namespace
class ScriptableEntity;

namespace Ignis {

using ScriptableEntity = ::ScriptableEntity;

// Raw POD structures for cross-DLL binary compatibility
struct RawRaycastHit {
    bool Hit;
    float Distance;
    glm::vec3 Point;
    glm::vec3 Normal;
    uint32_t EntityHandle;
};

struct InputContext {
    bool (*IsKeyPressed)(int keyCode);
    bool (*IsMouseButtonPressed)(int mouseCode);
    glm::vec2 (*GetMouseDelta)();
    void (*SetCursorMode)(int mode);
};

struct AudioContext {
    void* (*CreateSound)(const char* filepath, bool is3D, bool loop);
    void (*DestroySound)(void* sound);
    void (*PlaySound)(void* sound);
    void (*PauseSound)(void* sound);
    void (*StopSound)(void* sound);
    void (*SetSoundPosition)(void* sound, const glm::vec3& position);
    void (*SetSoundVolume)(void* sound, float volume);
    void (*SetSoundPitch)(void* sound, float pitch);
    void (*SetSoundLooping)(void* sound, bool loop);
    bool (*IsSoundPlaying)(void* sound);
};

struct PhysicsContext {
    bool (*Raycast)(void* scene, const glm::vec3& origin, const glm::vec3& dir, float maxDist, RawRaycastHit* outHit, uint32_t layerMask, bool includeTriggers);
    int (*RaycastAll)(void* scene, const glm::vec3& origin, const glm::vec3& dir, float maxDist, RawRaycastHit* outHits, int maxHits, uint32_t layerMask, bool includeTriggers);
    void (*CharacterControllerSimpleMove)(void* cc, void* scene, uint32_t entityHandle, const glm::vec3& horiz, float dt);
    void (*CharacterControllerJump)(void* cc, float force);
};

using ScriptCreatorFn = ::ScriptableEntity* (*)();

struct EngineContext {
    InputContext Input;
    AudioContext Audio;
    PhysicsContext Physics;
    void* ImGuiContext;
    void (*RegisterScript)(const char* name, ScriptCreatorFn creator);
    void (*UnregisterScript)(const char* name);
    void (*LogMessage)(int level, const char* message);
};

inline EngineContext* g_Context = nullptr;

inline EngineContext* GetContext() { return g_Context; }
inline void SetContext(EngineContext* ctx) { g_Context = ctx; }

} // namespace Ignis

#define IGNIS_GAME_MODULE(ModuleName) \
    void RegisterModuleScripts(); \
    extern "C" __declspec(dllexport) void InitializeGameModule(Ignis::EngineContext* context); \
    extern "C" __declspec(dllexport) void ShutdownGameModule(); \
    extern "C" __declspec(dllexport) const char* GetGameModuleName() { return ModuleName; } \
    void ShutdownGameModule() {} \
    void InitializeGameModule(Ignis::EngineContext* context) { \
        Ignis::SetContext(context); \
        if (context && context->ImGuiContext) { \
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(context->ImGuiContext)); \
        } \
        RegisterModuleScripts(); \
    } \
    void RegisterModuleScripts()

#define IGNIS_REGISTER_SCRIPT(Type) \
    if (Ignis::GetContext() && Ignis::GetContext()->RegisterScript) { \
        Ignis::GetContext()->RegisterScript(#Type, []() -> ::ScriptableEntity* { \
            return static_cast<::ScriptableEntity*>(new Type()); \
        }); \
    }
