// game.cpp
#include "game.h"
#include "error_handler.h"
#include "asset_manager.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <std_image/stb_image.h>
#include <chrono>
#include <iostream>

Game::Game() : 
	m_WindowSize{ DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT },
	m_MainCamera(glm::vec3(0.0f, 15.0f, 60.0f)),
	m_InputHandler(m_MainCamera, &m_WindowSize),
	m_Renderer(&m_WindowSize),
	m_DeltaTime(0.0f),
	m_LastFrameTime(0.0f),
	m_RenderTime(0.0f),
	m_Window(nullptr)
{
	stbi_set_flip_vertically_on_load(true);

	if (!glfwInit())
		throw std::runtime_error("Failed to initialize GLFW");

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
	
	m_Window = glfwCreateWindow(m_WindowSize.x, m_WindowSize.y, "OpenGL 3D Engine", nullptr, nullptr);
	if (!m_Window)
	{
		glfwTerminate();
		throw std::runtime_error("Failed to create GLFW window");
	}

	glfwMakeContextCurrent(m_Window);
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		throw std::runtime_error("Failed to initialize GLEW");
	}

	glfwSwapInterval(1); // Enable VSync

	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(GLDebugMessageCallback, nullptr);

	m_Renderer.Initialize();
	InitializeScene();

	m_Framebuffer = std::make_unique<Framebuffer>(FramebufferSpecification{ 1280, 720 });
	m_Editor = std::make_unique<Editor>(m_Scene, m_MainCamera, m_Renderer, *m_Framebuffer);

	m_InputHandler.SetCallbacks(m_Window);

	// Setup Dear ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	// Visual theme styling
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 5.0f;
	style.FrameRounding = 4.0f;
	style.GrabRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.ScrollbarRounding = 4.0f;

	ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

Game::~Game()
{
	m_Scene.Clear();
	AssetManager::Get().Clear();
	m_Renderer.Cleanup();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	if (m_Window)
	{
		glfwDestroyWindow(m_Window);
	}
	glfwTerminate();
}

void Game::InitializeScene()
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
		m_MainCamera.Position,
		m_MainCamera.Front,
		glm::vec3(1.0f, 1.0f, 1.0f),
		0.0f,
		12.5f,
		17.5f
	);
	m_Scene.AddLight(flashlight);

	m_Scene.SetSelectedEntity(houseEntity);
}

void Game::ProcessInput()
{
	m_InputHandler.Update(m_Window);

	// Align sun with camera view on pressing B
	if (glfwGetKey(m_Window, GLFW_KEY_B) == GLFW_PRESS)
	{
		auto sun = m_Scene.GetSunLight();
		if (sun)
		{
			sun->SetDirection(m_MainCamera.Front);
		}
	}
}

void Game::Update()
{
	m_Scene.Update(m_DeltaTime);

	// Keep flashlight synchronized with camera if present
	for (auto& light : m_Scene.GetLights())
	{
		if (light && light->GetType() == LightType::Spotlight && light->GetName() == "Flashlight")
		{
			light->SetPosition(m_MainCamera.Position);
			light->SetDirection(m_MainCamera.Front);
		}
	}
}

void Game::Render()
{
	if (!m_Framebuffer || !m_Editor) return;

	auto start = std::chrono::high_resolution_clock::now();

	glm::ivec2 viewportSize = m_Editor->GetViewportSize();
	if (viewportSize.x > 0 && viewportSize.y > 0)
	{
		// 1. Render Scene into Off-screen Framebuffer
		m_Renderer.RenderScene(viewportSize, m_MainCamera, m_Scene, m_Framebuffer->GetRendererID());

		// Render small light visualizer cubes in the offscreen scene
		Shader lightShader = m_Renderer.GetLightShader();
		lightShader.Bind();

		for (const auto& light : m_Scene.GetLights())
		{
			if (!light || !light->IsActive() || light->GetType() != LightType::Point)
				continue;

			lightShader.SetUniform4f("objectColor", glm::vec4(light->GetColor(), 1.0f));

			glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), light->GetPosition());
			modelMatrix = glm::scale(modelMatrix, glm::vec3(0.2f));
			lightShader.SetUniformMat4f("model", modelMatrix);
			lightShader.SetUniformMat4f("view", m_MainCamera.GetViewMatrix());
			lightShader.SetUniformMat4f("projection", m_MainCamera.GetProjectionMatrix(viewportSize));

			m_Renderer.RenderCube();
		}

		lightShader.Unbind();
	}

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float> duration = end - start;
	m_RenderTime = duration.count() * 1000.0f;

	m_Framebuffer->Unbind();

	// 2. Render Screen Framebuffer (Clear background for editor UI)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, m_WindowSize.x, m_WindowSize.y);
	glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Game::Run()
{
	while (!glfwWindowShouldClose(m_Window))
	{
		float currentFrameTime = static_cast<float>(glfwGetTime());
		m_DeltaTime = currentFrameTime - m_LastFrameTime;
		m_LastFrameTime = currentFrameTime;

		// Input & Simulation
		ProcessInput();
		Update();

		// Off-screen Scene 3D Rendering
		Render();

		// Dear ImGui Rendering
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (m_Editor)
		{
			m_Editor->RenderUI(m_DeltaTime, m_RenderTime);
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(m_Window);
		glfwPollEvents();
	}
}
