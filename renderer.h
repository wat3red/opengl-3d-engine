// renderer.h
#pragma once

#include "camera.h"
#include "shader.h"
#include "light.h"
#include "model_entity.h"
#include <vector>
#include <GL/glew.h>

#define MAX_SPOT_LIGHTS 5

class Renderer {
public:
	struct PointLightShadowData {
		GLuint depthCubemap;
		GLuint fbo;
	};

	Renderer(glm::ivec2 windowSize);
	void CalculateSceneBounds(const glm::vec3& lightDirection, 
		const glm::vec3& minBounds, const glm::vec3& maxBounds, float& sceneLeft, 
		float& sceneRight, float& sceneBottom, float& sceneTop, float& nearPlane, float& farPlane);
	void CalculateWorldBounds(const std::vector<ModelEntity*>& entities,
		glm::vec3& minBounds, glm::vec3& maxBounds);

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
	unsigned int m_DirDepthMapFBO, m_DirDepthMap;  // For directional and spotlight shadows
	std::vector<PointLightShadowData> m_PointLightShadows;
private:
	static constexpr GLuint DIR_SHADOW_WIDTH = 10000, DIR_SHADOW_HEIGHT = 10000;
	static constexpr GLuint POINT_SHADOW_WIDTH = 1024, POINT_SHADOW_HEIGHT = 1024;

	unsigned int cubeVAO, cubeVBO;

	Shader m_MainShader;
	Shader m_ShadowShader;
	Shader m_LightShader;
	Shader m_PointLightShadowShader;     // Shader for point light shadows
	Shader m_DirectionalSpotShadowShader; // Shader for directional and spot light shadows

	void SetupCube();
	void SetupDepthMaps(int numPointLights);

	void RenderShadowMaps(const glm::ivec2& windowSize, 
		const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities);
	
};
