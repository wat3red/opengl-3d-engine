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

	void Draw(Shader& shader) override {}

protected:
	LightType m_Type;
	glm::vec3 m_Color;
	float m_Intensity;
};

class DirectionalLight : public Light {
public:
	DirectionalLight(const std::string& name, const glm::vec3& direction, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f)
		: Light(name, LightType::Directional, color, intensity), m_Direction(glm::normalize(direction)) {}

	DirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity)
		: DirectionalLight("Directional Light", direction, color, intensity) {}

	glm::vec3 GetDirection() const override { return glm::normalize(m_Direction); }
	void SetDirection(const glm::vec3& direction) override { m_Direction = glm::normalize(direction); }

private:
	glm::vec3 m_Direction;
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

	float GetConstant() const { return m_Constant; }
	float GetLinear() const { return m_Linear; }
	float GetQuadratic() const { return m_Quadratic; }

private:
	float m_Range;
	float m_Constant;
	float m_Linear;
	float m_Quadratic;
};

class SpotLight : public Light {
public:
	SpotLight(const std::string& name, const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color = glm::vec3(1.0f), float intensity = 1.0f, float innerCutoff = 12.5f, float outerCutoff = 17.5f)
		: Light(name, LightType::Spotlight, color, intensity), m_Direction(glm::normalize(direction)), m_InnerCutoff(innerCutoff), m_OuterCutoff(outerCutoff),
		  m_Constant(1.0f), m_Linear(0.09f), m_Quadratic(0.032f)
	{
		m_Transform.Position = position;
	}

	SpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity, float innerCutoff, float outerCutoff)
		: SpotLight("Spot Light", position, direction, color, intensity, innerCutoff, outerCutoff) {}

	glm::vec3 GetDirection() const override { return glm::normalize(m_Direction); }
	void SetDirection(const glm::vec3& direction) override { m_Direction = glm::normalize(direction); }

	float GetInnerCutoff() const override { return m_InnerCutoff; }
	void SetInnerCutoff(float innerCutoff) override { m_InnerCutoff = innerCutoff; }

	float GetOuterCutoff() const override { return m_OuterCutoff; }
	void SetOuterCutoff(float outerCutoff) override { m_OuterCutoff = outerCutoff; }

	float GetConstant() const { return m_Constant; }
	float GetLinear() const { return m_Linear; }
	float GetQuadratic() const { return m_Quadratic; }

private:
	glm::vec3 m_Direction;
	float m_InnerCutoff;
	float m_OuterCutoff;
	float m_Constant;
	float m_Linear;
	float m_Quadratic;
};
