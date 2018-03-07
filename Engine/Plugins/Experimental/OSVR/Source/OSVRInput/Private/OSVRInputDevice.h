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

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "OSVRInputPrivate.h"
#include "IForceFeedbackSystem.h"
#include "IInputDevice.h"
#include "IMotionController.h"
#include "IOSVR.h"
#include "OSVRTypes.h"

THIRD_PARTY_INCLUDES_START
	#include <osvr/ClientKit/InterfaceC.h>
THIRD_PARTY_INCLUDES_END

THIRD_PARTY_INCLUDES_START
	#include <string>
	#include <vector>
	#include <queue>
	#include <map>
	#include <functional>
THIRD_PARTY_INCLUDES_END

DECLARE_LOG_CATEGORY_EXTERN(LogOSVRInputDevice, Log, All);

class OSVRButton;

/**
*
*/
class FOSVRInputDevice : public IInputDevice, public IMotionController
{
public:
	FOSVRInputDevice(const TSharedRef< FGenericApplicationMessageHandler >& MessageHandler,
		TSharedPtr<OSVREntryPoint, ESPMode::ThreadSafe> osvrEntryPoint, TSharedPtr<FOSVRHMD, ESPMode::ThreadSafe> osvrHMD);
	virtual ~FOSVRInputDevice();
	static void RegisterNewKeys();

	/* IMotionController interface*/

	virtual FName GetMotionControllerDeviceTypeName() const override 
	{
		const static FName DefaultName(TEXT("OSVRInputDevice"));
		return DefaultName;
	}

    /**
    * Returns the calibration-space orientation of the requested controller's hand.
    *
    * @param ControllerIndex	The Unreal controller (player) index of the contoller set
    * @param DeviceHand		Which hand, within the controller set for the player, to get the orientation and position for
    * @param OutOrientation	(out) If tracked, the orientation (in calibrated-space) of the controller in the specified hand
    * @param OutPosition		(out) If tracked, the position (in calibrated-space) of the controller in the specified hand
    * @return					True if the device requested is valid and tracked, false otherwise
    */
    virtual bool GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const override;

#if OSVR_UNREAL_4_11
    virtual ETrackingStatus GetControllerTrackingStatus(const int32, const EControllerHand) const override;
#endif

    /** Tick the interface (e.g. check for new controllers) */
    virtual void Tick(float DeltaTime) override;

    /** Poll for controller state and send events if needed */
    virtual void SendControllerEvents() override;

    /** Set which MessageHandler will get the events from SendControllerEvents. */
    virtual void SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler) override;

    /** Exec handler to allow console commands to be passed through for debugging */
    virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

    // IForceFeedbackSystem pass through functions
    virtual void SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
    virtual void SetChannelValues(int32 ControllerId, const FForceFeedbackValues& values) override;

    void EventReport(const FKey& Key, const FVector& Translation, const FQuat& Orientation);

private:
    TSharedPtr<OSVREntryPoint, ESPMode::ThreadSafe> mOSVREntryPoint;
    TSharedPtr<FOSVRHMD, ESPMode::ThreadSafe> mOSVRHMD;
    TMap<FString, OSVR_ClientInterface> interfaces;
    TArray<TSharedPtr<OSVRButton> > osvrButtons;
    OSVR_ClientContext context;
    FCriticalSection* contextMutex;
    TSharedRef<FGenericApplicationMessageHandler> MessageHandler;
    OSVR_ClientInterface leftHand;
    OSVR_ClientInterface rightHand;
    bool bLeftHandValid = false;
    bool bRightHandValid = false;
    bool bContextValid = false;
};
