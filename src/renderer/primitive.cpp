// primitive.cpp
#include "primitive.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

static Vertex MakeVertex(const glm::vec3& pos, const glm::vec3& norm = glm::vec3(0.0f), const glm::vec2& uv = glm::vec2(0.0f))
{
	Vertex v{};
	v.Position = pos;
	v.Normal = norm;
	v.TexCoords = uv;
	v.Tangent = glm::vec3(0.0f);
	v.Bitangent = glm::vec3(0.0f);
	for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
	{
		v.m_BoneIDs[i] = -1;
		v.m_Weights[i] = 0.0f;
	}
	return v;
}

std::shared_ptr<Mesh> Primitive::CreateCube(float size)
{
	float h = size * 0.5f;

	// 24 vertices (4 per face with dedicated face normals and UVs)
	std::vector<Vertex> vertices = {
		// Front face (Z = +h)
		MakeVertex({ -h, -h,  h }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f }),
		MakeVertex({  h, -h,  h }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f }),
		MakeVertex({  h,  h,  h }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f }),
		MakeVertex({ -h,  h,  h }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f }),

		// Back face (Z = -h)
		MakeVertex({  h, -h, -h }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f }),
		MakeVertex({ -h, -h, -h }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f }),
		MakeVertex({ -h,  h, -h }, {  0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f }),
		MakeVertex({  h,  h, -h }, {  0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f }),

		// Left face (X = -h)
		MakeVertex({ -h, -h, -h }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }),
		MakeVertex({ -h, -h,  h }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }),
		MakeVertex({ -h,  h,  h }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }),
		MakeVertex({ -h,  h, -h }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }),

		// Right face (X = +h)
		MakeVertex({  h, -h,  h }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f }),
		MakeVertex({  h, -h, -h }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f }),
		MakeVertex({  h,  h, -h }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f }),
		MakeVertex({  h,  h,  h }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f }),

		// Top face (Y = +h)
		MakeVertex({ -h,  h,  h }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f }),
		MakeVertex({  h,  h,  h }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f }),
		MakeVertex({  h,  h, -h }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f }),
		MakeVertex({ -h,  h, -h }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f }),

		// Bottom face (Y = -h)
		MakeVertex({ -h, -h, -h }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f }),
		MakeVertex({  h, -h, -h }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 0.0f }),
		MakeVertex({  h, -h,  h }, {  0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f }),
		MakeVertex({ -h, -h,  h }, {  0.0f, -1.0f,  0.0f }, { 0.0f, 1.0f })
	};

	std::vector<unsigned int> indices;
	indices.reserve(36);
	for (unsigned int i = 0; i < 6; ++i)
	{
		unsigned int offset = i * 4;
		indices.push_back(offset + 0);
		indices.push_back(offset + 1);
		indices.push_back(offset + 2);
		indices.push_back(offset + 2);
		indices.push_back(offset + 3);
		indices.push_back(offset + 0);
	}

	std::vector<Texture> textures;
	return std::make_shared<Mesh>(std::move(vertices), std::move(indices), std::move(textures));
}

std::shared_ptr<Mesh> Primitive::CreateQuad(float width, float height)
{
	float hw = width * 0.5f;
	float hh = height * 0.5f;

	std::vector<Vertex> vertices = {
		MakeVertex({ -hw, -hh, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }),
		MakeVertex({  hw, -hh, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }),
		MakeVertex({  hw,  hh, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }),
		MakeVertex({ -hw,  hh, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f })
	};

	std::vector<unsigned int> indices = { 0, 1, 2, 2, 3, 0 };
	std::vector<Texture> textures;
	return std::make_shared<Mesh>(std::move(vertices), std::move(indices), std::move(textures));
}

std::shared_ptr<Mesh> Primitive::CreateSphere(float radius, unsigned int rings, unsigned int sectors)
{
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	const float R = 1.0f / static_cast<float>(rings - 1);
	const float S = 1.0f / static_cast<float>(sectors - 1);

	vertices.reserve(rings * sectors);
	for (unsigned int r = 0; r < rings; ++r)
	{
		for (unsigned int s = 0; s < sectors; ++s)
		{
			float y = std::sin(-glm::half_pi<float>() + glm::pi<float>() * r * R);
			float x = std::cos(2.0f * glm::pi<float>() * s * S) * std::sin(glm::pi<float>() * r * R);
			float z = std::sin(2.0f * glm::pi<float>() * s * S) * std::sin(glm::pi<float>() * r * R);

			glm::vec3 norm = glm::normalize(glm::vec3(x, y, z));
			glm::vec3 pos = norm * radius;
			glm::vec2 uv = { s * S, r * R };

			vertices.push_back(MakeVertex(pos, norm, uv));
		}
	}

	indices.reserve(rings * sectors * 6);
	for (unsigned int r = 0; r < rings - 1; ++r)
	{
		for (unsigned int s = 0; s < sectors - 1; ++s)
		{
			unsigned int cur = r * sectors + s;
			unsigned int next = cur + sectors;

			indices.push_back(cur);
			indices.push_back(next);
			indices.push_back(next + 1);

			indices.push_back(cur);
			indices.push_back(next + 1);
			indices.push_back(cur + 1);
		}
	}

	std::vector<Texture> textures;
	return std::make_shared<Mesh>(std::move(vertices), std::move(indices), std::move(textures));
}

std::shared_ptr<Mesh> Primitive::GetUnitCube()
{
	static std::shared_ptr<Mesh> s_UnitCube = CreateCube(1.0f);
	return s_UnitCube;
}

std::shared_ptr<Mesh> Primitive::GetUnitQuad()
{
	static std::shared_ptr<Mesh> s_UnitQuad = CreateQuad(1.0f, 1.0f);
	return s_UnitQuad;
}

std::shared_ptr<Mesh> Primitive::GetUnitSphere()
{
	static std::shared_ptr<Mesh> s_UnitSphere = CreateSphere(1.0f);
	return s_UnitSphere;
}
