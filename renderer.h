// renderer.h
#pragma once

#include "camera.h"
#include "shader.h"
#include "light.h"
#include "model_entity.h"
#include <vector>
#include <GL/glew.h>

enum class ShadowQuality {
	LOW,
	MEDIUM,
	HIGH,
	ULTRA
};

struct ShadowSettings {
	int resolution;                     // Shadow map resolution
	int numCascades;                   // Number of cascades
	float cascadeSplitLambda;          // Cascade distribution factor (0 = linear, 1 = logarithmic)
	int filteringQuality;              // Filtering method (e.g., PCF kernel size)
	float shadowBias;                  // Global shadow bias
};

#define MAX_SPOT_LIGHTS 5

class Renderer {
public:
	ShadowSettings m_ShadowSettings;

	ShadowSettings GetShadowSettings(ShadowQuality quality) {
		switch (quality) {
		case ShadowQuality::LOW:
			return { 512, 3, 0.5f, 4, 0.005f };
		case ShadowQuality::MEDIUM:
			return { 2048, 3, 1.0f, 8, 0.0001f };
		case ShadowQuality::HIGH:
			return { 2048, 4, 1.5, 16, 0.0008f };
		case ShadowQuality::ULTRA:
			return { 4096, 4, 2.0, 32, 0.0005f }; 
		}
		return { 1024, 4, 0.8f, 8, 0.002f }; // Default to medium
	}

	std::vector<glm::mat4> lightMatricesCache;

	struct PointLightShadowData {
		GLuint depthCubemap;
		GLuint fbo;
	};

	glm::mat4 getLightSpaceMatrix(Camera camera, glm::ivec2 ws, glm::vec3 lightDir, const float nearPlane, const float farPlane);
	std::vector<glm::mat4> GetLightSpaceMatrices(glm::vec3 lightDir);
	Renderer(glm::ivec2* windowSize);

	void Initialize();
	void UpdateShadowSettings(ShadowQuality quality);
	void RecalculateShadowSettings();
	void RenderScene(const glm::ivec2& windowSize, const Camera& camera, const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities);
	
	void SetupCascadeDepthMaps();

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
	unsigned int lightDepthMaps;
	std::vector<float> shadowCascadeLevels{ };

	bool debug1;

private:
	std::vector<glm::mat4> cascadeLightSpaceMatrices;

	//static constexpr GLuint DIR_SHADOW_RESOLUTION = 4096;
	static constexpr GLuint POINT_SHADOW_WIDTH = 1024, POINT_SHADOW_HEIGHT = 1024;
	unsigned int matricesUBO;

	unsigned int lightFBO;

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
	void SetupDepthMaps(int numPointLights);

	void RenderShadowMaps(const glm::ivec2& windowSize, 
		const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities);
};
