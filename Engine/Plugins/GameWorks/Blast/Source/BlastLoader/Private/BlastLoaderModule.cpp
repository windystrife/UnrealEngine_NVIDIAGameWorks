#include "BlastLoaderModule.h"
#include "BlastLoader.h"

#include "Paths.h"
#include "ModuleManager.h"
#include "IPluginManager.h"
#include "Logging/LogMacros.h"

IMPLEMENT_MODULE(FBlastLoaderModule, BlastLoader);

DEFINE_LOG_CATEGORY_STATIC(LogBlastLoader, Log, All);

FBlastLoaderModule::FBlastLoaderModule():
	BlastHandle(nullptr),
	BlastGlobalsHandle(nullptr),
	BlastExtSerializationHandle(nullptr),
	BlastExtShadersHandle(nullptr),
	BlastExtStressHandle(nullptr)
{
}

void FBlastLoaderModule::StartupModule()
{
	FString DllPath = GetBlastDLLPath();

	BlastHandle = LoadBlastDLL(DllPath, TEXT(BLAST_LIB_DLL_PREFIX "NvBlast" BLAST_LIB_CONFIG_STRING BLAST_LIB_DLL_SUFFIX));
	BlastGlobalsHandle = LoadBlastDLL(DllPath, TEXT(BLAST_LIB_DLL_PREFIX "NvBlastGlobals"  BLAST_LIB_CONFIG_STRING BLAST_LIB_DLL_SUFFIX));
	BlastExtSerializationHandle = LoadBlastDLL(DllPath, TEXT(BLAST_LIB_DLL_PREFIX "NvBlastExtSerialization"  BLAST_LIB_CONFIG_STRING BLAST_LIB_DLL_SUFFIX));
	BlastExtShadersHandle = LoadBlastDLL(DllPath, TEXT(BLAST_LIB_DLL_PREFIX "NvBlastExtShaders" BLAST_LIB_CONFIG_STRING BLAST_LIB_DLL_SUFFIX));
	BlastExtStressHandle = LoadBlastDLL(DllPath, TEXT(BLAST_LIB_DLL_PREFIX "NvBlastExtStress" BLAST_LIB_CONFIG_STRING BLAST_LIB_DLL_SUFFIX));

	if (BlastHandle == nullptr)
	{
		UE_LOG(LogBlastLoader, Error, TEXT("Failed to load the Blast DLL at %s"), *DllPath);
		return;
	}

	if (BlastGlobalsHandle == nullptr)
	{
		UE_LOG(LogBlastLoader, Error, TEXT("Failed to load the Blast Globals DLL at %s"), *DllPath);
		return;
	}

	if (BlastExtSerializationHandle == nullptr)
	{
		UE_LOG(LogBlastLoader, Error, TEXT("Failed to load the Blast serialization dll at %s"), *DllPath);
		return;
	}

	if (BlastExtShadersHandle == nullptr)
	{
		UE_LOG(LogBlastLoader, Error, TEXT("Failed to load the Blast Damage Shaders dll at %s"), *DllPath);
		return;
	}

	if (BlastExtStressHandle == nullptr)
	{
		UE_LOG(LogBlastLoader, Error, TEXT("Failed to load the Blast Damage Stress dll at %s"), *DllPath);
		return;
	}

}

void FBlastLoaderModule::ShutdownModule()
{
	if (BlastHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(BlastHandle);
		BlastHandle = nullptr;
	}

	if (BlastGlobalsHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(BlastGlobalsHandle);
		BlastGlobalsHandle = nullptr;
}

	if (BlastExtSerializationHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(BlastExtSerializationHandle);
		BlastExtSerializationHandle = nullptr;
	}

	if (BlastExtShadersHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(BlastExtShadersHandle);
		BlastExtShadersHandle = nullptr;
	}

	if (BlastExtStressHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(BlastExtStressHandle);
		BlastExtStressHandle = nullptr;
	}
}
