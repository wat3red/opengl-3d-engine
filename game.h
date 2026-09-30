// game.h
#pragma once

#include "renderer.h"
#include "input_handler.h"
#include "framebuffer.h"
#include "scene.h"
#include "editor.h"

#include <GLFW/glfw3.h>
#include <memory>

class Game {
public:
	Game();
	~Game();

	void Run();

private:
	static constexpr int DEFAULT_WINDOW_WIDTH = 1600;
	static constexpr int DEFAULT_WINDOW_HEIGHT = 900;

	glm::ivec2 m_WindowSize;
	GLFWwindow* m_Window;

	float m_DeltaTime;
	float m_LastFrameTime;
	float m_RenderTime;

	Camera m_MainCamera;
	InputHandler m_InputHandler;
	Renderer m_Renderer;
	std::unique_ptr<Framebuffer> m_Framebuffer;
	Scene m_Scene;
	std::unique_ptr<Editor> m_Editor;

	void ProcessInput();
	void Update();
	void Render();
	void InitializeScene();
};
