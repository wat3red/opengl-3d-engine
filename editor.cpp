// editor.cpp
#include "editor.h"
#include <imgui/imgui.h>
#include <iostream>

Editor::Editor(Scene& scene, Camera& camera, Renderer& renderer, Framebuffer& framebuffer)
	: m_Scene(scene), m_Camera(camera), m_Renderer(renderer), m_Framebuffer(framebuffer)
{
}

void Editor::RenderUI(float deltaTime, float renderTime)
{
	DrawViewportPanel();
	DrawSceneHierarchyPanel();
	DrawEntityInspectorPanel();
	DrawEnvironmentPanel();
	DrawMetricsPanel(deltaTime, renderTime);
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
		m_Framebuffer.Resize(m_ViewportSize.x, m_ViewportSize.y);
	}

	unsigned int textureID = m_Framebuffer.GetColorAttachmentRendererID();
	if (textureID != 0)
	{
		ImGui::Image(
			(ImTextureID)(intptr_t)textureID,
			ImVec2(static_cast<float>(m_Framebuffer.GetWidth()), static_cast<float>(m_Framebuffer.GetHeight())),
			ImVec2(0.0f, 1.0f),
			ImVec2(1.0f, 0.0f)
		);
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

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (m_Scene.GetSelectedEntity() == entity)
			{
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			std::string label = entity->GetName() + "##Entity" + std::to_string(i);
			ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", label.c_str());

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

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (m_Scene.GetSelectedEntity() == light)
			{
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			std::string typeStr = "[Light] ";
			if (light->GetType() == LightType::Directional) typeStr = "[Sun] ";
			else if (light->GetType() == LightType::Point) typeStr = "[Point] ";
			else if (light->GetType() == LightType::Spotlight) typeStr = "[Spot] ";

			std::string label = typeStr + light->GetName() + "##Light" + std::to_string(i);
			ImGui::TreeNodeEx((void*)(intptr_t)(1000 + i), flags, "%s", label.c_str());

			if (ImGui::IsItemClicked())
			{
				m_Scene.SetSelectedEntity(light);
			}
		}
	}

	if (ImGui::Button("Clear Selection"))
	{
		m_Scene.SetSelectedEntity(nullptr);
	}

	ImGui::End();
}

void Editor::DrawEntityInspectorPanel()
{
	ImGui::Begin("Entity Inspector");

	auto selectedEntity = m_Scene.GetSelectedEntity();
	if (!selectedEntity)
	{
		ImGui::TextDisabled("Select an entity from Scene Hierarchy to inspect.");
		ImGui::End();
		return;
	}

	// Entity Name
	char nameBuffer[256];
	strncpy_s(nameBuffer, sizeof(nameBuffer), selectedEntity->GetName().c_str(), sizeof(nameBuffer) - 1);
	if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
	{
		selectedEntity->SetName(nameBuffer);
	}

	bool active = selectedEntity->IsActive();
	if (ImGui::Checkbox("Active", &active))
	{
		selectedEntity->SetActive(active);
	}

	ImGui::Separator();

	// Transform Component
	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		Transform& transform = selectedEntity->GetTransform();

		ImGui::DragFloat3("Position", &transform.Position.x, 0.1f);
		ImGui::DragFloat3("Rotation", &transform.Rotation.x, 0.5f, -360.0f, 360.0f);
		ImGui::DragFloat3("Scale", &transform.Scale.x, 0.05f, 0.001f, 100.0f);

		if (ImGui::Button("Reset Transform"))
		{
			transform.Position = glm::vec3(0.0f);
			transform.Rotation = glm::vec3(0.0f);
			transform.Scale = glm::vec3(1.0f);
		}
	}

	// Model Entity Component
	auto modelEntity = std::dynamic_pointer_cast<ModelEntity>(selectedEntity);
	if (modelEntity && modelEntity->GetModel())
	{
		if (ImGui::CollapsingHeader("Model Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Source: %s", modelEntity->GetModelPath().c_str());
			ImGui::Text("Meshes: %d", static_cast<int>(modelEntity->GetModel()->GetMeshCount()));

			glm::vec3 bmin = modelEntity->GetBoundingBoxMin();
			glm::vec3 bmax = modelEntity->GetBoundingBoxMax();
			ImGui::Text("Bounds Min: (%.1f, %.1f, %.1f)", bmin.x, bmin.y, bmin.z);
			ImGui::Text("Bounds Max: (%.1f, %.1f, %.1f)", bmax.x, bmax.y, bmax.z);
		}
	}

	// Light Component
	auto light = std::dynamic_pointer_cast<Light>(selectedEntity);
	if (light)
	{
		if (ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			glm::vec3 color = light->GetColor();
			if (ImGui::ColorEdit3("Color", &color.x))
			{
				light->SetColor(color);
			}

			float intensity = light->GetIntensity();
			if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 10.0f))
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

	if (ImGui::CollapsingHeader("Shadow Settings", ImGuiTreeNodeFlags_DefaultOpen))
	{
		static int shadowQuality = 4; // Default ULTRA
		const char* qualityNames[] = { "Very Low", "Low", "Medium", "High", "Ultra" };
		if (ImGui::Combo("Shadow Quality", &shadowQuality, qualityNames, IM_ARRAYSIZE(qualityNames)))
		{
			m_Renderer.m_ShadowRenderer.UpdateShadowSettings(static_cast<ShadowRenderer::ShadowQuality>(shadowQuality));
		}

		ImGui::SliderFloat("Shadow Bias", &m_Renderer.m_ShadowRenderer.m_ShadowSettings.shadowBias, 0.0f, 0.01f, "%.5f");

		if (!m_Renderer.m_ShadowRenderer.shadowCascadeLevels.empty())
		{
			ImGui::SliderFloat4("Cascade Splits", &m_Renderer.m_ShadowRenderer.shadowCascadeLevels[0], 0.1f, 1000.0f);
		}

		ImGui::Checkbox("Debug Cascade Frustums", &m_Renderer.debug1);
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

void Editor::DrawMetricsPanel(float deltaTime, float renderTime)
{
	ImGui::Begin("Performance Metrics");

	ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
	ImGui::Text("Frame Time: %.2f ms", deltaTime * 1000.0f);
	ImGui::Text("GPU Render Time: %.2f ms", renderTime);

	ImGui::Separator();
	ImGui::Text("Viewport: %d x %d", m_ViewportSize.x, m_ViewportSize.y);
	ImGui::Text("Entities: %zu", m_Scene.GetEntities().size());
	ImGui::Text("Lights: %zu", m_Scene.GetLights().size());

	ImGui::End();
}
