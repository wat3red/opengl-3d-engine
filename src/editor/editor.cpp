#include "editor.h"
#include "skybox.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <chrono>

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
	m_Framebuffer = std::make_unique<Framebuffer>(FramebufferSpecification{ 1280, 720 });
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
	if (ImGui::GetCurrentContext())
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
	m_Framebuffer.reset();
}

void Editor::ProcessInput(GLFWwindow* window, float deltaTime)
{
	m_InputHandler.Update(window, deltaTime);
}

void Editor::RenderSceneViewport()
{
	if (!m_Framebuffer)
	{
		return;
	}

	auto start = std::chrono::high_resolution_clock::now();

	if (m_ViewportSize.x > 0 && m_ViewportSize.y > 0)
	{
		m_Renderer.RenderScene(m_ViewportSize, m_Camera, m_Scene, m_Framebuffer->GetRendererID());

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
	}

	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float> duration = end - start;
	m_GpuRenderTime = duration.count() * 1000.0f;

	m_Framebuffer->Unbind();
}

void Editor::RenderUI(float deltaTime)
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

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
		if (m_Framebuffer && (m_Framebuffer->GetWidth() != m_ViewportSize.x || m_Framebuffer->GetHeight() != m_ViewportSize.y))
		{
			m_Framebuffer->Resize(m_ViewportSize.x, m_ViewportSize.y);
		}
	}

	if (m_Framebuffer)
	{
		unsigned int textureID = m_Framebuffer->GetColorAttachmentRendererID();
		if (textureID != 0)
		{
			ImGui::Image(
				(ImTextureID)(intptr_t)textureID,
				ImVec2(static_cast<float>(m_Framebuffer->GetWidth()), static_cast<float>(m_Framebuffer->GetHeight())),
				ImVec2(0.0f, 1.0f),
				ImVec2(1.0f, 0.0f)
			);
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
	ImGui::Text("Lights: %zu", m_Scene.GetLights().size());

	ImGui::End();
}
