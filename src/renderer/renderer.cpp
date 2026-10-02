//renderer.cpp
#include "renderer.h"
#include "scene.h"
#include "light.h"
#include "primitive.h"
#include "skybox.h"
#include <iostream>
#include <format>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <vector>

Renderer::Renderer()
	: m_WindowSize(nullptr), m_ShadowRenderer(nullptr), m_DebugCascades(false), m_AlphaThreshold(0.5f)
{}

Renderer::Renderer(glm::ivec2* windowSize)
	: m_WindowSize(windowSize), m_ShadowRenderer(windowSize), m_DebugCascades(false), m_AlphaThreshold(0.5f)
{}

void Renderer::Initialize() {
	m_ShadowRenderer.Initialize(&m_CurrentCamera, ShadowRenderer::ShadowQuality::ULTRA);

	m_MainShader = Shader("shaders/Main.shader");
	unsigned int mainUBO = glGetUniformBlockIndex(m_MainShader.RendererID(), "LightSpaceMatrices");
	if (mainUBO != GL_INVALID_INDEX) {
		glUniformBlockBinding(m_MainShader.RendererID(), mainUBO, 0);
	}
	m_LightShader = Shader("shaders/Light.shader");

	//m_PointLightShadowShader = Shader("shaders/PointShadowsDepth.shader");
	//m_DirectionalSpotShadowShader = Shader("shaders/DirectionalSpotShadowsDepth.shader");

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_CLAMP);
}

void Renderer::RenderScene(const glm::ivec2& windowSize, const Camera& camera, const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities, unsigned int targetFBO) {
	m_CurrentCamera = camera;

	m_ShadowRenderer.RenderShadowMaps(windowSize, lights, entities);

	glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
	glViewport(0, 0, windowSize.x, windowSize.y);
	glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_MainShader.Bind();

	m_MainShader.SetUniformMat4f("projection", camera.GetProjectionMatrix(windowSize));
	m_MainShader.SetUniformMat4f("view", camera.GetViewMatrix());

	m_MainShader.SetUniform3f("viewPos", camera.Position);

	m_MainShader.SetUniform1f("alphaThreshold", m_AlphaThreshold);
	//std::cout << m_ShadowSettings.shadowBias << "\n";
	m_MainShader.SetUniform1f("shadowBias", m_ShadowRenderer.GetShadowSettings().shadowBias);

	m_MainShader.SetUniform1i("debugCascades", m_DebugCascades);

	const auto& cascadeLevels = m_ShadowRenderer.GetShadowCascadeLevels();
	m_MainShader.SetUniform1i("cascadeCount", static_cast<int>(cascadeLevels.size()));

	for (size_t i = 0; i < cascadeLevels.size(); ++i)
	{
		m_MainShader.SetUniform1f("cascadePlaneDistances[" + std::to_string(i) + "]", cascadeLevels[i]);
	}

	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_ShadowRenderer.GetLightDepthMaps());
	m_MainShader.SetUniform1i("shadowMap", 10);

	// Set material properties
	m_MainShader.SetUniform1f("material.shininess", 32.0f);

	int pointLightCount = 0;
	for (const Light* light : lights) {
		switch (light->GetType()) {
		case DIRECTIONAL: {
			m_MainShader.SetUniform3f("dirLight.direction", light->GetDirection());
			m_MainShader.SetUniform3f("dirLight.ambient", light->GetColor() * light->GetIntensity() * 0.05f);
			m_MainShader.SetUniform3f("dirLight.diffuse", light->GetColor() * light->GetIntensity() * 0.4f);
			m_MainShader.SetUniform3f("dirLight.specular", light->GetColor() * light->GetIntensity() * 0.5f);
			break;
		}
		case POINT: {
			std::string uniformName = std::format("pointLights[{}]", pointLightCount);
			m_MainShader.SetUniform3f(uniformName + ".position", light->GetPosition());
			m_MainShader.SetUniform3f(uniformName + ".ambient", light->GetColor() * light->GetIntensity() * 0.05f);
			m_MainShader.SetUniform3f(uniformName + ".diffuse", light->GetColor() * light->GetIntensity() * 0.8f);
			m_MainShader.SetUniform3f(uniformName + ".specular", light->GetColor() * light->GetIntensity() * 1.0f);
			m_MainShader.SetUniform1f(uniformName + ".constant", 1.0f);
			m_MainShader.SetUniform1f(uniformName + ".linear", 0.09f);
			m_MainShader.SetUniform1f(uniformName + ".quadratic", 0.032f);
			m_MainShader.SetUniform1i(uniformName + ".shadowMap", 6);
			m_MainShader.SetUniform1f(uniformName + ".farPlane", 25.0f);
			++pointLightCount;
			break;
		}

		case SPOTLIGHT: {
			m_MainShader.SetUniform3f("spotLight.position", light->GetPosition());
			m_MainShader.SetUniform3f("spotLight.direction", light->GetDirection());
			m_MainShader.SetUniform3f("spotLight.ambient", light->GetColor() * light->GetIntensity() * 0.05f);
			m_MainShader.SetUniform3f("spotLight.diffuse", light->GetColor() * light->GetIntensity() * 0.8f);
			m_MainShader.SetUniform3f("spotLight.specular", light->GetColor() * light->GetIntensity() * 1.0f);
			m_MainShader.SetUniform1f("spotLight.constant", 1.0f);
			m_MainShader.SetUniform1f("spotLight.linear", 0.09f);
			m_MainShader.SetUniform1f("spotLight.quadratic", 0.032f);
			m_MainShader.SetUniform1f("spotLight.cutOff", glm::cos(glm::radians(light->GetInnerCutoff())));
			m_MainShader.SetUniform1f("spotLight.outerCutOff", glm::cos(glm::radians(light->GetOuterCutoff())));
			break;
		}
		}
	}

	const auto& pointShadows = m_ShadowRenderer.GetPointLightShadows();
	for (size_t i = 0; i < pointShadows.size(); ++i) {
		glActiveTexture(GL_TEXTURE6 + static_cast<GLenum>(i));
		glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadows[i].depthCubemap);
		std::string uniformName = std::format("pointLights[{}].shadowMap", i);
		m_MainShader.SetUniform1i(uniformName, static_cast<int>(6 + i));
	}

	// Render all entities in the scene
	for (Entity* entity : entities) {
		entity->Draw(m_MainShader);
	}

	m_MainShader.Unbind();
}

void Renderer::RenderScene(const glm::ivec2& windowSize, const Camera& camera, Scene& scene, unsigned int targetFBO) {
	std::vector<Light*> rawLights;
	rawLights.reserve(scene.GetLights().size());
	for (const auto& light : scene.GetLights()) {
		if (light && light->IsActive()) rawLights.push_back(light.get());
	}

	std::vector<ModelEntity*> rawEntities;
	rawEntities.reserve(scene.GetEntities().size());
	for (const auto& entity : scene.GetEntities()) {
		if (entity && entity->IsActive()) rawEntities.push_back(entity.get());
	}

	RenderScene(windowSize, camera, rawLights, rawEntities, targetFBO);

	if (scene.GetSkybox() && scene.GetSkybox()->IsActive()) {
		glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
		glViewport(0, 0, windowSize.x, windowSize.y);
		scene.GetSkybox()->Draw(camera, windowSize);
	}
}

void Renderer::Cleanup() {
}

void Renderer::RenderCube() {
	Primitive::GetUnitCube()->Draw(m_LightShader);
}