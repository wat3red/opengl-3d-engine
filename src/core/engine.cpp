// engine.cpp
#include "engine.h"
#include "asset_manager.h"
#include "skybox.h"

#include <std_image/stb_image.h>
#include <cassert>
#include <iostream>

Engine* Engine::s_Instance = nullptr;

Engine::Engine()
	: m_IsRunning(true),
	  m_DeltaTime(0.0f),
	  m_LastFrameTime(0.0f)
{
	assert(!s_Instance && "Engine instance already exists!");
	s_Instance = this;

	Initialize();
}

Engine::~Engine()
{
	Shutdown();
	s_Instance = nullptr;
}

void Engine::Close()
{
	m_IsRunning = false;
	if (m_Window)
	{
		m_Window->SetShouldClose(true);
	}
}

void Engine::Initialize()
{
	stbi_set_flip_vertically_on_load(true);

	// 1. Initialize Window & Context
	m_Window = std::make_unique<Window>(WindowProperties{
		"Ignis Engine",
		DEFAULT_WINDOW_WIDTH,
		DEFAULT_WINDOW_HEIGHT,
		true
	});

	// 2. Initialize Renderer
	m_Renderer = std::make_unique<Renderer>();
	m_Renderer->Initialize();

	// 3. Initialize Scene
	InitializeScene();

	// 4. Initialize Editor Tooling
	m_Editor = std::make_unique<Editor>(m_Scene, *m_Renderer);
	m_Editor->Initialize(m_Window->GetNativeWindow());
}

void Engine::Shutdown()
{
	m_Scene.Clear();
	AssetManager::Get().Clear();

	if (m_Editor)
	{
		m_Editor->Shutdown();
		m_Editor.reset();
	}

	if (m_Renderer)
	{
		m_Renderer->Cleanup();
		m_Renderer.reset();
	}

	m_Window.reset();
}

void Engine::InitializeScene()
{
	// 1. Load Main Model Entity
	auto houseEntity = std::make_shared<ModelEntity>(
		"Clock Tower Map",
		"res/models/house/source/free_fire_maxx_new_clock_tower_map_download_free.glb"
	);
	houseEntity->SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
	houseEntity->SetRotation(glm::vec3(0.0f, 0.0f, 0.0f));
	houseEntity->SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
	m_Scene.AddEntity(houseEntity);

	// 2. Add Directional Sun Light
	auto sun = std::make_shared<DirectionalLight>(
		"Sun",
		glm::vec3(-0.2f, -1.0f, -0.3f),
		glm::vec3(1.0f, 1.0f, 0.95f),
		0.8f
	);
	m_Scene.AddLight(sun);

	// 3. Add Point Lights
	auto lamp1 = std::make_shared<PointLight>(
		"Courtyard Lamp",
		glm::vec3(0.0f, 6.0f, 0.0f),
		glm::vec3(1.0f, 0.85f, 0.6f),
		2.0f,
		35.0f
	);
	m_Scene.AddLight(lamp1);

	auto lamp2 = std::make_shared<PointLight>(
		"Tower Lamp",
		glm::vec3(0.0f, 18.0f, 0.0f),
		glm::vec3(0.4f, 0.7f, 1.0f),
		1.5f,
		25.0f
	);
	m_Scene.AddLight(lamp2);

	// 4. Add Spotlight (Player Flashlight)
	auto flashlight = std::make_shared<SpotLight>(
		"Flashlight",
		glm::vec3(0.0f, 15.0f, 60.0f),
		glm::vec3(0.0f, 0.0f, -1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
		0.0f,
		12.5f,
		17.5f
	);
	m_Scene.AddLight(flashlight);

	// 5. Initialize Skybox
	std::vector<std::string> skyboxFaces = {
		"res/textures/skybox_Storforsen/posx.jpg",
		"res/textures/skybox_Storforsen/negx.jpg",
		"res/textures/skybox_Storforsen/posy.jpg",
		"res/textures/skybox_Storforsen/negy.jpg",
		"res/textures/skybox_Storforsen/posz.jpg",
		"res/textures/skybox_Storforsen/negz.jpg"
	};
	auto skyboxCube = AssetManager::Get().LoadCubemap("storforsen", skyboxFaces);
	auto skyboxShader = AssetManager::Get().LoadShader("skybox", "shaders/Skybox.shader");
	if (skyboxCube && skyboxShader)
	{
		m_Scene.SetSkybox(std::make_shared<Skybox>(skyboxCube, skyboxShader));
	}

	m_Scene.SetSelectedEntity(houseEntity);
}

void Engine::ProcessInput(float deltaTime)
{
	if (m_Editor && m_Window)
	{
		m_Editor->ProcessInput(m_Window->GetNativeWindow(), deltaTime);

		// Align sun with editor camera view on pressing B
		if (glfwGetKey(m_Window->GetNativeWindow(), GLFW_KEY_B) == GLFW_PRESS)
		{
			auto sun = m_Scene.GetSunLight();
			if (sun)
			{
				sun->SetDirection(m_Editor->GetCamera().Front);
			}
		}
	}
}

void Engine::Update(float deltaTime)
{
	m_Scene.Update(deltaTime);

	// Synchronize flashlight with editor camera
	if (m_Editor)
	{
		for (auto& light : m_Scene.GetLights())
		{
			if (light && light->GetType() == LightType::Spotlight && light->GetName() == "Flashlight")
			{
				light->SetPosition(m_Editor->GetCamera().Position);
				light->SetDirection(m_Editor->GetCamera().Front);
			}
		}
	}
}

void Engine::Render()
{
	if (!m_Editor || !m_Renderer || !m_Window)
	{
		return;
	}

	// 1. Off-screen 3D Scene Viewport Pass
	m_Editor->RenderSceneViewport();

	// 2. Clear Screen Framebuffer (Editor background)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glm::ivec2 windowSize = m_Window->GetSize();
	glViewport(0, 0, windowSize.x, windowSize.y);
	glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// 3. Render Editor UI (ImGui docking, panels, viewport image)
	m_Editor->RenderUI(m_DeltaTime);
}

void Engine::Run()
{
	while (m_IsRunning && !m_Window->ShouldClose())
	{
		float currentFrameTime = static_cast<float>(glfwGetTime());
		m_DeltaTime = currentFrameTime - m_LastFrameTime;
		m_LastFrameTime = currentFrameTime;

		// 1. Input & Simulation
		ProcessInput(m_DeltaTime);
		Update(m_DeltaTime);

		// 2. Rendering
		Render();

		// 3. Swap buffers & Poll OS events
		m_Window->OnUpdate();
	}
}
