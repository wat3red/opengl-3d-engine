//#include "shadow_renderer.h"
//#include <glm/gtx/string_cast.hpp>
//#include <iostream>
//
//ShadowRenderer::ShadowRenderer(glm::ivec2* windowSize, const ShadowSettings& settings)
//	: m_WindowSize(windowSize), m_ShadowSettings(settings), lightFBO(0), lightDepthMaps(0), matricesUBO(0) {}
//
//void ShadowRenderer::Initialize() {
//	// Initialize shaders
//	m_DirectionalShadowShader = Shader("path/to/directional_shadow_shader.glsl");
//	m_PointShadowShader = Shader("path/to/point_shadow_shader.glsl");
//
//	// Setup cascade and point light depth maps
//	SetupCascadeDepthMaps();
//	SetupPointLightDepthMaps(2);
//}
//
//void ShadowRenderer::SetupCascadeDepthMaps() {
//	glGenFramebuffers(1, &lightFBO);
//	glGenTextures(1, &lightDepthMaps);
//	glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMaps);
//	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, m_ShadowSettings.resolution,
//		m_ShadowSettings.resolution, m_ShadowSettings.numCascades + 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
//	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//	float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
//	glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, bordercolor);
//	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
//	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
//
//	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
//	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, lightDepthMaps, 0);
//	glDrawBuffer(GL_NONE);
//	glReadBuffer(GL_NONE);
//
//	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
//		std::cerr << "ERROR::FRAMEBUFFER:: Cascade shadow framebuffer is not complete!" << std::endl;
//		throw std::runtime_error("Framebuffer not complete.");
//	}
//
//	glBindFramebuffer(GL_FRAMEBUFFER, 0);
//
//	glGenBuffers(1, &matricesUBO);
//	glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
//	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4) * 16, nullptr, GL_STATIC_DRAW);
//	glBindBufferBase(GL_UNIFORM_BUFFER, 0, matricesUBO);
//	glBindBuffer(GL_UNIFORM_BUFFER, 0);
//}
//
//void ShadowRenderer::SetupPointLightDepthMaps(int numPointLights) {
//	for (int i = 0; i < numPointLights; ++i) {
//		PointLightShadowData shadowData;
//		glGenTextures(1, &shadowData.depthCubemap);
//		glBindTexture(GL_TEXTURE_CUBE_MAP, shadowData.depthCubemap);
//		for (unsigned int face = 0; face < 6; ++face) {
//			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT,
//				m_ShadowSettings.pointShadowWidth, m_ShadowSettings.pointShadowHeight, 0,
//				GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
//		}
//		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
//
//		glGenFramebuffers(1, &shadowData.fbo);
//		glBindFramebuffer(GL_FRAMEBUFFER, shadowData.fbo);
//		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowData.depthCubemap, 0);
//		glDrawBuffer(GL_NONE);
//		glReadBuffer(GL_NONE);
//		glBindFramebuffer(GL_FRAMEBUFFER, 0);
//
//		m_PointLightShadows.push_back(shadowData);
//	}
//}
//
//void ShadowRenderer::RenderDirectionalShadows(const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities) {
//	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
//	glViewport(0, 0, m_ShadowSettings.resolution, m_ShadowSettings.resolution);
//	glClear(GL_DEPTH_BUFFER_BIT);
//
//	m_DirectionalShadowShader.Bind();
//
//	for (const auto& light : lights) {
//		if (light->GetType() == DIRECTIONAL) {
//			glm::vec3 lightDir = light->GetDirection();
//			cascadeLightSpaceMatrices = CalculateCascadeLightMatrices(lightDir);
//
//			glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
//			for (size_t i = 0; i < cascadeLightSpaceMatrices.size(); ++i) {
//				glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4), sizeof(glm::mat4), &cascadeLightSpaceMatrices[i]);
//			}
//			glBindBuffer(GL_UNIFORM_BUFFER, 0);
//
//			for (auto& entity : entities) {
//				entity->Draw(m_DirectionalShadowShader);
//			}
//		}
//	}
//
//	glBindFramebuffer(GL_FRAMEBUFFER, 0);
//}
//
//void ShadowRenderer::Cleanup() {
//	glDeleteFramebuffers(1, &lightFBO);
//	glDeleteTextures(1, &lightDepthMaps);
//	glDeleteBuffers(1, &matricesUBO);
//	for (auto& shadowData : m_PointLightShadows) {
//		glDeleteFramebuffers(1, &shadowData.fbo);
//		glDeleteTextures(1, &shadowData.depthCubemap);
//	}
//}
//
//const std::vector<glm::mat4>& ShadowRenderer::GetCascadeLightSpaceMatrices() const {
//	return cascadeLightSpaceMatrices;
//}
//
//GLuint ShadowRenderer::GetLightDepthMaps() const {
//	return lightDepthMaps;
//}
