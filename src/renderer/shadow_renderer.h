// shadow_renderer.h
#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "light.h"
#include "camera.h"
#include "shader.h"
#include "entity.h"
#include "model_entity.h"

#define GLM_ENABLE_EXPERIMENTAL

struct PointLightShadowData {
	GLuint depthCubemap;
	GLuint fbo;
};

struct ShadowSettings {
	int resolution;                     // Shadow map resolution
	int numCascades;                   // Number of cascades
	float cascadeSplitLambda;          // Cascade distribution factor (0 = linear, 1 = logarithmic)
	int filteringQuality;              // Filtering method (e.g., PCF kernel size)
	float shadowBias;                  // Global shadow bias
};

class ShadowRenderer {
public:
	enum class ShadowQuality {
		VERY_LOW,
		LOW,
		MEDIUM,
		HIGH,
		ULTRA
	};

	static ShadowSettings GetShadowSettings(ShadowQuality quality) {
		switch (quality) {
		case ShadowQuality::VERY_LOW:
			return { 256, 3, 0.4f, 2, 0.01f }; // Minimal quality
		case ShadowQuality::LOW:
			return { 512, 3, 0.5f, 4, 0.005f }; // Low quality
		case ShadowQuality::MEDIUM:
			return { 1024, 3, 0.6f, 8, 0.002f }; // Medium quality
		case ShadowQuality::HIGH:
			return { 2048, 4, 0.8f, 16, 0.001f }; // High quality
		case ShadowQuality::ULTRA:
			return { 4096, 4, 1.0f, 32, 0.0005f }; // Ultra quality
		default:
			return { 1024, 4, 0.8f, 8, 0.002f };
		}
	}

	explicit ShadowRenderer(glm::ivec2* windowSize = nullptr);

	void Initialize(Camera* camera, const ShadowQuality& settings);
	void SetupCascadeDepthMaps();
	void Cleanup();
	std::vector<float> CalculateCascadeSplits(int numCascades, float lambda);
	void RenderShadowMaps(const glm::ivec2& windowSize, const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities);

	glm::mat4 GetLightSpaceMatrix(Camera camera, glm::ivec2 ws, glm::vec3 lightDir, float nearPlane, float farPlane);
	void RecalculateShadowSettings();
	void UpdateShadowSettings(ShadowQuality quality);

	const std::vector<glm::mat4>& GetCascadeLightSpaceMatrices() const;
	GLuint GetLightDepthMaps() const;

	std::vector<float>& GetShadowCascadeLevels() { return m_ShadowCascadeLevels; }
	const std::vector<float>& GetShadowCascadeLevels() const { return m_ShadowCascadeLevels; }

	ShadowSettings& GetShadowSettings() { return m_ShadowSettings; }
	const ShadowSettings& GetShadowSettings() const { return m_ShadowSettings; }

	const std::vector<PointLightShadowData>& GetPointLightShadows() const { return m_PointLightShadows; }

	static constexpr unsigned int POINT_SHADOW_WIDTH = 1024, POINT_SHADOW_HEIGHT = 1024;

private:
	std::vector<float> m_ShadowCascadeLevels;
	unsigned int m_LightDepthMaps = 0;
	unsigned int m_MatricesUBO = 0;
	unsigned int m_LightFBO = 0;

	std::vector<PointLightShadowData> m_PointLightShadows;
	ShadowSettings m_ShadowSettings{};

	glm::ivec2* m_WindowSize = nullptr;
	Camera* m_CurrentCamera = nullptr;

	std::vector<glm::mat4> m_CascadeLightSpaceMatrices;

	Shader m_DirectionalShadowShader;
	Shader m_PointShadowShader;
	
	void SetupPointLightDepthMaps(int numPointLights);
	std::vector<glm::mat4> CalculateCascadeLightMatrices(glm::vec3 lightDir, const glm::ivec2& windowSize);
	glm::mat4 CalculateLightSpaceMatrix(const Camera& camera, glm::vec3 lightDir, float nearPlane, float farPlane);
	std::vector<glm::mat4> CalculatePointLightMatrices(glm::vec3 lightPos, float nearPlane, float farPlane);
};
