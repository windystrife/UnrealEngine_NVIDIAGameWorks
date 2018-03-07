//
// Copyright 2016 Sensics Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "CoreMinimal.h"
#include "OSVRPrivate.h"
#include "InputCoreTypes.h"
#include "GameFramework/InputSettings.h"

#include "OSVREntryPoint.h"

#include "OSVRHMD.h"

DEFINE_LOG_CATEGORY(OSVRLog);

class FOSVR : public IOSVR
{
private:
    TSharedPtr<FOSVRHMD, ESPMode::ThreadSafe> hmd;
    FCriticalSection mModuleMutex;
    TSharedPtr<class OSVREntryPoint, ESPMode::ThreadSafe> EntryPoint;
    bool bModulesLoaded = false;
    bool bHmdCreationAttempted = false;
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** IHeadMountedDisplayModule implementation */
	FString GetModuleKeyName() const override
	{
		return FString(TEXT("OSVR"));
	}
    virtual TSharedPtr<class IXRTrackingSystem, ESPMode::ThreadSafe> CreateTrackingSystem() override;
#if OSVR_UNREAL_4_12
    virtual bool IsHMDConnected() override;
#endif

    // Pre-init the HMD module (optional).
    //virtual void PreInit() override;

    virtual TSharedPtr<OSVREntryPoint, ESPMode::ThreadSafe> GetEntryPoint() override;
    virtual TSharedPtr<FOSVRHMD, ESPMode::ThreadSafe> GetHMD() override;
    virtual void LoadOSVRClientKitModule() override;
};

IMPLEMENT_MODULE(FOSVR, OSVR)

TSharedPtr<class OSVREntryPoint, ESPMode::ThreadSafe> FOSVR::GetEntryPoint()
{
    return EntryPoint;
}

TSharedPtr<FOSVRHMD, ESPMode::ThreadSafe> FOSVR::GetHMD()
{
    if (bHmdCreationAttempted)
    {
        return hmd;
    }
    if (EntryPoint->IsOSVRConnected())
    {
        TSharedPtr< FOSVRHMD, ESPMode::ThreadSafe > OSVRHMD(new FOSVRHMD(EntryPoint));
        bHmdCreationAttempted = true;
        if (OSVRHMD->IsInitialized() && OSVRHMD->IsHMDConnected())
        {
            hmd = OSVRHMD;
            return OSVRHMD;
        }
    }
    return nullptr;
}

void FOSVR::LoadOSVRClientKitModule()
{
    FScopeLock lock(&mModuleMutex);
    if (!bModulesLoaded)
    {
#if PLATFORM_WINDOWS
        TArray<FString> osvrDlls;
        osvrDlls.Add(FString("osvrClientKit.dll"));
        osvrDlls.Add(FString("osvrClient.dll"));
        osvrDlls.Add(FString("osvrCommon.dll"));
        osvrDlls.Add(FString("osvrUtil.dll"));
        osvrDlls.Add(FString("osvrRenderManager.dll"));
        osvrDlls.Add(FString("d3dcompiler_47.dll"));
        osvrDlls.Add(FString("glew32.dll"));
        osvrDlls.Add(FString("SDL2.dll"));

#if PLATFORM_64BITS
        TArray<FString> pathsToTry;
        pathsToTry.Add(FPaths::ProjectPluginsDir() / "OSVR/Source/OSVRClientKit/bin/Win64/");
        pathsToTry.Add(FPaths::EngineDir() / "Plugins/Runtime/OSVR/Source/OSVRClientKit/bin/Win64/");
        pathsToTry.Add(FPaths::EngineDir() / "Binaries/ThirdParty/OSVRClientKit/bin/Win64/");
        pathsToTry.Add(FPaths::EngineDir() / "Source/ThirdParty/OSVRClientKit/bin/Win64/");

#else
        TArray<FString> pathsToTry;
        pathsToTry.Add(FPaths::ProjectPluginsDir() / "OSVR/Source/OSVRClientKit/bin/Win32/");
        pathsToTry.Add(FPaths::EngineDir() / "Plugins/Runtime/OSVR/Source/OSVRClientKit/bin/Win32/");
        pathsToTry.Add(FPaths::EngineDir() / "Binaries/ThirdParty/OSVRClientKit/bin/Win32/");
        pathsToTry.Add(FPaths::EngineDir() / "Source/ThirdParty/OSVRClientKit/bin/Win32/");
#endif

        FString osvrClientKitLibPath;
        for (auto& it : pathsToTry)
        {
            if (FPaths::DirectoryExists(it))
            {
                osvrClientKitLibPath = it;
                break;
            }
        }
        if (osvrClientKitLibPath.Len() == 0)
        {
            UE_LOG(OSVRLog, Warning, TEXT("Could not find OSVRClientKit module binaries in either the engine plugins or game plugins folder."));
            return;
        }
        FPlatformProcess::PushDllDirectory(*osvrClientKitLibPath);

        for (auto& it : osvrDlls)
        {
            void* libHandle = nullptr;
            auto path = osvrClientKitLibPath + it;
            libHandle = FPlatformProcess::GetDllHandle(*path);
            if (!libHandle)
            {
                UE_LOG(OSVRLog, Warning, TEXT("FAILED to load %s"), *path);
            }
        }
        FPlatformProcess::PopDllDirectory(*osvrClientKitLibPath);
#endif
        bModulesLoaded = true;
    }
}

TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FOSVR::CreateTrackingSystem()
{
    return GetHMD();
}

#if OSVR_UNREAL_4_12
bool FOSVR::IsHMDConnected()
{
    return EntryPoint->IsOSVRConnected();
}
#endif

void FOSVR::StartupModule()
{
    LoadOSVRClientKitModule();
    IHeadMountedDisplayModule::StartupModule();

    EntryPoint = MakeShareable(new OSVREntryPoint());
}

void FOSVR::ShutdownModule()
{
    EntryPoint = nullptr;

    IHeadMountedDisplayModule::ShutdownModule();
}
