#pragma once

#include "mesh.h"
#include <memory>

class Primitive {
public:
	// Procedural generation helpers
	static std::shared_ptr<Mesh> CreateCube(float size = 1.0f);
	static std::shared_ptr<Mesh> CreateQuad(float width = 1.0f, float height = 1.0f);
	static std::shared_ptr<Mesh> CreateSphere(float radius = 1.0f, unsigned int rings = 16, unsigned int sectors = 32);

	// Shared singleton instances (allocated once on the GPU, reused everywhere)
	static std::shared_ptr<Mesh> GetUnitCube();
	static std::shared_ptr<Mesh> GetUnitQuad();
	static std::shared_ptr<Mesh> GetUnitSphere();
};
