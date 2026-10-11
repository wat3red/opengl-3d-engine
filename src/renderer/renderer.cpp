#include "renderer.h"
#include "scene.h"
#include "light.h"
#include "primitive.h"
#include "skybox.h"
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

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_CLAMP);
}

void Renderer::RenderScene(const glm::ivec2& windowSize, const Camera& camera, Scene& scene, unsigned int targetFBO) {
	m_CurrentCamera = camera;

	m_ShadowRenderer.RenderShadowMaps(windowSize, scene);

	auto& registry = scene.GetRegistry();

	// Frustum Culling
	auto meshView = registry.view<TagComponent, TransformComponent, MeshComponent>();
	m_Stats.totalEntities = meshView.size_hint();
	m_Stats.renderedEntities = 0;
	m_Stats.culledEntities = 0;

	glm::mat4 viewProj = camera.GetProjectionMatrix(windowSize) * camera.GetViewMatrix();
	Frustum cameraFrustum = Frustum::FromViewProjection(viewProj);

	struct RenderItem {
		MeshComponent* mesh;
		glm::mat4 transform;
		const MaterialProperties* materialProps = nullptr;
	};
	std::vector<RenderItem> visibleItems;
	visibleItems.reserve(meshView.size_hint());

	for (auto [entityHandle, tag, tc, mesh] : meshView.each()) {
		if (!tag.Active || !mesh.ModelAsset) continue;

		glm::mat4 modelMat = tc.GetMatrix();
		const MaterialProperties* matProps = nullptr;
		if (auto* matComp = registry.try_get<MaterialComponent>(entityHandle)) {
			matProps = &matComp->Properties;
		}

		if (m_FrustumCullingEnabled) {
			glm::vec3 bmin, bmax;
			mesh.GetWorldAABB(modelMat, bmin, bmax);
			if (cameraFrustum.IsAABBVisible(bmin, bmax)) {
				visibleItems.push_back({ &mesh, modelMat, matProps });
				m_Stats.renderedEntities++;
			} else {
				m_Stats.culledEntities++;
			}
		} else {
			visibleItems.push_back({ &mesh, modelMat, matProps });
			m_Stats.renderedEntities++;
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);
	glViewport(0, 0, windowSize.x, windowSize.y);
	glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	m_MainShader.Bind();

	m_MainShader.SetUniformMat4f("projection", camera.GetProjectionMatrix(windowSize));
	m_MainShader.SetUniformMat4f("view", camera.GetViewMatrix());

	m_MainShader.SetUniform3f("viewPos", camera.Position);

	m_MainShader.SetUniform1f("alphaThreshold", m_AlphaThreshold);
	m_MainShader.SetUniform1f("shadowBias", m_ShadowRenderer.GetShadowSettings().shadowBias);

	m_MainShader.SetUniform1i("debugCascades", m_DebugCascades);

	// Atmospheric Fog Uniforms
	m_MainShader.SetUniform1i("fogEnabled", m_FogSettings.enabled ? 1 : 0);
	m_MainShader.SetUniform3f("fogColor", m_FogSettings.color);
	m_MainShader.SetUniform1f("fogDensity", m_FogSettings.density);
	m_MainShader.SetUniform1f("fogStart", m_FogSettings.start);
	m_MainShader.SetUniform1f("fogEnd", m_FogSettings.end);
	m_MainShader.SetUniform1f("fogHeightFalloff", m_FogSettings.heightFalloff);

	const auto& cascadeLevels = m_ShadowRenderer.GetShadowCascadeLevels();
	m_MainShader.SetUniform1i("cascadeCount", static_cast<int>(cascadeLevels.size()));

	for (size_t i = 0; i < cascadeLevels.size(); ++i)
	{
		m_MainShader.SetUniform1f("cascadePlaneDistances[" + std::to_string(i) + "]", cascadeLevels[i]);
	}

	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_ShadowRenderer.GetLightDepthMaps());
	m_MainShader.SetUniform1i("shadowMap", 10);

	bool hasDirLight = false;
	int pointLightCount = 0;
	int spotLightCount = 0;

	auto lightView = registry.view<TagComponent, LightComponent, TransformComponent>();
	for (auto [entityHandle, tag, light, tc] : lightView.each()) {
		if (!tag.Active) continue;

		switch (light.Type) {
		case LightType::Directional: {
			if (!hasDirLight) {
				glm::vec3 dir = light.Direction;
				if (glm::length(dir) < 0.001f) dir = glm::vec3(0.0f, -1.0f, 0.0f);
				else dir = glm::normalize(dir);

				m_MainShader.SetUniform3f("dirLight.direction", dir);
				m_MainShader.SetUniform3f("dirLight.color", light.Color);
				m_MainShader.SetUniform1f("dirLight.intensity", light.Intensity);
				hasDirLight = true;
			}
			break;
		}
		case LightType::Point: {
			if (pointLightCount < 4) {
				std::string uniformName = std::format("pointLights[{}]", pointLightCount);
				m_MainShader.SetUniform3f(uniformName + ".position", tc.Position);
				m_MainShader.SetUniform3f(uniformName + ".color", light.Color);
				m_MainShader.SetUniform1f(uniformName + ".intensity", light.Intensity);
				m_MainShader.SetUniform1f(uniformName + ".constant", light.Constant);
				m_MainShader.SetUniform1f(uniformName + ".linear", light.Linear);
				m_MainShader.SetUniform1f(uniformName + ".quadratic", light.Quadratic);
				m_MainShader.SetUniform1i(uniformName + ".shadowMap", 6 + pointLightCount);
				m_MainShader.SetUniform1f(uniformName + ".farPlane", light.Range);
				++pointLightCount;
			}
			break;
		}
		case LightType::Spotlight: {
			if (spotLightCount < 4) {
				glm::vec3 dir = light.Direction;
				if (glm::length(dir) < 0.001f) dir = glm::vec3(0.0f, -1.0f, 0.0f);
				else dir = glm::normalize(dir);

				std::string uniformName = std::format("spotLights[{}]", spotLightCount);
				m_MainShader.SetUniform3f(uniformName + ".position", tc.Position);
				m_MainShader.SetUniform3f(uniformName + ".direction", dir);
				m_MainShader.SetUniform3f(uniformName + ".color", light.Color);
				m_MainShader.SetUniform1f(uniformName + ".intensity", light.Intensity);
				m_MainShader.SetUniform1f(uniformName + ".range", light.Range);
				m_MainShader.SetUniform1f(uniformName + ".constant", light.Constant);
				m_MainShader.SetUniform1f(uniformName + ".linear", light.Linear);
				m_MainShader.SetUniform1f(uniformName + ".quadratic", light.Quadratic);
				m_MainShader.SetUniform1f(uniformName + ".cutOff", glm::cos(glm::radians(light.InnerCutoff)));
				m_MainShader.SetUniform1f(uniformName + ".outerCutOff", glm::cos(glm::radians(light.OuterCutoff)));
				++spotLightCount;
			}
			break;
		}
		}
	}

	if (!hasDirLight) {
		m_MainShader.SetUniform1f("dirLight.intensity", 0.0f);
	}

	for (int i = pointLightCount; i < 4; ++i) {
		m_MainShader.SetUniform1f(std::format("pointLights[{}].intensity", i), 0.0f);
	}

	for (int i = spotLightCount; i < 4; ++i) {
		m_MainShader.SetUniform1f(std::format("spotLights[{}].intensity", i), 0.0f);
	}

	const auto& pointShadows = m_ShadowRenderer.GetPointLightShadows();
	for (size_t i = 0; i < pointShadows.size(); ++i) {
		glActiveTexture(GL_TEXTURE6 + static_cast<GLenum>(i));
		glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadows[i].depthCubemap);
		std::string uniformName = std::format("pointLights[{}].shadowMap", i);
		m_MainShader.SetUniform1i(uniformName, static_cast<int>(6 + i));
	}

	const auto& spotShadows = m_ShadowRenderer.GetSpotLightShadows();
	for (size_t i = 0; i < spotShadows.size(); ++i) {
		glActiveTexture(GL_TEXTURE11 + static_cast<GLenum>(i));
		glBindTexture(GL_TEXTURE_2D, spotShadows[i].depthMap);
		std::string uniformName = std::format("spotLights[{}]", i);
		m_MainShader.SetUniform1i(uniformName + ".shadowMap", static_cast<int>(11 + i));
		m_MainShader.SetUniformMat4f(uniformName + ".lightSpaceMatrix", spotShadows[i].lightSpaceMatrix);
		m_MainShader.SetUniform1i(uniformName + ".hasShadow", (i < spotLightCount) ? 1 : 0);
	}

	for (const auto& item : visibleItems) {
		item.mesh->Draw(m_MainShader, item.transform, item.materialProps);
	}

	m_MainShader.Unbind();

	if (scene.GetSkybox() && scene.GetSkybox()->IsActive()) {
		scene.GetSkybox()->SetFog(m_FogSettings.enabled, m_FogSettings.color);
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