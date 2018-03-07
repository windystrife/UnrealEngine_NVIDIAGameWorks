#include "utilities/Utilities.h"
#include <NvCloth/Factory.h>

TEST_LEAK_FIXTURE_WITH_PARAM(FactoryCreation, PlatformTestParameter)

TEST_P(FactoryCreation,Creation)
{
	ScopedFactoryHelper factoryHelper(GetParam().mPlatform);
	nv::cloth::Factory* factory = factoryHelper.CreateFactory();
	ASSERT_PTR(factory);
	NvClothDestroyFactory(factory);
}

INSTANTIATE_PLATFORM_TEST_CASE_P(FactoryCreation)