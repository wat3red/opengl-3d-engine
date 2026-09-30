//renderer.cpp
#include "renderer.h"
#include "light.h"
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <vector>

Renderer::Renderer(glm::ivec2* windowSize)
	: cubeVAO(0), cubeVBO(0), m_WindowSize(windowSize), m_ShadowRenderer(windowSize)
{}

void Renderer::Initialize() {
	m_ShadowRenderer.Initialize(&m_CurrentCamera, ShadowRenderer::ShadowQuality::ULTRA);

	m_MainShader = Shader("shaders/Main.shader");
	m_LightShader = Shader("shaders/Light.shader");

	//m_PointLightShadowShader = Shader("shaders/PointShadowsDepth.shader");
	//m_DirectionalSpotShadowShader = Shader("shaders/DirectionalSpotShadowsDepth.shader");

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_CLAMP);

	SetupCube();
	//SetupDepthMaps(2);
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

void Renderer::RenderScene(const glm::ivec2& windowSize, const Camera& camera, const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities) {
	m_CurrentCamera = camera;

	m_ShadowRenderer.RenderShadowMaps(windowSize, lights, entities);

	m_MainShader.Bind();

	m_MainShader.SetUniformMat4f("projection", camera.GetProjectionMatrix(windowSize));
	m_MainShader.SetUniformMat4f("view", camera.GetViewMatrix());

	m_MainShader.SetUniform3f("viewPos", camera.Position);
	m_MainShader.SetUniform1f("farPlane", camera.farPlane);

	m_MainShader.SetUniform1f("alphaThreshold", 0.4);
	//std::cout << m_ShadowSettings.shadowBias << "\n";
	m_MainShader.SetUniform1f("shadowBias", m_ShadowRenderer.m_ShadowSettings.shadowBias);

	m_MainShader.SetUniform1i("debugCascades", debug1);

	m_MainShader.SetUniform1i("cascadeCount", m_ShadowRenderer.shadowCascadeLevels.size());

	for (size_t i = 0; i < m_ShadowRenderer.shadowCascadeLevels.size(); ++i)
	{
		m_MainShader.SetUniform1f("cascadePlaneDistances[" + std::to_string(i) + "]", m_ShadowRenderer.shadowCascadeLevels[i]);
	}

	glActiveTexture(GL_TEXTURE0 + 64);
	glBindTexture(GL_TEXTURE_2D_ARRAY, m_ShadowRenderer.lightDepthMaps);
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

	for (int i = 0; i < m_ShadowRenderer.m_PointLightShadows.size(); ++i) {
		glActiveTexture(GL_TEXTURE6 + i);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_ShadowRenderer.m_PointLightShadows[i].depthCubemap);
		std::string uniformName = "pointLights[" + std::to_string(i) + "].shadowMap";
		m_MainShader.SetUniform1i(uniformName, 6 + i);
	}

	// Render all entities in the scene
	for (Entity* entity : entities) {
		entity->Draw(m_MainShader);
	}

	m_MainShader.Unbind();
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