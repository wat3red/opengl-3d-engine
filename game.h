// game.h
#pragma once
#include "renderer.h"
#include "input_handler.h"
#include "entity.h"

#include <GLFW/glfw3.h>
#include <unordered_map>

class Light;

const int WINDOW_WIDTH = 640;
const int WINDOW_HEIGHT = 480;

class Game {
public:
	Game();
	void Run();
	~Game();

private:
	float deltaTime;
	float lastFrameTime;


	Shader nord;


	GLFWwindow* m_Window;
	Renderer m_Renderer;
	Camera m_MainCamera;
	InputHandler m_InputHandler;
	std::vector<ModelEntity*> m_Entities;
	std::vector<Light*> m_Lights;

	glm::ivec2 m_WindowSize;

	void ProcessInput();
	void DrawGUI();
	void Update();
	void Render();
	void InitializeEntities();
};

