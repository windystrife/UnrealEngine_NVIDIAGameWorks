#include "utilities/Utilities.h"
#include <NvCloth/Factory.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Cloth.h>
#include <NvCloth/Range.h>
#include <NvCloth/Solver.h>
#include "utilities/ClothMeshGenerator.h"
#include <PsFoundation.h>
#include <foundation/PxMat44.h>
#include "utilities/SimulationFixture.h"
#define NOMINMAX
#ifdef _MSC_VER
#pragma warning(disable : 4668 4917 4365 4061 4005)
#if NV_XBOXONE
#include <d3d11_x.h>
#else
#include <d3d11.h>
#endif
#endif

TEST_P(Simulation,GravityAnchoredPoint)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();

	ASSERT_TRUE(SetupTestCloth(10.0f, 2, ANCHOR_PARTICLE_TOP_LEFT,physx::PxMat44(physx::PxMat33(physx::PxIdentity),physx::PxVec3(5.0f,0.0f,5.0f))));

	mCloth->setGravity(physx::PxVec3(0.0f, -9.8f, 0.0f));
	mCloth->setDamping(physx::PxVec3(0.1f,0.1f,0.1f));

	mSolver->addCloth(mCloth);
	
	for(int i = 0; i < 1000; i++)
	{
		mSolver->beginSimulation(1.0f / 60.0f);
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSolver->simulateChunk(j);
		mSolver->endSimulation();
	}

	{
		// Length of cloth diagonal
		// ||{10, 10}|| = 14.14

		nv::cloth::MappedRange<physx::PxVec4> particles = mCloth->getCurrentParticles(); //inside scope so it won't outlive the cloth

		/*for(unsigned int i = 0; i < mCloth->getNumParticles(); i++)
		{
			printf("[%d]=%.2f %.2f %.2f\n", i, particles[i].x, particles[i].y, particles[i].z);
		}*/

		//bottom end
		EXPECT_LE(particles[8].y, -14.14f+0.5f);
		EXPECT_GE(particles[8].y, -14.14f-0.5f);

		//middle
		EXPECT_LE(particles[4].y, (-14.14f+0.5f)*0.5f);
		EXPECT_GE(particles[4].y, (-14.14f-0.5f)*0.5f);

		//anchor point
		EXPECT_FLOAT_EQ(particles[0].y, 0.0f);
	}

	mSolver->removeCloth(mCloth);
}

TEST_P(Simulation, Wind)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();

	ASSERT_TRUE(SetupTestCloth(10.0f, 2, ANCHOR_PARTICLE_TOP_LEFT, physx::PxMat44(physx::PxMat33(physx::PxIdentity), physx::PxVec3(5.0f, 0.0f, 5.0f))));

	mCloth->setGravity(physx::PxVec3(0.0f, -9.8f, 0.0f));
	mCloth->setDamping(physx::PxVec3(0.1f, 0.1f, 0.1f));

	//One of these lines is enough to enable the applyWind part of the kernel to run.
	mCloth->setDragCoefficient(0.05f);
	mCloth->setLiftCoefficient(0.1f);

	mSolver->addCloth(mCloth);

	for(int i = 0; i < 1000; i++)
	{
		mSolver->beginSimulation(1.0f / 60.0f);
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSolver->simulateChunk(j);
		mSolver->endSimulation();
	}

	{
		nv::cloth::MappedRange<physx::PxVec4> particles = mCloth->getCurrentParticles(); //inside scope so it won't outlive the cloth

		//check values?
	}

	mSolver->removeCloth(mCloth);
}

TEST_P(Simulation, SinglePlane)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();

	SetupTestCloth(10, 2, 0);

	mCloth->setGravity(physx::PxVec3(0.0f, -9.8f, 10.0f));

	mSolver->addCloth(mCloth);

	physx::PxVec4 plane(0.0f, 1.0f, 0.0f, 0.0f);
	mCloth->setPlanes(nv::cloth::Range<physx::PxVec4>(&plane, &plane + 1),0,mCloth->getNumPlanes());
	uint32_t mask = 1 << 0;
	mCloth->setConvexes(nv::cloth::Range<uint32_t>(&mask, &mask + 1), 0, mCloth->getNumConvexes());

	for(int i = 0; i < 200; i++)
	{
		mSolver->beginSimulation(1.0f / 60.0f);
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSolver->simulateChunk(j);
		mSolver->endSimulation();
	}

	{
		nv::cloth::MappedRange<physx::PxVec4> particles = mCloth->getCurrentParticles(); //inside scope so it won't outlive the cloth

		for(unsigned int i = 0; i < mCloth->getNumParticles(); i++)
		{
			EXPECT_GE(particles[i].y, -10.0f);
			//printf("[%d]=%.2f %.2f %.2f\n", i, particles[i].x, particles[i].y, particles[i].z);
		}
	}

	mSolver->removeCloth(mCloth);
}

TEST_P(Simulation, SinglePlaneMask)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();

	SetupTestCloth(10, 2, 0);

	mCloth->setGravity(physx::PxVec3(0.0f, -9.8f, 10.0f));

	mSolver->addCloth(mCloth);

	physx::PxVec4 planes[4];
	planes[0] = physx::PxVec4(0.0f, 1.0f, 0.0f, 10.0f);
	planes[1] = physx::PxVec4(0.0f, 1.0f, 0.0f, 10.0f);
	planes[2] = physx::PxVec4(0.0f, 1.0f, 0.0f, 30.0f);
	planes[3] = physx::PxVec4(0.0f, 1.0f, 0.0f, 10.0f);
	mCloth->setPlanes(nv::cloth::Range<physx::PxVec4>(planes, planes + 4), 0, mCloth->getNumPlanes());
	uint32_t mask = 3 << 0;
	mCloth->setConvexes(nv::cloth::Range<uint32_t>(&mask, &mask + 1), 0, mCloth->getNumConvexes());

	for(int i = 0; i < 200; i++)
	{
		mSolver->beginSimulation(1.0f / 60.0f);
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSolver->simulateChunk(j);
		mSolver->endSimulation();
	}

	{
		nv::cloth::MappedRange<physx::PxVec4> particles = mCloth->getCurrentParticles(); //inside scope so it won't outlive the cloth

		for(unsigned int i = 0; i < mCloth->getNumParticles(); i++)
		{
			EXPECT_GE(particles[i].y, -30.0f);
			EXPECT_LE(particles[i].y, -10.0f);
			//printf("[%d]=%.2f %.2f %.2f\n", i, particles[i].x, particles[i].y, particles[i].z);
		}
	}

	mSolver->removeCloth(mCloth);
}

TEST_P(Simulation, 32PlaneConvex)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();

	SetupTestCloth(10, 2, 0);

	mCloth->setGravity(physx::PxVec3(0.0f, -9.8f, 10.0f));

	mSolver->addCloth(mCloth);

	physx::PxVec4 planes[32];
	for(int i = 0; i < 32; i++)
	{
		planes[i] = physx::PxVec4(physx::PxVec3((float)i / 128.0f, 1.0f, 0.0f).getNormalized(), 2);
	}
	mCloth->setPlanes(nv::cloth::Range<physx::PxVec4>(planes, planes + 32), 0, mCloth->getNumPlanes());
	uint32_t mask = 0xFFFFFFFF;
	mCloth->setConvexes(nv::cloth::Range<uint32_t>(&mask, &mask + 1), 0, mCloth->getNumConvexes());

	for(int i = 0; i < 200; i++)
	{
		mSolver->beginSimulation(1.0f / 60.0f);
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSolver->simulateChunk(j);
		mSolver->endSimulation();
	}

	mSolver->removeCloth(mCloth);
}

TEST_P(Simulation, 32Planes)
{
	ASSERT_TRUE(SetupSolver(GetParam().mPlatform)) << GetParam();

	SetupTestCloth(10, 2, 0);

	mCloth->setGravity(physx::PxVec3(0.0f, -9.8f, 10.0f));

	mSolver->addCloth(mCloth);

	physx::PxVec4 planes[32];
	for(int i = 0; i < 32; i++)
	{
		planes[i] = physx::PxVec4(physx::PxVec3((float)i / 128.0f, 1.0f, 0.0f).getNormalized(), 2);
	}
	mCloth->setPlanes(nv::cloth::Range<physx::PxVec4>(planes, planes + 32), 0, mCloth->getNumPlanes());
	uint32_t masks[32];
	for(int i = 0; i < 32; i++)
	{
		masks[i] = 1 << i;
	}
	mCloth->setConvexes(nv::cloth::Range<uint32_t>(masks, masks + 32), 0, mCloth->getNumConvexes());

	for(int i = 0; i < 200; i++)
	{
		mSolver->beginSimulation(1.0f / 60.0f);
		for(int j = 0; j < mSolver->getSimulationChunkCount(); j++)
			mSolver->simulateChunk(j);
		mSolver->endSimulation();
	}

	mSolver->removeCloth(mCloth);
}

INSTANTIATE_PLATFORM_TEST_CASE_P(Simulation)