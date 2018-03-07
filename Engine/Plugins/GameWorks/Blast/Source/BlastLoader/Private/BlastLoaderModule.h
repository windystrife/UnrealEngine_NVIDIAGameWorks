#pragma once
#include "CoreMinimal.h"

#include "ModuleInterface.h"

//Since we cannot use delay loading on all platforms we need a simple module to load the DLLs which others can depend on, which itself *does not* link them.
//This doesn't work for monolithic builds, but luckily we can set LD_LIBRARY_PATH in the wrapper script which is generated.
//Blast modules only used by the Editor are in BlastLoaderEditor
class FBlastLoaderModule : public IModuleInterface
{
public:
	FBlastLoaderModule();
	virtual ~FBlastLoaderModule() = default;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	void*	BlastHandle;
	void*	BlastGlobalsHandle;
	void*	BlastExtSerializationHandle;
	void*	BlastExtShadersHandle;
	void*	BlastExtStressHandle;
};
