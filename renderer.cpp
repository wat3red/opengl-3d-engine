//renderer.cpp
#include "renderer.h"
#include "light.h"
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

/// <summary>

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

glm::mat4 Renderer::getLightSpaceMatrix(Camera camera, glm::ivec2 ws, glm::vec3 lightDir, float nearPlane, float farPlane) {
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

	// Align radius to texel boundaries
	radius = std::ceil(radius * 16.0f) / 16.0f;

	// Orthographic projection bounds
	glm::vec3 min = glm::vec3(-radius);
	glm::vec3 max = glm::vec3(radius);

	// Compute the orthographic projection
	glm::mat4 lightProjection = glm::ortho(min.x, max.x, min.y, max.y, -radius, radius);

	// Compute the light's view matrix
	glm::mat4 lightView = glm::lookAt(center + lightDir, center, glm::vec3(0.0f, 1.0f, 0.0f));

	// Combine the matrices
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;

	// Snapping to texel alignment
	glm::vec4 shadowOrigin = lightSpaceMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	shadowOrigin *= m_ShadowSettings.resolution / 2.0f;

	glm::vec4 roundedOrigin = glm::round(shadowOrigin);
	glm::vec4 roundOffset = (roundedOrigin - shadowOrigin) * (2.0f / m_ShadowSettings.resolution);

	lightSpaceMatrix[3][0] += roundOffset.x;
	lightSpaceMatrix[3][1] += roundOffset.y;

	return lightSpaceMatrix;
}

std::vector<glm::mat4> Renderer::GetLightSpaceMatrices(glm::vec3 lightDir) {
	std::vector<glm::mat4> matrices;

	for (size_t i = 0; i < shadowCascadeLevels.size() + 1; ++i) {
		float nearPlane = (i == 0) ? m_CurrentCamera.nearPlane : shadowCascadeLevels[i - 1];
		float farPlane = (i < shadowCascadeLevels.size()) ? shadowCascadeLevels[i] : m_CurrentCamera.farPlane;

		glm::mat4 lightMatrix = getLightSpaceMatrix(m_CurrentCamera, *m_WindowSize, lightDir, nearPlane, farPlane);
		matrices.push_back(lightMatrix);
	}

	return matrices;
}

/// </summary>

Renderer::Renderer(glm::ivec2* windowSize)
	: cubeVAO(0), cubeVBO(0), m_WindowSize(windowSize)
{}

#include <vector>

std::vector<float> calculateCascadeSplits(float nearPlane, float farPlane, int numCascades, float lambda) {
	std::vector<float> cascadeSplits;

	for (int i = 0; i < numCascades; ++i) {
		float value = exp(i + lambda);

		cascadeSplits.push_back(value);
	}

	return cascadeSplits;
}

void Renderer::Initialize() {
	m_ShadowSettings = GetShadowSettings(ShadowQuality::ULTRA);
	RecalculateShadowSettings();

	m_MainShader = Shader("D:/Desktop/LearningOpenGL/shaders/Main.shader");
	m_LightShader = Shader("D:/Desktop/LearningOpenGL/shaders/Light.shader");

	m_PointLightShadowShader = Shader("D:/Desktop/LearningOpenGL/shaders/PointShadowsDepth.shader");
	m_DirectionalSpotShadowShader = Shader("D:/Desktop/LearningOpenGL/shaders/DirectionalSpotShadowsDepth.shader");

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_CLAMP);

	SetupCube();
	SetupDepthMaps(2);
}

void Renderer::UpdateShadowSettings(ShadowQuality quality) {
	m_ShadowSettings = GetShadowSettings(quality);
	RecalculateShadowSettings();
}

void Renderer::RecalculateShadowSettings() {
	shadowCascadeLevels = calculateCascadeSplits(
		m_CurrentCamera.nearPlane,
		m_CurrentCamera.farPlane,
		m_ShadowSettings.numCascades,
		m_ShadowSettings.cascadeSplitLambda
	);

	SetupCascadeDepthMaps();

	/*for (auto& shadowCascadeLevel : shadowCascadeLevels) {
		std::cout << shadowCascadeLevel << "\n";
	}*/
}

void Renderer::SetupCube() {
	float vertices[] = {
		// back face
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
		 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
		-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
		-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
		// front face
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
		-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
		-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
		// left face
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
		-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
		// right face
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
		 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
		 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
		 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
		 // bottom face
		 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		  1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
		  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
		 -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
		 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
		 // top face
		 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		  1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		  1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
		  1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
		 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
		 -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
	};
	glGenVertexArrays(1, &cubeVAO);
	glGenBuffers(1, &cubeVBO);
	glBindVertexArray(cubeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void Renderer::SetupDepthMaps(int numPointLights) {
	// Clear any existing shadow data
	for (int i = 0; i < numPointLights; ++i) {
		PointLightShadowData shadowData;

		// Generate cubemap
		glGenTextures(1, &shadowData.depthCubemap);
		glBindTexture(GL_TEXTURE_CUBE_MAP, shadowData.depthCubemap);
		for (unsigned int face = 0; face < 6; ++face) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT,
				POINT_SHADOW_WIDTH, POINT_SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		// Generate framebuffer
		glGenFramebuffers(1, &shadowData.fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, shadowData.fbo);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowData.depthCubemap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		m_PointLightShadows.push_back(shadowData);
	}
}

void Renderer::RenderShadowMaps(const glm::ivec2& windowSize, const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities) {
	// Directional/Spot Lights Shadow Map
	glViewport(0, 0, m_ShadowSettings.resolution, m_ShadowSettings.resolution);
	glBindFramebuffer(GL_FRAMEBUFFER, m_DirDepthMapFBO);
	glClear(GL_DEPTH_BUFFER_BIT);

	for (const Light* light : lights) {
		if (light->GetType() == DIRECTIONAL ) {
			glm::vec3 lightDir = light->GetDirection();
			cascadeLightSpaceMatrices = GetLightSpaceMatrices(-lightDir);
			//std::cout << cascadeLightSpaceMatrices.size() << "\n";
			//
			glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
			for (size_t i = 0; i < cascadeLightSpaceMatrices.size(); ++i)
			{
				glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &cascadeLightSpaceMatrices[i]);
			}
			glBindBuffer(GL_UNIFORM_BUFFER, 0);
			//
			m_DirectionalSpotShadowShader.Bind();
			glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
			glViewport(0, 0, m_ShadowSettings.resolution, m_ShadowSettings.resolution);
			glClear(GL_DEPTH_BUFFER_BIT);
			glCullFace(GL_FRONT);  // peter panning
			glEnable(GL_DEPTH_CLAMP);
			for (Entity* entity : entities) {
				entity->Draw(m_DirectionalSpotShadowShader);
			}
			glCullFace(GL_BACK);
			glDisable(GL_DEPTH_CLAMP);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// reset viewport
			glViewport(0, 0, windowSize.x, windowSize.y);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	glViewport(0, 0, POINT_SHADOW_WIDTH, POINT_SHADOW_HEIGHT);
	// Point Lights Shadow Map
	m_PointLightShadowShader.Bind();
	int pointLightIndex = 0;
	for (const Light* light : lights) {
		if (light->GetType() == POINT && pointLightIndex < m_PointLightShadows.size()) {
			const auto& shadowData = m_PointLightShadows[pointLightIndex];
			glBindFramebuffer(GL_FRAMEBUFFER, shadowData.fbo);
			glClear(GL_DEPTH_BUFFER_BIT);

			glm::vec3 lightPos = light->GetPosition();
			m_PointLightShadowShader.SetUniform3f("lightPos", lightPos);

			float nearPlane = 0.1f;
			float farPlane = 25.0f;
			m_PointLightShadowShader.SetUniform1f("farPlane", farPlane);

			// Create projection matrix
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
				m_PointLightShadowShader.SetUniformMat4f("shadowMatrices[" + std::to_string(i) + "]", shadowTransforms[i]);
			}

			for (Entity* entity : entities) {
				if (glm::distance(entity->GetPosition(), lightPos) < farPlane) {
					entity->Draw(m_PointLightShadowShader);
				}
			}

			++pointLightIndex;
		}
	}
	m_PointLightShadowShader.Unbind();

	glViewport(0, 0, windowSize.x, windowSize.y);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glCullFace(GL_BACK); // don't forget to reset original culling face
}

void Renderer::RenderScene(const glm::ivec2& windowSize, const Camera& camera, const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities) {
	m_CurrentCamera = camera;

	RenderShadowMaps(windowSize, lights, entities);

	m_MainShader.Bind();

	m_MainShader.SetUniformMat4f("projection", camera.GetProjectionMatrix(windowSize));
	m_MainShader.SetUniformMat4f("view", camera.GetViewMatrix());

	m_MainShader.SetUniform3f("viewPos", camera.Position);
	m_MainShader.SetUniform1f("farPlane", camera.farPlane);

	m_MainShader.SetUniform1f("alphaThreshold", 0.4);

	m_MainShader.SetUniform1f("shadowBias", m_ShadowSettings.shadowBias);

	m_MainShader.SetUniform1i("debugCascades", debug1);

	m_MainShader.SetUniform1i("cascadeCount", shadowCascadeLevels.size());

	for (size_t i = 0; i < shadowCascadeLevels.size(); ++i)
	{
		//std::cout << shadowCascadeLevels[i] << "\n";
		m_MainShader.SetUniform1f("cascadePlaneDistances[" + std::to_string(i) + "]", shadowCascadeLevels[i]);
	}

	glActiveTexture(GL_TEXTURE0 + 64);
	glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMaps);
	m_MainShader.SetUniform1i("shadowMap", 64);

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
			std::string uniformName = "pointLights[" + std::to_string(pointLightCount) + "]";
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

	for (int i = 0; i < m_PointLightShadows.size(); ++i) {
		glActiveTexture(GL_TEXTURE6 + i);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_PointLightShadows[i].depthCubemap);
		std::string uniformName = "pointLights[" + std::to_string(i) + "].shadowMap";
		m_MainShader.SetUniform1i(uniformName, 6 + i);
	}

	// Render all entities in the scene
	for (Entity* entity : entities) {
		entity->Draw(m_MainShader);
	}

	m_MainShader.Unbind();
}

void Renderer::SetupCascadeDepthMaps() {
	// configure light FBO
	// -----------------------
	glGenFramebuffers(1, &lightFBO);

	glGenTextures(1, &lightDepthMaps);
	glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMaps);
	glTexImage3D(
		GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, m_ShadowSettings.resolution, m_ShadowSettings.resolution, int(shadowCascadeLevels.size()) + 1,
		0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	constexpr float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, bordercolor);

	glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, lightDepthMaps, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!";
		throw 0;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// configure UBO
	// --------------------
	glGenBuffers(1, &matricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * 16, nullptr, GL_STATIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, matricesUBO);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void Renderer::Cleanup() {
	glDeleteVertexArrays(1, &cubeVAO);
	glDeleteBuffers(1, &cubeVBO);
}

void Renderer::RenderCube() {
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}