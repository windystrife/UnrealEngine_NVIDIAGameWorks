#include "utilities/Utilities.h"
#include "utilities/ClothMeshGenerator.h"

void TestCookerWithPlaneCloth(int xSegments, int ySegments, const physx::PxVec3& gravity, bool useGeodesicTether)
{
	auto cpuFactory = NvClothCreateFactoryCPU();
	ASSERT_PTR(cpuFactory);

	ClothMeshData testCloth;
	testCloth.GeneratePlaneCloth(20.0f, 20.0f, xSegments, ySegments);
	auto meshDesc = testCloth.GetClothMeshDesc();
	ASSERT_TRUE(meshDesc.isValid());

	nv::cloth::Fabric* fabric = NvClothCookFabricFromMesh(cpuFactory, meshDesc, gravity, nullptr, useGeodesicTether);
	ASSERT_PTR(fabric);

	fabric->decRefCount();
	NvClothDestroyFactory(cpuFactory);
}

TEST_LEAK_FIXTURE(Cooker)
TEST_F(Cooker,Plane2x2)
{
	TestCookerWithPlaneCloth(2, 2, physx::PxVec3(0.0f, -9.8f, 0.0f), false);
}

TEST_F(Cooker,Plane2x2Geodesic)
{
	TestCookerWithPlaneCloth(2, 2, physx::PxVec3(0.0f, -9.8f, 0.0f), true);
}


TEST_F(Cooker,Plane200x200)
{
	TestCookerWithPlaneCloth(200, 200, physx::PxVec3(0.0f, -9.8f, 0.0f), false);
}

TEST_F(Cooker,Plane200x200Geodesic)
{
	TestCookerWithPlaneCloth(200, 200, physx::PxVec3(0.0f, -9.8f, 0.0f), true);
}

TEST_F(Cooker,Plane1000x2)
{
	TestCookerWithPlaneCloth(1000, 2, physx::PxVec3(0.0f, -9.8f, 0.0f), false);
}

TEST_F(Cooker,Plane1000x2Geodesic)
{
	TestCookerWithPlaneCloth(1000, 2, physx::PxVec3(0.0f, -9.8f, 0.0f), true);
}

TEST_F(Cooker,Plane3x1000)
{
	TestCookerWithPlaneCloth(3, 1000, physx::PxVec3(0.0f, -9.8f, 0.0f), false);
}

TEST_F(Cooker,Plane3x1000Geodesic)
{
	TestCookerWithPlaneCloth(3, 1000, physx::PxVec3(0.0f, -9.8f, 0.0f), true);
}