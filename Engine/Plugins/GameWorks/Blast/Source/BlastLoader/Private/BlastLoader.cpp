#include "BlastLoader.h"
#include "IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

FString GetBlastDLLPath()
{
	FString BaseDir = IPluginManager::Get().FindPlugin(TEXT("Blast"))->GetBaseDir();
	FString DllPath;
#if PLATFORM_WINDOWS && PLATFORM_64BITS
	DllPath = FPaths::Combine(BaseDir, TEXT("Libraries/Win64/"));
#elif PLATFORM_LINUX
	DllPath = FPaths::Combine(BaseDir, TEXT("Libraries/Linux/"));
#else
#error No Blast libraries for this platform
#endif
	return FPaths::ConvertRelativePathToFull(DllPath);
}

void* LoadBlastDLL(const FString& DLLPath, const TCHAR* BaseName)
{
	FString FullPath = DLLPath / BaseName;
	return FPlatformProcess::GetDllHandle(*FullPath);
}
