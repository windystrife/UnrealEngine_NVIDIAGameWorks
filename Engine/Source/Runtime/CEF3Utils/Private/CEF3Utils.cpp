// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CEF3Utils.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "CEF3UtilsLog.h"

DEFINE_LOG_CATEGORY(LogCEF3Utils);

IMPLEMENT_MODULE(FDefaultModuleImpl, CEF3Utils);

#if WITH_CEF3
namespace CEF3Utils
{
#if PLATFORM_WINDOWS
    void* CEF3DLLHandle = nullptr;
	void* ElfHandle = nullptr;
	void* D3DHandle = nullptr;
	void* GLESHandle = nullptr;
    void* EGLHandle = nullptr;
#endif

	void* LoadDllCEF(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		void* Handle = FPlatformProcess::GetDllHandle(*Path);
		if (!Handle)
		{
			int32 ErrorNum = FPlatformMisc::GetLastError();
			TCHAR ErrorMsg[1024];
			FPlatformMisc::GetSystemErrorMessage(ErrorMsg, 1024, ErrorNum);
			UE_LOG(LogCEF3Utils, Error, TEXT("Failed to get CEF3 DLL handle for %s: %s (%d)"), *Path, ErrorMsg, ErrorNum);
		}
		return Handle;
	}

	void LoadCEF3Modules()
	{
#if PLATFORM_WINDOWS
	#if PLATFORM_64BITS
		FString DllPath(FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries/ThirdParty/CEF3/Win64")));
	#else
		FString DllPath(FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries/ThirdParty/CEF3/Win32")));
	#endif

		FPlatformProcess::PushDllDirectory(*DllPath);
		CEF3DLLHandle = LoadDllCEF(FPaths::Combine(*DllPath, TEXT("libcef.dll")));
		if (CEF3DLLHandle)
		{
			ElfHandle = LoadDllCEF(FPaths::Combine(*DllPath, TEXT("chrome_elf.dll")));

	#if WINVER >= 0x600 // Different dll used pre-Vista
			D3DHandle = LoadDllCEF(FPaths::Combine(*DllPath, TEXT("d3dcompiler_47.dll")));
	#else
			D3DHandle = LoadDllCEF(FPaths::Combine(*DllPath, TEXT("d3dcompiler_43.dll")));
	#endif
			GLESHandle = LoadDllCEF(FPaths::Combine(*DllPath, TEXT("libGLESv2.dll")));
			EGLHandle = LoadDllCEF(FPaths::Combine(*DllPath, TEXT("libEGL.dll")));
		}
		FPlatformProcess::PopDllDirectory(*DllPath);
#endif
	}

	void UnloadCEF3Modules()
	{
#if PLATFORM_WINDOWS
		FPlatformProcess::FreeDllHandle(CEF3DLLHandle);
		CEF3DLLHandle = nullptr;
		FPlatformProcess::FreeDllHandle(ElfHandle);
		ElfHandle = nullptr;
		FPlatformProcess::FreeDllHandle(D3DHandle);
		D3DHandle = nullptr;
		FPlatformProcess::FreeDllHandle(GLESHandle);
		GLESHandle = nullptr;
		FPlatformProcess::FreeDllHandle(EGLHandle);
		EGLHandle = nullptr;
#endif
	}
};
#endif //WITH_CEF3
