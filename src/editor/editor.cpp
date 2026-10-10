#include "editor.h"
#include "skybox.h"
#include "primitive.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include "scene_serializer.h"

#include <chrono>
#include <algorithm>

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

void Editor::ProcessInput(GLFWwindow* window, float deltaTime)
{
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

	if (m_ViewportSize.x > 0 && m_ViewportSize.y > 0)
	{
		// 1. Render 3D scene into HDR Framebuffer (GL_RGBA16F)
		m_Renderer.RenderScene(m_ViewportSize, m_Camera, m_Scene, m_HDRFramebuffer->GetRendererID());

		Shader& lightShader = m_Renderer.GetLightShader();
		lightShader.Bind();

		for (const auto& light : m_Scene.GetLights())
		{
			if (!light || !light->IsActive() || light->GetType() != LightType::Point)
				continue;

			lightShader.SetUniform4f("objectColor", glm::vec4(light->GetColor(), 1.0f));

			glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), light->GetPosition());
			modelMatrix = glm::scale(modelMatrix, glm::vec3(0.2f));
			lightShader.SetUniformMat4f("model", modelMatrix);
			lightShader.SetUniformMat4f("view", m_Camera.GetViewMatrix());
			lightShader.SetUniformMat4f("projection", m_Camera.GetProjectionMatrix(m_ViewportSize));

			m_Renderer.RenderCube();
		}

		lightShader.Unbind();
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
	DrawModals();
	DrawViewportPanel();
	DrawSceneHierarchyPanel();
	DrawEntityInspectorPanel();
	DrawEnvironmentPanel();
	DrawMetricsPanel(deltaTime);

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Editor::DrawViewportPanel()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Scene Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	m_IsViewportFocused = ImGui::IsWindowFocused();
	m_IsViewportHovered = ImGui::IsWindowHovered();

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

		// 3D Viewport Gizmos
		auto selectedEntity = m_Scene.GetSelectedEntity();
		if (selectedEntity && m_GizmoType != -1)
		{
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();

			ImVec2 windowPos = ImGui::GetWindowPos();
			ImVec2 windowSize = ImGui::GetWindowSize();
			ImGuizmo::SetRect(windowPos.x, windowPos.y, windowSize.x, windowSize.y);

			glm::mat4 cameraView = m_Camera.GetViewMatrix();
			glm::mat4 cameraProj = m_Camera.GetProjectionMatrix(m_ViewportSize);
			glm::mat4 modelMatrix = selectedEntity->GetModelMatrix();

			bool snap = m_GizmoSnap;
			float snapVal = m_GizmoSnapTranslation;
			if (m_GizmoType == ImGuizmo::ROTATE)
				snapVal = m_GizmoSnapRotation;
			else if (m_GizmoType == ImGuizmo::SCALE)
				snapVal = m_GizmoSnapScale;
			float snapValues[3] = { snapVal, snapVal, snapVal };

			if (ImGuizmo::Manipulate(
				glm::value_ptr(cameraView),
				glm::value_ptr(cameraProj),
				static_cast<ImGuizmo::OPERATION>(m_GizmoType),
				static_cast<ImGuizmo::MODE>(m_GizmoMode),
				glm::value_ptr(modelMatrix),
				nullptr,
				snap ? snapValues : nullptr))
			{
				float translation[3], rotation[3], scale[3];
				ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMatrix), translation, rotation, scale);

				selectedEntity->SetPosition(glm::vec3(translation[0], translation[1], translation[2]));
				selectedEntity->SetRotation(glm::vec3(rotation[0], rotation[1], rotation[2]));
				selectedEntity->SetScale(glm::vec3(scale[0], scale[1], scale[2]));
			}
		}
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

void Editor::DrawSceneHierarchyPanel()
{
	ImGui::Begin("Scene Hierarchy");

	if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& entities = m_Scene.GetEntities();
		for (size_t i = 0; i < entities.size(); ++i)
		{
			auto& entity = entities[i];
			if (!entity) continue;

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (m_Scene.GetSelectedEntity() == entity)
			{
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			std::string label = entity->GetName() + "##Entity" + std::to_string(i);
			ImGui::TreeNodeEx(label.c_str(), flags);

			if (ImGui::IsItemClicked())
			{
				m_Scene.SetSelectedEntity(entity);
			}
		}
	}

	if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& lights = m_Scene.GetLights();
		for (size_t i = 0; i < lights.size(); ++i)
		{
			auto& light = lights[i];
			if (!light) continue;

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (m_Scene.GetSelectedEntity() == light)
			{
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			std::string label = light->GetName() + "##Light" + std::to_string(i);
			ImGui::TreeNodeEx(label.c_str(), flags);

			if (ImGui::IsItemClicked())
			{
				m_Scene.SetSelectedEntity(light);
			}
		}
	}

	if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem("Add Directional Light"))
		{
			auto light = std::make_shared<DirectionalLight>("Directional Light", glm::vec3(-0.2f, -1.0f, -0.3f), glm::vec3(1.0f), 1.0f);
			m_Scene.AddLight(light);
			m_Scene.SetSelectedEntity(light);
		}
		if (ImGui::MenuItem("Add Point Light"))
		{
			auto light = std::make_shared<PointLight>("Point Light", m_Camera.Position + m_Camera.Front * 5.0f, glm::vec3(1.0f, 0.9f, 0.7f), 3.0f, 25.0f);
			m_Scene.AddLight(light);
			m_Scene.SetSelectedEntity(light);
		}
		if (ImGui::MenuItem("Add Spotlight"))
		{
			auto light = std::make_shared<SpotLight>("Spotlight", m_Camera.Position, m_Camera.Front, glm::vec3(1.0f), 2.0f, 12.5f, 17.5f);
			m_Scene.AddLight(light);
			m_Scene.SetSelectedEntity(light);
		}
		ImGui::EndPopup();
	}

	ImGui::End();
}

void Editor::DrawEntityInspectorPanel()
{
	ImGui::Begin("Entity Inspector");

	auto selectedEntity = m_Scene.GetSelectedEntity();
	if (!selectedEntity)
	{
		ImGui::TextDisabled("No entity selected in hierarchy.");
		ImGui::End();
		return;
	}

	char nameBuffer[128];
	strncpy_s(nameBuffer, selectedEntity->GetName().c_str(), sizeof(nameBuffer));
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		selectedEntity->SetName(std::string(nameBuffer));
	}

	bool active = selectedEntity->IsActive();
	if (ImGui::Checkbox("Active", &active))
	{
		selectedEntity->SetActive(active);
	}

	ImGui::Separator();

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

		glm::vec3 pos = selectedEntity->GetPosition();
		if (ImGui::DragFloat3("Position", &pos.x, 0.1f))
		{
			selectedEntity->SetPosition(pos);
		}

		glm::vec3 rot = selectedEntity->GetRotation();
		if (ImGui::DragFloat3("Rotation", &rot.x, 0.5f))
		{
			selectedEntity->SetRotation(rot);
		}

		glm::vec3 scale = selectedEntity->GetScale();
		if (ImGui::DragFloat3("Scale", &scale.x, 0.05f, 0.001f, 100.0f))
		{
			selectedEntity->SetScale(scale);
		}
	}

	auto light = std::dynamic_pointer_cast<Light>(selectedEntity);
	if (light)
	{
		ImGui::Separator();
		if (ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			glm::vec3 color = light->GetColor();
			if (ImGui::ColorEdit3("Color", &color.x))
			{
				light->SetColor(color);
			}

			float intensity = light->GetIntensity();
			if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 20.0f))
			{
				light->SetIntensity(intensity);
			}

			if (light->GetType() == LightType::Directional)
			{
				glm::vec3 dir = light->GetDirection();
				if (ImGui::DragFloat3("Direction", &dir.x, 0.01f, -1.0f, 1.0f))
				{
					if (glm::length(dir) > 0.001f)
						light->SetDirection(glm::normalize(dir));
				}
			}
			else if (light->GetType() == LightType::Point)
			{
				float range = light->GetRange();
				if (ImGui::SliderFloat("Range", &range, 1.0f, 100.0f))
				{
					light->SetRange(range);
				}
			}
			else if (light->GetType() == LightType::Spotlight)
			{
				glm::vec3 dir = light->GetDirection();
				if (ImGui::DragFloat3("Direction", &dir.x, 0.01f, -1.0f, 1.0f))
				{
					if (glm::length(dir) > 0.001f)
						light->SetDirection(glm::normalize(dir));
				}

				float inner = light->GetInnerCutoff();
				float outer = light->GetOuterCutoff();
				if (ImGui::SliderFloat("Inner Cutoff", &inner, 0.0f, 89.0f))
				{
					light->SetInnerCutoff(inner);
				}
				if (ImGui::SliderFloat("Outer Cutoff", &outer, inner, 89.0f))
				{
					light->SetOuterCutoff(outer);
				}
			}
		}
	}

	ImGui::Separator();
	if (ImGui::Button("Delete Selected Entity", ImVec2(-1.0f, 0.0f)))
	{
		auto& ents = m_Scene.GetEntities();
		ents.erase(std::remove_if(ents.begin(), ents.end(), [&](const auto& e) { return e == selectedEntity; }), ents.end());

		auto& lights = m_Scene.GetLights();
		lights.erase(std::remove_if(lights.begin(), lights.end(), [&](const auto& l) { return l == selectedEntity; }), lights.end());

		m_Scene.SetSelectedEntity(nullptr);
		ImGui::End();
		return;
	}

	ImGui::End();
}

void Editor::DrawEnvironmentPanel()
{
	ImGui::Begin("Lighting & Environment");

	auto sun = m_Scene.GetSunLight();
	if (sun)
	{
		if (ImGui::CollapsingHeader("Sun Light", ImGuiTreeNodeFlags_DefaultOpen))
		{
			glm::vec3 dir = sun->GetDirection();
			if (ImGui::DragFloat3("Sun Direction", &dir.x, 0.01f, -1.0f, 1.0f))
			{
				if (glm::length(dir) > 0.001f)
					sun->SetDirection(glm::normalize(dir));
			}

			glm::vec3 color = sun->GetColor();
			if (ImGui::ColorEdit3("Sun Color", &color.x))
			{
				sun->SetColor(color);
			}

			float intensity = sun->GetIntensity();
			if (ImGui::SliderFloat("Sun Intensity", &intensity, 0.0f, 5.0f))
			{
				sun->SetIntensity(intensity);
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
	ImGui::Begin("Performance Metrics");

	ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
	ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);
	ImGui::Text("GPU Render Time: %.2f ms", m_GpuRenderTime);

	ImGui::Separator();
	ImGui::Text("Viewport: %d x %d", m_ViewportSize.x, m_ViewportSize.y);
	ImGui::Text("Entities: %zu", m_Scene.GetEntities().size());
	const auto& stats = m_Renderer.GetStats();
	ImGui::Text("Visible / Culled: %zu / %zu", stats.renderedEntities, stats.culledEntities);
	ImGui::Text("Lights: %zu", m_Scene.GetLights().size());

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

		if (ImGui::BeginMenu("Add"))
		{
			if (ImGui::MenuItem("Directional Light"))
			{
				auto sun = std::make_shared<DirectionalLight>("Directional Light", glm::vec3(-0.2f, -1.0f, -0.3f), glm::vec3(1.0f), 1.0f);
				m_Scene.AddLight(sun);
				m_Scene.SetSelectedEntity(sun);
			}
			if (ImGui::MenuItem("Point Light"))
			{
				auto lamp = std::make_shared<PointLight>("Point Light", m_Camera.Position + m_Camera.Front * 5.0f, glm::vec3(1.0f, 0.9f, 0.7f), 3.0f, 25.0f);
				m_Scene.AddLight(lamp);
				m_Scene.SetSelectedEntity(lamp);
			}
			if (ImGui::MenuItem("Spotlight"))
			{
				auto spot = std::make_shared<SpotLight>("Spotlight", m_Camera.Position, m_Camera.Front, glm::vec3(1.0f), 2.0f, 12.5f, 17.5f);
				m_Scene.AddLight(spot);
				m_Scene.SetSelectedEntity(spot);
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
