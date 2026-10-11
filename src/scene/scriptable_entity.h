#pragma once

#include "entity.h"

class ScriptableEntity {
public:
	virtual ~ScriptableEntity() = default;

	template<typename T>
	T& GetComponent()
	{
		return m_Entity.GetComponent<T>();
	}

	template<typename T>
	const T& GetComponent() const
	{
		return m_Entity.GetComponent<T>();
	}

	template<typename T>
	bool HasComponent() const
	{
		return m_Entity.HasComponent<T>();
	}

	Entity GetEntity() const { return m_Entity; }
	Scene* GetScene() const { return m_Entity.GetScene(); }

protected:
	virtual void OnCreate() {}
	virtual void OnUpdate(float deltaTime) {}
	virtual void OnDestroy() {}

private:
	Entity m_Entity;
	friend class Scene;
};
