// renderer.h
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
	ShadowRenderer m_ShadowRenderer;

	std::vector<glm::mat4> lightMatricesCache;

	struct PointLightShadowData {
		GLuint depthCubemap;
		GLuint fbo;
	};

	glm::mat4 getLightSpaceMatrix(Camera camera, glm::ivec2 ws, glm::vec3 lightDir, const float nearPlane, const float farPlane);
	std::vector<glm::mat4> GetLightSpaceMatrices(glm::vec3 lightDir);
	Renderer(glm::ivec2* windowSize);

	void Initialize();
	void RenderScene(const glm::ivec2& windowSize, const Camera& camera, const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities, unsigned int targetFBO = 0);
	void RenderScene(const glm::ivec2& windowSize, const Camera& camera, Scene& scene, unsigned int targetFBO = 0);

	void Cleanup();
	void RenderCube();

	Shader& GetLightShader() { return m_LightShader; }
	const Shader& GetLightShader() const { return m_LightShader; }

	Shader& GetMainShader() { return m_MainShader; }
	const Shader& GetMainShader() const { return m_MainShader; }

	Shader& GetShadowShader() { return m_ShadowShader; }
	const Shader& GetShadowShader() const { return m_ShadowShader; }

	bool debug1 = false;
private:
	std::vector<glm::mat4> cascadeLightSpaceMatrices;

	static constexpr GLuint POINT_SHADOW_WIDTH = 1024, POINT_SHADOW_HEIGHT = 1024;

	std::vector<GLuint> m_CascadeDepthMaps;
	std::vector<GLuint> m_CascadeFBOs;

	glm::ivec2* m_WindowSize;
	Camera m_CurrentCamera;

	unsigned int cubeVAO, cubeVBO;

	Shader m_DebugShader;

	Shader m_MainShader;
	Shader m_ShadowShader;
	Shader m_LightShader;
	Shader m_PointLightShadowShader;
	Shader m_DirectionalSpotShadowShader;

	void SetupCube();
};