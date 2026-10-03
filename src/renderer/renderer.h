#pragma once

#include "camera.h"
#include "shader.h"
#include "light.h"
#include "model_entity.h"
#include "shadow_renderer.h"
#include <vector>
#include <GL/glew.h>

#define GLM_ENABLE_EXPERIMENTAL
#define MAX_SPOT_LIGHTS 5

class Scene;

class Renderer {
public:
	Renderer();
	explicit Renderer(glm::ivec2* windowSize);

	void Initialize();
	void RenderScene(const glm::ivec2& windowSize, const Camera& camera, const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities, unsigned int targetFBO = 0);
	void RenderScene(const glm::ivec2& windowSize, const Camera& camera, Scene& scene, unsigned int targetFBO = 0);

	void Cleanup();
	void RenderCube();

	glm::mat4 GetLightSpaceMatrix(Camera camera, glm::ivec2 ws, glm::vec3 lightDir, float nearPlane, float farPlane)
	{
		return m_ShadowRenderer.GetLightSpaceMatrix(camera, ws, lightDir, nearPlane, farPlane);
	}

	Shader& GetLightShader() { return m_LightShader; }
	const Shader& GetLightShader() const { return m_LightShader; }

	Shader& GetMainShader() { return m_MainShader; }
	const Shader& GetMainShader() const { return m_MainShader; }

	Shader& GetShadowShader() { return m_ShadowShader; }
	const Shader& GetShadowShader() const { return m_ShadowShader; }

	ShadowRenderer& GetShadowRenderer() { return m_ShadowRenderer; }
	const ShadowRenderer& GetShadowRenderer() const { return m_ShadowRenderer; }

	bool& GetDebugCascades() { return m_DebugCascades; }
	bool IsDebugCascadesEnabled() const { return m_DebugCascades; }
	void SetDebugCascades(bool enabled) { m_DebugCascades = enabled; }

	float& GetAlphaThreshold() { return m_AlphaThreshold; }
	float GetAlphaThreshold() const { return m_AlphaThreshold; }
	void SetAlphaThreshold(float threshold) { m_AlphaThreshold = threshold; }

private:
	ShadowRenderer m_ShadowRenderer;
	bool m_DebugCascades = false;
	float m_AlphaThreshold = 0.5f;

	std::vector<glm::mat4> m_CascadeLightSpaceMatrices;

	static constexpr GLuint POINT_SHADOW_WIDTH = 1024, POINT_SHADOW_HEIGHT = 1024;

	std::vector<GLuint> m_CascadeDepthMaps;
	std::vector<GLuint> m_CascadeFBOs;

	glm::ivec2* m_WindowSize = nullptr;
	Camera m_CurrentCamera;
	Shader m_DebugShader;

	Shader m_MainShader;
	Shader m_ShadowShader;
	Shader m_LightShader;
	Shader m_PointLightShadowShader;
	Shader m_DirectionalSpotShadowShader;
};