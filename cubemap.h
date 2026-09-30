#pragma once

#include <string>
#include <vector>

class Cubemap {
private:
	unsigned int m_RendererID;
	std::vector<std::string> m_FacePathes;

public:
	Cubemap(const std::vector<std::string>& m_FacePathes);
	~Cubemap();

	void Render() const;
	void Unbind() const;

	inline int GetRendererID() const { return m_RendererID; }
};
