#include "utilities/Utilities.h"
#include <NvCloth/Factory.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Cloth.h>
#include <NvCloth/Range.h>
#include "utilities/ClothMeshGenerator.h"
#include <PsFoundation.h>

struct CloningTestParameters
{
	CloningTestParameters(nv::cloth::Platform platform1, nv::cloth::Platform platform2)
	{
		mPlatform1 = platform1;
		mPlatform2 = platform2;
	}
	testing::Message& GetShortName(testing::Message& msg) const
	{
		return msg<<GetPlatformName(mPlatform1) << ">"<< GetPlatformName(mPlatform2);
	}

	nv::cloth::Platform mPlatform1;
	nv::cloth::Platform mPlatform2;
};

TEST_LEAK_FIXTURE_WITH_PARAM(Cloning, CloningTestParameters)

TEST_P(Cloning,ClothCloning)
{
	FactoryHelper* factoryHelper1 = FactoryHelper::CreateFactoryHelper(GetParam().mPlatform1);
	FactoryHelper* factoryHelper2 = factoryHelper1;
	if(GetParam().mPlatform1 != GetParam().mPlatform2)
		factoryHelper2 = FactoryHelper::CreateFactoryHelper(GetParam().mPlatform2);

	auto factory = factoryHelper1->CreateFactory();
	auto factory2 = factoryHelper2->CreateFactory();
	ASSERT_PTR(factory);
	ASSERT_PTR(factory2);

	ClothMeshData testCloth;
	testCloth.GeneratePlaneCloth(10, 10, 2, 2);
	auto meshDesc = testCloth.GetClothMeshDesc();
	ASSERT_TRUE(meshDesc.isValid());

	nv::cloth::Fabric* fabric = NvClothCookFabricFromMesh(factory, meshDesc, physx::PxVec3(0.0f, -9.8f, 0.0f), nullptr, false);
	ASSERT_PTR(fabric);

	std::vector<physx::PxVec4> particlesCopy;
	particlesCopy.resize(testCloth.mVertices.size());
	for(int i = 0; i < (int)testCloth.mVertices.size(); i++)
		particlesCopy[i] = physx::PxVec4(testCloth.mVertices[i], 1.0f);

	auto cloth = factory->createCloth(CreateRange(particlesCopy), *fabric);
	ASSERT_PTR(cloth);

	auto clothClone = factory2->clone(*cloth);
	ASSERT_PTR(clothClone);

	NV_CLOTH_DELETE(clothClone);
	NV_CLOTH_DELETE(cloth);
	fabric->decRefCount();
	NvClothDestroyFactory(factory);
	NvClothDestroyFactory(factory2);

	delete factoryHelper1;
	if(GetParam().mPlatform1 != GetParam().mPlatform2)
		delete factoryHelper2;
}


#if USE_CUDA
#if USE_DX11
INSTANTIATE_TEST_CASE_P(PlatformPairs,
	Cloning,
	::testing::Values(	 CloningTestParameters(nv::cloth::Platform::CPU , nv::cloth::Platform::CPU)
						,CloningTestParameters(nv::cloth::Platform::CPU , nv::cloth::Platform::DX11)
						,CloningTestParameters(nv::cloth::Platform::CPU , nv::cloth::Platform::CUDA)
						,CloningTestParameters(nv::cloth::Platform::DX11, nv::cloth::Platform::CPU)
						,CloningTestParameters(nv::cloth::Platform::DX11, nv::cloth::Platform::DX11)
						//,CloningTestParameters(nv::cloth::Platform::DX11, nv::cloth::Platform::CUDA) //Not supported
						,CloningTestParameters(nv::cloth::Platform::CUDA, nv::cloth::Platform::CPU)
						//,CloningTestParameters(nv::cloth::Platform::CUDA, nv::cloth::Platform::DX11) //Not supported
						,CloningTestParameters(nv::cloth::Platform::CUDA, nv::cloth::Platform::CUDA)
		)
	);
#else
INSTANTIATE_TEST_CASE_P(PlatformPairs,
	Cloning,
	::testing::Values(CloningTestParameters(nv::cloth::Platform::CPU, nv::cloth::Platform::CPU)
		, CloningTestParameters(nv::cloth::Platform::CPU, nv::cloth::Platform::DX11)
		, CloningTestParameters(nv::cloth::Platform::CPU, nv::cloth::Platform::CUDA)
		, CloningTestParameters(nv::cloth::Platform::CUDA, nv::cloth::Platform::CPU)
		, CloningTestParameters(nv::cloth::Platform::CUDA, nv::cloth::Platform::CUDA)
		)
	);
#endif
#else
#if USE_DX11
INSTANTIATE_TEST_CASE_P(PlatformPairs,
	Cloning,
	::testing::Values(	 CloningTestParameters(nv::cloth::Platform::CPU , nv::cloth::Platform::CPU)
						,CloningTestParameters(nv::cloth::Platform::CPU , nv::cloth::Platform::DX11)
						,CloningTestParameters(nv::cloth::Platform::DX11, nv::cloth::Platform::CPU)
						,CloningTestParameters(nv::cloth::Platform::DX11, nv::cloth::Platform::DX11)
		)
	);
#else
INSTANTIATE_TEST_CASE_P(PlatformPairs,
	Cloning,
	::testing::Values(CloningTestParameters(nv::cloth::Platform::CPU, nv::cloth::Platform::CPU)
		)
	);
#endif
#endif
