#pragma once

#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

struct ShaderSources {
	std::string VertexShader;
	std::string FragmentShader;
	std::string GeometryShader;
};

class Shader
{
private:
	std::string m_FilePath;
	unsigned int m_RendererID;
	std::unordered_map<std::string, int> m_UniformLocationCache;

	int CompileShader(unsigned int type, const std::string& source);
	int GetUniformLocation(const std::string& name);
	int CreateShader(ShaderSources shaderSources);
	ShaderSources ParseShaders(std::string filename);
public:
	Shader(const std::string& filePath);
	Shader();

	~Shader();
	
	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;
	
	Shader(Shader&& other) noexcept;
	Shader& operator=(Shader&& other) noexcept;

	void Bind() const;
	void Unbind() const;

	bool SetUniform4f(const std::string& name, float v0, float v1, float v2, float v3);
	bool SetUniform4f(const std::string& name, glm::vec4 values);
	bool SetUniform3f(const std::string& name, float v0, float v1, float v2);
	bool SetUniform3f(const std::string& name, glm::vec3 value);
	bool SetUniform1i(const std::string& name, int value);
	bool SetUniform1f(const std::string& name, float value);
	bool SetUniform2f(const std::string& name, float value1, float value2);
	bool SetUniformMat4f(const std::string& name, glm::mat4 matrix);
	inline unsigned int RendererID() const { return m_RendererID; }
};
