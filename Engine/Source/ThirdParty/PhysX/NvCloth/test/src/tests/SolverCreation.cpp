#include "utilities/Utilities.h"
#include <NvCloth/Factory.h>
#include <NvCloth/Solver.h>
#include "utilities/ClothMeshGenerator.h"

TEST_LEAK_FIXTURE_WITH_PARAM(SolverCreation, PlatformTestParameter)

TEST_P(SolverCreation,Creation)
{
	ScopedFactoryHelper factoryHelper(GetParam().mPlatform);
	nv::cloth::Factory* factory = factoryHelper.CreateFactory();
	ASSERT_PTR(factory);
	nv::cloth::Solver* cpuSolver = factory->createSolver();
	ASSERT_PTR(cpuSolver);
	delete cpuSolver;
	NvClothDestroyFactory(factory);
}

INSTANTIATE_PLATFORM_TEST_CASE_P(SolverCreation)