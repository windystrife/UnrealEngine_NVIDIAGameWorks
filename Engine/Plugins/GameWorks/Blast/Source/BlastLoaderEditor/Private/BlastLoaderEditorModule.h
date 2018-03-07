#pragma once
#include "CoreMinimal.h"

#include "ModuleInterface.h"

class FBlastLoaderEditorModule : public IModuleInterface
{
public:
	FBlastLoaderEditorModule();
	virtual ~FBlastLoaderEditorModule() = default;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	void*	BlastExtAssetUtilsHandle;
	void*	BlastExtAuthoringHandle;
};
