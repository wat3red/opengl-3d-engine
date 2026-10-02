// skybox.h
#pragma once

#include "texture_cube.h"
#include "shader.h"
#include "camera.h"
#include "mesh.h"
#include <memory>

class Skybox {
public:
	Skybox(std::shared_ptr<TextureCube> cubemap, std::shared_ptr<Shader> shader = nullptr);
	~Skybox() = default;

	void Draw(const Camera& camera, const glm::ivec2& viewportSize);

	void SetCubemap(std::shared_ptr<TextureCube> cubemap) { m_Cubemap = std::move(cubemap); }
	std::shared_ptr<TextureCube> GetCubemap() const { return m_Cubemap; }

	void SetShader(std::shared_ptr<Shader> shader) { m_Shader = std::move(shader); }
	std::shared_ptr<Shader> GetShader() const { return m_Shader; }

	bool IsActive() const { return m_Active; }
	void SetActive(bool active) { m_Active = active; }

private:
	std::shared_ptr<TextureCube> m_Cubemap;
	std::shared_ptr<Shader> m_Shader;
	std::shared_ptr<Mesh> m_CubeMesh;
	bool m_Active = true;
};
