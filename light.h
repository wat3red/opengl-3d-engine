// Light.h
#pragma once

#include "entity.h"
#include "shader.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <GL/glew.h>

enum LightType { DIRECTIONAL, POINT, SPOTLIGHT };

class Light : public Entity {
public:
	Light(LightType type, const glm::vec3& color, float intensity)
		: m_Type(type), m_Color(color), m_Intensity(intensity) {}

	virtual ~Light() = default;

	// General Getters
	LightType GetType() const { return m_Type; }
	glm::vec3 GetColor() const { return m_Color; }
	float GetIntensity() const { return m_Intensity; }

	// General Setters
	void SetColor(const glm::vec3& color) { m_Color = color; }
	void SetIntensity(float intensity) { m_Intensity = intensity; }

	// Type-specific methods with default implementations
	virtual glm::vec3 GetDirection() const { return glm::vec3(0.0f); }
	virtual void SetDirection(const glm::vec3&) {}

	virtual float GetRange() const { return 0.0f; }
	virtual void SetRange(float) {}

	virtual float GetInnerCutoff() const { return 0.0f; }
	virtual void SetInnerCutoff(float) {}

	virtual float GetOuterCutoff() const { return 0.0f; }
	virtual void SetOuterCutoff(float) {}

	virtual void Draw(Shader& shader) = 0;

protected:
	LightType m_Type;
	glm::vec3 m_Color;
	float m_Intensity;
};

// Directional Light
class DirectionalLight : public Light {
public:
	DirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity)
		: Light(DIRECTIONAL, color, intensity), m_Direction(direction) {}

	glm::vec3 GetDirection() const override {
		return glm::normalize(m_Direction);
	}
	void SetDirection(const glm::vec3& direction) override { m_Direction = direction; }

	void Draw(Shader& shader) override {
		// Implement shader setup for directional light
	}

private:
	glm::vec3 m_Direction;
};

// Point Light
class PointLight : public Light {
public:
	PointLight(const glm::vec3& position, const glm::vec3& color, float intensity, float range)
		: Light(POINT, color, intensity), m_Range(range) {
		SetPosition(position); // Initialize with position
	}

	float GetRange() const override { return m_Range; }
	void SetRange(float range) override { m_Range = range; }

	void Draw(Shader& shader) override {
		// Implement shader setup for point light
	}

private:
	float m_Range;
};

// Spotlight
class SpotLight : public Light {
public:
	SpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity, float innerCutoff, float outerCutoff)
		: Light(SPOTLIGHT, color, intensity), m_Direction(direction), m_InnerCutoff(innerCutoff), m_OuterCutoff(outerCutoff) {
		SetPosition(position); // Initialize with position
	}

	glm::vec3 GetDirection() const override { return m_Direction; }
	void SetDirection(const glm::vec3& direction) override { m_Direction = direction; }

	float GetInnerCutoff() const override { return m_InnerCutoff; }
	void SetInnerCutoff(float innerCutoff) override { m_InnerCutoff = innerCutoff; }

	float GetOuterCutoff() const override { return m_OuterCutoff; }
	void SetOuterCutoff(float outerCutoff) override { m_OuterCutoff = outerCutoff; }

	void Draw(Shader& shader) override {
		// Implement shader setup for spotlight
	}

private:
	glm::vec3 m_Direction;
	float m_InnerCutoff;
	float m_OuterCutoff;
};
