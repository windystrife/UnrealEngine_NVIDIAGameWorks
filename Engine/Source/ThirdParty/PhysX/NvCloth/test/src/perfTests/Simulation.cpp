#include "utilities/Utilities.h"
#include <NvCloth/Factory.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Cloth.h>
#include <NvCloth/Range.h>
#include <NvCloth/Solver.h>
#include "utilities/ClothMeshGenerator.h"
#include <PsFoundation.h>
#include <foundation/PxMat44.h>
#define NOMINMAX
#ifdef _MSC_VER
#pragma warning(disable : 4668 4917 4365 4061 4005)
#if NV_XBOXONE
#include <d3d11_x.h>
#else
#include <d3d11.h>
#endif
#endif

#include "utilities/SimulationFixture.h"

#include "utilities/PerformanceTimer.h"

TEST_P(Simulation,SimpleCloth)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();

	ASSERT_TRUE(SetupTestCloth(10.0f, 200, ANCHOR_PARTICLE_TOP_LEFT,physx::PxMat44(physx::PxMat33(physx::PxIdentity),physx::PxVec3(5.0f,0.0f,5.0f))));

	mCloth->setGravity(physx::PxVec3(0.0f, -9.8f, 0.0f));
	mCloth->setDamping(physx::PxVec3(0.1f,0.1f,0.1f));

	mSolver->addCloth(mCloth);

	PerformanceTimer timer;
	timer.Begin(1000);
	for(int i = 0; i < 1000; i++)
	{
		mSolver->beginSimulation(1.0f / 60.0f);
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSolver->simulateChunk(j);
		mSolver->endSimulation();

		timer.FrameEnd();
	}
	timer.End();

	mSolver->removeCloth(mCloth);
}

TEST_P(Simulation, SimpleClothTriangles)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();

	ASSERT_TRUE(SetupTestCloth(10.0f, 200, ANCHOR_PARTICLE_TOP_LEFT, physx::PxMat44(physx::PxMat33(physx::PxIdentity), physx::PxVec3(5.0f, 0.0f, 5.0f)),false));

	mCloth->setGravity(physx::PxVec3(0.0f, -9.8f, 0.0f));
	mCloth->setDamping(physx::PxVec3(0.1f, 0.1f, 0.1f));

	mSolver->addCloth(mCloth);

	PerformanceTimer timer;
	timer.Begin(1000);
	for(int i = 0; i < 1000; i++)
	{
		mSolver->beginSimulation(1.0f / 60.0f);
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSolver->simulateChunk(j);
		mSolver->endSimulation();

		timer.FrameEnd();
	}
	timer.End();

	mSolver->removeCloth(mCloth);
}

TEST_P(Simulation, Wind)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();

	ASSERT_TRUE(SetupTestCloth(10.0f, 200, ANCHOR_PARTICLE_TOP_LEFT, physx::PxMat44(physx::PxMat33(physx::PxIdentity), physx::PxVec3(5.0f, 0.0f, 5.0f))));

	mCloth->setGravity(physx::PxVec3(0.0f, -9.8f, 0.0f));
	mCloth->setDamping(physx::PxVec3(0.1f, 0.1f, 0.1f));

	//One of these lines is enough to enable the applyWind part of the kernel to run.
	mCloth->setDragCoefficient(0.05f);
	mCloth->setLiftCoefficient(0.1f);

	mSolver->addCloth(mCloth);

	PerformanceTimer timer;
	timer.Begin(1000);
	for(int i = 0; i < 1000; i++)
	{
		mSolver->beginSimulation(1.0f / 60.0f);
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSolver->simulateChunk(j);
		mSolver->endSimulation();

		timer.FrameEnd();
	}
	timer.End();

	mSolver->removeCloth(mCloth);
}

TEST_P(Simulation, SinglePlane)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();

	SetupTestCloth(10.0f, 200, 0);

	mCloth->setGravity(physx::PxVec3(0.0f, -9.8f, 10.0f));

	mSolver->addCloth(mCloth);

	physx::PxVec4 plane(0.0f, 1.0f, 0.0f, 0.0f);
	mCloth->setPlanes(nv::cloth::Range<physx::PxVec4>(&plane, &plane + 1),0,mCloth->getNumPlanes());
	uint32_t mask = 1 << 0;
	mCloth->setConvexes(nv::cloth::Range<uint32_t>(&mask, &mask + 1), 0, mCloth->getNumConvexes());

	PerformanceTimer timer;
	timer.Begin(1000);
	for(int i = 0; i < 200; i++)
	{
		mSolver->beginSimulation(1.0f / 60.0f);
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSolver->simulateChunk(j);
		mSolver->endSimulation();

		timer.FrameEnd();
	}
	timer.End();

	mSolver->removeCloth(mCloth);
}

//Xbox doesn't have enough memory for these tests
#if !NV_XBOXONE
TEST_P(Simulation, SmallCloth1000)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();
	std::vector<physx::PxVec4> particlesCopy;
	mFabric = CreateTestFabric(mFactory, 10.0f, 50, ANCHOR_PARTICLE_TOP_LEFT, physx::PxMat44(physx::PxMat33(physx::PxIdentity), physx::PxVec3(5.0f, 0.0f, 5.0f)), &particlesCopy);
	nv::cloth::Cloth* cloths[1000];

	JobManager jobManager;
	jobManager.ParallelLoop<1000>([this,&cloths,&particlesCopy](int i)
	{
		cloths[i] = CreateTestCloth(mFactory, mFabric, &particlesCopy);

		cloths[i]->setGravity(physx::PxVec3(0.0f, -9.8f, 0.0f));
		cloths[i]->setDamping(physx::PxVec3(0.1f, 0.1f, 0.1f));
	}
	);

	for(int i = 0; i < 1000; i++)
		mSolver->addCloth(cloths[i]);
	
	MultithreadedSolverHelper solverHelper;
	solverHelper.Initialize(mSolver, &jobManager);

	PerformanceTimer timer;
	timer.Begin(1000);
	for(int i = 0; i < 1000; i++)
	{
		solverHelper.StartSimulation(1.0f / 60.0f);
		solverHelper.WaitForSimulation();

		timer.FrameEnd();
	}
	timer.End();

	for(int i = 0; i < 1000; i++)
	{
		mSolver->removeCloth(cloths[i]);
		NV_CLOTH_DELETE(cloths[i]);
		//ASSERT_EQ(_HEAPOK,_heapchk());
		mFactoryHelper->FlushDevice(); //Trigger memory clean up for GPU devices
	}

	mFabric->decRefCount();
}

#if !NV_ORBIS
TEST_P(Simulation, SmallCloth1000In100Solvers)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();

	JobManager jobManager;

	nv::cloth::Solver* solvers[100];
	MultithreadedSolverHelper solverHelpers[100];
	nv::cloth::Cloth* cloths[1000];
	jobManager.ParallelLoop<100>([this, &solvers, &solverHelpers, &jobManager](int i)
	{
		solvers[i] = mFactory->createSolver();
		solverHelpers[i].Initialize(solvers[i], &jobManager);
	});

	std::vector<physx::PxVec4> particlesCopy;
	mFabric = CreateTestFabric(mFactory, 10.0f, 50, ANCHOR_PARTICLE_TOP_LEFT, physx::PxMat44(physx::PxMat33(physx::PxIdentity), physx::PxVec3(5.0f, 0.0f, 5.0f)), &particlesCopy);

	jobManager.ParallelLoop<1000>([this, &cloths, &particlesCopy](int i)
	{

		cloths[i] = CreateTestCloth(mFactory, mFabric, &particlesCopy);

		cloths[i]->setGravity(physx::PxVec3(0.0f, -9.8f, 0.0f));
		cloths[i]->setDamping(physx::PxVec3(0.1f, 0.1f, 0.1f));
	});

	for(int i = 0; i < 1000; i++)
	{
		solvers[i/10]->addCloth(cloths[i]);
	}

	PerformanceTimer timer;
	timer.Begin(1000);
	for(int j = 0; j < 1000; j++)
	{	
		for(int i = 0; i < 100; i++)
			solverHelpers[i].StartSimulation(1.0f / 60.0f);

		for(int i = 0; i < 100; i++)
			solverHelpers[i].WaitForSimulation();

		timer.FrameEnd();
	}
	timer.End();

	for(int i = 0; i < 1000; i++)
	{
		solvers[i/10]->removeCloth(cloths[i]);
		delete cloths[i];
		
		mFactoryHelper->FlushDevice(); //Trigger memory clean up for GPU devices
	}
	for(int i = 0; i < 100; i++)
		delete solvers[i];

	mFabric->decRefCount();
}
#endif
#endif

INSTANTIATE_PLATFORM_TEST_CASE_P(Simulation)