#include "utilities/Utilities.h"
#include <NvCloth/Callbacks.h>

TEST(UnitTestCallbackTests,ExpectMessage)
{
	NvClothEnvironment* env = NvClothEnvironment::GetEnv();
	ErrorCallback* errorCallbackHanlder = env->GetErrorCallback();

	ExpectErrorMessage qwerty1("QWERTY", 1,physx::PxErrorCode::Enum::eDEBUG_WARNING);
	ExpectErrorMessage qwerty2("QWERTY", 1,physx::PxErrorCode::Enum::eINTERNAL_ERROR);
	ExpectErrorMessage asdf3("ASDF", 3);
	ExpectErrorMessage asdf2("ASDF", 2,physx::PxErrorCode::Enum::eABORT);
	errorCallbackHanlder->reportError(physx::PxErrorCode::Enum::eABORT, "ASDF", __FILE__, __LINE__);
	{
		ExpectErrorMessage asdf1("ASDF", 1,physx::PxErrorCode::Enum::eABORT);
		errorCallbackHanlder->reportError(physx::PxErrorCode::Enum::eABORT, "ASDF", __FILE__, __LINE__);
		errorCallbackHanlder->reportError(physx::PxErrorCode::Enum::eDEBUG_WARNING, "ASDF", __FILE__, __LINE__);
		errorCallbackHanlder->reportError(physx::PxErrorCode::Enum::eDEBUG_WARNING, "QWERTY", __FILE__, __LINE__);
		errorCallbackHanlder->reportError(physx::PxErrorCode::Enum::eINTERNAL_ERROR, "QWERTY", __FILE__, __LINE__);
	}
	errorCallbackHanlder->reportError(physx::PxErrorCode::Enum::eDEBUG_INFO, "This should print without causing a fail", __FILE__, __LINE__);

}