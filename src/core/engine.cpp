#include "engine.h"
#include "asset_manager.h"
#include "skybox.h"
#include "light.h"
#include "input.h"
#include "audio_engine.h"
#include "scene_serializer.h"
#include "primitive.h"
#include "scene/game_module.h"

#include <std_image/stb_image.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <cassert>
#include <print>

Engine* Engine::s_Instance = nullptr;

Engine::Engine(int argc, char** argv)
	: m_IsRunning(true),
	  m_DeltaTime(0.0f),
	  m_LastFrameTime(0.0f)
{
	assert(!s_Instance && "Engine instance already exists!");
	s_Instance = this;

	ProjectPackager::DetectAppMode(argc, argv, m_Config);

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
		m_Config.GameTitle,
		m_Config.WindowWidth,
		m_Config.WindowHeight,
		m_Config.VSync
	});

	if (m_Config.Fullscreen)
	{
		m_Window->SetFullscreen(true);
	}

	Input::Init(m_Window->GetNativeWindow());
	AudioEngine::Get().Initialize();

	m_Renderer = std::make_unique<Renderer>();
	m_Renderer->Initialize();

	if (m_Config.Mode == AppMode::Editor)
	{
		m_Editor = std::make_unique<Editor>(m_Scene, *m_Renderer);
		m_Editor->Initialize(m_Window->GetNativeWindow());

		std::string sceneToLoad = m_Config.StartingScene.empty() ? "res/scenes/default.scene" : m_Config.StartingScene;

		// Guard: If starting scene does not exist or has degraded (fewer than 10 entities), restore from factory template
		if (!std::filesystem::exists(sceneToLoad) || SceneSerializer::GetEntityCountInFile(sceneToLoad) < 10)
		{
			if (std::filesystem::exists("res/scenes/doom_template.scene"))
			{
				std::println("[Engine] Starting scene '{}' missing or degraded. Restoring from factory template 'res/scenes/doom_template.scene'...", sceneToLoad);
				std::error_code ec;
				std::filesystem::copy_file("res/scenes/doom_template.scene", sceneToLoad, std::filesystem::copy_options::overwrite_existing, ec);
			}
		}

		if (std::filesystem::exists(sceneToLoad))
		{
			m_Editor->LoadScene(sceneToLoad);
		}
		else if (std::filesystem::exists("res/scenes/doom_template.scene"))
		{
			m_Editor->LoadScene("res/scenes/doom_template.scene");
		}
		else
		{
			std::println(stderr, "[Engine] No scene files found. Falling back to default scene setup.");
			InitializeScene();
		}
	}
	else
	{
		InitializeStandalone();
	}
}

void Engine::InitializeStandalone()
{
	glm::ivec2 winSize = m_Window->GetSize();
	uint32_t width = winSize.x > 0 ? static_cast<uint32_t>(winSize.x) : m_Config.WindowWidth;
	uint32_t height = winSize.y > 0 ? static_cast<uint32_t>(winSize.y) : m_Config.WindowHeight;

	FramebufferSpecification hdrSpec;
	hdrSpec.Width = width;
	hdrSpec.Height = height;
	hdrSpec.ColorFormat = FramebufferTextureFormat::RGBA16F;
	m_HDRFramebuffer = std::make_unique<Framebuffer>(hdrSpec);

	m_ToneMapShader = Shader("shaders/ToneMap.shader");
	m_BloomRenderer.Initialize(width, height);

	// Initialize ImGui for Standalone Game HUD
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(m_Window->GetNativeWindow(), true);
	ImGui_ImplOpenGL3_Init("#version 330");

	GameModule::Get().SyncImGuiContext(ImGui::GetCurrentContext());

	// Load starting scene (restore from template if missing or degraded)
	std::string sceneToLoad = m_Config.StartingScene.empty() ? "res/scenes/default.scene" : m_Config.StartingScene;
	if (!std::filesystem::exists(sceneToLoad) || SceneSerializer::GetEntityCountInFile(sceneToLoad) < 10)
	{
		if (std::filesystem::exists("res/scenes/doom_template.scene"))
		{
			std::error_code ec;
			std::filesystem::copy_file("res/scenes/doom_template.scene", sceneToLoad, std::filesystem::copy_options::overwrite_existing, ec);
		}
	}

	SceneSerializer serializer(m_Scene, m_Renderer.get());
	if (!serializer.Deserialize(sceneToLoad))
	{
		if (std::filesystem::exists("res/scenes/doom_template.scene") && serializer.Deserialize("res/scenes/doom_template.scene"))
		{
			std::println("[Engine] Loaded fallback template 'res/scenes/doom_template.scene'.");
		}
		else
		{
			std::println(stderr, "[Engine] Could not load starting scene '{}'. Falling back to default scene setup.", sceneToLoad);
			InitializeScene();
		}
	}

	// Start game runtime immediately
	m_Scene.OnRuntimeStart();
	Input::SetCursorMode(CursorMode::Locked);

	std::println("[Engine] Running in Standalone Mode: '{}' (Scene: '{}')", m_Config.GameTitle, sceneToLoad);
}

void Engine::Shutdown()
{
	if (m_Config.Mode == AppMode::Standalone)
	{
		m_Scene.OnRuntimeStop();
		m_BloomRenderer.Shutdown();
		if (ImGui::GetCurrentContext())
		{
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
		}
		m_HDRFramebuffer.reset();
	}

	m_Scene.Clear();
	AudioEngine::Get().Shutdown();
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
	SceneSerializer serializer(m_Scene, m_Renderer.get(), m_Editor.get());
	if (std::filesystem::exists("res/scenes/default.scene") && serializer.Deserialize("res/scenes/default.scene"))
	{
		return;
	}
	if (std::filesystem::exists("res/scenes/doom_template.scene") && serializer.Deserialize("res/scenes/doom_template.scene"))
	{
		return;
	}

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

void Engine::TogglePause()
{
	m_IsPaused = !m_IsPaused;
	if (m_IsPaused)
	{
		Input::SetCursorMode(CursorMode::Normal);
	}
	else
	{
		Input::SetCursorMode(CursorMode::Locked);
	}
}

void Engine::RestartStandaloneGame()
{
	m_Scene.OnRuntimeStop();
	m_Scene.Clear();
	SceneSerializer serializer(m_Scene, m_Renderer.get());
	serializer.Deserialize(m_Config.StartingScene);
	m_Scene.OnRuntimeStart();
	m_IsPaused = false;
	Input::SetCursorMode(CursorMode::Locked);
}

void Engine::ProcessInput(float deltaTime)
{
	if (m_Config.Mode == AppMode::Editor)
	{
		if (m_Editor && m_Window)
		{
			m_Editor->ProcessInput(m_Window->GetNativeWindow(), deltaTime);
		}
	}
	else
	{
		GLFWwindow* win = m_Window->GetNativeWindow();

		// ESC toggles pause menu in standalone mode
		static bool s_EscWasPressed = false;
		bool escPressed = (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS);
		if (escPressed && !s_EscWasPressed)
		{
			TogglePause();
		}
		s_EscWasPressed = escPressed;

		// F11 or Alt+Enter toggles fullscreen
		static bool s_F11WasPressed = false;
		bool altPressed = (glfwGetKey(win, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS);
		bool enterPressed = (glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS);
		bool f11Pressed = (glfwGetKey(win, GLFW_KEY_F11) == GLFW_PRESS) || (altPressed && enterPressed);
		if (f11Pressed && !s_F11WasPressed)
		{
			m_Window->ToggleFullscreen();
		}
		s_F11WasPressed = f11Pressed;
	}
}

void Engine::Update(float deltaTime)
{
	if (m_Config.Mode == AppMode::Editor)
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
	}
	else
	{
		// Standalone game: run runtime simulation when unpaused
		if (!m_IsPaused)
		{
			m_Scene.OnUpdateRuntime(deltaTime);
		}
	}
}

void Engine::RenderStandalone()
{
	glm::ivec2 windowSize = m_Window->GetSize();
	if (windowSize.x <= 0 || windowSize.y <= 0)
	{
		return;
	}

	// Dynamic resize of HDR framebuffer when window is resized
	if (m_HDRFramebuffer->GetSpecification().Width != static_cast<uint32_t>(windowSize.x) ||
	    m_HDRFramebuffer->GetSpecification().Height != static_cast<uint32_t>(windowSize.y))
	{
		m_HDRFramebuffer->Resize(windowSize.x, windowSize.y);
		m_BloomRenderer.Resize(windowSize.x, windowSize.y);
	}

	// 1. Primary Camera extraction
	Camera activeCamera(glm::vec3(0.0f, 1.8f, 0.0f));
	Entity primaryCam = m_Scene.GetPrimaryCameraEntity();
	if (primaryCam && primaryCam.HasComponent<CameraComponent>())
	{
		auto& tc = primaryCam.GetTransform();
		auto& cc = primaryCam.GetComponent<CameraComponent>();
		float eyeOffset = primaryCam.HasComponent<CharacterControllerComponent>() ? 1.6f : 0.0f;
		cc.CameraInstance.Position = tc.Position + glm::vec3(0.0f, eyeOffset, 0.0f);
		cc.CameraInstance.SetYawAndPitch(tc.Rotation.y, tc.Rotation.x);
		activeCamera = cc.CameraInstance;
	}

	// 2. Render 3D Scene into HDR Framebuffer (GL_RGBA16F)
	m_Renderer->RenderScene(windowSize, activeCamera, m_Scene, m_HDRFramebuffer->GetRendererID());
	m_HDRFramebuffer->Unbind();

	// 3. Multi-scale Dual Filter Bloom Pass
	if (m_BloomEnabled)
	{
		m_BloomRenderer.RenderBloom(m_HDRFramebuffer->GetColorAttachmentRendererID(), m_BloomThreshold, m_BloomFilterRadius);
	}

	// 4. Post-processing Tonemapping directly to Window Backbuffer (0)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowSize.x, windowSize.y);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	m_ToneMapShader.Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_HDRFramebuffer->GetColorAttachmentRendererID());
	m_ToneMapShader.SetUniform1i("hdrBuffer", 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_BloomRenderer.GetBloomTexture());
	m_ToneMapShader.SetUniform1i("bloomBuffer", 1);
	m_ToneMapShader.SetUniform1i("enableBloom", m_BloomEnabled ? 1 : 0);
	m_ToneMapShader.SetUniform1f("bloomIntensity", m_BloomIntensity);
	m_ToneMapShader.SetUniform1f("exposure", m_Exposure);
	m_ToneMapShader.SetUniform1i("toneMappingOperator", m_ToneMappingOperator);
	m_ToneMapShader.SetUniform1f("gamma", m_Gamma);

	Primitive::GetFullScreenQuad()->Draw(m_ToneMapShader);
	m_ToneMapShader.Unbind();

	// 5. ImGui Fullscreen HUD & Pause Menu Overlay
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Fullscreen HUD overlay
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	if (ImGui::Begin("##StandaloneHUD", nullptr,
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoBackground))
	{
		m_Scene.OnRuntimeGUI();
	}
	ImGui::End();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor();

	// In-game Pause Menu
	if (m_IsPaused)
	{
		DrawStandalonePauseMenu();
	}

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Engine::DrawStandalonePauseMenu()
{
	ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
	ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(360, 320), ImGuiCond_Always);

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.06f, 0.06f, 0.94f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.85f, 0.2f, 0.15f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);

	if (ImGui::Begin("Game Paused", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::Spacing();
		std::string titleStr = "=== " + m_Config.GameTitle + " ===";
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(titleStr.c_str()).x) * 0.5f);
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.25f, 1.0f), "%s", titleStr.c_str());
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float btnWidth = 280.0f;
		float btnX = (ImGui::GetWindowWidth() - btnWidth) * 0.5f;

		ImGui::SetCursorPosX(btnX);
		if (ImGui::Button("Resume Game (ESC)", ImVec2(btnWidth, 42)))
		{
			TogglePause();
		}

		ImGui::Spacing();
		ImGui::SetCursorPosX(btnX);
		if (ImGui::Button("Restart Level", ImVec2(btnWidth, 42)))
		{
			RestartStandaloneGame();
		}

		ImGui::Spacing();
		ImGui::SetCursorPosX(btnX);
		std::string fsText = m_Window->IsFullscreen() ? "Windowed Mode (F11)" : "Fullscreen Mode (F11)";
		if (ImGui::Button(fsText.c_str(), ImVec2(btnWidth, 42)))
		{
			m_Window->ToggleFullscreen();
		}

		ImGui::Spacing();
		ImGui::SetCursorPosX(btnX);
		if (ImGui::Button("Quit to Desktop", ImVec2(btnWidth, 42)))
		{
			Close();
		}
	}
	ImGui::End();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(2);
}

void Engine::Render()
{
	if (!m_Renderer || !m_Window)
	{
		return;
	}

	if (m_Config.Mode == AppMode::Editor)
	{
		if (!m_Editor) return;
		m_Editor->RenderSceneViewport();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glm::ivec2 windowSize = m_Window->GetSize();
		glViewport(0, 0, windowSize.x, windowSize.y);
		glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		m_Editor->RenderUI(m_DeltaTime);
	}
	else
	{
		RenderStandalone();
	}
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
