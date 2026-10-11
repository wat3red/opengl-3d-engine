#pragma once

#include "scene.h"
#include "camera.h"
#include "renderer.h"
#include "framebuffer.h"
#include "input_handler.h"
#include "bloom_renderer.h"
#include <glm/glm.hpp>
#include <memory>
#include <filesystem>

struct GLFWwindow;

class Editor {
public:
	Editor(Scene& scene, Renderer& renderer);
	~Editor();

	Editor(const Editor&) = delete;
	Editor& operator=(const Editor&) = delete;

	void Initialize(GLFWwindow* window);
	void Shutdown();

	void ProcessInput(GLFWwindow* window, float deltaTime);
	void RenderSceneViewport();
	void RenderUI(float deltaTime);

	Camera& GetCamera() { return m_Camera; }
	const Camera& GetCamera() const { return m_Camera; }

	Framebuffer& GetFramebuffer() { return *m_LDRFramebuffer; }
	const Framebuffer& GetFramebuffer() const { return *m_LDRFramebuffer; }

	bool IsViewportFocused() const { return m_IsViewportFocused; }
	bool IsViewportHovered() const { return m_IsViewportHovered; }
	glm::ivec2 GetViewportSize() const { return m_ViewportSize; }
	float GetGpuRenderTime() const { return m_GpuRenderTime; }

	float& GetExposure() { return m_Exposure; }
	float GetExposure() const { return m_Exposure; }

	float& GetGamma() { return m_Gamma; }
	float GetGamma() const { return m_Gamma; }

	int& GetToneMappingOperator() { return m_ToneMappingOperator; }
	int GetToneMappingOperator() const { return m_ToneMappingOperator; }

	bool& GetBloomEnabled() { return m_BloomEnabled; }
	float& GetBloomIntensity() { return m_BloomIntensity; }
	float& GetBloomThreshold() { return m_BloomThreshold; }
	float& GetBloomFilterRadius() { return m_BloomFilterRadius; }

	int GetGizmoType() const { return m_GizmoType; }
	void SetGizmoType(int type) { m_GizmoType = type; }
	int GetGizmoMode() const { return m_GizmoMode; }
	void SetGizmoMode(int mode) { m_GizmoMode = mode; }
	bool& GetGizmoSnap() { return m_GizmoSnap; }

	void SaveScene(const std::string& filepath = "res/scenes/default.scene");
	void LoadScene(const std::string& filepath = "res/scenes/default.scene");
	void NewScene();
	void ResetDefaultLayout() { m_ResetLayout = true; }

	// Runtime Scene State Management
	enum class SceneState {
		Edit = 0,
		Play = 1,
		Pause = 2
	};

	SceneState GetSceneState() const { return m_SceneState; }
	void OnScenePlay();
	void OnSceneStop();
	void OnScenePause();

private:
	Scene& m_Scene;
	Renderer& m_Renderer;

	Camera m_Camera;
	InputHandler m_InputHandler;
	std::unique_ptr<Framebuffer> m_HDRFramebuffer;
	std::unique_ptr<Framebuffer> m_LDRFramebuffer;
	Shader m_ToneMapShader;
	BloomRenderer m_BloomRenderer;

	float m_Exposure = 1.0f;
	int m_ToneMappingOperator = 0;
	float m_Gamma = 2.2f;

	bool m_BloomEnabled = true;
	float m_BloomThreshold = 1.0f;
	float m_BloomIntensity = 0.8f;
	float m_BloomFilterRadius = 0.005f;

	bool m_IsViewportFocused = false;
	bool m_IsViewportHovered = false;
	glm::ivec2 m_ViewportSize = { 1280, 720 };
	float m_GpuRenderTime = 0.0f;

	int m_GizmoType = 7; // ImGuizmo::TRANSLATE
	int m_GizmoMode = 0; // ImGuizmo::LOCAL
	bool m_GizmoSnap = false;
	float m_GizmoSnapTranslation = 0.5f;
	float m_GizmoSnapRotation = 45.0f;
	float m_GizmoSnapScale = 0.5f;

	std::string m_CurrentScenePath = "res/scenes/default.scene";
	char m_ScenePathBuffer[256] = "res/scenes/default.scene";
	bool m_ShowSaveAsModal = false;
	bool m_ShowOpenModal = false;
	std::string m_StatusMessage = "";
	float m_StatusTimer = 0.0f;
	bool m_ResetLayout = false;

	SceneState m_SceneState = SceneState::Edit;
	std::unique_ptr<Scene> m_EditorSceneBackup = nullptr;

	bool m_ShowContentBrowser = true;
	std::filesystem::path m_ContentBrowserCurrentPath = "res";

	void DrawMainMenuBar();
	void DrawModals();
	void DrawToolbar();
	void DrawViewportPanel();
	void DrawSceneHierarchyPanel();
	void DrawEntityInspectorPanel();
	void DrawEnvironmentPanel();
	void DrawMetricsPanel(float deltaTime);
	void DrawContentBrowserPanel();
};
