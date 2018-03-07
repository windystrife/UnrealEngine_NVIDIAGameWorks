#pragma once
#include "utilities/Utilities.h"
#include <NvCloth/Factory.h>
#include <NvCloth/Fabric.h>
#include <NvCloth/Cloth.h>
#include <NvCloth/Solver.h>
#include <foundation/PxMat44.h>

class Simulation :	public TestLeakDetectorWithParam<PlatformTestParameter>
{
	public:
		typedef TestLeakDetectorWithParam<PlatformTestParameter> Super;
		Simulation();
		virtual void SetUp() override;
		virtual void TearDown() override;
		bool SetupSolver(nv::cloth::Platform platform);

		enum
		{
			ANCHOR_PARTICLE_TOP_LEFT = 1<<0
		};

		nv::cloth::Fabric* CreateTestFabric(nv::cloth::Factory* factory, float size, int segments, int particleLockFlags, physx::PxMat44 transform, std::vector<physx::PxVec4>* particlesCopyOut, bool generateQuads = true);
		nv::cloth::Cloth* CreateTestCloth(nv::cloth::Factory* factory, nv::cloth::Fabric* fabric, std::vector<physx::PxVec4> const* particlesCopy);
		nv::cloth::Cloth* CreateTestCloth(nv::cloth::Factory* factory, float size, int segments, int particleLockFlags, physx::PxMat44 transform, nv::cloth::Fabric** fabricOut = nullptr, bool generateQuads = true);
		bool SetupTestCloth(float size, int segments, int particleLockFlags, physx::PxMat44 transform = physx::PxIdentity, bool generateQuads = true);

		FactoryHelper* mFactoryHelper;
		nv::cloth::Factory* mFactory;
		nv::cloth::Solver* mSolver;

		nv::cloth::Fabric* mFabric;
		nv::cloth::Cloth* mCloth;
};
