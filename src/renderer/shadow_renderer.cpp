#include "shadow_renderer.h"
#include <glm/gtx/string_cast.hpp>
#include <print>

ShadowRenderer::ShadowRenderer(glm::ivec2* windowSize)
	: m_WindowSize(windowSize), m_CurrentCamera(nullptr), m_LightFBO(0), m_LightDepthMaps(0), m_MatricesUBO(0), m_ShadowSettings{} {}

void ShadowRenderer::Initialize(Camera* camera, const ShadowQuality& settings) {
	m_ShadowSettings = ShadowRenderer::GetShadowSettings(settings);

	m_DirectionalShadowShader = Shader("shaders/DirectionalShadowsDepth.shader");
	unsigned int shadowUBO = glGetUniformBlockIndex(m_DirectionalShadowShader.RendererID(), "LightSpaceMatrices");
	if (shadowUBO != GL_INVALID_INDEX) {
		glUniformBlockBinding(m_DirectionalShadowShader.RendererID(), shadowUBO, 0);
	}
	m_PointShadowShader = Shader("shaders/PointShadowsDepth.shader");
	m_CurrentCamera = camera;
	SetupPointLightDepthMaps(2);

	RecalculateShadowSettings();
}

void ShadowRenderer::UpdateShadowSettings(ShadowQuality quality) {
	m_ShadowSettings = GetShadowSettings(quality);
	RecalculateShadowSettings();
}

void ShadowRenderer::RecalculateShadowSettings() {
	if (m_CurrentCamera == nullptr) return;
	m_ShadowCascadeLevels = CalculateCascadeSplits(
		m_ShadowSettings.numCascades,
		m_ShadowSettings.cascadeSplitLambda
	);

	SetupCascadeDepthMaps();
}

void ShadowRenderer::SetupCascadeDepthMaps() {
	if (m_LightFBO != 0) {
		glDeleteFramebuffers(1, &m_LightFBO);
		glDeleteTextures(1, &m_LightDepthMaps);
		glDeleteBuffers(1, &m_MatricesUBO);
	}

	glGenFramebuffers(1, &m_LightFBO);

	glGenTextures(1, &m_LightDepthMaps);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_LightDepthMaps);
	glTexImage3D(
		GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, m_ShadowSettings.resolution, m_ShadowSettings.resolution, int(m_ShadowCascadeLevels.size()) + 1,
		0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	constexpr float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, bordercolor);

	glBindFramebuffer(GL_FRAMEBUFFER, m_LightFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_LightDepthMaps, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		std::println(stderr, "ERROR::FRAMEBUFFER:: Framebuffer is not complete! Status: 0x{:x}", status);
		std::println(stderr, "GL Version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
		std::println(stderr, "GL Renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
		std::println(stderr, "GL Vendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
		std::println(stderr, "Resolution: {}, Layers: {}", m_ShadowSettings.resolution, m_ShadowCascadeLevels.size() + 1);
		GLenum err;
		while ((err = glGetError()) != GL_NO_ERROR) {
			std::println(stderr, "GL Error: 0x{:x}", err);
		}
		throw 0;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenBuffers(1, &m_MatricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, m_MatricesUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * 16, nullptr, GL_STATIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_MatricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void ShadowRenderer::SetupPointLightDepthMaps(int numPointLights) {
	for (int i = 0; i < numPointLights; ++i) {
		PointLightShadowData shadowData;
		glGenTextures(1, &shadowData.depthCubemap);
		glBindTexture(GL_TEXTURE_CUBE_MAP, shadowData.depthCubemap);
		for (unsigned int face = 0; face < 6; ++face) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT,
				POINT_SHADOW_HEIGHT, POINT_SHADOW_HEIGHT, 0,
				GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		glGenFramebuffers(1, &shadowData.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, shadowData.fbo);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowData.depthCubemap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		m_PointLightShadows.push_back(shadowData);
	}
}

void ShadowRenderer::RenderShadowMaps(const glm::ivec2& windowSize, const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities) {
	for (const Light* light : lights) {
		if (light->GetType() == DIRECTIONAL) {
			glm::vec3 lightDir = light->GetDirection();
			m_CascadeLightSpaceMatrices = CalculateCascadeLightMatrices(-lightDir, windowSize);

			glBindBuffer(GL_UNIFORM_BUFFER, m_MatricesUBO);
			for (size_t i = 0; i < m_CascadeLightSpaceMatrices.size(); ++i)
			{
				glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &m_CascadeLightSpaceMatrices[i]);
			}
			glBindBuffer(GL_UNIFORM_BUFFER, 0);

			m_DirectionalShadowShader.Bind();
			m_DirectionalShadowShader.SetUniform1f("alphaThreshold", 0.5f);
			glBindFramebuffer(GL_FRAMEBUFFER, m_LightFBO);
			glViewport(0, 0, m_ShadowSettings.resolution, m_ShadowSettings.resolution);
			glClear(GL_DEPTH_BUFFER_BIT);
			glCullFace(GL_FRONT); // Front-face culling to mitigate shadow acne
			glEnable(GL_DEPTH_CLAMP);
			for (Entity* entity : entities) {
				if (entity && entity->CastsShadow()) {
					entity->Draw(m_DirectionalShadowShader);
				}
			}
			glCullFace(GL_BACK);
			glDisable(GL_DEPTH_CLAMP);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, POINT_SHADOW_WIDTH, POINT_SHADOW_HEIGHT);
	m_PointShadowShader.Bind();
	int pointLightIndex = 0;
	for (const Light* light : lights) {
		if (light->GetType() == POINT && pointLightIndex < m_PointLightShadows.size()) {
			const auto& shadowData = m_PointLightShadows[pointLightIndex];
			glBindFramebuffer(GL_FRAMEBUFFER, shadowData.fbo);
			glClear(GL_DEPTH_BUFFER_BIT);

			glm::vec3 lightPos = light->GetPosition();
			m_PointShadowShader.SetUniform3f("lightPos", lightPos);

			float nearPlane = 0.1f;
			float farPlane = light->GetRange();
			m_PointShadowShader.SetUniform1f("farPlane", farPlane);

			glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, nearPlane, farPlane);

			// Shadow transforms for all six faces
			std::vector<glm::mat4> shadowTransforms = {
				shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),
				shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
				shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
				shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)),
				shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),
				shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0))
			};

			for (unsigned int i = 0; i < shadowTransforms.size(); ++i) {
				m_PointShadowShader.SetUniformMat4f("shadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
			}

			for (Entity* entity : entities) {
				if (entity && entity->CastsShadow()) {
					entity->Draw(m_PointShadowShader);
				}
			}

			++pointLightIndex;
		}
	}
	m_PointShadowShader.Unbind();

	glViewport(0, 0, windowSize.x, windowSize.y);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glCullFace(GL_BACK);
}

std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& projview) {
	const auto inv = glm::inverse(projview);
	std::vector<glm::vec4> frustumCorners;

	for (unsigned int x = 0; x < 2; ++x) {
		for (unsigned int y = 0; y < 2; ++y) {
			for (unsigned int z = 0; z < 2; ++z) {
				const glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
				frustumCorners.push_back(pt / pt.w);
			}
		}
	}

	return frustumCorners;
}

std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view)
{
	return getFrustumCornersWorldSpace(proj * view);
}

glm::mat4 ShadowRenderer::GetLightSpaceMatrix(Camera camera, glm::ivec2 ws, glm::vec3 lightDir, float nearPlane, float farPlane) {
	// Perspective projection for the camera's view frustum slice
	const auto proj = glm::perspective(
		glm::radians(camera.Zoom),
		static_cast<float>(ws.x) / static_cast<float>(ws.y),
		nearPlane,
		farPlane
	);

	auto corners = getFrustumCornersWorldSpace(proj * camera.GetViewMatrix());

	// Compute the frustum's center in world space
	glm::vec3 center(0.0f);
	for (const auto& v : corners) {
		center += glm::vec3(v);
	}
	center /= static_cast<float>(corners.size());

	// Compute the radius of the bounding sphere
	float radius = 0.0f;
	for (const auto& v : corners) {
		radius = glm::max(radius, glm::length(glm::vec3(v) - center));
	}

	// Align radius to texel boundaries to prevent edge shimmering
	radius = std::ceil(radius * 16.0f) / 16.0f;

	// Orthographic projection bounds
	glm::vec3 min = glm::vec3(-radius);
	glm::vec3 max = glm::vec3(radius);

	glm::mat4 lightProjection = glm::ortho(min.x, max.x, min.y, max.y, -radius, radius);
	glm::mat4 lightView = glm::lookAt(center + lightDir, center, glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;

	// Texel snapping: round shadow origin to eliminate sub-pixel crawling
	glm::vec4 shadowOrigin = lightSpaceMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	shadowOrigin *= m_ShadowSettings.resolution / 2.0f;

	glm::vec4 roundedOrigin = glm::round(shadowOrigin);
	glm::vec4 roundOffset = (roundedOrigin - shadowOrigin) * (2.0f / m_ShadowSettings.resolution);

	lightSpaceMatrix[3][0] += roundOffset.x;
	lightSpaceMatrix[3][1] += roundOffset.y;

	return lightSpaceMatrix;
}

std::vector<glm::mat4> ShadowRenderer::CalculateCascadeLightMatrices(glm::vec3 lightDir, const glm::ivec2& windowSize) {
	std::vector<glm::mat4> matrices;

	for (size_t i = 0; i < m_ShadowCascadeLevels.size() + 1; ++i) {
		float nearPlane = (i == 0) ? m_CurrentCamera->nearPlane : m_ShadowCascadeLevels[i - 1];
		float farPlane = (i < m_ShadowCascadeLevels.size()) ? m_ShadowCascadeLevels[i] : m_CurrentCamera->farPlane;

		glm::mat4 lightMatrix = GetLightSpaceMatrix(*m_CurrentCamera, windowSize, lightDir, nearPlane, farPlane);
		matrices.push_back(lightMatrix);
	}

	return matrices;
}

void ShadowRenderer::Cleanup() {
	glDeleteFramebuffers(1, &m_LightFBO);
	glDeleteTextures(1, &m_LightDepthMaps);
	glDeleteBuffers(1, &m_MatricesUBO);
	for (auto& shadowData : m_PointLightShadows) {
		glDeleteFramebuffers(1, &shadowData.fbo);
		glDeleteTextures(1, &shadowData.depthCubemap);
	}
}

std::vector<float> ShadowRenderer::CalculateCascadeSplits(int numCascades, float lambda) {
	std::vector<float> cascadeSplits(numCascades);

	float nearPlane = m_CurrentCamera->nearPlane;
	float farPlane = m_CurrentCamera->farPlane;

	float p = farPlane * nearPlane;

	if (numCascades == 3) {
		cascadeSplits[0] = farPlane / (p / lambda);
		cascadeSplits[1] = cascadeSplits[0] * (p * lambda / 10.0f);
		cascadeSplits[2] = farPlane * (lambda * lambda);
	}
	if (numCascades == 4) {
		cascadeSplits[0] = farPlane / (p / lambda);
		cascadeSplits[3] = farPlane * lambda;
		
		cascadeSplits[1] = cascadeSplits[0] * (p * lambda / 10.0f);
		cascadeSplits[2] = (cascadeSplits[3] - cascadeSplits[1]) / (4 * lambda);
	}

	return cascadeSplits;
}

const std::vector<glm::mat4>& ShadowRenderer::GetCascadeLightSpaceMatrices() const {
	return m_CascadeLightSpaceMatrices;
}

GLuint ShadowRenderer::GetLightDepthMaps() const {
	return m_LightDepthMaps;
}
