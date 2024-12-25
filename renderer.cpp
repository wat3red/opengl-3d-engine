//renderer.cpp
#include "renderer.h"
#include "light.h"
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

glm::mat4 lightProjection, lightView, lightSpaceMatrix;
Renderer::Renderer(glm::ivec2 windowSize)
	: cubeVAO(0), cubeVBO(0) {}

void Renderer::Initialize() {
	m_MainShader = Shader("D:/Desktop/LearningOpenGL/shaders/Main.shader");
	m_LightShader = Shader("D:/Desktop/LearningOpenGL/shaders/Light.shader");

	m_PointLightShadowShader = Shader("D:/Desktop/LearningOpenGL/shaders/PointShadowsDepth.shader");
	m_DirectionalSpotShadowShader = Shader("D:/Desktop/LearningOpenGL/shaders/DirectionalSpotShadowsDepth.shader");

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	SetupCube();
	SetupDepthMaps(2);
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
	// 2D depth map for directional/spotlight shadows
	glGenFramebuffers(1, &m_DirDepthMapFBO);
	glGenTextures(1, &m_DirDepthMap);
	glBindTexture(GL_TEXTURE_2D, m_DirDepthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, DIR_SHADOW_WIDTH, DIR_SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glBindFramebuffer(GL_FRAMEBUFFER, m_DirDepthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DirDepthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Clear any existing shadow data
	for (auto& shadowData : m_PointLightShadows) {
		glDeleteTextures(1, &shadowData.depthCubemap);
		glDeleteFramebuffers(1, &shadowData.fbo);
	}
	m_PointLightShadows.clear();

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

float sceneLeft = -200;   // Replace with your scene's minimum X
float sceneRight = 200;   // Replace with your scene's maximum X
float sceneBottom = -200; // Replace with your scene's minimum Y
float sceneTop = 200;     // Replace with your scene's maximum Y
float nearPlane = 1.f;     // Keep near plane close
float farPlane = 400.0f;    // Adjust to cover the entire scene depth

void Renderer::RenderShadowMaps(const glm::ivec2& windowSize, const std::vector<Light*>& lights, const std::vector<ModelEntity*>& entities) {
	// Directional/Spot Lights Shadow Map
	glCullFace(GL_FRONT); 
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.0f, 2.0f);
	glViewport(0, 0, DIR_SHADOW_WIDTH, DIR_SHADOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, m_DirDepthMapFBO);
	glClear(GL_DEPTH_BUFFER_BIT);

	m_DirectionalSpotShadowShader.Bind();
	for (const Light* light : lights) {
		if (light->GetType() == DIRECTIONAL /*|| light->GetType() == SPOTLIGHT*/) {
			//glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 7.5f);

			glm::mat4 lightProjection = glm::ortho(sceneLeft, sceneRight, sceneBottom, sceneTop, nearPlane, farPlane);
			glm::mat4 lightView = glm::lookAt(-light->GetDirection() * 100.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			glm::mat4 lightSpaceMatrix = lightProjection * lightView;
			m_DirectionalSpotShadowShader.SetUniformMat4f("lightSpaceMatrix", lightSpaceMatrix);

			for (Entity* entity : entities) {
				entity->Draw(m_DirectionalSpotShadowShader);
			}
		}
	}
	glDisable(GL_POLYGON_OFFSET_FILL);
	m_DirectionalSpotShadowShader.Unbind();
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
	RenderShadowMaps(windowSize, lights, entities);

	
	m_MainShader.Bind();


	// Set view and projection matrices

	m_MainShader.SetUniformMat4f("projection", camera.GetProjectionMatrix(windowSize));
	m_MainShader.SetUniformMat4f("view", camera.GetViewMatrix());
	m_MainShader.SetUniform3f("viewPos", camera.Position);

	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, m_DirDepthMap);
	m_MainShader.SetUniform1i("dirSpotShadowMap", 5);

	//glActiveTexture(GL_TEXTURE6);
	//glBindTexture(GL_TEXTURE_CUBE_MAP, m_PointDepthMap);
	//m_MainShader.SetUniform1i("pointShadowMap", 6);
	m_MainShader.SetUniform1f("farPlane", 25.0f);

	// Set material properties
	m_MainShader.SetUniform1f("material.shininess", 32.0f);



	for (const Light* light : lights) {
		if (light->GetType() == DIRECTIONAL /*|| light->GetType() == SPOTLIGHT*/) {

			glm::mat4 lightProjection = glm::ortho(sceneLeft, sceneRight, sceneBottom, sceneTop, nearPlane, farPlane);

			glm::mat4 lightView = glm::lookAt(-light->GetDirection() * 100.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			glm::mat4 lightSpaceMatrix = lightProjection * lightView;
			m_MainShader.SetUniformMat4f("lightSpaceMatrix", lightSpaceMatrix);
			break; // Assume only one directional or spotlight uses this matrix
		}
	}

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

void Renderer::Cleanup() {
	glDeleteVertexArrays(1, &cubeVAO);
	glDeleteBuffers(1, &cubeVBO);
}


void Renderer::RenderCube() {
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}