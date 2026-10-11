#pragma once

#include "window.h"
#include "renderer.h"
#include "scene.h"
#include "editor.h"
#include "project_packager.h"
#include "framebuffer.h"
#include "bloom_renderer.h"
#include "shader.h"

#include <memory>

class Engine {
public:
	explicit Engine(int argc = 0, char** argv = nullptr);
	~Engine();

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

	void Run();
	void Close();

	[[nodiscard]] static Engine& Get() { return *s_Instance; }

	[[nodiscard]] AppMode GetMode() const { return m_Config.Mode; }
	[[nodiscard]] bool IsStandalone() const { return m_Config.Mode == AppMode::Standalone; }
	[[nodiscard]] const AppConfig& GetConfig() const { return m_Config; }

	[[nodiscard]] auto& GetWindow(this auto&& self) { return *self.m_Window; }
	[[nodiscard]] auto& GetRenderer(this auto&& self) { return *self.m_Renderer; }
	[[nodiscard]] auto& GetScene(this auto&& self) { return self.m_Scene; }
	[[nodiscard]] auto& GetEditor(this auto&& self) { return *self.m_Editor; }

private:
	void Initialize();
	void InitializeScene();
	void InitializeStandalone();
	void Shutdown();

	void ProcessInput(float deltaTime);
	void Update(float deltaTime);
	void Render();
	void RenderStandalone();

	void TogglePause();
	void RestartStandaloneGame();
	void DrawStandalonePauseMenu();

private:
	static Engine* s_Instance;

	AppConfig m_Config;

	bool m_IsRunning = false;
	float m_DeltaTime = 0.0f;
	float m_LastFrameTime = 0.0f;

	std::unique_ptr<Window> m_Window;
	std::unique_ptr<Renderer> m_Renderer;
	Scene m_Scene;
	std::unique_ptr<Editor> m_Editor;

	// Standalone post-processing pipeline
	std::unique_ptr<Framebuffer> m_HDRFramebuffer;
	BloomRenderer m_BloomRenderer;
	Shader m_ToneMapShader;
	bool m_BloomEnabled = true;
	float m_BloomIntensity = 0.8f;
	float m_BloomThreshold = 1.0f;
	float m_BloomFilterRadius = 0.005f;
	float m_Exposure = 1.0f;
	int m_ToneMappingOperator = 0; // ACES Filmic
	float m_Gamma = 2.2f;

	bool m_IsPaused = false;
};
