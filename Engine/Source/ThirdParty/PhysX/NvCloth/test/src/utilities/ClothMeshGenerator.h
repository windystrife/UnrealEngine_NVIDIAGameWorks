#pragma once
#include <vector>
#include <foundation/PxVec3.h>
#include "NvClothExt/ClothFabricCooker.h"

struct ClothMeshData
{
	struct Triangle
	{
		Triangle(){}
		Triangle(uint32_t _a, uint32_t _b, uint32_t _c) :
			a(_a), b(_b), c(_c){}
		uint32_t a, b, c;
	};
	struct Quad
	{
		Quad(){}
		Quad(uint32_t _a, uint32_t _b, uint32_t _c, uint32_t _d) :
			a(_a), b(_b), c(_c), d(_d){}
		uint32_t a, b, c, d;
	};
	std::vector<physx::PxVec3> mVertices;
	std::vector<Triangle> mTriangles;
	std::vector<Quad> mQuads;
	std::vector<physx::PxReal> mInvMasses;

	void Clear();
	void GeneratePlaneCloth(float width, float height, int segmentsX, int segmentsY, bool generateQuads = true);

	nv::cloth::ClothMeshDesc GetClothMeshDesc();
};
