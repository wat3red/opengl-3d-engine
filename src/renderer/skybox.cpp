// skybox.cpp
#include "skybox.h"
#include "primitive.h"
#include <GL/glew.h>

Skybox::Skybox(std::shared_ptr<TextureCube> cubemap, std::shared_ptr<Shader> shader)
	: m_Cubemap(std::move(cubemap)), m_Shader(std::move(shader)), m_CubeMesh(Primitive::GetUnitCube()), m_Active(true)
{
}

void Skybox::Draw(const Camera& camera, const glm::ivec2& viewportSize)
{
	if (!m_Active || !m_Cubemap || !m_Shader || !m_CubeMesh)
		return;

	// Use GL_LEQUAL depth test so fragments at z = 1.0 (far plane) pass behind all scene objects
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);

	GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
	if (cullFaceEnabled)
		glDisable(GL_CULL_FACE);

	m_Shader->Bind();

	// Strip translation from view matrix so the skybox stays infinitely far and centered at the camera
	glm::mat4 view = glm::mat4(glm::mat3(camera.GetViewMatrix()));
	glm::mat4 projection = camera.GetProjectionMatrix(viewportSize);

	m_Shader->SetUniformMat4f("view", view);
	m_Shader->SetUniformMat4f("projection", projection);

	m_Cubemap->Bind(0);
	m_Shader->SetUniform1i("skybox", 0);

	m_CubeMesh->Draw(*m_Shader);

	m_Shader->Unbind();

	if (cullFaceEnabled)
		glEnable(GL_CULL_FACE);

	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
}
