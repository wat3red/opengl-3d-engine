#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "light.h"
#include "camera.h"
#include "shader.h"
#include "entity.h"
#include "renderer.h"

class ShadowRenderer {
public:
	ShadowRenderer(glm::ivec2* windowSize, const ShadowSettings& settings);

	void Initialize();
	void RenderDirectionalShadows(const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities);
	void RenderPointShadows(const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities);
	void SetupCascadeDepthMaps();
	void Cleanup();

	const std::vector<glm::mat4>& GetCascadeLightSpaceMatrices() const;
	GLuint GetLightDepthMaps() const;

private:
	glm::ivec2* m_WindowSize;
	ShadowSettings m_ShadowSettings;

	GLuint lightFBO;
	GLuint lightDepthMaps;
	GLuint matricesUBO;

	std::vector<glm::mat4> cascadeLightSpaceMatrices;

	struct PointLightShadowData {
		GLuint depthCubemap;
		GLuint fbo;
	};
	std::vector<PointLightShadowData> m_PointLightShadows;

	Shader m_DirectionalShadowShader;
	Shader m_PointShadowShader;

	void SetupPointLightDepthMaps(int numPointLights);
	glm::mat4 CalculateLightSpaceMatrix(const Camera& camera, glm::vec3 lightDir, float nearPlane, float farPlane);
	std::vector<glm::mat4> CalculatePointLightMatrices(glm::vec3 lightPos, float nearPlane, float farPlane);
};

