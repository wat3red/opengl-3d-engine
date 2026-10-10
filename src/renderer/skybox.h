#pragma once

#include "texture_cube.h"
#include "shader.h"
#include "camera.h"
#include "mesh.h"
#include "model.h"
#include <memory>
#include <string>

class Skybox {
public:
	enum class Type {
		Cubemap,
		MeshDome
	};

	Skybox(std::shared_ptr<TextureCube> cubemap, std::shared_ptr<Shader> shader = nullptr);
	Skybox(std::shared_ptr<Model> domeModel, std::shared_ptr<Shader> shader = nullptr);
	Skybox(const std::string& modelPath, std::shared_ptr<Shader> shader = nullptr);
	~Skybox() = default;

	void Draw(const Camera& camera, const glm::ivec2& viewportSize);

	Type GetType() const { return m_Type; }

	void SetCubemap(std::shared_ptr<TextureCube> cubemap) { m_Cubemap = std::move(cubemap); m_Type = Type::Cubemap; }
	std::shared_ptr<TextureCube> GetCubemap() const { return m_Cubemap; }

	void SetDomeModel(std::shared_ptr<Model> model) { m_DomeModel = std::move(model); m_Type = Type::MeshDome; }
	std::shared_ptr<Model> GetDomeModel() const { return m_DomeModel; }

	void SetShader(std::shared_ptr<Shader> shader) { m_Shader = std::move(shader); }
	std::shared_ptr<Shader> GetShader() const { return m_Shader; }

	bool IsActive() const { return m_Active; }
	void SetActive(bool active) { m_Active = active; }

private:
	Type m_Type = Type::Cubemap;
	std::shared_ptr<TextureCube> m_Cubemap;
	std::shared_ptr<Model> m_DomeModel;
	std::shared_ptr<Shader> m_Shader;
	std::shared_ptr<Mesh> m_CubeMesh;
	bool m_Active = true;
};
