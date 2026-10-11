#include "editor.h"
#include "skybox.h"
#include "primitive.h"
#include "light.h"
#include "core/input.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include "scene_serializer.h"
#include "physics/collision.h"
#include "scripts/fps_controller.h"
#include "scripts/doom_game_state.h"
#include "scripts/doom_player.h"
#include "scripts/doom_enemy.h"
#include "scripts/doom_pickup.h"

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <cctype>

namespace {

std::vector<std::string>& GetDiscoveredModels()
{
	static std::vector<std::string> s_DiscoveredModels;
	static bool s_ModelsScanned = false;
	if (!s_ModelsScanned)
	{
		s_DiscoveredModels.clear();
		std::error_code ec;
		for (const auto& entry : std::filesystem::recursive_directory_iterator("res/models", ec))
		{
			if (entry.is_regular_file())
			{
				auto ext = entry.path().extension().string();
				for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj")
				{
					s_DiscoveredModels.push_back(entry.path().generic_string());
				}
			}
		}
		std::sort(s_DiscoveredModels.begin(), s_DiscoveredModels.end());
		s_ModelsScanned = true;
	}
	return s_DiscoveredModels;
}

void RescanDiscoveredModels()
{
	auto& models = GetDiscoveredModels();
	models.clear();
	std::error_code ec;
	for (const auto& entry : std::filesystem::recursive_directory_iterator("res/models", ec))
	{
		if (entry.is_regular_file())
		{
			auto ext = entry.path().extension().string();
			for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj")
			{
				models.push_back(entry.path().generic_string());
			}
		}
	}
	std::sort(models.begin(), models.end());
}

} // namespace

Editor::Editor(Scene& scene, Renderer& renderer)
	: m_Scene(scene),
	  m_Renderer(renderer),
	  m_Camera(glm::vec3(0.0f, 15.0f, 60.0f)),
	  m_InputHandler(m_Camera),
	  m_GpuRenderTime(0.0f)
{
}

Editor::~Editor()
{
	Shutdown();
}

void Editor::Initialize(GLFWwindow* window)
{
	FramebufferSpecification hdrSpec;
	hdrSpec.Width = 1280;
	hdrSpec.Height = 720;
	hdrSpec.ColorFormat = FramebufferTextureFormat::RGBA16F;
	m_HDRFramebuffer = std::make_unique<Framebuffer>(hdrSpec);

	FramebufferSpecification ldrSpec;
	ldrSpec.Width = 1280;
	ldrSpec.Height = 720;
	ldrSpec.ColorFormat = FramebufferTextureFormat::RGBA8;
	m_LDRFramebuffer = std::make_unique<Framebuffer>(ldrSpec);

	m_ToneMapShader = Shader("shaders/ToneMap.shader");
	m_BloomRenderer.Initialize(1280, 720);

	m_InputHandler.SetCallbacks(window);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 5.0f;
	style.FrameRounding = 4.0f;
	style.GrabRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.ScrollbarRounding = 4.0f;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

void Editor::Shutdown()
{
	if (m_SceneState != SceneState::Edit)
	{
		OnSceneStop();
	}

	m_BloomRenderer.Shutdown();
	if (ImGui::GetCurrentContext())
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
	m_HDRFramebuffer.reset();
	m_LDRFramebuffer.reset();
}

void Editor::OnScenePlay()
{
	m_SceneState = SceneState::Play;
	m_EditorSceneBackup = std::make_unique<Scene>();
	m_EditorSceneBackup->DuplicateFrom(m_Scene);
	m_Scene.OnRuntimeStart();
	m_StatusMessage = "Play mode started";
	m_StatusTimer = 2.0f;
}

void Editor::OnSceneStop()
{
	m_Scene.OnRuntimeStop();
	m_SceneState = SceneState::Edit;
	if (m_EditorSceneBackup)
	{
		m_Scene.DuplicateFrom(*m_EditorSceneBackup);
		m_EditorSceneBackup.reset();
	}
	Input::SetCursorMode(CursorMode::Normal);
	m_StatusMessage = "Returned to Edit mode";
	m_StatusTimer = 2.0f;
}

void Editor::OnScenePause()
{
	if (m_SceneState == SceneState::Play)
	{
		m_SceneState = SceneState::Pause;
		m_StatusMessage = "Simulation paused";
		m_StatusTimer = 2.0f;
	}
	else if (m_SceneState == SceneState::Pause)
	{
		m_SceneState = SceneState::Play;
		m_StatusMessage = "Simulation resumed";
		m_StatusTimer = 2.0f;
	}
}

void Editor::ProcessInput(GLFWwindow* window, float deltaTime)
{
	// Global hotkey: F5 toggles Play / Stop
	static bool s_F5WasPressed = false;
	bool f5Pressed = (glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS);
	if (f5Pressed && !s_F5WasPressed && !ImGui::GetIO().WantTextInput)
	{
		if (m_SceneState == SceneState::Edit)
			OnScenePlay();
		else
			OnSceneStop();
	}
	s_F5WasPressed = f5Pressed;

	if (m_SceneState == SceneState::Play)
	{
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			Input::SetCursorMode(CursorMode::Normal);
		}
		return;
	}

	m_InputHandler.Update(window, deltaTime);

	// Shortcuts for Save, Open, New (Ctrl+S, Ctrl+O, Ctrl+N)
	bool ctrl = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
	if (ctrl && !ImGui::GetIO().WantTextInput)
	{
		static bool s_CtrlActionFired = false;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		{
			if (!s_CtrlActionFired) { SaveScene(m_CurrentScenePath); s_CtrlActionFired = true; }
		}
		else if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
		{
			if (!s_CtrlActionFired) { m_ShowOpenModal = true; s_CtrlActionFired = true; }
		}
		else if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS)
		{
			if (!s_CtrlActionFired) { NewScene(); s_CtrlActionFired = true; }
		}
		else
		{
			s_CtrlActionFired = false;
		}
	}

	// Gizmo shortcuts (W: Translate, E: Rotate, R: Scale, Q: None)
	if (!m_Camera.isCapturingMovements && !ImGui::GetIO().WantTextInput && !ctrl)
	{
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			m_GizmoType = ImGuizmo::TRANSLATE;
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
			m_GizmoType = ImGuizmo::ROTATE;
		if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
			m_GizmoType = ImGuizmo::SCALE;
		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
			m_GizmoType = -1;
	}
}

void Editor::RenderSceneViewport()
{
	if (!m_HDRFramebuffer || !m_LDRFramebuffer)
	{
		return;
	}

	auto start = std::chrono::high_resolution_clock::now();

	Camera activeCamera = m_Camera;
	if (m_SceneState == SceneState::Play || m_SceneState == SceneState::Pause)
	{
		Entity primaryCam = m_Scene.GetPrimaryCameraEntity();
		if (primaryCam && primaryCam.HasComponent<CameraComponent>())
		{
			auto& tc = primaryCam.GetTransform();
			auto& cc = primaryCam.GetComponent<CameraComponent>();
			cc.CameraInstance.Position = tc.Position;
			cc.CameraInstance.Front = DirectionFromEuler(tc.Rotation);
			cc.CameraInstance.Yaw = tc.Rotation.y;
			cc.CameraInstance.Pitch = tc.Rotation.x;
			activeCamera = cc.CameraInstance;
		}
	}

	if (m_ViewportSize.x > 0 && m_ViewportSize.y > 0)
	{
		// 1. Render 3D scene into HDR Framebuffer (GL_RGBA16F)
		m_Renderer.RenderScene(m_ViewportSize, activeCamera, m_Scene, m_HDRFramebuffer->GetRendererID());

		// Light debug cubes are only drawn in Edit mode
		if (m_SceneState == SceneState::Edit)
		{
			Shader& lightShader = m_Renderer.GetLightShader();
			lightShader.Bind();

			auto lightView = m_Scene.GetRegistry().view<TagComponent, LightComponent, TransformComponent>();
			for (auto [handle, tag, light, tc] : lightView.each())
			{
				if (!tag.Active)
					continue;

				// Do not render debug icon if light is closer than 0.5m to camera to avoid camera clipping
				if (glm::distance(tc.Position, m_Camera.Position) < 0.5f)
					continue;

				lightShader.SetUniform4f("objectColor", glm::vec4(light.Color, 1.0f));

				if (light.Type == LightType::Point)
				{
					glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), tc.Position);
					modelMatrix = glm::scale(modelMatrix, glm::vec3(0.2f));
					lightShader.SetUniformMat4f("model", modelMatrix);
					lightShader.SetUniformMat4f("view", m_Camera.GetViewMatrix());
					lightShader.SetUniformMat4f("projection", m_Camera.GetProjectionMatrix(m_ViewportSize));
					m_Renderer.RenderCube();
				}
				else if (light.Type == LightType::Spotlight)
				{
					glm::mat4 modelMatrix = tc.GetMatrix();
					modelMatrix = glm::scale(modelMatrix, glm::vec3(0.15f, 0.15f, 0.4f));
					lightShader.SetUniformMat4f("model", modelMatrix);
					lightShader.SetUniformMat4f("view", m_Camera.GetViewMatrix());
					lightShader.SetUniformMat4f("projection", m_Camera.GetProjectionMatrix(m_ViewportSize));
					m_Renderer.RenderCube();
				}
			}

			lightShader.Unbind();
		}

		m_HDRFramebuffer->Unbind();

		// 2. Post-processing Bloom pass (multi-scale Dual Filter Bloom)
		if (m_BloomEnabled)
		{
			m_BloomRenderer.RenderBloom(m_HDRFramebuffer->GetColorAttachmentRendererID(), m_BloomThreshold, m_BloomFilterRadius);
		}

		// 3. Post-processing tonemapping pass: HDR + Bloom -> LDR
		m_LDRFramebuffer->Bind();
		glViewport(0, 0, m_ViewportSize.x, m_ViewportSize.y);
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
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_DEPTH_TEST);
		m_LDRFramebuffer->Unbind();
	}

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float> duration = end - start;
	m_GpuRenderTime = duration.count() * 1000.0f;
}

void Editor::RenderUI(float deltaTime)
{
	if (m_StatusTimer > 0.0f)
		m_StatusTimer -= deltaTime;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	DrawMainMenuBar();
	DrawToolbar();
	DrawModals();
	DrawViewportPanel();
	DrawSceneHierarchyPanel();
	DrawEntityInspectorPanel();
	DrawEnvironmentPanel();
	DrawMetricsPanel(deltaTime);
	if (m_ShowContentBrowser)
		DrawContentBrowserPanel();

	m_ResetLayout = false;

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Editor::DrawToolbar()
{
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.14f, 0.95f));

	ImVec2 centerPos(ImGui::GetIO().DisplaySize.x * 0.5f - 140.0f, 26.0f);
	ImGui::SetNextWindowPos(centerPos, ImGuiCond_Always);

	if (ImGui::Begin("##Toolbar", nullptr, flags))
	{
		if (m_SceneState == SceneState::Edit)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.6f, 0.25f, 0.9f));
			if (ImGui::Button("  Play (F5)  "))
			{
				OnScenePlay();
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 0.95f));
			if (ImGui::Button(" [DOOM FPS ARENA] "))
			{
				SetupDoomArenaLevel();
			}
			ImGui::PopStyleColor();
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.2f, 0.2f, 0.9f));
			if (ImGui::Button("  Stop (F5)  "))
			{
				OnSceneStop();
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			const char* pauseLabel = (m_SceneState == SceneState::Play) ? "  Pause  " : "  Resume  ";
			if (ImGui::Button(pauseLabel))
			{
				OnScenePause();
			}
		}
		ImGui::End();
	}
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void Editor::DrawViewportPanel()
{
	ImGuiWindowFlags viewportFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (ImGuizmo::IsOver() || ImGuizmo::IsUsing())
	{
		viewportFlags |= ImGuiWindowFlags_NoMove;
	}

	if (m_ResetLayout)
	{
		ImGui::SetNextWindowPos(ImVec2(290, 25), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(1010, 865), ImGuiCond_Always);
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Scene Viewport", nullptr, viewportFlags);

	m_IsViewportFocused = ImGui::IsWindowFocused();
	m_IsViewportHovered = ImGui::IsWindowHovered();

	// Allow camera look-around only when right clicking inside the viewport in Edit mode and not manipulating gizmos
	m_InputHandler.SetCanCaptureRightClick(m_SceneState == SceneState::Edit && m_IsViewportHovered && !ImGuizmo::IsUsing());

	ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
	if (viewportPanelSize.x > 0 && viewportPanelSize.y > 0)
	{
		m_ViewportSize = { static_cast<int>(viewportPanelSize.x), static_cast<int>(viewportPanelSize.y) };
		if (m_HDRFramebuffer && (m_HDRFramebuffer->GetWidth() != m_ViewportSize.x || m_HDRFramebuffer->GetHeight() != m_ViewportSize.y))
		{
			m_HDRFramebuffer->Resize(m_ViewportSize.x, m_ViewportSize.y);
		}
		if (m_LDRFramebuffer && (m_LDRFramebuffer->GetWidth() != m_ViewportSize.x || m_LDRFramebuffer->GetHeight() != m_ViewportSize.y))
		{
			m_LDRFramebuffer->Resize(m_ViewportSize.x, m_ViewportSize.y);
			m_BloomRenderer.Resize(m_ViewportSize.x, m_ViewportSize.y);
		}
	}

	if (m_LDRFramebuffer)
	{
		unsigned int textureID = m_LDRFramebuffer->GetColorAttachmentRendererID();
		if (textureID != 0)
		{
			ImGui::Image(
				(ImTextureID)(intptr_t)textureID,
				ImVec2(static_cast<float>(m_LDRFramebuffer->GetWidth()), static_cast<float>(m_LDRFramebuffer->GetHeight())),
				ImVec2(0.0f, 1.0f),
				ImVec2(1.0f, 0.0f)
			);
		}

		ImVec2 imagePos = ImGui::GetItemRectMin();
		ImVec2 imageSize = ImGui::GetItemRectSize();

		// 3D Viewport Gizmos (Edit mode only)
		Entity selectedEntity = m_Scene.GetSelectedEntity();
		if (m_SceneState == SceneState::Edit && selectedEntity && selectedEntity.HasComponent<TransformComponent>() && m_GizmoType != -1)
		{
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);
			ImGuizmo::PushID(static_cast<int>(selectedEntity.GetHandle()));

			glm::mat4 cameraView = m_Camera.GetViewMatrix();
			glm::mat4 cameraProj = m_Camera.GetProjectionMatrix(m_ViewportSize);
			auto& tc = selectedEntity.GetComponent<TransformComponent>();
			glm::mat4 modelMatrix = tc.GetMatrix();

			ImGuizmo::OPERATION operation = static_cast<ImGuizmo::OPERATION>(m_GizmoType);
			bool isLight = selectedEntity.HasComponent<LightComponent>();
			if (isLight)
			{
				const auto& light = selectedEntity.GetComponent<LightComponent>();
				if (light.Type == LightType::Point)
				{
					operation = ImGuizmo::TRANSLATE;
				}
				else if (light.Type == LightType::Directional && operation != ImGuizmo::ROTATE)
				{
					operation = ImGuizmo::ROTATE;
				}
				else if (light.Type == LightType::Spotlight && operation == ImGuizmo::SCALE)
				{
					operation = ImGuizmo::TRANSLATE;
				}
			}

			bool snap = m_GizmoSnap;
			float snapVal = m_GizmoSnapTranslation;
			if (operation == ImGuizmo::ROTATE)
				snapVal = m_GizmoSnapRotation;
			else if (operation == ImGuizmo::SCALE)
				snapVal = m_GizmoSnapScale;
			float snapValues[3] = { snapVal, snapVal, snapVal };

			if (ImGuizmo::Manipulate(
				glm::value_ptr(cameraView),
				glm::value_ptr(cameraProj),
				operation,
				static_cast<ImGuizmo::MODE>(m_GizmoMode),
				glm::value_ptr(modelMatrix),
				nullptr,
				snap ? snapValues : nullptr))
			{
				float translation[3], rotation[3], scale[3];
				ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMatrix), translation, rotation, scale);

				tc.Position = glm::vec3(translation[0], translation[1], translation[2]);

				if (operation == ImGuizmo::ROTATE)
				{
					tc.Rotation = glm::vec3(rotation[0], rotation[1], rotation[2]);
					if (isLight)
					{
						auto& lc = selectedEntity.GetComponent<LightComponent>();
						if (lc.Type == LightType::Directional || lc.Type == LightType::Spotlight)
						{
							lc.Direction = DirectionFromEuler(tc.Rotation);
						}
					}
				}
				else if (operation == ImGuizmo::SCALE && !isLight)
				{
					tc.Scale = glm::vec3(scale[0], scale[1], scale[2]);
				}
			}

			ImGuizmo::PopID();
		}

		// Draw Wireframe Collider & Character Controller Gizmos (Edit mode)
		if (m_SceneState == SceneState::Edit && selectedEntity && selectedEntity.HasComponent<TransformComponent>())
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			glm::mat4 cameraView = m_Camera.GetViewMatrix();
			glm::mat4 cameraProj = m_Camera.GetProjectionMatrix(m_ViewportSize);
			glm::mat4 viewProj = cameraProj * cameraView;

			auto WorldToScreen = [&](const glm::vec3& worldPos, ImVec2& outScreen) -> bool {
				glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);
				if (clip.w <= 0.001f) return false;
				glm::vec3 ndc = glm::vec3(clip) / clip.w;
				if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
				outScreen.x = imagePos.x + (ndc.x * 0.5f + 0.5f) * imageSize.x;
				outScreen.y = imagePos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * imageSize.y;
				return true;
			};

			auto Draw3DLine = [&](const glm::vec3& p0, const glm::vec3& p1, ImU32 col, float thickness = 1.5f) {
				ImVec2 s0, s1;
				if (WorldToScreen(p0, s0) && WorldToScreen(p1, s1))
				{
					drawList->AddLine(s0, s1, col, thickness);
				}
			};

			const auto& tc = selectedEntity.GetComponent<TransformComponent>();
			glm::mat4 entityWorld = tc.GetMatrix();

			// Character Controller (Capsule)
			if (selectedEntity.HasComponent<CharacterControllerComponent>())
			{
				const auto& cc = selectedEntity.GetComponent<CharacterControllerComponent>();
				glm::vec3 baseCenter = tc.Position + cc.Center;
				float r = cc.Radius;
				float h = cc.Height;
				float halfH = std::max(0.0f, h * 0.5f - r);
				glm::vec3 topCenter = baseCenter + glm::vec3(0.0f, halfH, 0.0f);
				glm::vec3 botCenter = baseCenter - glm::vec3(0.0f, halfH, 0.0f);
				ImU32 col = IM_COL32(80, 240, 100, 220); // bright green

				const int segs = 16;
				for (int i = 0; i < segs; ++i)
				{
					float a0 = (i * 2.0f * 3.14159265f) / segs;
					float a1 = ((i + 1) * 2.0f * 3.14159265f) / segs;

					Draw3DLine(topCenter + glm::vec3(std::cos(a0) * r, 0.0f, std::sin(a0) * r),
					           topCenter + glm::vec3(std::cos(a1) * r, 0.0f, std::sin(a1) * r), col);
					Draw3DLine(botCenter + glm::vec3(std::cos(a0) * r, 0.0f, std::sin(a0) * r),
					           botCenter + glm::vec3(std::cos(a1) * r, 0.0f, std::sin(a1) * r), col);
				}

				Draw3DLine(topCenter + glm::vec3( r, 0.0f, 0.0f), botCenter + glm::vec3( r, 0.0f, 0.0f), col);
				Draw3DLine(topCenter + glm::vec3(-r, 0.0f, 0.0f), botCenter + glm::vec3(-r, 0.0f, 0.0f), col);
				Draw3DLine(topCenter + glm::vec3(0.0f, 0.0f,  r), botCenter + glm::vec3(0.0f, 0.0f,  r), col);
				Draw3DLine(topCenter + glm::vec3(0.0f, 0.0f, -r), botCenter + glm::vec3(0.0f, 0.0f, -r), col);
			}

			// Box Collider
			if (selectedEntity.HasComponent<BoxColliderComponent>())
			{
				const auto& bc = selectedEntity.GetComponent<BoxColliderComponent>();
				glm::mat4 boxWorld = entityWorld * glm::translate(glm::mat4(1.0f), bc.Center);
				glm::vec3 h = bc.Size * 0.5f;
				ImU32 col = IM_COL32(240, 200, 60, 220); // yellow

				glm::vec3 c[8] = {
					glm::vec3(boxWorld * glm::vec4(-h.x, -h.y, -h.z, 1.0f)),
					glm::vec3(boxWorld * glm::vec4( h.x, -h.y, -h.z, 1.0f)),
					glm::vec3(boxWorld * glm::vec4( h.x,  h.y, -h.z, 1.0f)),
					glm::vec3(boxWorld * glm::vec4(-h.x,  h.y, -h.z, 1.0f)),
					glm::vec3(boxWorld * glm::vec4(-h.x, -h.y,  h.z, 1.0f)),
					glm::vec3(boxWorld * glm::vec4( h.x, -h.y,  h.z, 1.0f)),
					glm::vec3(boxWorld * glm::vec4( h.x,  h.y,  h.z, 1.0f)),
					glm::vec3(boxWorld * glm::vec4(-h.x,  h.y,  h.z, 1.0f))
				};

				Draw3DLine(c[0], c[1], col); Draw3DLine(c[1], c[2], col); Draw3DLine(c[2], c[3], col); Draw3DLine(c[3], c[0], col);
				Draw3DLine(c[4], c[5], col); Draw3DLine(c[5], c[6], col); Draw3DLine(c[6], c[7], col); Draw3DLine(c[7], c[4], col);
				Draw3DLine(c[0], c[4], col); Draw3DLine(c[1], c[5], col); Draw3DLine(c[2], c[6], col); Draw3DLine(c[3], c[7], col);
			}

			// Sphere Collider
			if (selectedEntity.HasComponent<SphereColliderComponent>())
			{
				const auto& sc = selectedEntity.GetComponent<SphereColliderComponent>();
				glm::vec3 center = glm::vec3(entityWorld * glm::vec4(sc.Center, 1.0f));
				float maxScale = std::max({ tc.Scale.x, tc.Scale.y, tc.Scale.z });
				float r = sc.Radius * maxScale;
				ImU32 col = IM_COL32(60, 200, 240, 220); // cyan

				const int segs = 16;
				for (int i = 0; i < segs; ++i)
				{
					float a0 = (i * 2.0f * 3.14159265f) / segs;
					float a1 = ((i + 1) * 2.0f * 3.14159265f) / segs;

					Draw3DLine(center + glm::vec3(std::cos(a0) * r, 0.0f, std::sin(a0) * r),
					           center + glm::vec3(std::cos(a1) * r, 0.0f, std::sin(a1) * r), col);
					Draw3DLine(center + glm::vec3(std::cos(a0) * r, std::sin(a0) * r, 0.0f),
					           center + glm::vec3(std::cos(a1) * r, std::sin(a1) * r, 0.0f), col);
					Draw3DLine(center + glm::vec3(0.0f, std::cos(a0) * r, std::sin(a0) * r),
					           center + glm::vec3(0.0f, std::cos(a1) * r, std::sin(a1) * r), col);
				}
			}
		}

		// In-Game Retro Doom HUD (Play mode)
		if (m_SceneState == SceneState::Play)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 center(imagePos.x + imageSize.x * 0.5f, imagePos.y + imageSize.y * 0.5f);

			// 1. Crosshair
			const float chSize = 7.0f;
			const float chGap = 3.0f;
			ImU32 chCol = IM_COL32(230, 40, 40, 240); // Red crosshair
			drawList->AddLine(ImVec2(center.x - chSize - chGap, center.y), ImVec2(center.x - chGap, center.y), chCol, 2.0f);
			drawList->AddLine(ImVec2(center.x + chGap, center.y), ImVec2(center.x + chSize + chGap, center.y), chCol, 2.0f);
			drawList->AddLine(ImVec2(center.x, center.y - chSize - chGap), ImVec2(center.x, center.y - chGap), chCol, 2.0f);
			drawList->AddLine(ImVec2(center.x, center.y + chGap), ImVec2(center.x, center.y + chSize + chGap), chCol, 2.0f);
			drawList->AddCircleFilled(center, 1.5f, chCol);

			// 2. Hitmarker (White X on demon hit)
			if (DoomGameState::HitmarkerTimer > 0.0f)
			{
				float hmSize = 10.0f;
				ImU32 hmCol = IM_COL32(255, 255, 255, (int)(std::min(1.0f, DoomGameState::HitmarkerTimer / 0.15f) * 255.0f));
				drawList->AddLine(ImVec2(center.x - hmSize, center.y - hmSize), ImVec2(center.x + hmSize, center.y + hmSize), hmCol, 2.0f);
				drawList->AddLine(ImVec2(center.x + hmSize, center.y - hmSize), ImVec2(center.x - hmSize, center.y + hmSize), hmCol, 2.0f);
			}

			// 3. Screen Damage Flash (Red overlay)
			if (DoomGameState::DamageFlashTimer > 0.0f)
			{
				float alpha = std::min(1.0f, DoomGameState::DamageFlashTimer / 0.35f);
				drawList->AddRectFilled(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), IM_COL32(220, 20, 20, (int)(alpha * 125.0f)));
			}

			// 4. Muzzle Flash (Warm yellow/orange burst)
			if (DoomGameState::MuzzleFlashTimer > 0.0f)
			{
				float alpha = std::min(1.0f, DoomGameState::MuzzleFlashTimer / 0.08f);
				drawList->AddRectFilled(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), IM_COL32(255, 210, 80, (int)(alpha * 65.0f)));
			}

			// 5. Retro Doom Status Bar (Bottom of viewport)
			float barH = 50.0f;
			float barMargin = 15.0f;
			ImVec2 barMin(imagePos.x + barMargin, imagePos.y + imageSize.y - barH - barMargin);
			ImVec2 barMax(imagePos.x + imageSize.x - barMargin, imagePos.y + imageSize.y - barMargin);

			drawList->AddRectFilled(barMin, barMax, IM_COL32(18, 18, 22, 235), 4.0f);
			drawList->AddRect(barMin, barMax, IM_COL32(180, 40, 40, 255), 4.0f, 0, 2.5f);

			float textY = barMin.y + 16.0f;
			float usableW = barMax.x - barMin.x;
			float colW = usableW / 5.0f;

			// HEALTH
			ImU32 hpColor = (DoomGameState::PlayerHealth > 50) ? IM_COL32(60, 225, 70, 255) :
			                (DoomGameState::PlayerHealth > 25) ? IM_COL32(245, 210, 40, 255) : IM_COL32(245, 40, 40, 255);
			std::string hpStr = "+ HEALTH: " + std::to_string(DoomGameState::PlayerHealth) + "%";
			drawList->AddText(ImVec2(barMin.x + 15.0f, textY), hpColor, hpStr.c_str());

			// ARMOR
			std::string armorStr = "SHIELD: " + std::to_string(DoomGameState::PlayerArmor) + "%";
			drawList->AddText(ImVec2(barMin.x + colW + 10.0f, textY), IM_COL32(70, 180, 255, 255), armorStr.c_str());

			// SHELLS
			std::string ammoStr = "SHELLS: " + std::to_string(DoomGameState::ShellsAmmo) + " / " + std::to_string(DoomGameState::MaxShells);
			drawList->AddText(ImVec2(barMin.x + colW * 2.0f + 10.0f, textY), IM_COL32(255, 200, 50, 255), ammoStr.c_str());

			// DEMONS
			std::string enemyStr = "DEMONS: " + std::to_string(DoomGameState::DemonsRemaining);
			drawList->AddText(ImVec2(barMin.x + colW * 3.0f + 10.0f, textY), IM_COL32(240, 60, 60, 255), enemyStr.c_str());

			// SCORE
			std::string scoreStr = "SCORE: " + std::to_string(DoomGameState::Score);
			drawList->AddText(ImVec2(barMin.x + colW * 4.0f + 10.0f, textY), IM_COL32(230, 230, 230, 255), scoreStr.c_str());

			// 6. Game Over / Victory Overlays
			if (DoomGameState::GameOver)
			{
				ImVec2 bannerMin(center.x - 220.0f, center.y - 70.0f);
				ImVec2 bannerMax(center.x + 220.0f, center.y + 70.0f);
				drawList->AddRectFilled(bannerMin, bannerMax, IM_COL32(40, 5, 5, 235), 8.0f);
				drawList->AddRect(bannerMin, bannerMax, IM_COL32(230, 30, 30, 255), 8.0f, 0, 3.0f);

				drawList->AddText(ImVec2(center.x - 85.0f, center.y - 45.0f), IM_COL32(255, 40, 40, 255), "=== YOU DIED ===");
				drawList->AddText(ImVec2(center.x - 145.0f, center.y - 15.0f), IM_COL32(220, 200, 200, 255), "DEMONS CONSUMED YOUR FLESH & SOUL");
				drawList->AddText(ImVec2(center.x - 110.0f, center.y + 18.0f), IM_COL32(255, 230, 80, 255), "Press [R] to Respawn & Restart");
			}
			else if (DoomGameState::Victory)
			{
				ImVec2 bannerMin(center.x - 220.0f, center.y - 70.0f);
				ImVec2 bannerMax(center.x + 220.0f, center.y + 70.0f);
				drawList->AddRectFilled(bannerMin, bannerMax, IM_COL32(15, 35, 15, 235), 8.0f);
				drawList->AddRect(bannerMin, bannerMax, IM_COL32(60, 230, 60, 255), 8.0f, 0, 3.0f);

				drawList->AddText(ImVec2(center.x - 105.0f, center.y - 45.0f), IM_COL32(60, 255, 80, 255), "*** VICTORY ACHIEVED ***");
				drawList->AddText(ImVec2(center.x - 135.0f, center.y - 15.0f), IM_COL32(220, 240, 220, 255), "ARENA PURGED OF ALL HELLSPAWN!");
				std::string finalScore = "Final Score: " + std::to_string(DoomGameState::Score) + "  |  Press [R] to Replay";
				drawList->AddText(ImVec2(center.x - 140.0f, center.y + 18.0f), IM_COL32(255, 230, 80, 255), finalScore.c_str());
			}
		}
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

void Editor::DrawSceneHierarchyPanel()
{
	if (m_ResetLayout)
	{
		ImGui::SetNextWindowPos(ImVec2(5, 25), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(280, 420), ImGuiCond_Always);
	}

	ImGui::Begin("Scene Hierarchy");

	auto view = m_Scene.GetRegistry().view<TagComponent>();
	for (auto entityHandle : view)
	{
		Entity entity = { entityHandle, &m_Scene };
		const auto& tag = entity.GetComponent<TagComponent>();

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (m_Scene.GetSelectedEntity() == entity)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		std::string componentBadges = "";
		if (entity.HasComponent<MeshComponent>()) componentBadges += "[Mesh] ";
		if (entity.HasComponent<MaterialComponent>()) componentBadges += "[Mat] ";
		if (entity.HasComponent<AudioSourceComponent>() || entity.HasComponent<AudioListenerComponent>()) componentBadges += "[Audio] ";
		if (entity.HasComponent<LightComponent>()) componentBadges += "[Light] ";
		if (entity.HasComponent<CameraComponent>()) componentBadges += "[Camera] ";
		if (entity.HasComponent<BoxColliderComponent>() || entity.HasComponent<SphereColliderComponent>() || entity.HasComponent<CapsuleColliderComponent>() || entity.HasComponent<MeshColliderComponent>()) componentBadges += "[Col] ";
		if (entity.HasComponent<CharacterControllerComponent>()) componentBadges += "[Controller] ";
		if (entity.HasComponent<NativeScriptComponent>()) componentBadges += "[Script] ";

		std::string label = tag.Tag + " " + componentBadges + "##Entity" + std::to_string(static_cast<uint32_t>(entityHandle));
		ImGui::TreeNodeEx(label.c_str(), flags);

		if (ImGui::IsItemClicked())
		{
			m_Scene.SetSelectedEntity(entity);
		}
	}

	if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("Create Empty Entity"))
		{
			Entity e = m_Scene.CreateEntity("New Entity");
			m_Scene.SetSelectedEntity(e);
		}
		if (ImGui::BeginMenu("Add 3D Model"))
		{
			if (ImGui::MenuItem("Empty Mesh Entity"))
			{
				Entity e = m_Scene.CreateEntity("New Mesh Entity");
				e.AddComponent<MeshComponent>();
				m_Scene.SetSelectedEntity(e);
			}
			ImGui::Separator();
			const auto& models = GetDiscoveredModels();
			for (const auto& modelPath : models)
			{
				std::filesystem::path p(modelPath);
				std::string label = p.stem().string() + " (" + p.extension().string() + ")";
				if (ImGui::MenuItem(label.c_str()))
				{
					Entity e = m_Scene.CreateModelEntity(p.stem().string(), modelPath);
					e.SetPosition(m_Camera.Position + m_Camera.Front * 5.0f);
					m_Scene.SetSelectedEntity(e);
				}
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Add Camera"))
		{
			Entity cam = m_Scene.CreateEntity("Camera");
			cam.SetPosition(m_Camera.Position);
			cam.SetRotation(glm::vec3(m_Camera.Pitch, m_Camera.Yaw, 0.0f));
			cam.AddComponent<CameraComponent>(m_Camera, true);
			m_Scene.SetSelectedEntity(cam);
		}
		if (ImGui::MenuItem("Add Directional Light"))
		{
			Entity light = m_Scene.CreateDirectionalLight("Directional Light", glm::vec3(-0.2f, -1.0f, -0.3f), glm::vec3(1.0f), 1.0f);
			m_Scene.SetSelectedEntity(light);
		}
		if (ImGui::MenuItem("Add Point Light"))
		{
			Entity light = m_Scene.CreatePointLight("Point Light", m_Camera.Position + m_Camera.Front * 5.0f, glm::vec3(1.0f, 0.9f, 0.7f), 3.0f, 25.0f);
			m_Scene.SetSelectedEntity(light);
		}
		if (ImGui::MenuItem("Add Spotlight"))
		{
			Entity light = m_Scene.CreateSpotLight("Spotlight", m_Camera.Position + m_Camera.Front * 5.0f, m_Camera.Front, glm::vec3(1.0f), 2.0f, 12.5f, 17.5f, 35.0f);
			m_Scene.SetSelectedEntity(light);
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

void Editor::DrawEntityInspectorPanel()
{
	if (m_ResetLayout)
	{
		ImGui::SetNextWindowPos(ImVec2(1305, 25), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(290, 560), ImGuiCond_Always);
	}

	ImGui::Begin("Entity Inspector");

	Entity selectedEntity = m_Scene.GetSelectedEntity();
	if (!selectedEntity)
	{
		ImGui::TextDisabled("No entity selected in hierarchy.");
		ImGui::End();
		return;
	}

	// Tag Component
	if (selectedEntity.HasComponent<TagComponent>())
	{
		auto& tag = selectedEntity.GetComponent<TagComponent>();
		char nameBuffer[128];
		strncpy_s(nameBuffer, tag.Tag.c_str(), sizeof(nameBuffer));
		if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
		{
			tag.Tag = std::string(nameBuffer);
		}

		ImGui::Checkbox("Active", &tag.Active);
	}

	ImGui::Separator();

	// Transform Component
	if (selectedEntity.HasComponent<TransformComponent>())
	{
		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Gizmo (W/E/R/Q):");
			if (ImGui::RadioButton("Translate##G", m_GizmoType == ImGuizmo::TRANSLATE)) m_GizmoType = ImGuizmo::TRANSLATE;
			ImGui::SameLine();
			if (ImGui::RadioButton("Rotate##G", m_GizmoType == ImGuizmo::ROTATE)) m_GizmoType = ImGuizmo::ROTATE;
			ImGui::SameLine();
			if (ImGui::RadioButton("Scale##G", m_GizmoType == ImGuizmo::SCALE)) m_GizmoType = ImGuizmo::SCALE;
			ImGui::SameLine();
			if (ImGui::RadioButton("Hide##G", m_GizmoType == -1)) m_GizmoType = -1;

			if (m_GizmoType != -1)
			{
				const char* modeNames[] = { "Local", "World" };
				ImGui::Combo("Coord Space", &m_GizmoMode, modeNames, IM_ARRAYSIZE(modeNames));
				ImGui::Checkbox("Snap", &m_GizmoSnap);
				if (m_GizmoSnap)
				{
					ImGui::SameLine();
					if (m_GizmoType == ImGuizmo::ROTATE)
						ImGui::DragFloat("Angle", &m_GizmoSnapRotation, 1.0f, 1.0f, 90.0f, "%.0f deg");
					else if (m_GizmoType == ImGuizmo::SCALE)
						ImGui::DragFloat("Scale Step", &m_GizmoSnapScale, 0.05f, 0.01f, 2.0f, "%.2f");
					else
						ImGui::DragFloat("Grid Step", &m_GizmoSnapTranslation, 0.1f, 0.01f, 10.0f, "%.2f m");
				}
			}

			ImGui::Separator();

			auto& tc = selectedEntity.GetComponent<TransformComponent>();
			ImGui::DragFloat3("Position", &tc.Position.x, 0.1f);
			if (ImGui::DragFloat3("Rotation", &tc.Rotation.x, 0.5f))
			{
				if (selectedEntity.HasComponent<LightComponent>())
				{
					auto& lc = selectedEntity.GetComponent<LightComponent>();
					if (lc.Type == LightType::Directional || lc.Type == LightType::Spotlight)
					{
						lc.Direction = DirectionFromEuler(tc.Rotation);
					}
				}
			}
			ImGui::DragFloat3("Scale", &tc.Scale.x, 0.05f, 0.001f, 100.0f);
		}
	}

	// Mesh Component
	if (selectedEntity.HasComponent<MeshComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Mesh Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& mc = selectedEntity.GetComponent<MeshComponent>();

			// Path input buffer
			char modelPathBuf[260];
			strncpy_s(modelPathBuf, mc.ModelPath.c_str(), sizeof(modelPathBuf));
			if (ImGui::InputText("Model File", modelPathBuf, sizeof(modelPathBuf), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				mc.LoadModel(modelPathBuf, mc.FlipUVs);
				if (mc.ModelAsset && !selectedEntity.HasComponent<RigidbodyComponent>())
				{
					selectedEntity.AddComponent<RigidbodyComponent>(mc.ModelAsset.get());
				}
			}

			// Discovered models dropdown / picker
			const auto& discoveredModels = GetDiscoveredModels();
			if (ImGui::BeginCombo("Quick Pick Model", mc.ModelPath.empty() ? "(Select Model...)" : mc.ModelPath.c_str()))
			{
				for (const auto& modelPath : discoveredModels)
				{
					bool isSelected = (mc.ModelPath == modelPath);
					if (ImGui::Selectable(modelPath.c_str(), isSelected))
					{
						mc.LoadModel(modelPath, mc.FlipUVs);
						if (mc.ModelAsset && !selectedEntity.HasComponent<RigidbodyComponent>())
						{
							selectedEntity.AddComponent<RigidbodyComponent>(mc.ModelAsset.get());
						}
					}
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (ImGui::Button("Load / Reload"))
			{
				mc.LoadModel(std::string(modelPathBuf), mc.FlipUVs);
				if (mc.ModelAsset && !selectedEntity.HasComponent<RigidbodyComponent>())
				{
					selectedEntity.AddComponent<RigidbodyComponent>(mc.ModelAsset.get());
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Rescan Folder"))
			{
				RescanDiscoveredModels();
			}

			if (ImGui::Checkbox("Flip UVs##Mesh", &mc.FlipUVs))
			{
				if (!mc.ModelPath.empty())
				{
					mc.LoadModel(mc.ModelPath, mc.FlipUVs);
				}
			}
			ImGui::SameLine();
			ImGui::Checkbox("Casts Shadow##Mesh", &mc.CastsShadow);

			if (mc.ModelAsset)
			{
				const auto& aabb = mc.ModelAsset->GetAABB();
				glm::vec3 extents = aabb.max - aabb.min;
				ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "Meshes: %zu", mc.ModelAsset->GetMeshCount());
				ImGui::SameLine();
				ImGui::TextDisabled("| Extents: (%.1f, %.1f, %.1f)", extents.x, extents.y, extents.z);
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "No 3D model loaded. Select or enter a file above.");
			}

			ImGui::Separator();
			if (ImGui::Button("Remove Mesh Component"))
			{
				selectedEntity.RemoveComponent<MeshComponent>();
			}
		}
	}

	// Material Component
	if (selectedEntity.HasComponent<MaterialComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("PBR Material", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& mat = selectedEntity.GetComponent<MaterialComponent>();

			char matNameBuf[64];
			strncpy_s(matNameBuf, mat.MaterialName.c_str(), sizeof(matNameBuf));
			if (ImGui::InputText("Material Name", matNameBuf, sizeof(matNameBuf)))
			{
				mat.MaterialName = matNameBuf;
			}

			// Preset selector
			const char* presetNames[] = {
				"Select Preset...",
				"Default (White)",
				"Metal / Chrome",
				"Gold",
				"Copper",
				"Plastic (Rough)",
				"Glass (Smooth)",
				"Emissive Core",
				"Concrete / Stone"
			};
			static int selectedPreset = 0;
			if (ImGui::Combo("Presets", &selectedPreset, presetNames, IM_ARRAYSIZE(presetNames)))
			{
				if (selectedPreset > 0)
				{
					mat.SetPreset(static_cast<MaterialPreset>(selectedPreset));
					switch (static_cast<MaterialPreset>(selectedPreset))
					{
					case MaterialPreset::Metal: mat.MaterialName = "Metal"; break;
					case MaterialPreset::Gold: mat.MaterialName = "Gold"; break;
					case MaterialPreset::Copper: mat.MaterialName = "Copper"; break;
					case MaterialPreset::PlasticRough: mat.MaterialName = "Plastic Rough"; break;
					case MaterialPreset::GlassSmooth: mat.MaterialName = "Glass Smooth"; break;
					case MaterialPreset::EmissiveCore: mat.MaterialName = "Emissive Core"; break;
					case MaterialPreset::ConcreteStone: mat.MaterialName = "Concrete"; break;
					default: mat.MaterialName = "Default"; break;
					}
					selectedPreset = 0;
				}
			}

			ImGui::ColorEdit4("Albedo Color", &mat.Properties.AlbedoColor.x);
			ImGui::SliderFloat("Metallic", &mat.Properties.Metallic, 0.0f, 1.0f);
			ImGui::SliderFloat("Roughness", &mat.Properties.Roughness, 0.0f, 1.0f);
			ImGui::SliderFloat("Ambient Occlusion", &mat.Properties.AO, 0.0f, 1.0f);

			ImGui::ColorEdit3("Emission Color", &mat.Properties.EmissionColor.x);
			ImGui::DragFloat("Emission Intensity", &mat.Properties.EmissionIntensity, 0.1f, 0.0f, 50.0f);

			ImGui::DragFloat2("UV Tiling", &mat.Properties.Tiling.x, 0.05f, 0.01f, 100.0f);
			ImGui::DragFloat2("UV Offset", &mat.Properties.Offset.x, 0.01f, -10.0f, 10.0f);

			if (ImGui::TreeNodeEx("Custom Texture Maps", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto DrawTextureSlot = [&](const char* label, std::string& path, std::shared_ptr<Texture2D>& tex) {
					char pathBuf[256];
					strncpy_s(pathBuf, path.c_str(), sizeof(pathBuf));
					ImGui::PushID(label);
					if (ImGui::InputText(label, pathBuf, sizeof(pathBuf)))
					{
						path = pathBuf;
					}
					ImGui::SameLine();
					if (ImGui::Button("Load"))
					{
						path = pathBuf;
						tex = AssetManager::Get().LoadTexture(path);
					}
					if (!path.empty())
					{
						ImGui::SameLine();
						if (ImGui::Button("Clear"))
						{
							path = "";
							tex = nullptr;
						}
					}
					if (tex)
					{
						ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "  -> Bound (%dx%d)", tex->GetWidth(), tex->GetHeight());
					}
					ImGui::PopID();
				};

				DrawTextureSlot("Albedo Map", mat.Properties.AlbedoMapPath, mat.Properties.AlbedoMap);
				DrawTextureSlot("Normal Map", mat.Properties.NormalMapPath, mat.Properties.NormalMap);
				DrawTextureSlot("Metallic-Roughness", mat.Properties.MetallicRoughnessMapPath, mat.Properties.MetallicRoughnessMap);
				DrawTextureSlot("AO Map", mat.Properties.AOMapPath, mat.Properties.AOMap);

				ImGui::TreePop();
			}

			if (ImGui::Button("Remove Material Component"))
			{
				selectedEntity.RemoveComponent<MaterialComponent>();
			}
		}
	}

	// Light Component
	if (selectedEntity.HasComponent<LightComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& lc = selectedEntity.GetComponent<LightComponent>();
			const char* lightTypeNames[] = { "Directional", "Point", "Spotlight" };
			int currentType = static_cast<int>(lc.Type);
			if (ImGui::Combo("Light Type", &currentType, lightTypeNames, IM_ARRAYSIZE(lightTypeNames)))
			{
				lc.Type = static_cast<LightType>(currentType);
			}

			ImGui::ColorEdit3("Color", &lc.Color.x);
			ImGui::SliderFloat("Intensity", &lc.Intensity, 0.0f, 20.0f);
			ImGui::Checkbox("Casts Shadow##Light", &lc.CastsShadow);

			if (lc.Type == LightType::Directional)
			{
				if (ImGui::DragFloat3("Direction", &lc.Direction.x, 0.01f, -1.0f, 1.0f))
				{
					if (glm::length(lc.Direction) > 0.001f)
					{
						lc.Direction = glm::normalize(lc.Direction);
						selectedEntity.SetRotation(EulerFromDirection(lc.Direction));
					}
				}
			}
			else if (lc.Type == LightType::Point)
			{
				ImGui::SliderFloat("Range", &lc.Range, 1.0f, 100.0f);
			}
			else if (lc.Type == LightType::Spotlight)
			{
				if (ImGui::DragFloat3("Direction", &lc.Direction.x, 0.01f, -1.0f, 1.0f))
				{
					if (glm::length(lc.Direction) > 0.001f)
					{
						lc.Direction = glm::normalize(lc.Direction);
						selectedEntity.SetRotation(EulerFromDirection(lc.Direction));
					}
				}
				ImGui::SliderFloat("Range", &lc.Range, 1.0f, 150.0f);
				if (ImGui::SliderFloat("Inner Cutoff", &lc.InnerCutoff, 0.0f, 89.0f))
				{
					if (lc.InnerCutoff > lc.OuterCutoff) lc.OuterCutoff = lc.InnerCutoff;
				}
				if (ImGui::SliderFloat("Outer Cutoff", &lc.OuterCutoff, lc.InnerCutoff, 89.0f))
				{
				}
			}

			if (ImGui::Button("Remove Light"))
			{
				selectedEntity.RemoveComponent<LightComponent>();
			}
		}
	}

	// Rigidbody Component
	if (selectedEntity.HasComponent<RigidbodyComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Rigidbody Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& rc = selectedEntity.GetComponent<RigidbodyComponent>();
			ImGui::DragFloat("Mass", &rc.Mass, 0.1f, 0.01f, 1000.0f);
			ImGui::DragFloat3("Velocity", &rc.Velocity.x, 0.1f);
			ImGui::Checkbox("Use Gravity", &rc.UseGravity);

			if (ImGui::Button("Remove Rigidbody"))
			{
				selectedEntity.RemoveComponent<RigidbodyComponent>();
			}
		}
	}

	// Camera Component
	if (selectedEntity.HasComponent<CameraComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& cc = selectedEntity.GetComponent<CameraComponent>();
			ImGui::Checkbox("Primary Camera", &cc.Primary);
			ImGui::SliderFloat("Field of View", &cc.CameraInstance.Zoom, 10.0f, 120.0f);

			if (ImGui::Button("Remove Camera"))
			{
				selectedEntity.RemoveComponent<CameraComponent>();
			}
		}
	}

	auto DrawLayerCombo = [](const char* label, uint32_t& layer) {
		const char* layerNames[] = {
			"Default",
			"Static Environment",
			"Player",
			"Enemy",
			"Projectile",
			"Trigger"
		};
		const uint32_t layerValues[] = {
			Physics::Layer_Default,
			Physics::Layer_Static,
			Physics::Layer_Player,
			Physics::Layer_Enemy,
			Physics::Layer_Projectile,
			Physics::Layer_Trigger
		};

		int currentIdx = 0;
		for (int i = 0; i < 6; ++i)
		{
			if (layer == layerValues[i])
			{
				currentIdx = i;
				break;
			}
		}

		if (ImGui::Combo(label, &currentIdx, layerNames, 6))
		{
			layer = layerValues[currentIdx];
		}
	};

	// Box Collider Component
	if (selectedEntity.HasComponent<BoxColliderComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Box Collider", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& bc = selectedEntity.GetComponent<BoxColliderComponent>();
			ImGui::DragFloat3("Size##Box", &bc.Size.x, 0.05f, 0.01f, 1000.0f);
			ImGui::DragFloat3("Center##Box", &bc.Center.x, 0.05f);
			ImGui::Checkbox("Is Trigger##Box", &bc.IsTrigger);
			DrawLayerCombo("Layer##Box", bc.Layer);

			if (ImGui::Button("Remove Box Collider"))
			{
				selectedEntity.RemoveComponent<BoxColliderComponent>();
			}
		}
	}

	// Sphere Collider Component
	if (selectedEntity.HasComponent<SphereColliderComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Sphere Collider", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& sc = selectedEntity.GetComponent<SphereColliderComponent>();
			ImGui::DragFloat("Radius##Sphere", &sc.Radius, 0.05f, 0.01f, 500.0f);
			ImGui::DragFloat3("Center##Sphere", &sc.Center.x, 0.05f);
			ImGui::Checkbox("Is Trigger##Sphere", &sc.IsTrigger);
			DrawLayerCombo("Layer##Sphere", sc.Layer);

			if (ImGui::Button("Remove Sphere Collider"))
			{
				selectedEntity.RemoveComponent<SphereColliderComponent>();
			}
		}
	}

	// Capsule Collider Component
	if (selectedEntity.HasComponent<CapsuleColliderComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Capsule Collider", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& cc = selectedEntity.GetComponent<CapsuleColliderComponent>();
			ImGui::DragFloat("Radius##Capsule", &cc.Radius, 0.05f, 0.01f, 500.0f);
			ImGui::DragFloat("Height##Capsule", &cc.Height, 0.05f, 0.01f, 500.0f);
			ImGui::DragFloat3("Center##Capsule", &cc.Center.x, 0.05f);
			ImGui::Checkbox("Is Trigger##Capsule", &cc.IsTrigger);
			DrawLayerCombo("Layer##Capsule", cc.Layer);

			if (ImGui::Button("Remove Capsule Collider"))
			{
				selectedEntity.RemoveComponent<CapsuleColliderComponent>();
			}
		}
	}

	// Mesh Collider Component
	if (selectedEntity.HasComponent<MeshColliderComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Mesh Collider", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& mc = selectedEntity.GetComponent<MeshColliderComponent>();
			ImGui::Checkbox("Is Trigger##MeshCol", &mc.IsTrigger);
			DrawLayerCombo("Layer##MeshCol", mc.Layer);

			if (ImGui::Button("Remove Mesh Collider"))
			{
				selectedEntity.RemoveComponent<MeshColliderComponent>();
			}
		}
	}

	// Character Controller Component
	if (selectedEntity.HasComponent<CharacterControllerComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Character Controller", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& cc = selectedEntity.GetComponent<CharacterControllerComponent>();
			ImGui::DragFloat("Radius##CC", &cc.Radius, 0.02f, 0.05f, 5.0f, "%.2f m");
			ImGui::DragFloat("Height##CC", &cc.Height, 0.05f, 0.2f, 10.0f, "%.2f m");
			ImGui::DragFloat("Step Offset##CC", &cc.StepOffset, 0.02f, 0.0f, 2.0f, "%.2f m");
			ImGui::SliderFloat("Slope Limit##CC", &cc.SlopeLimit, 0.0f, 89.0f, "%.1f deg");
			ImGui::DragFloat("Skin Width##CC", &cc.SkinWidth, 0.005f, 0.001f, 0.2f, "%.3f m");
			ImGui::DragFloat("Gravity##CC", &cc.Gravity, 0.5f, -100.0f, 0.0f, "%.1f m/s2");
			ImGui::DragFloat3("Center Offset##CC", &cc.Center.x, 0.05f);

			DrawLayerCombo("Layer##CC", cc.Layer);

			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Live Physics Status:");
			ImGui::Text("  Grounded: %s", cc.IsGrounded ? "YES" : "NO (Airborne)");
			ImGui::Text("  Ground Normal: (%.2f, %.2f, %.2f)", cc.GroundNormal.x, cc.GroundNormal.y, cc.GroundNormal.z);
			ImGui::Text("  Velocity: (%.2f, %.2f, %.2f) m/s", cc.Velocity.x, cc.Velocity.y, cc.Velocity.z);

			ImGui::Spacing();
			if (ImGui::Button("Remove Character Controller"))
			{
				selectedEntity.RemoveComponent<CharacterControllerComponent>();
			}
		}
	}

	// Native Script Component
	if (selectedEntity.HasComponent<NativeScriptComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Native Script", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& nsc = selectedEntity.GetComponent<NativeScriptComponent>();
			ImGui::Text("Script: %s", nsc.ScriptName.c_str());
			ImGui::TextDisabled("Status: %s", (nsc.Instance ? "Active / Running" : "Idle (runs in Play mode)"));

			ImGui::Spacing();
			if (ImGui::Button("Bind FPSController"))
			{
				nsc.Bind<FPSController>("FPSController");
				if (!selectedEntity.HasComponent<CharacterControllerComponent>())
				{
					selectedEntity.AddComponent<CharacterControllerComponent>();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Bind DoomPlayer"))
			{
				nsc.Bind<DoomPlayer>("DoomPlayer");
				if (!selectedEntity.HasComponent<CharacterControllerComponent>())
				{
					selectedEntity.AddComponent<CharacterControllerComponent>();
				}
			}
			if (ImGui::Button("Bind DoomEnemy"))
			{
				nsc.Bind<DoomEnemy>("DoomEnemy");
			}
			ImGui::SameLine();
			if (ImGui::Button("Bind DoomPickup"))
			{
				nsc.Bind<DoomPickup>("DoomPickup");
			}

			ImGui::Spacing();
			if (ImGui::Button("Remove Script"))
			{
				selectedEntity.RemoveComponent<NativeScriptComponent>();
			}
		}
	}

	// Audio Source Component
	if (selectedEntity.HasComponent<AudioSourceComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("3D Audio Source", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& asc = selectedEntity.GetComponent<AudioSourceComponent>();

			char soundPathBuf[256];
			strncpy_s(soundPathBuf, asc.SoundPath.c_str(), sizeof(soundPathBuf));
			if (ImGui::InputText("Sound File", soundPathBuf, sizeof(soundPathBuf)))
			{
				asc.SoundPath = soundPathBuf;
			}
			ImGui::SameLine();
			if (ImGui::Button("Load##Audio"))
			{
				asc.LoadSound(soundPathBuf);
			}

			// Quick picker from res/audio/
			std::vector<std::string> audioFiles;
			std::error_code ec;
			if (std::filesystem::exists("res/audio", ec))
			{
				for (const auto& entry : std::filesystem::recursive_directory_iterator("res/audio", ec))
				{
					if (entry.is_regular_file())
					{
						std::string ext = entry.path().extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
						if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
						{
							audioFiles.push_back(entry.path().generic_string());
						}
					}
				}
			}

			if (!audioFiles.empty())
			{
				std::vector<const char*> fileLabels;
				fileLabels.push_back("Select Sound File...");
				int currentItem = 0;
				for (size_t i = 0; i < audioFiles.size(); ++i)
				{
					fileLabels.push_back(audioFiles[i].c_str());
					if (audioFiles[i] == asc.SoundPath)
					{
						currentItem = static_cast<int>(i + 1);
					}
				}

				if (ImGui::Combo("Quick Pick##Audio", &currentItem, fileLabels.data(), static_cast<int>(fileLabels.size())))
				{
					if (currentItem > 0)
					{
						asc.LoadSound(audioFiles[currentItem - 1]);
					}
				}
			}

			// Preview playback buttons
			if (asc.IsPlaying())
			{
				if (ImGui::Button("Pause Preview"))
				{
					asc.Pause();
				}
				ImGui::SameLine();
				if (ImGui::Button("Stop Preview"))
				{
					asc.Stop();
				}
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Playing...");
			}
			else
			{
				if (ImGui::Button("Play Preview"))
				{
					asc.Play();
				}
				ImGui::SameLine();
				ImGui::TextDisabled("Stopped");
			}

			ImGui::SliderFloat("Volume##Audio", &asc.Volume, 0.0f, 2.0f);
			if (asc.SoundInstance) asc.SoundInstance->SetVolume(asc.Volume);

			ImGui::SliderFloat("Pitch##Audio", &asc.Pitch, 0.1f, 3.0f);
			if (asc.SoundInstance) asc.SoundInstance->SetPitch(asc.Pitch);

			ImGui::Checkbox("Loop##Audio", &asc.Loop);
			if (asc.SoundInstance) asc.SoundInstance->SetLooping(asc.Loop);

			ImGui::SameLine();
			ImGui::Checkbox("Play On Awake##Audio", &asc.PlayOnAwake);

			ImGui::Checkbox("3D Spatial##Audio", &asc.Spatial);
			if (asc.SoundInstance) asc.SoundInstance->SetSpatial(asc.Spatial);

			if (asc.Spatial)
			{
				ImGui::DragFloat("Min Distance", &asc.MinDistance, 0.1f, 0.01f, 100.0f);
				ImGui::DragFloat("Max Distance", &asc.MaxDistance, 0.5f, 1.0f, 1000.0f);
				ImGui::DragFloat("Roll-Off Factor", &asc.RollOff, 0.05f, 0.0f, 10.0f);
				if (asc.SoundInstance) asc.SoundInstance->SetAttenuation(asc.MinDistance, asc.MaxDistance, asc.RollOff);
			}

			ImGui::Separator();
			if (ImGui::Button("Remove Audio Source"))
			{
				asc.Stop();
				selectedEntity.RemoveComponent<AudioSourceComponent>();
			}
		}
	}

	// Audio Listener Component
	if (selectedEntity.HasComponent<AudioListenerComponent>())
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("3D Audio Listener", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& alc = selectedEntity.GetComponent<AudioListenerComponent>();
			ImGui::Checkbox("Primary Listener", &alc.Primary);
			ImGui::SliderFloat("Listener Volume", &alc.Volume, 0.0f, 2.0f);

			if (ImGui::Button("Remove Audio Listener"))
			{
				selectedEntity.RemoveComponent<AudioListenerComponent>();
			}
		}
	}

	// Add Component Button
	ImGui::Separator();
	if (ImGui::Button("Add Component...", ImVec2(-1.0f, 0.0f)))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		if (!selectedEntity.HasComponent<MeshComponent>())
		{
			if (ImGui::MenuItem("Mesh Component"))
			{
				selectedEntity.AddComponent<MeshComponent>();
			}
		}
		if (!selectedEntity.HasComponent<MaterialComponent>())
		{
			if (ImGui::MenuItem("Material Component"))
			{
				selectedEntity.AddComponent<MaterialComponent>();
			}
		}
		if (!selectedEntity.HasComponent<LightComponent>())
		{
			if (ImGui::MenuItem("Light Component"))
			{
				selectedEntity.AddComponent<LightComponent>();
			}
		}
		if (!selectedEntity.HasComponent<RigidbodyComponent>())
		{
			if (ImGui::MenuItem("Rigidbody Component"))
			{
				selectedEntity.AddComponent<RigidbodyComponent>();
			}
		}
		if (!selectedEntity.HasComponent<CameraComponent>())
		{
			if (ImGui::MenuItem("Camera Component"))
			{
				selectedEntity.AddComponent<CameraComponent>();
			}
		}
		if (!selectedEntity.HasComponent<BoxColliderComponent>())
		{
			if (ImGui::MenuItem("Box Collider"))
			{
				selectedEntity.AddComponent<BoxColliderComponent>();
			}
		}
		if (!selectedEntity.HasComponent<SphereColliderComponent>())
		{
			if (ImGui::MenuItem("Sphere Collider"))
			{
				selectedEntity.AddComponent<SphereColliderComponent>();
			}
		}
		if (!selectedEntity.HasComponent<CapsuleColliderComponent>())
		{
			if (ImGui::MenuItem("Capsule Collider"))
			{
				selectedEntity.AddComponent<CapsuleColliderComponent>();
			}
		}
		if (!selectedEntity.HasComponent<MeshColliderComponent>())
		{
			if (ImGui::MenuItem("Mesh Collider"))
			{
				selectedEntity.AddComponent<MeshColliderComponent>();
			}
		}
		if (!selectedEntity.HasComponent<CharacterControllerComponent>())
		{
			if (ImGui::MenuItem("Character Controller"))
			{
				selectedEntity.AddComponent<CharacterControllerComponent>();
			}
		}
		if (!selectedEntity.HasComponent<NativeScriptComponent>())
		{
			if (ImGui::MenuItem("Native Script Component"))
			{
				selectedEntity.AddComponent<NativeScriptComponent>();
			}
		}
		if (!selectedEntity.HasComponent<AudioSourceComponent>())
		{
			if (ImGui::MenuItem("Audio Source Component"))
			{
				selectedEntity.AddComponent<AudioSourceComponent>();
			}
		}
		if (!selectedEntity.HasComponent<AudioListenerComponent>())
		{
			if (ImGui::MenuItem("Audio Listener Component"))
			{
				selectedEntity.AddComponent<AudioListenerComponent>();
			}
		}
		ImGui::EndPopup();
	}

	ImGui::Separator();
	if (ImGui::Button("Delete Selected Entity", ImVec2(-1.0f, 0.0f)))
	{
		m_Scene.DestroyEntity(selectedEntity);
		m_Scene.SetSelectedEntity({});
		ImGui::End();
		return;
	}

	ImGui::End();
}

void Editor::DrawEnvironmentPanel()
{
	if (m_ResetLayout)
	{
		ImGui::SetNextWindowPos(ImVec2(5, 450), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(280, 440), ImGuiCond_Always);
	}

	ImGui::Begin("Lighting & Environment");

	Entity sun = m_Scene.GetSunLight();
	if (sun && sun.HasComponent<LightComponent>())
	{
		if (ImGui::CollapsingHeader("Sun Light", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& light = sun.GetComponent<LightComponent>();
			glm::vec3 dir = light.Direction;
			if (ImGui::DragFloat3("Sun Direction", &dir.x, 0.01f, -1.0f, 1.0f))
			{
				if (glm::length(dir) > 0.001f)
				{
					light.Direction = glm::normalize(dir);
					sun.SetRotation(EulerFromDirection(light.Direction));
				}
			}

			glm::vec3 color = light.Color;
			if (ImGui::ColorEdit3("Sun Color", &color.x))
			{
				light.Color = color;
			}

			float intensity = light.Intensity;
			if (ImGui::SliderFloat("Sun Intensity", &intensity, 0.0f, 5.0f))
			{
				light.Intensity = intensity;
			}
		}
	}

	auto skybox = m_Scene.GetSkybox();
	if (skybox)
	{
		if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool active = skybox->IsActive();
			if (ImGui::Checkbox("Enable Skybox", &active))
			{
				skybox->SetActive(active);
			}
		}
	}

	if (ImGui::CollapsingHeader("Renderer Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Frustum Culling", &m_Renderer.GetFrustumCullingEnabled());
		ImGui::SliderFloat("Alpha Cutoff", &m_Renderer.GetAlphaThreshold(), 0.0f, 1.0f, "%.2f");
	}

	if (ImGui::CollapsingHeader("Shadow Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		static int shadowQuality = 4; // Default ULTRA
		const char* qualityNames[] = { "Very Low", "Low", "Medium", "High", "Ultra" };
		if (ImGui::Combo("Shadow Quality", &shadowQuality, qualityNames, IM_ARRAYSIZE(qualityNames)))
		{
			m_Renderer.GetShadowRenderer().UpdateShadowSettings(static_cast<ShadowRenderer::ShadowQuality>(shadowQuality));
		}

		ImGui::SliderFloat("Shadow Bias", &m_Renderer.GetShadowRenderer().GetShadowSettings().shadowBias, 0.0f, 0.01f, "%.5f");

		if (!m_Renderer.GetShadowRenderer().GetShadowCascadeLevels().empty())
		{
			ImGui::SliderFloat4("Cascade Splits", &m_Renderer.GetShadowRenderer().GetShadowCascadeLevels()[0], 0.1f, 1000.0f);
		}

		ImGui::Checkbox("Debug Cascade Frustums", &m_Renderer.GetDebugCascades());
	}

	if (ImGui::CollapsingHeader("Post-Processing & Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Enable HDR Bloom", &m_BloomEnabled);
		if (m_BloomEnabled)
		{
			ImGui::SliderFloat("Bloom Intensity", &m_BloomIntensity, 0.0f, 5.0f, "%.2f");
			ImGui::SliderFloat("Bloom Threshold", &m_BloomThreshold, 0.1f, 5.0f, "%.2f");
			ImGui::SliderFloat("Bloom Radius", &m_BloomFilterRadius, 0.001f, 0.02f, "%.4f");
		}
		ImGui::Separator();
		ImGui::SliderFloat("Exposure", &m_Exposure, 0.01f, 10.0f, "%.2f");
		const char* tonemapOps[] = { "ACES Filmic", "Reinhard", "None (Clamp)" };
		ImGui::Combo("Tone Mapping", &m_ToneMappingOperator, tonemapOps, IM_ARRAYSIZE(tonemapOps));
		ImGui::SliderFloat("Gamma", &m_Gamma, 1.0f, 3.0f, "%.2f");
	}

	if (ImGui::CollapsingHeader("Atmospheric Fog", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& fog = m_Renderer.GetFogSettings();
		ImGui::Checkbox("Enable Fog", &fog.enabled);
		if (fog.enabled)
		{
			ImGui::ColorEdit3("Fog Color", &fog.color.x);
			ImGui::SliderFloat("Density", &fog.density, 0.001f, 0.1f, "%.4f");
			ImGui::DragFloat("Start Distance", &fog.start, 1.0f, 0.0f, 500.0f, "%.1f");
			ImGui::DragFloat("End Distance", &fog.end, 1.0f, 5.0f, 1000.0f, "%.1f");
			ImGui::SliderFloat("Height Falloff", &fog.heightFalloff, 0.0f, 0.1f, "%.4f");
		}
	}

	if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderFloat("Movement Speed", &m_Camera.MovementSpeed, 0.5f, 100.0f);
		ImGui::SliderFloat("Field of View", &m_Camera.Zoom, 10.0f, 120.0f);
		ImGui::Text("Position: (%.1f, %.1f, %.1f)", m_Camera.Position.x, m_Camera.Position.y, m_Camera.Position.z);
		ImGui::Text("Direction: (%.2f, %.2f, %.2f)", m_Camera.Front.x, m_Camera.Front.y, m_Camera.Front.z);

		if (ImGui::Button("Reset Camera"))
		{
			m_Camera.Position = glm::vec3(0.0f, 15.0f, 60.0f);
			m_Camera.Yaw = -90.0f;
			m_Camera.Pitch = 0.0f;
		}
	}

	ImGui::End();
}

void Editor::DrawMetricsPanel(float deltaTime)
{
	if (m_ResetLayout)
	{
		ImGui::SetNextWindowPos(ImVec2(1305, 590), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(290, 300), ImGuiCond_Always);
	}

	ImGui::Begin("Performance Metrics");

	ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
	ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);
	ImGui::Text("GPU Render Time: %.2f ms", m_GpuRenderTime);

	ImGui::Separator();
	ImGui::Text("Viewport: %d x %d", m_ViewportSize.x, m_ViewportSize.y);
	ImGui::Text("Entities: %zu", m_Scene.GetRegistry().view<TagComponent>().size());
	const auto& stats = m_Renderer.GetStats();
	ImGui::Text("Visible / Culled: %zu / %zu", stats.renderedEntities, stats.culledEntities);
	ImGui::Text("Lights: %zu", m_Scene.GetRegistry().view<LightComponent>().size());

	ImGui::End();
}

void Editor::SaveScene(const std::string& filepath)
{
	SceneSerializer serializer(m_Scene, &m_Renderer, this);
	if (serializer.Serialize(filepath))
	{
		m_CurrentScenePath = filepath;
		m_StatusMessage = "Scene saved: " + filepath;
		m_StatusTimer = 3.5f;
	}
	else
	{
		m_StatusMessage = "Failed to save scene: " + filepath;
		m_StatusTimer = 4.0f;
	}
}

void Editor::LoadScene(const std::string& filepath)
{
	SceneSerializer serializer(m_Scene, &m_Renderer, this);
	if (serializer.Deserialize(filepath))
	{
		m_CurrentScenePath = filepath;
		m_StatusMessage = "Scene loaded: " + filepath;
		m_StatusTimer = 3.5f;
	}
	else
	{
		m_StatusMessage = "Failed to load scene: " + filepath;
		m_StatusTimer = 4.0f;
	}
}

void Editor::NewScene()
{
	m_Scene.Clear();
	m_CurrentScenePath = "res/scenes/untitled.scene";
	m_StatusMessage = "New empty scene created";
	m_StatusTimer = 3.0f;
}

void Editor::SetupDoomArenaLevel()
{
	if (m_SceneState != SceneState::Edit)
	{
		OnSceneStop();
	}

	m_Scene.Clear();
	m_CurrentScenePath = "res/scenes/doom_level.scene";

	// 1. Editor Camera
	m_Camera.Position = glm::vec3(0.0f, 12.0f, 32.0f);
	m_Camera.Yaw = -90.0f;
	m_Camera.Pitch = -15.0f;

	// 2. Atmospheric Fog & Post-Processing (Doom Gothic Blood Mood)
	m_Exposure = 1.35f;
	m_BloomIntensity = 1.15f;
	m_BloomEnabled = true;

	auto& fog = m_Renderer.GetFogSettings();
	fog.enabled = true;
	fog.color = glm::vec3(0.14f, 0.04f, 0.04f); // Crimson fog
	fog.density = 0.018f;
	fog.start = 8.0f;
	fog.end = 110.0f;

	// 3. Castle Arena Geometry
	const std::string mapPath = "res/models/house/source/free_fire_maxx_new_clock_tower_map_download_free.glb";
	if (std::filesystem::exists(mapPath))
	{
		Entity arenaEntity = m_Scene.CreateModelEntity("Clock Tower Arena", mapPath);
		arenaEntity.SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
		arenaEntity.SetScale(glm::vec3(100.0f));
	}

	// 4. Ground Collider & Perimeter Walls
	Entity ground = m_Scene.CreateEntity("Arena Floor Collider");
	ground.SetPosition(glm::vec3(0.0f, -0.5f, 0.0f));
	ground.AddComponent<BoxColliderComponent>(glm::vec3(260.0f, 1.0f, 260.0f), glm::vec3(0.0f), false, Physics::Layer_Static);

	Entity wallN = m_Scene.CreateEntity("Boundary Wall North");
	wallN.SetPosition(glm::vec3(0.0f, 6.0f, 85.0f));
	wallN.AddComponent<BoxColliderComponent>(glm::vec3(180.0f, 16.0f, 4.0f), glm::vec3(0.0f), false, Physics::Layer_Static);

	Entity wallS = m_Scene.CreateEntity("Boundary Wall South");
	wallS.SetPosition(glm::vec3(0.0f, 6.0f, -85.0f));
	wallS.AddComponent<BoxColliderComponent>(glm::vec3(180.0f, 16.0f, 4.0f), glm::vec3(0.0f), false, Physics::Layer_Static);

	Entity wallE = m_Scene.CreateEntity("Boundary Wall East");
	wallE.SetPosition(glm::vec3(85.0f, 6.0f, 0.0f));
	wallE.AddComponent<BoxColliderComponent>(glm::vec3(4.0f, 16.0f, 180.0f), glm::vec3(0.0f), false, Physics::Layer_Static);

	Entity wallW = m_Scene.CreateEntity("Boundary Wall West");
	wallW.SetPosition(glm::vec3(-85.0f, 6.0f, 0.0f));
	wallW.AddComponent<BoxColliderComponent>(glm::vec3(4.0f, 16.0f, 180.0f), glm::vec3(0.0f), false, Physics::Layer_Static);

	// 5. Lighting (Blood Sun + Courtyard Torches)
	Entity sun = m_Scene.CreateEntity("Blood Sun");
	auto& sunLight = sun.AddComponent<LightComponent>();
	sunLight.Type = LightType::Directional;
	sunLight.Color = glm::vec3(0.95f, 0.35f, 0.15f);
	sunLight.Intensity = 1.25f;
	sunLight.Direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.4f));

	struct TorchData { const char* name; glm::vec3 pos; };
	TorchData torches[] = {
		{ "Courtyard Torch NE", { 14.0f, 4.0f, 14.0f } },
		{ "Courtyard Torch NW", { -14.0f, 4.0f, 14.0f } },
		{ "Courtyard Torch SE", { 14.0f, 4.0f, -14.0f } },
		{ "Courtyard Torch SW", { -14.0f, 4.0f, -14.0f } },
	};
	for (const auto& t : torches)
	{
		Entity torch = m_Scene.CreateEntity(t.name);
		torch.SetPosition(t.pos);
		auto& pl = torch.AddComponent<LightComponent>();
		pl.Type = LightType::Point;
		pl.Color = glm::vec3(1.0f, 0.45f, 0.1f);
		pl.Intensity = 3.5f;
		pl.Range = 25.0f;
	}

	// 6. Player (Doom Slayer)
	Entity player = m_Scene.CreateEntity("Doom Slayer (Player)");
	player.SetPosition(glm::vec3(0.0f, 2.0f, 0.0f));
	auto& camComp = player.AddComponent<CameraComponent>(true);
	camComp.CameraInstance.Position = glm::vec3(0.0f, 2.0f, 0.0f);
	player.AddComponent<CharacterControllerComponent>();
	player.AddComponent<AudioListenerComponent>();
	auto& playerScript = player.AddComponent<NativeScriptComponent>();
	playerScript.Bind<DoomPlayer>("DoomPlayer");

	// 7. Demon Enemies
	struct DemonData { const char* name; glm::vec3 pos; };
	DemonData demons[] = {
		{ "Cacodemon (Heavy)",   { 0.0f, 1.2f, 26.0f } },
		{ "Imp (Stalker East)",   { 20.0f, 1.2f, 14.0f } },
		{ "Imp (Stalker West)",   { -20.0f, 1.2f, 14.0f } },
		{ "Baron (North Hall)",   { 14.0f, 1.2f, -22.0f } },
		{ "Baron (Crypt Path)",   { -14.0f, 1.2f, -22.0f } },
	};
	const std::string enemyModelPath = "res/models/sphere/source/office desk.fbx";
	bool hasEnemyModel = std::filesystem::exists(enemyModelPath);

	for (const auto& d : demons)
	{
		Entity demon = hasEnemyModel ? m_Scene.CreateModelEntity(d.name, enemyModelPath)
		                             : m_Scene.CreateEntity(d.name);
		demon.SetPosition(d.pos);
		demon.SetScale(glm::vec3(1.2f));
		demon.AddComponent<CharacterControllerComponent>();
		demon.AddComponent<BoxColliderComponent>(glm::vec3(1.4f, 2.2f, 1.4f), glm::vec3(0.0f, 1.1f, 0.0f), false, Physics::Layer_Enemy);
		if (!demon.HasComponent<MaterialComponent>())
		{
			demon.AddComponent<MaterialComponent>();
		}
		auto& mat = demon.GetComponent<MaterialComponent>();
		mat.Properties.EmissionColor = glm::vec3(0.9f, 0.1f, 0.05f);
		mat.Properties.EmissionIntensity = 2.0f;
		auto& nsc = demon.AddComponent<NativeScriptComponent>();
		nsc.Bind<DoomEnemy>("DoomEnemy");
	}

	// 8. Pickups (Health & Shells)
	const std::string pickupModelPath = "res/models/bottle/source/Bottle_Water_perfect.glb";
	bool hasPickupModel = std::filesystem::exists(pickupModelPath);

	auto CreatePickup = [&](const char* name, const glm::vec3& pos) {
		Entity item = hasPickupModel ? m_Scene.CreateModelEntity(name, pickupModelPath)
		                             : m_Scene.CreateEntity(name);
		item.SetPosition(pos);
		item.SetScale(glm::vec3(0.35f));
		auto& nsc = item.AddComponent<NativeScriptComponent>();
		nsc.Bind<DoomPickup>("DoomPickup");
		return item;
	};

	CreatePickup("Health Potion (East)", glm::vec3(12.0f, 1.0f, 6.0f));
	CreatePickup("Health Potion (West)", glm::vec3(-14.0f, 1.0f, -8.0f));
	CreatePickup("Ammo Shells (North)",  glm::vec3(-12.0f, 1.0f, 10.0f));
	CreatePickup("Ammo Shells (South)",  glm::vec3(14.0f, 1.0f, -10.0f));

	DoomGameState::Reset();

	// 9. Persist to scene file
	SaveScene("res/scenes/doom_level.scene");
	m_StatusMessage = "Doom FPS arena assembled & saved to res/scenes/doom_level.scene! Press Play (F5) to test.";
	m_StatusTimer = 5.0f;
}

void Editor::DrawMainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene", "Ctrl+N"))
			{
				NewScene();
			}
			if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
			{
				m_ShowOpenModal = true;
			}
			if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
			{
				SaveScene(m_CurrentScenePath);
			}
			if (ImGui::MenuItem("Save Scene As..."))
			{
				m_ShowSaveAsModal = true;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Scene"))
		{
			if (m_SceneState == SceneState::Edit)
			{
				if (ImGui::MenuItem("Play Mode", "F5"))
				{
					OnScenePlay();
				}
			}
			else
			{
				if (ImGui::MenuItem("Stop Mode", "F5"))
				{
					OnSceneStop();
				}
				if (ImGui::MenuItem(m_SceneState == SceneState::Play ? "Pause Simulation" : "Resume Simulation"))
				{
					OnScenePause();
				}
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Add"))
		{
			if (ImGui::MenuItem("Create Empty Entity"))
			{
				Entity e = m_Scene.CreateEntity("New Entity");
				m_Scene.SetSelectedEntity(e);
			}
			if (ImGui::BeginMenu("Add 3D Model"))
			{
				if (ImGui::MenuItem("Empty Mesh Entity"))
				{
					Entity e = m_Scene.CreateEntity("New Mesh Entity");
					e.AddComponent<MeshComponent>();
					m_Scene.SetSelectedEntity(e);
				}
				ImGui::Separator();
				const auto& models = GetDiscoveredModels();
				for (const auto& modelPath : models)
				{
					std::filesystem::path p(modelPath);
					std::string label = p.stem().string() + " (" + p.extension().string() + ")";
					if (ImGui::MenuItem(label.c_str()))
					{
						Entity e = m_Scene.CreateModelEntity(p.stem().string(), modelPath);
						e.SetPosition(m_Camera.Position + m_Camera.Front * 5.0f);
						m_Scene.SetSelectedEntity(e);
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Camera"))
			{
				Entity cam = m_Scene.CreateEntity("Camera");
				cam.SetPosition(m_Camera.Position);
				cam.SetRotation(glm::vec3(m_Camera.Pitch, m_Camera.Yaw, 0.0f));
				cam.AddComponent<CameraComponent>(m_Camera, true);
				m_Scene.SetSelectedEntity(cam);
			}
			if (ImGui::MenuItem("Directional Light"))
			{
				Entity sun = m_Scene.CreateDirectionalLight("Directional Light", glm::vec3(-0.2f, -1.0f, -0.3f), glm::vec3(1.0f), 1.0f);
				m_Scene.SetSelectedEntity(sun);
			}
			if (ImGui::MenuItem("Point Light"))
			{
				Entity lamp = m_Scene.CreatePointLight("Point Light", m_Camera.Position + m_Camera.Front * 5.0f, glm::vec3(1.0f, 0.9f, 0.7f), 3.0f, 25.0f);
				m_Scene.SetSelectedEntity(lamp);
			}
			if (ImGui::MenuItem("Spotlight"))
			{
				Entity spot = m_Scene.CreateSpotLight("Spotlight", m_Camera.Position + m_Camera.Front * 5.0f, m_Camera.Front, glm::vec3(1.0f), 2.0f, 12.5f, 17.5f, 35.0f);
				m_Scene.SetSelectedEntity(spot);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			ImGui::MenuItem("Content Browser", nullptr, &m_ShowContentBrowser);
			ImGui::Separator();
			if (ImGui::MenuItem("Reset Layout"))
			{
				m_ResetLayout = true;
			}
			ImGui::EndMenu();
		}

		if (m_StatusTimer > 0.0f)
		{
			ImGui::SameLine(ImGui::GetWindowWidth() - 320.0f);
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", m_StatusMessage.c_str());
		}

		ImGui::EndMainMenuBar();
	}
}

void Editor::DrawModals()
{
	if (m_ShowSaveAsModal)
	{
		ImGui::OpenPopup("Save Scene As");
		m_ShowSaveAsModal = false;
	}

	if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Enter target scene file path:");
		ImGui::InputText("##SaveAsPath", m_ScenePathBuffer, sizeof(m_ScenePathBuffer));

		if (ImGui::Button("Save", ImVec2(120, 0)))
		{
			SaveScene(std::string(m_ScenePathBuffer));
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (m_ShowOpenModal)
	{
		ImGui::OpenPopup("Open Scene");
		m_ShowOpenModal = false;
	}

	if (ImGui::BeginPopupModal("Open Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Enter scene file path to open:");
		ImGui::InputText("##OpenPath", m_ScenePathBuffer, sizeof(m_ScenePathBuffer));

		if (ImGui::Button("Open", ImVec2(120, 0)))
		{
			LoadScene(std::string(m_ScenePathBuffer));
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void Editor::DrawContentBrowserPanel()
{
	if (m_ResetLayout)
	{
		ImGui::SetNextWindowPos(ImVec2(290, 620), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(1010, 270), ImGuiCond_Always);
	}

	if (!ImGui::Begin("Content Browser", &m_ShowContentBrowser))
	{
		ImGui::End();
		return;
	}

	// Navigation Bar
	bool hasParent = (m_ContentBrowserCurrentPath != "res" && m_ContentBrowserCurrentPath.has_parent_path());
	if (hasParent)
	{
		if (ImGui::Button("<- Back"))
		{
			m_ContentBrowserCurrentPath = m_ContentBrowserCurrentPath.parent_path();
		}
		ImGui::SameLine();
	}

	// Quick jump buttons
	if (ImGui::Button("res")) m_ContentBrowserCurrentPath = "res";
	ImGui::SameLine();
	if (ImGui::Button("models")) m_ContentBrowserCurrentPath = "res/models";
	ImGui::SameLine();
	if (ImGui::Button("textures")) m_ContentBrowserCurrentPath = "res/textures";
	ImGui::SameLine();
	if (ImGui::Button("scenes")) m_ContentBrowserCurrentPath = "res/scenes";
	ImGui::SameLine();
	if (ImGui::Button("audio")) m_ContentBrowserCurrentPath = "res/audio";
	ImGui::SameLine();
	if (ImGui::Button("shaders")) m_ContentBrowserCurrentPath = "shaders";
	ImGui::SameLine();

	ImGui::TextDisabled("| Path: %s", m_ContentBrowserCurrentPath.string().c_str());

	ImGui::Separator();

	// Search filter
	static char searchFilter[64] = "";
	ImGui::SetNextItemWidth(250.0f);
	ImGui::InputTextWithHint("##SearchFilter", "Search files...", searchFilter, sizeof(searchFilter));
	std::string searchStr = searchFilter;
	std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

	ImGui::Separator();

	std::error_code ec;
	if (std::filesystem::exists(m_ContentBrowserCurrentPath, ec) && std::filesystem::is_directory(m_ContentBrowserCurrentPath, ec))
	{
		Entity selectedEntity = m_Scene.GetSelectedEntity();

		if (ImGui::BeginTable("ContentBrowserTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Type / Size", ImGuiTableColumnFlags_WidthFixed, 110.0f);
			ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 230.0f);
			ImGui::TableHeadersRow();

			for (const auto& entry : std::filesystem::directory_iterator(m_ContentBrowserCurrentPath, ec))
			{
				const auto& path = entry.path();
				std::string filename = path.filename().string();

				if (!searchStr.empty())
				{
					std::string lowerName = filename;
					std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
					if (lowerName.find(searchStr) == std::string::npos)
						continue;
				}

				ImGui::TableNextRow();

				if (entry.is_directory())
				{
					ImGui::TableSetColumnIndex(0);
					std::string dirLabel = "[DIR] " + filename;
					if (ImGui::Selectable(dirLabel.c_str(), false))
					{
						m_ContentBrowserCurrentPath /= path.filename();
					}

					ImGui::TableSetColumnIndex(1);
					ImGui::TextDisabled("Folder");

					ImGui::TableSetColumnIndex(2);
					if (ImGui::SmallButton(("Open##" + filename).c_str()))
					{
						m_ContentBrowserCurrentPath /= path.filename();
					}
				}
				else
				{
					std::string ext = path.extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

					ImGui::TableSetColumnIndex(0);
					std::string fileLabel = filename;
					if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj") fileLabel = "[3D] " + filename;
					else if (ext == ".scene") fileLabel = "[SCENE] " + filename;
					else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") fileLabel = "[TEX] " + filename;
					else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac") fileLabel = "[AUD] " + filename;
					else if (ext == ".shader") fileLabel = "[SHD] " + filename;
					else fileLabel = "[FILE] " + filename;

					ImGui::Text("%s", fileLabel.c_str());

					ImGui::TableSetColumnIndex(1);
					auto fileSize = entry.file_size(ec);
					if (fileSize > 1024 * 1024)
						ImGui::Text("%.1f MB", static_cast<float>(fileSize) / (1024.0f * 1024.0f));
					else if (fileSize > 1024)
						ImGui::Text("%.1f KB", static_cast<float>(fileSize) / 1024.0f);
					else
						ImGui::Text("%llu B", fileSize);

					ImGui::TableSetColumnIndex(2);
					std::string fullPathStr = path.generic_string();

					if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj")
					{
						if (ImGui::SmallButton(("Spawn##" + filename).c_str()))
						{
							Entity e = m_Scene.CreateModelEntity(path.stem().string(), fullPathStr);
							e.SetPosition(m_Camera.Position + m_Camera.Front * 5.0f);
							m_Scene.SetSelectedEntity(e);
						}
						if (selectedEntity && selectedEntity.HasComponent<MeshComponent>())
						{
							ImGui::SameLine();
							if (ImGui::SmallButton(("Assign##" + filename).c_str()))
							{
								selectedEntity.GetComponent<MeshComponent>().LoadModel(fullPathStr);
							}
						}
					}
					else if (ext == ".scene")
					{
						if (ImGui::SmallButton(("Load Scene##" + filename).c_str()))
						{
							LoadScene(fullPathStr);
						}
					}
					else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga")
					{
						if (selectedEntity && selectedEntity.HasComponent<MaterialComponent>())
						{
							auto& mat = selectedEntity.GetComponent<MaterialComponent>();
							if (ImGui::SmallButton(("Albedo##" + filename).c_str()))
							{
								mat.Properties.AlbedoMapPath = fullPathStr;
								mat.Properties.AlbedoMap = AssetManager::Get().LoadTexture(fullPathStr);
							}
							ImGui::SameLine();
							if (ImGui::SmallButton(("Norm##" + filename).c_str()))
							{
								mat.Properties.NormalMapPath = fullPathStr;
								mat.Properties.NormalMap = AssetManager::Get().LoadTexture(fullPathStr);
							}
							ImGui::SameLine();
							if (ImGui::SmallButton(("MR##" + filename).c_str()))
							{
								mat.Properties.MetallicRoughnessMapPath = fullPathStr;
								mat.Properties.MetallicRoughnessMap = AssetManager::Get().LoadTexture(fullPathStr);
							}
							ImGui::SameLine();
							if (ImGui::SmallButton(("AO##" + filename).c_str()))
							{
								mat.Properties.AOMapPath = fullPathStr;
								mat.Properties.AOMap = AssetManager::Get().LoadTexture(fullPathStr);
							}
						}
						else
						{
							ImGui::TextDisabled("Select entity w/ Mat");
						}
					}
					else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
					{
						if (ImGui::SmallButton(("Preview##" + filename).c_str()))
						{
							AudioEngine::Get().Play2D(fullPathStr);
						}
						if (selectedEntity)
						{
							ImGui::SameLine();
							if (selectedEntity.HasComponent<AudioSourceComponent>())
							{
								if (ImGui::SmallButton(("Assign##" + filename).c_str()))
								{
									selectedEntity.GetComponent<AudioSourceComponent>().LoadSound(fullPathStr);
								}
							}
							else
							{
								if (ImGui::SmallButton(("Add Audio##" + filename).c_str()))
								{
									selectedEntity.AddComponent<AudioSourceComponent>(fullPathStr);
								}
							}
						}
					}
					else
					{
						ImGui::TextDisabled("-");
					}
				}
			}

			ImGui::EndTable();
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Directory '%s' does not exist.", m_ContentBrowserCurrentPath.string().c_str());
		if (ImGui::Button("Reset to 'res'"))
		{
			m_ContentBrowserCurrentPath = "res";
		}
	}

	ImGui::End();
}

