// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LoadDLL.h" 
#include <windows.h>
#include <string>

namespace HTML5Win32 { 

HMODULE PhysX3CommonHandle = 0;
HMODULE	PhysX3Handle = 0;
HMODULE	PhysX3CookingHandle = 0;
HMODULE	nvToolsExtHandle = 0;




void LoadANGLE( const char* EngineRoot)
{
	 LoadLibraryA ((std::string(EngineRoot)  +  std::string("/Binaries/ThirdParty/ANGLE/libGLESv2.dll")).c_str()); 
	 LoadLibraryA ((std::string(EngineRoot)  +  std::string("/Binaries/ThirdParty/ANGLE/libEGL.dll")).c_str()); 
}


void LoadPhysXDLL(const char* EngineRoot) 
{
#if _MSC_VER >= 1900
    std::string  DllRoot  =   std::string(EngineRoot)  +  std::string("/Binaries/ThirdParty/PhysX/Win32/VS2015/");
#endif
	
#if UE_BUILD_DEBUG && !defined(NDEBUG)	// Use !defined(NDEBUG) to check to see if we actually are linking with Debug third party libraries (bDebugBuildsActuallyUseDebugCRT)

	PhysX3CommonHandle = LoadLibraryA( (DllRoot +  "PhysX3CommonDEBUG_x86.dll").c_str());
	nvToolsExtHandle = LoadLibraryA((DllRoot + "nvToolsExt32_1.dll").c_str());
	PhysX3Handle = LoadLibraryA((DllRoot +  "PhysX3DEBUG_x86.dll").c_str());
	PhysX3CookingHandle = LoadLibraryA((DllRoot + "PhysX3CookingDEBUG_x86.dll").c_str());

#else

	PhysX3CommonHandle = LoadLibraryA( (DllRoot +  "PhysX3CommonPROFILE_x86.dll").c_str());
	nvToolsExtHandle = LoadLibraryA((DllRoot + "nvToolsExt32_1.dll").c_str());
	PhysX3Handle = LoadLibraryA((DllRoot +  "PhysX3PROFILE_x86.dll").c_str());
	PhysX3CookingHandle = LoadLibraryA((DllRoot + "PhysX3CookingPROFILE_x86.dll").c_str());

#endif 

}

void ShutDownPhysXDLL()
{
	 
}
void LoadOpenAL(const char* EngineRoot)
{
	std::string  DllRoot  =   std::string(EngineRoot)  +  std::string("Binaries/ThirdParty/OpenAL/OpenAL32.dll");
	LoadLibraryA( DllRoot.c_str()); 
}

}