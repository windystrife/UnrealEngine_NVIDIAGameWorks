#include "BlastLoaderEditorModule.h"
#include "BlastLoader.h"

#include "Paths.h"
#include "ModuleManager.h"
#include "Logging/LogMacros.h"

IMPLEMENT_MODULE(FBlastLoaderEditorModule, BlastLoaderEditor);

DEFINE_LOG_CATEGORY_STATIC(LogBlastLoaderEditor, Log, All);

FBlastLoaderEditorModule::FBlastLoaderEditorModule() :
	BlastExtAssetUtilsHandle(nullptr),
	BlastExtAuthoringHandle(nullptr)
{
}

void FBlastLoaderEditorModule::StartupModule()
{
	FString DllPath = GetBlastDLLPath();

	BlastExtAssetUtilsHandle = LoadBlastDLL(DllPath, TEXT(BLAST_LIB_DLL_PREFIX "NvBlastExtAssetUtils" BLAST_LIB_CONFIG_STRING BLAST_LIB_DLL_SUFFIX));
	BlastExtAuthoringHandle = LoadBlastDLL(DllPath, TEXT(BLAST_LIB_DLL_PREFIX "NvBlastExtAuthoring" BLAST_LIB_CONFIG_STRING BLAST_LIB_DLL_SUFFIX));


	if (BlastExtAssetUtilsHandle == nullptr)
	{
		UE_LOG(LogBlastLoaderEditor, Error, TEXT("Failed to load the NvBlastExtAssetUtils DLL at %s"), *DllPath);
		return;
	}

	if (BlastExtAuthoringHandle == nullptr)
	{
		UE_LOG(LogBlastLoaderEditor, Error, TEXT("Failed to load the NvBlastExtAuthoring DLL at %s"), *DllPath);
		return;
	}
}

void FBlastLoaderEditorModule::ShutdownModule()
{
	if (BlastExtAssetUtilsHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(BlastExtAssetUtilsHandle);
		BlastExtAssetUtilsHandle = nullptr;
	}

	if (BlastExtAuthoringHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(BlastExtAuthoringHandle);
		BlastExtAuthoringHandle = nullptr;
	}
}
