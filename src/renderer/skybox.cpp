#include "skybox.h"
#include "primitive.h"
#include "asset_manager.h"
#include <GL/glew.h>

Skybox::Skybox(std::shared_ptr<TextureCube> cubemap, std::shared_ptr<Shader> shader)
	: m_Type(Type::Cubemap), m_Cubemap(std::move(cubemap)), m_Shader(std::move(shader)),
	  m_CubeMesh(Primitive::GetUnitCube()), m_Active(true)
{
}

Skybox::Skybox(std::shared_ptr<Model> domeModel, std::shared_ptr<Shader> shader)
	: m_Type(Type::MeshDome), m_DomeModel(std::move(domeModel)), m_Shader(std::move(shader)),
	  m_Active(true)
{
}

Skybox::Skybox(const std::string& modelPath, std::shared_ptr<Shader> shader)
	: m_Type(Type::MeshDome), m_Shader(std::move(shader)), m_Active(true)
{
	m_DomeModel = AssetManager::Get().LoadModel(modelPath);
}

void Skybox::Draw(const Camera& camera, const glm::ivec2& viewportSize)
{
	if (!m_Active || !m_Shader)
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

	if (m_Type == Type::Cubemap)
	{
		if (m_Cubemap && m_CubeMesh)
		{
			m_Cubemap->Bind(0);
			m_Shader->SetUniform1i("skybox", 0);
			m_CubeMesh->Draw(*m_Shader);
		}
	}
	else if (m_Type == Type::MeshDome)
	{
		if (m_DomeModel)
		{
			m_Shader->SetUniformMat4f("model", glm::mat4(1.0f));
			m_DomeModel->Draw(*m_Shader);
		}
	}

	m_Shader->Unbind();

	if (cullFaceEnabled)
		glEnable(GL_CULL_FACE);

	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
}
