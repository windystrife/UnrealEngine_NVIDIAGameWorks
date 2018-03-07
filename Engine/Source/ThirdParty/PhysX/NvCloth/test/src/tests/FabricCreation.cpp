#include "utilities/Utilities.h"
#include <NvCloth/Factory.h>
#include <NvCloth/Fabric.h>
#include "utilities/ClothMeshGenerator.h"

TEST_LEAK_FIXTURE_WITH_PARAM(FabricCreation, PlatformTestParameter)

TEST_P(FabricCreation,Cooker)
{
	ScopedFactoryHelper factoryHelper(GetParam().mPlatform);
	nv::cloth::Factory* factory = factoryHelper.CreateFactory();
	ASSERT_PTR(factory);

	ClothMeshData testCloth;
	testCloth.GeneratePlaneCloth(10, 10, 2, 2);
	auto meshDesc = testCloth.GetClothMeshDesc();
	ASSERT_TRUE(meshDesc.isValid());

	nv::cloth::Fabric* fabric = NvClothCookFabricFromMesh(factory, meshDesc, physx::PxVec3(0.0f, -9.8f, 0.0f));
	ASSERT_PTR(fabric);
	fabric->decRefCount();

	NvClothDestroyFactory(factory);
}

TEST_P(FabricCreation,Manual)
{
	ScopedFactoryHelper factoryHelper(GetParam().mPlatform);
	nv::cloth::Factory* factory = factoryHelper.CreateFactory();
	ASSERT_PTR(factory);

	ClothMeshData testCloth;
	testCloth.GeneratePlaneCloth(10, 10, 2, 2);
	auto meshDesc = testCloth.GetClothMeshDesc();
	ASSERT_TRUE(meshDesc.isValid());

	nv::cloth::Fabric* fabric;
	{
		nv::cloth::ClothFabricCooker* cooker = NvClothCreateFabricCooker();
		cooker->cook(meshDesc, physx::PxVec3(0.0f, -9.8f, 0.0f));

		nv::cloth::CookedData data = cooker->getCookedData();

		fabric = factory->createFabric(
			data.mNumParticles,
			data.mPhaseIndices,
			data.mSets,
			data.mRestvalues,
			nv::cloth::Range<const float>(nullptr,nullptr),
			data.mIndices,
			data.mAnchors,
			data.mTetherLengths,
			data.mTriangles
			);
		delete cooker;
	}

	ASSERT_PTR(fabric);
	fabric->decRefCount();

	NvClothDestroyFactory(factory);
}

INSTANTIATE_PLATFORM_TEST_CASE_P(FabricCreation)