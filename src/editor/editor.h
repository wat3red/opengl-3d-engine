// editor.h
#pragma once

#include "scene.h"
#include "camera.h"
#include "renderer.h"
#include "framebuffer.h"
#include "input_handler.h"
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

	Framebuffer& GetFramebuffer() { return *m_Framebuffer; }
	const Framebuffer& GetFramebuffer() const { return *m_Framebuffer; }

	bool IsViewportFocused() const { return m_IsViewportFocused; }
	bool IsViewportHovered() const { return m_IsViewportHovered; }
	glm::ivec2 GetViewportSize() const { return m_ViewportSize; }
	float GetGpuRenderTime() const { return m_GpuRenderTime; }

private:
	Scene& m_Scene;
	Renderer& m_Renderer;

	Camera m_Camera;
	InputHandler m_InputHandler;
	std::unique_ptr<Framebuffer> m_Framebuffer;

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
