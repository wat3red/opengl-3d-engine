#pragma once

#include "entity.h"
#include "shader.h"
#include <glm/glm.hpp>
#include <string>

enum class LightType {
	Directional,
	Point,
	Spotlight
};

// Aliases for backwards compatibility with legacy enums
inline constexpr LightType DIRECTIONAL = LightType::Directional;
inline constexpr LightType POINT = LightType::Point;
inline constexpr LightType SPOTLIGHT = LightType::Spotlight;

class Light : public Entity {
public:
	Light(const std::string& name, LightType type, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f)
		: Entity(name), m_Type(type), m_Color(color), m_Intensity(intensity) {}

	Light(LightType type, const glm::vec3& color, float intensity)
		: Light("Light", type, color, intensity) {}

	~Light() override = default;

	LightType GetType() const { return m_Type; }
	glm::vec3 GetColor() const { return m_Color; }
	void SetColor(const glm::vec3& color) { m_Color = color; }

	float GetIntensity() const { return m_Intensity; }
	void SetIntensity(float intensity) { m_Intensity = intensity; }

	virtual glm::vec3 GetDirection() const { return glm::vec3(0.0f, -1.0f, 0.0f); }
	virtual void SetDirection(const glm::vec3&) {}

	virtual float GetRange() const { return 25.0f; }
	virtual void SetRange(float) {}

	virtual float GetInnerCutoff() const { return 12.5f; }
	virtual void SetInnerCutoff(float) {}

	virtual float GetOuterCutoff() const { return 17.5f; }
	virtual void SetOuterCutoff(float) {}

	virtual float GetConstant() const { return 1.0f; }
	virtual float GetLinear() const { return 0.09f; }
	virtual float GetQuadratic() const { return 0.032f; }

	void Draw(Shader& shader) override {}

protected:
	LightType m_Type;
	glm::vec3 m_Color;
	float m_Intensity;
};

inline glm::mat4 RotationMatrixFromEuler(const glm::vec3& rotation)
{
	glm::mat4 m = glm::mat4(1.0f);
	m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	return m;
}

inline glm::vec3 DirectionFromEuler(const glm::vec3& rotation)
{
	glm::mat4 m = RotationMatrixFromEuler(rotation);
	return glm::normalize(glm::vec3(m * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
}

inline glm::vec3 EulerFromDirection(const glm::vec3& direction)
{
	glm::vec3 f = glm::normalize(direction);
	float rx = glm::degrees(std::asin(glm::clamp(f.y, -1.0f, 1.0f)));
	float ry = glm::degrees(std::atan2(-f.x, -f.z));
	return glm::vec3(rx, ry, 0.0f);
}

class DirectionalLight : public Light {
public:
	DirectionalLight(const std::string& name, const glm::vec3& direction, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f)
		: Light(name, LightType::Directional, color, intensity)
	{
		SetDirection(direction);
	}

	DirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity)
		: DirectionalLight("Directional Light", direction, color, intensity) {}

	glm::vec3 GetDirection() const override { return glm::normalize(m_Direction); }
	void SetDirection(const glm::vec3& direction) override {
		if (glm::length(direction) > 0.001f) {
			m_Direction = glm::normalize(direction);
			m_Transform.Rotation = EulerFromDirection(m_Direction);
		}
	}
	void SetRotation(const glm::vec3& rotation) override {
		m_Transform.Rotation = rotation;
		m_Direction = DirectionFromEuler(rotation);
	}

private:
	glm::vec3 m_Direction{ 0.0f, -1.0f, 0.0f };
};

class PointLight : public Light {
public:
	PointLight(const std::string& name, const glm::vec3& position, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f, float range = 25.0f)
		: Light(name, LightType::Point, color, intensity), m_Range(range), m_Constant(1.0f), m_Linear(0.09f), m_Quadratic(0.032f)
	{
		m_Transform.Position = position;
	}

	PointLight(const glm::vec3& position, const glm::vec3& color, float intensity, float range)
		: PointLight("Point Light", position, color, intensity, range) {}

	float GetRange() const override { return m_Range; }
	void SetRange(float range) override { m_Range = range; }

	float GetConstant() const override { return m_Constant; }
	float GetLinear() const override { return m_Linear; }
	float GetQuadratic() const override { return m_Quadratic; }

private:
	float m_Range;
	float m_Constant;
	float m_Linear;
	float m_Quadratic;
};

class SpotLight : public Light {
public:
	SpotLight(const std::string& name, const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f, float innerCutoff = 12.5f, float outerCutoff = 17.5f, float range = 35.0f)
		: Light(name, LightType::Spotlight, color, intensity), m_InnerCutoff(innerCutoff), m_OuterCutoff(outerCutoff), m_Range(range),
		  m_Constant(1.0f), m_Linear(0.09f), m_Quadratic(0.032f)
	{
		m_Transform.Position = position;
		SetDirection(direction);
	}

	SpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity, float innerCutoff, float outerCutoff, float range = 35.0f)
		: SpotLight("Spot Light", position, direction, color, intensity, innerCutoff, outerCutoff, range) {}

	glm::vec3 GetDirection() const override { return glm::normalize(m_Direction); }
	void SetDirection(const glm::vec3& direction) override {
		if (glm::length(direction) > 0.001f) {
			m_Direction = glm::normalize(direction);
			m_Transform.Rotation = EulerFromDirection(m_Direction);
		}
	}
	void SetRotation(const glm::vec3& rotation) override {
		m_Transform.Rotation = rotation;
		m_Direction = DirectionFromEuler(rotation);
	}

	float GetInnerCutoff() const override { return m_InnerCutoff; }
	void SetInnerCutoff(float innerCutoff) override { m_InnerCutoff = innerCutoff; }

	float GetOuterCutoff() const override { return m_OuterCutoff; }
	void SetOuterCutoff(float outerCutoff) override { m_OuterCutoff = outerCutoff; }

	float GetRange() const override { return m_Range; }
	void SetRange(float range) override { m_Range = range; }

	float GetConstant() const override { return m_Constant; }
	float GetLinear() const override { return m_Linear; }
	float GetQuadratic() const override { return m_Quadratic; }

private:
	glm::vec3 m_Direction{ 0.0f, 0.0f, -1.0f };
	float m_InnerCutoff;
	float m_OuterCutoff;
	float m_Range;
	float m_Constant;
	float m_Linear;
	float m_Quadratic;
};
