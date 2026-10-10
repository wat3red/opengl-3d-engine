#include "editor.h"
#include "skybox.h"
#include "primitive.h"
#include "light.h"

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

	m_ResetLayout = false;

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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

	// Allow camera look-around only when right clicking inside the viewport and not currently manipulating gizmos
	m_InputHandler.SetCanCaptureRightClick(m_IsViewportHovered && !ImGuizmo::IsUsing());

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

		// 3D Viewport Gizmos
		Entity selectedEntity = m_Scene.GetSelectedEntity();
		if (selectedEntity && selectedEntity.HasComponent<TransformComponent>() && m_GizmoType != -1)
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
		if (entity.HasComponent<LightComponent>()) componentBadges += "[Light] ";
		if (entity.HasComponent<CameraComponent>()) componentBadges += "[Camera] ";

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
			ImGui::TextWrapped("Model: %s", mc.ModelPath.c_str());
			ImGui::Checkbox("Casts Shadow##Mesh", &mc.CastsShadow);
			if (ImGui::Button("Reload Model"))
			{
				mc.LoadModel(mc.ModelPath, mc.FlipUVs);
			}
			ImGui::SameLine();
			if (ImGui::Button("Remove Mesh"))
			{
				selectedEntity.RemoveComponent<MeshComponent>();
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

			if (ImGui::Button("Remove Camera"))
			{
				selectedEntity.RemoveComponent<CameraComponent>();
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
			if (ImGui::MenuItem("Create Empty Entity"))
			{
				Entity e = m_Scene.CreateEntity("New Entity");
				m_Scene.SetSelectedEntity(e);
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
