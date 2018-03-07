#include "utilities/Utilities.h"
#include <NvCloth/Factory.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Cloth.h>
#include <NvCloth/Range.h>
#include "utilities/ClothMeshGenerator.h"

TEST_LEAK_FIXTURE_WITH_PARAM(ClothCreation, PlatformTestParameter)

TEST_P(ClothCreation,Creation)
{
	ScopedFactoryHelper factoryHelper(GetParam().mPlatform);
	nv::cloth::Factory* factory = factoryHelper.CreateFactory();
	ASSERT_PTR(factory);

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
	EXPECT_EQ(cloth->getNumParticles(), particlesCopy.size());
	EXPECT_EQ(cloth->getCurrentParticles().size(), particlesCopy.size());

	fabric->decRefCount(); //Cloth will cause fabric to be deleted once the refcount reaches 0
	NV_CLOTH_DELETE(cloth);
	NvClothDestroyFactory(factory);
}

INSTANTIATE_PLATFORM_TEST_CASE_P(ClothCreation)
