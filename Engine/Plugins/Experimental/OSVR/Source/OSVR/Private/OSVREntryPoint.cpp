//
// Copyright 2014, 2015 Razer Inc.
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

#include "OSVREntryPoint.h"
#include "OSVRPrivate.h"

#if OSVR_DEPRECATED_BLUEPRINT_API_ENABLED
#include "OSVRInterfaceCollection.h"
#endif

#include <osvr/ClientKit/ServerAutoStartC.h>

#include "Runtime/Core/Public/Misc/DateTime.h"

DEFINE_LOG_CATEGORY(OSVREntryPointLog);

OSVREntryPoint::OSVREntryPoint()
{
    // avoid BuildCookRun hangs
    if (IsRunningCommandlet() || IsRunningDedicatedServer())
    {
        UE_LOG(OSVREntryPointLog, Display, TEXT("OSVREntryPoint::OSVREntryPoint(): running as commandlet or dedicated server - skipping client context startup."));
        return;
    }

    osvrClientAttemptServerAutoStart();

    osvrClientContext = osvrClientInit("com.osvr.unreal.plugin");

    {
        bool bClientContextOK = false;
		bool bFailure = false;
		auto begin = FDateTime::Now().GetTicks();
		auto end = begin + 10 * ETimespan::TicksPerSecond;
		while (FDateTime::Now().GetTicks() < end && !bClientContextOK && !bFailure)
        {
            bClientContextOK = osvrClientCheckStatus(osvrClientContext) == OSVR_RETURN_SUCCESS;
            if (!bClientContextOK)
            {
                bFailure = osvrClientUpdate(osvrClientContext) == OSVR_RETURN_FAILURE;
                if (bFailure)
                {
                    UE_LOG(OSVREntryPointLog, Display, TEXT("osvrClientUpdate failed during startup. Treating this as if the HMD is not connected."));
                    break;
                }
                FPlatformProcess::Sleep(0.2f);
            }
        }
        if (!bClientContextOK)
        {
            UE_LOG(OSVREntryPointLog, Display, TEXT("OSVR client context could not connect. Most likely the server isn't running. Treating this as if the HMD is not connected."));
        }
    }

#if OSVR_DEPRECATED_BLUEPRINT_API_ENABLED
    InterfaceCollection = MakeShareable(new OSVRInterfaceCollection(osvrClientContext));
#endif
}

OSVREntryPoint::~OSVREntryPoint()
{
    FScopeLock lock(this->GetClientContextMutex());

#if OSVR_DEPRECATED_BLUEPRINT_API_ENABLED
    InterfaceCollection = nullptr;
#endif

    if (osvrClientContext)
    {
       osvrClientShutdown(osvrClientContext);
    }

    osvrClientReleaseAutoStartedServer();
}

void OSVREntryPoint::Tick(float DeltaTime)
{
    FScopeLock lock(this->GetClientContextMutex());
    osvrClientUpdate(osvrClientContext);
}

bool OSVREntryPoint::IsOSVRConnected()
{
    return osvrClientContext && osvrClientCheckStatus(osvrClientContext) == OSVR_RETURN_SUCCESS;
}

#if OSVR_DEPRECATED_BLUEPRINT_API_ENABLED
OSVRInterfaceCollection* OSVREntryPoint::GetInterfaceCollection()
{
    return InterfaceCollection.Get();
}
#endif
