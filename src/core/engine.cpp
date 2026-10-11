#include "engine.h"
#include "asset_manager.h"
#include "skybox.h"
#include "light.h"
#include "input.h"

#include <std_image/stb_image.h>
#include <imgui/imgui.h>
#include <cassert>

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

	m_Window = std::make_unique<Window>(WindowProperties{
		"Ignis Engine",
		DEFAULT_WINDOW_WIDTH,
		DEFAULT_WINDOW_HEIGHT,
		true
	});

	Input::Init(m_Window->GetNativeWindow());

	m_Renderer = std::make_unique<Renderer>();
	m_Renderer->Initialize();

	InitializeScene();

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
	auto houseEntity = m_Scene.CreateModelEntity(
		"Clock Tower Map",
		"res/models/house/source/free_fire_maxx_new_clock_tower_map_download_free.glb"
	);
	houseEntity.SetScale(glm::vec3(100.0f, 100.0f, 100.0f));

	m_Scene.CreateDirectionalLight(
		"Sun",
		glm::vec3(-0.2f, -1.0f, -0.3f),
		glm::vec3(1.0f, 1.0f, 0.95f),
		2.5f
	);

	m_Scene.CreatePointLight(
		"Courtyard Lamp",
		glm::vec3(0.0f, 6.0f, 0.0f),
		glm::vec3(1.0f, 0.85f, 0.6f),
		2.0f,
		35.0f
	);

	m_Scene.CreatePointLight(
		"Tower Lamp",
		glm::vec3(0.0f, 18.0f, 0.0f),
		glm::vec3(0.4f, 0.7f, 1.0f),
		1.5f,
		25.0f
	);

	m_Scene.CreateSpotLight(
		"Flashlight",
		glm::vec3(0.0f, 15.0f, 60.0f),
		glm::vec3(0.0f, 0.0f, -1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
		0.0f,
		12.5f,
		17.5f,
		35.0f
	);

	auto skyDomeShader = AssetManager::Get().LoadShader("skydome", "shaders/SkyDome.shader");
	if (skyDomeShader)
	{
		m_Scene.SetSkybox(std::make_shared<Skybox>("res/models/skybox/source/basic_skybox_3d.fbx", skyDomeShader));
	}

	m_Scene.SetSelectedEntity(houseEntity);
}

void Engine::ProcessInput(float deltaTime)
{
	if (m_Editor && m_Window)
	{
		m_Editor->ProcessInput(m_Window->GetNativeWindow(), deltaTime);

		if (m_Editor->GetSceneState() == Editor::SceneState::Edit)
		{
			// Align sun with editor camera view on pressing B (Edit mode only)
			if (glfwGetKey(m_Window->GetNativeWindow(), GLFW_KEY_B) == GLFW_PRESS)
			{
				Entity sun = m_Scene.GetSunLight();
				if (sun && sun.HasComponent<LightComponent>())
				{
					auto& lc = sun.GetComponent<LightComponent>();
					lc.Direction = m_Editor->GetCamera().Front;
					sun.SetRotation(EulerFromDirection(m_Editor->GetCamera().Front));
				}
			}

			// Toggle flashlight on pressing F (Edit mode only)
			static bool s_FKeyWasPressed = false;
			bool fPressed = (glfwGetKey(m_Window->GetNativeWindow(), GLFW_KEY_F) == GLFW_PRESS);
			if (fPressed && !s_FKeyWasPressed && !ImGui::GetIO().WantTextInput)
			{
				auto view = m_Scene.GetRegistry().view<TagComponent, LightComponent>();
				for (auto [handle, tag, light] : view.each())
				{
					if (light.Type == LightType::Spotlight && tag.Tag == "Flashlight")
					{
						if (light.Intensity > 0.001f)
							light.Intensity = 0.0f;
						else
							light.Intensity = 2.5f;
					}
				}
			}
			s_FKeyWasPressed = fPressed;
		}
	}
}

void Engine::Update(float deltaTime)
{
	if (m_Editor)
	{
		if (m_Editor->GetSceneState() == Editor::SceneState::Play)
		{
			m_Scene.OnUpdateRuntime(deltaTime);
		}
		else if (m_Editor->GetSceneState() == Editor::SceneState::Edit)
		{
			m_Scene.OnUpdateEditor(deltaTime);

			// Synchronize flashlight with editor camera in Edit mode
			auto view = m_Scene.GetRegistry().view<TagComponent, TransformComponent, LightComponent>();
			for (auto [handle, tag, tc, light] : view.each())
			{
				if (light.Type == LightType::Spotlight && tag.Tag == "Flashlight")
				{
					tc.Position = m_Editor->GetCamera().Position;
					light.Direction = m_Editor->GetCamera().Front;
					tc.Rotation = EulerFromDirection(m_Editor->GetCamera().Front);
				}
			}
		}
	}
	else
	{
		m_Scene.OnUpdateRuntime(deltaTime);
	}
}

void Engine::Render()
{
	if (!m_Editor || !m_Renderer || !m_Window)
	{
		return;
	}

	m_Editor->RenderSceneViewport();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glm::ivec2 windowSize = m_Window->GetSize();
	glViewport(0, 0, windowSize.x, windowSize.y);
	glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_Editor->RenderUI(m_DeltaTime);
}

void Engine::Run()
{
	while (m_IsRunning && !m_Window->ShouldClose())
	{
		float currentFrameTime = static_cast<float>(glfwGetTime());
		m_DeltaTime = currentFrameTime - m_LastFrameTime;
		m_LastFrameTime = currentFrameTime;

		Input::Update();

		ProcessInput(m_DeltaTime);
		Update(m_DeltaTime);

		Render();

		m_Window->OnUpdate();
	}
}
