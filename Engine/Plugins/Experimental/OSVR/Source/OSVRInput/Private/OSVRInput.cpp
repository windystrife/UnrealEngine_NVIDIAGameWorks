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
#include "OSVRInputPrivate.h"
#include "IOSVR.h"
#include "IOSVRInput.h"
#include "OSVREntryPoint.h"
#include "OSVRInputDevice.h"
#include "Misc/ScopeLock.h"

#include "InputCoreTypes.h"
#include "GameFramework/InputSettings.h"

class FOSVRInput : public IOSVRInput
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual TSharedPtr< class IInputDevice > CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override;

	TSharedPtr< class FOSVRInputDevice > InputDevice;
};

IMPLEMENT_MODULE(FOSVRInput, OSVRInput)

TSharedPtr< class IInputDevice > FOSVRInput::CreateInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
    auto& osvr = IOSVR::Get();
    auto entryPoint = osvr.GetEntryPoint();
    auto osvrHMD = osvr.GetHMD();
    FScopeLock lock(entryPoint->GetClientContextMutex());
    if (entryPoint->IsOSVRConnected())
    {
        FOSVRInputDevice::RegisterNewKeys();

        InputDevice = MakeShareable(new FOSVRInputDevice(InMessageHandler, entryPoint, osvrHMD));
        return InputDevice;
    }
    return nullptr;
}

void FOSVRInput::StartupModule()
{
	IInputDeviceModule::StartupModule();
}

void FOSVRInput::ShutdownModule()
{
	IInputDeviceModule::ShutdownModule();
}
