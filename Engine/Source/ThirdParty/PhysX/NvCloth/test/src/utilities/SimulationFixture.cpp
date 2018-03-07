#include "SimulationFixture.h"
#include <NvCloth/Range.h>
#include "utilities/ClothMeshGenerator.h"
Simulation::Simulation()
{
	mFactoryHelper = nullptr;
	mFactory = nullptr;
	mSolver = nullptr;
	mCloth = nullptr;
	mFabric = nullptr;
}

void Simulation::SetUp()
{
	Super::SetUp();
}

void Simulation::TearDown()
{
	NV_CLOTH_DELETE(mCloth);
	NV_CLOTH_DELETE(mSolver);
	if(mFactory)
	NvClothDestroyFactory(mFactory);
			
	delete mFactoryHelper;

	Super::TearDown();
}


bool Simulation::SetupSolver(nv::cloth::Platform platform)
{
	mFactoryHelper = FactoryHelper::CreateFactoryHelper(platform);
	EXPECT_PTR(mFactoryHelper);
	if(mFactoryHelper == 0)
		return false;
	mFactory = mFactoryHelper->CreateFactory();
	EXPECT_PTR(mFactory);
	if(mFactory == 0)
		return false;
	mSolver = mFactory->createSolver();
	EXPECT_PTR(mSolver);
	return mSolver != nullptr;
}

nv::cloth::Fabric* Simulation::CreateTestFabric(nv::cloth::Factory* factory, float size, int segments, int particleLockFlags, physx::PxMat44 transform, std::vector<physx::PxVec4>* particlesCopyOut, bool generateQuads)
{
	EXPECT_PTR(factory);
	if(!factory)
		return nullptr;
	ClothMeshData testCloth;
	testCloth.GeneratePlaneCloth(size, size, segments, segments, generateQuads);
	auto meshDesc = testCloth.GetClothMeshDesc();
	EXPECT_TRUE(meshDesc.isValid());
	if(!meshDesc.isValid())
		return nullptr;

	nv::cloth::Fabric* fabric = NvClothCookFabricFromMesh(factory, meshDesc, physx::PxVec3(0.0f, 0.0f, -1.0f), nullptr, false);
	EXPECT_PTR(fabric);

	particlesCopyOut->resize(testCloth.mVertices.size());
	for(int i = 0; i < testCloth.mVertices.size(); i++)
	{
		(*particlesCopyOut)[i] = physx::PxVec4(transform.transform(testCloth.mVertices[i]), 1.0f);
	}

	if(particleLockFlags & ANCHOR_PARTICLE_TOP_LEFT)
	{
		(*particlesCopyOut)[0] = physx::PxVec4((*particlesCopyOut)[0].getXYZ(), 0.0f);
	}

	return fabric;

}
nv::cloth::Cloth* Simulation::CreateTestCloth(nv::cloth::Factory* factory, nv::cloth::Fabric* fabric, std::vector<physx::PxVec4> const * particlesCopy)
{
	nv::cloth::Cloth* cloth = factory->createCloth(CreateConstRange(*particlesCopy), *fabric);

	std::vector<nv::cloth::PhaseConfig> phases(fabric->getNumPhases());
	for(int i = 0; i < phases.size(); i++)
	{
		phases[i].mPhaseIndex = (uint16_t)i;
	}

	cloth->setPhaseConfig(CreateRange(phases));

	return cloth;

}

nv::cloth::Cloth* Simulation::CreateTestCloth(nv::cloth::Factory* factory, float size, int segments, int particleLockFlags, physx::PxMat44 transform, nv::cloth::Fabric** fabricOut, bool generateQuads)
{
	std::vector<physx::PxVec4> particlesCopy;
	nv::cloth::Fabric* fabric = CreateTestFabric(factory, size, segments, particleLockFlags, transform, &particlesCopy, generateQuads);
	if(fabricOut)
	{
		*fabricOut = fabric;
	}
	nv::cloth::Cloth* cloth = CreateTestCloth(factory, fabric, &particlesCopy);
	fabric->decRefCount(); //Only cloth has a reference, so the caller doesn't need to worry about freeing fabric.
	return cloth;
}

bool Simulation::SetupTestCloth(float size, int segments, int particleLockFlags, physx::PxMat44 transform, bool generateQuads)
{
	EXPECT_PTR(mFactory);
	EXPECT_NULLPTR(mFabric);
	EXPECT_NULLPTR(mCloth);
	if(!mFactory || mFabric || mCloth)
		return false;
	mCloth = CreateTestCloth(mFactory, size, segments, particleLockFlags, transform, &mFabric, generateQuads);
	return mCloth != nullptr;
}
