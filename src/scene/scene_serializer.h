#pragma once

#include "scene.h"
#include <string>

class Renderer;
class Editor;

class SceneSerializer {
public:
	explicit SceneSerializer(Scene& scene, Renderer* renderer = nullptr, Editor* editor = nullptr);

	bool Serialize(const std::string& filepath);
	bool Deserialize(const std::string& filepath);

private:
	Scene& m_Scene;
	Renderer* m_Renderer;
	Editor* m_Editor;
};
