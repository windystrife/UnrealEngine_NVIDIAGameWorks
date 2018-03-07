#include "ClothMeshGenerator.h"

void ClothMeshData::Clear()
{
	mVertices.clear();
	mTriangles.clear();
	mQuads.clear();
}

void ClothMeshData::GeneratePlaneCloth(float width, float height, int segmentsX, int segmentsY, bool generateQuads)
{

/*
GeneratePlaneCloth(x,y,2,2) generates:

    v0______v1_____v2     v0______v1_____v2
     |      |      |       |\     |\     |
     |  Q0  |  Q1  |       |  \t0 |  \t2 |
     |      |      |       | t1 \ | t3 \ |
    v3------v4-----v5     v3-----\v4----\v5
     |      |      |       | \    | \    |
     |  Q2  |  Q3  |       |   \t4|   \t6|
     |______|______|       |_t5_\_|_t7__\|
    v6      v7     v8     v6      v7     v8
*/


	Clear();
	mVertices.resize((segmentsX + 1) * (segmentsY + 1));
	mInvMasses.resize((segmentsX + 1) * (segmentsY + 1));
	mTriangles.resize(segmentsX * segmentsY * 2);
	if(generateQuads)
		mQuads.resize(segmentsX*segmentsY);

	physx::PxVec3 topLeft(-width*0.5f, 0.0f, -height*0.5f);

	//Vertices
	for(int y = 0; y < segmentsY + 1; y++)
	{
		for(int x = 0; x < segmentsX + 1; x++)
		{
			mVertices[x + y * (segmentsX + 1)] = topLeft + physx::PxVec3( ((float)x / (float)segmentsX) * width,
																		0.f,
																		((float)y / (float)segmentsY) * height);
			mInvMasses[x + y * (segmentsX + 1)] = 1.0f;
		}
	}

	if(generateQuads)
	{
		//Quads
		for(int y = 0; y < segmentsY; y++)
		{
			for(int x = 0; x < segmentsX; x++)
			{
				mQuads[(x + y * segmentsX)] = Quad((uint32_t)(x + 0) + (y + 0) * (segmentsX + 1),
												(uint32_t)(x + 1) + (y + 0) * (segmentsX + 1),
												(uint32_t)(x + 1) + (y + 1) * (segmentsX + 1),
												(uint32_t)(x + 0) + (y + 1) * (segmentsX + 1));
			}
		}
	}

	//Triangles
	for(int y = 0; y < segmentsY; y++)
	{
		for(int x = 0; x < segmentsX; x++)
		{
			mTriangles[(x + y * segmentsX)* 2 + 0] = Triangle((uint32_t)(x + 0) + (y + 0) * (segmentsX + 1),
															(uint32_t)(x + 1) + (y + 0) * (segmentsX + 1),
															(uint32_t)(x + 1) + (y + 1) * (segmentsX + 1));
																		    	   	      
			mTriangles[(x + y * segmentsX)* 2 + 1] = Triangle((uint32_t)(x + 0) + (y + 0) * (segmentsX + 1),
															(uint32_t)(x + 1) + (y + 1) * (segmentsX + 1),
															(uint32_t)(x + 0) + (y + 1) * (segmentsX + 1));
		}																    		 
	}
}

template <typename T>
nv::cloth::BoundedData ToBoundedData(T& vector)
{
	nv::cloth::BoundedData d;
	d.data = vector.size()?&vector[0]:nullptr;
	d.stride = sizeof(vector[0]);
	d.count = (physx::PxU32)vector.size();
	return d;
}

nv::cloth::ClothMeshDesc ClothMeshData::GetClothMeshDesc()
{
	nv::cloth::ClothMeshDesc d;
	d.setToDefault();
	d.points = ToBoundedData(mVertices);
	d.quads = ToBoundedData(mQuads);
	d.triangles = ToBoundedData(mTriangles);
	d.invMasses = ToBoundedData(mInvMasses);
	return d;
}