#include <GL/glew.h>

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

#include "shader.h"

ShaderSources Shader::ParseShaders(std::string filename) {
	std::ifstream filestream(filename);
	std::string line;
	ShaderSources output;
	// 0 for none, 1 for vertex, 2 for fragment and 3 for geometry
	char currentShaderType;
	while (getline(filestream, line)) {
		if (line.find("#shader") != std::string::npos) {
			if (line.find("vertex") != std::string::npos) {
				currentShaderType = 1;
				continue;
			}
			else if (line.find("fragment") != std::string::npos) {
				currentShaderType = 2;
				continue;
			}
			else if (line.find("geometry") != std::string::npos) {
				currentShaderType = 3;
				continue;
			}
		}
		else {
			switch (currentShaderType) {
			case 1:
				output.VertexShader += line + "\n";
				break;
			case 2:
				output.FragmentShader += line + "\n";
				break;
			case 3:
				output.GeometryShader += line + "\n";
				break;
			}
		}
	}
	return output;
}

int Shader::CompileShader(unsigned int type, const std::string& source)
{
	if (source == "") return 0;
	unsigned int id = glCreateShader(type);
	const char* src = source.c_str();
	glShaderSource(id, 1, &src, nullptr);
	glCompileShader(id);

	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	if (!result) {
		int length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);

		std::string message(length, '\0');
		glGetShaderInfoLog(id, length, &length, &message[0]);

		const char* shaderTypeName = "undefined";
		switch (type) {
		case GL_VERTEX_SHADER:
			shaderTypeName = "vertex";
			break;
		case GL_FRAGMENT_SHADER:
			shaderTypeName = "fragment";
			break;
		case GL_GEOMETRY_SHADER:
			shaderTypeName = "geometry";
		}
		std::cout << "Failed to compile " << shaderTypeName << " shader!\n" << message << std::endl;

		glDeleteShader(id);
		return 0;
	}

	return id;
}

int Shader::GetUniformLocation(const std::string& name)
{
	if (m_UniformLocationCache.find(name) != m_UniformLocationCache.end())
		return m_UniformLocationCache[name];

	int location = glGetUniformLocation(RendererID(), name.c_str());
	if (location == -1)
		std::cout << "Uniform with name " << name << " doesn't exists in shader " << m_FilePath << "!" << "\n";
	m_UniformLocationCache[name] = location;
	return location;
}

Shader::Shader(const std::string& filePath) : m_RendererID(0), m_FilePath(filePath)
{
	std::cout << "Loading " << filePath << "\n";
	ShaderSources basicShaders = ParseShaders(filePath);
	m_RendererID = CreateShader(basicShaders);
}

Shader::Shader() : m_RendererID(0) {}

Shader::Shader(Shader&& other) noexcept
	: m_RendererID(std::exchange(other.m_RendererID, 0)),
	m_FilePath(std::move(other.m_FilePath)),
	m_UniformLocationCache(std::move(other.m_UniformLocationCache)) {}

Shader& Shader::operator=(Shader&& other) noexcept
{
	if (this != &other)
	{
		if (m_RendererID != 0)
		{
			glDeleteProgram(m_RendererID);
		}

		m_RendererID = std::exchange(other.m_RendererID, 0);
		m_FilePath = std::move(other.m_FilePath);
		m_UniformLocationCache = std::move(other.m_UniformLocationCache);
	}
	return *this;
}

Shader::~Shader() {
	if (RendererID() != 0)
		glDeleteProgram(RendererID());
}

void Shader::Bind() const
{
	glUseProgram(RendererID());
}

void Shader::Unbind() const
{
	glUseProgram(0);
}

int Shader::CreateShader(ShaderSources shaderSources)
{
	unsigned int program = glCreateProgram();
	unsigned int vs = CompileShader(GL_VERTEX_SHADER, shaderSources.VertexShader);
	unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, shaderSources.FragmentShader);
	unsigned int gs = CompileShader(GL_GEOMETRY_SHADER, shaderSources.GeometryShader);

	glAttachShader(program, vs);
	glAttachShader(program, fs);
	if (gs != 0)
		glAttachShader(program, gs);

	glLinkProgram(program);
	glValidateProgram(program);

	glDeleteShader(vs);
	glDeleteShader(fs);
	if (gs != 0)
		glDeleteShader(gs);

	return program;
}

bool Shader::SetUniform4f(const std::string& name, glm::vec4 values)
{
	int location = GetUniformLocation(name);
	if (location == -1)
		return false;
	glUniform4f(location, values[0], values[1], values[2], values[3]);
	return true;
}

bool Shader::SetUniform4f(const std::string& name, float v0, float v1, float v2, float v3)
{
	int location = GetUniformLocation(name);
	if (location == -1)
		return false;
	glUniform4f(location, v0, v1, v2, v3);
	return true;
}

bool Shader::SetUniform3f(const std::string& name, float v0, float v1, float v2)
{
	int location = GetUniformLocation(name);
	if (location == -1)
		return false;
	glUniform3f(location, v0, v1, v2);
	return true;
}

bool Shader::SetUniform3f(const std::string& name, glm::vec3 value)
{
	int location = GetUniformLocation(name);
	if (location == -1)
		return false;
	glUniform3f(location, value[0], value[1], value[2]);
	return true;
}

bool Shader::SetUniform1i(const std::string& name, int value)
{
	int location = GetUniformLocation(name);
	if (location == -1)
		return false;
	glUniform1i(location, value);
	return true;
}

bool Shader::SetUniform1f(const std::string& name, float value)
{
	int location = GetUniformLocation(name);
	if (location == -1)
		return false;
	glUniform1f(location, value);
	return true;
}

bool Shader::SetUniform2f(const std::string& name, float value1, float value2)
{
	int location = GetUniformLocation(name);
	if (location == -1)
		return false;
	glUniform2f(location, value1, value2);
	return true;
}



bool Shader::SetUniformMat4f(const std::string& name, glm::mat4 matrix)
{
	int location = GetUniformLocation(name);
	if (location == -1)
		return false;
	glUniformMatrix4fv(location, 1, false, &matrix[0].x);
	return true;
}
