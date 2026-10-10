#pragma once

#include "scene.h"
#include "camera.h"
#include "renderer.h"
#include "framebuffer.h"
#include "input_handler.h"
#include "bloom_renderer.h"
#include <glm/glm.hpp>
#include <memory>

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

	void DrawViewportPanel();
	void DrawSceneHierarchyPanel();
	void DrawEntityInspectorPanel();
	void DrawEnvironmentPanel();
	void DrawMetricsPanel(float deltaTime);
};
