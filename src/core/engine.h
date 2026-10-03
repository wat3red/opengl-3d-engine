#pragma once

#include "window.h"
#include "renderer.h"
#include "scene.h"
#include "editor.h"

#include <memory>

class Engine {
public:
	Engine();
	~Engine();

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

	void Run();
	void Close();

	[[nodiscard]] static Engine& Get() { return *s_Instance; }

	[[nodiscard]] auto& GetWindow(this auto&& self) { return *self.m_Window; }
	[[nodiscard]] auto& GetRenderer(this auto&& self) { return *self.m_Renderer; }
	[[nodiscard]] auto& GetScene(this auto&& self) { return self.m_Scene; }
	[[nodiscard]] auto& GetEditor(this auto&& self) { return *self.m_Editor; }

private:
	void Initialize();
	void InitializeScene();
	void Shutdown();

	void ProcessInput(float deltaTime);
	void Update(float deltaTime);
	void Render();

private:
	static Engine* s_Instance;

	static constexpr uint32_t DEFAULT_WINDOW_WIDTH = 1600;
	static constexpr uint32_t DEFAULT_WINDOW_HEIGHT = 900;

	bool m_IsRunning = false;
	float m_DeltaTime = 0.0f;
	float m_LastFrameTime = 0.0f;

	std::unique_ptr<Window> m_Window;
	std::unique_ptr<Renderer> m_Renderer;
	Scene m_Scene;
	std::unique_ptr<Editor> m_Editor;
};
