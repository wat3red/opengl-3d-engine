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

class Renderer {
public:
	ShadowSettings m_ShadowSettings;
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
	void RenderScene(const glm::ivec2& windowSize, const Camera& camera, const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities);

	void Cleanup();
	void RenderCube();

	Shader GetLightShader() const {
		return m_LightShader;
	}
	Shader GetMainShader() const {
		return m_MainShader;
	}
	Shader GetShadowShader() const {
		return m_ShadowShader;
	}

	bool debug1;

private:
	std::vector<glm::mat4> cascadeLightSpaceMatrices;

	//static constexpr GLuint DIR_SHADOW_RESOLUTION = 4096;
	static constexpr GLuint POINT_SHADOW_WIDTH = 1024, POINT_SHADOW_HEIGHT = 1024;

	std::vector<GLuint> m_CascadeDepthMaps; // “екстуры дл€ теней
	std::vector<GLuint> m_CascadeFBOs;     // FBO дл€ каждой каскадной текстуры

	glm::ivec2* m_WindowSize;
	Camera m_CurrentCamera;

	unsigned int cubeVAO, cubeVBO;

	Shader m_DebugShader;

	Shader m_MainShader;
	Shader m_ShadowShader;
	Shader m_LightShader;
	Shader m_PointLightShadowShader;     // Shader for point light shadows
	Shader m_DirectionalSpotShadowShader; // Shader for directional and spot light shadows

	void SetupCube();
	//void SetupDepthMaps(int numPointLights);

	/*void RenderShadowMaps(const glm::ivec2& windowSize,
		const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities);*/
};
