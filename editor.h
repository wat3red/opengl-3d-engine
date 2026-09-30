// editor.h
#pragma once

#include "scene.h"
#include "camera.h"
#include "renderer.h"
#include "framebuffer.h"
#include <glm/glm.hpp>

class Editor {
public:
	Editor(Scene& scene, Camera& camera, Renderer& renderer, Framebuffer& framebuffer);

	void RenderUI(float deltaTime, float renderTime);

	bool IsViewportFocused() const { return m_IsViewportFocused; }
	bool IsViewportHovered() const { return m_IsViewportHovered; }
	glm::ivec2 GetViewportSize() const { return m_ViewportSize; }

private:
	Scene& m_Scene;
	Camera& m_Camera;
	Renderer& m_Renderer;
	Framebuffer& m_Framebuffer;

	bool m_IsViewportFocused = false;
	bool m_IsViewportHovered = false;
	glm::ivec2 m_ViewportSize = { 1280, 720 };

	void DrawViewportPanel();
	void DrawSceneHierarchyPanel();
	void DrawEntityInspectorPanel();
	void DrawEnvironmentPanel();
	void DrawMetricsPanel(float deltaTime, float renderTime);
};
