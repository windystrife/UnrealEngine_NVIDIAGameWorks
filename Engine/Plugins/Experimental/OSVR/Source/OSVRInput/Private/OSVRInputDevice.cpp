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

#include "OSVRInputDevice.h"
#include "OSVRInputPrivate.h"
#include "Containers/Queue.h"

#include "GenericPlatformMath.h"
#include "OSVREntryPoint.h"
#include "GenericApplicationMessageHandler.h"
#include "OSVRTypes.h"
#include "IOSVR.h"
#include "OSVRHMD.h"

THIRD_PARTY_INCLUDES_START
	#include <osvr/ClientKit/InterfaceStateC.h>
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY(LogOSVRInputDevice);

namespace
{
    enum OSVRButtonType
    {
        OSVR_BUTTON_TYPE_DIGITAL,
        OSVR_BUTTON_TYPE_ANALOG,
        OSVR_BUTTON_TYPE_THRESHOLD
    };

    enum OSVRThresholdType
    {
        OSVR_THRESHOLD_TYPE_GT,
        OSVR_THRESHOLD_TYPE_LT
    };
}

class OSVRButton
{

public:
    OSVRButton() {}
    OSVRButton(OSVRButtonType _type, FName _key, const FString& _ifacePath) :
        type(_type), key(_key), ifacePath(_ifacePath)
    {
    }

    OSVRButton(OSVRButtonType _type, OSVRThresholdType _thresholdType, float _threshold, FName _key, const FString& _ifacePath) :
        type(_type), thresholdType(_thresholdType), threshold(_threshold), key(_key), ifacePath(_ifacePath)
    {
    }

    bool bOldState = false;
    bool bIsValid = true;
    float threshold = 0.75f;
    FName key;
    FString ifacePath;
    OSVRButtonType type;
    OSVRThresholdType thresholdType = OSVR_THRESHOLD_TYPE_GT;
    TQueue<bool> digitalStateQueue;
    TQueue<float> analogStateQueue;
};

namespace {
    inline void CheckOSVR(OSVR_ReturnCode rc, const char* msg)
    {
        if (rc == OSVR_RETURN_FAILURE)
        {
            UE_LOG(LogOSVRInputDevice, Warning, TEXT("%s"), msg);
        }
    }

    void buttonCallback(void *userdata, const OSVR_TimeValue *timestamp, const OSVR_ButtonReport *report)
    {
        OSVRButton* button = static_cast<OSVRButton*>(userdata);
        button->digitalStateQueue.Enqueue(report->state == OSVR_BUTTON_PRESSED);
    }

    void analogCallback(void *userdata, const OSVR_TimeValue *timestamp, const OSVR_AnalogReport *report)
    {
        OSVRButton* button = static_cast<OSVRButton*>(userdata);
        if (button->type == OSVR_BUTTON_TYPE_THRESHOLD)
        {
            bool bNewState = (button->thresholdType == OSVR_THRESHOLD_TYPE_GT && report->state > button->threshold) ||
                (button->thresholdType == OSVR_THRESHOLD_TYPE_LT && report->state < button->threshold);

            if (bNewState != button->bOldState)
            {
                button->digitalStateQueue.Enqueue(bNewState);
            }
            button->bOldState = bNewState;
        }
        else
        {
            button->analogStateQueue.Enqueue(report->state);
        }
    }
}

void FOSVRInputDevice::RegisterNewKeys()
{
}

FOSVRInputDevice::FOSVRInputDevice(
    const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler,
    TSharedPtr<OSVREntryPoint, ESPMode::ThreadSafe> osvrEntryPoint,
    TSharedPtr<FOSVRHMD, ESPMode::ThreadSafe> osvrHMD) :
    mOSVREntryPoint(osvrEntryPoint), mOSVRHMD(osvrHMD), MessageHandler(InMessageHandler)
{
    // make sure OSVR module is loaded.
    contextMutex = mOSVREntryPoint->GetClientContextMutex();
    FScopeLock lock(contextMutex);
    context = mOSVREntryPoint->GetClientContext();

    bContextValid = context && osvrClientCheckStatus(context) == OSVR_RETURN_SUCCESS;

    if (bContextValid)
    {
        const float defaultThreshold = 0.25f;

        TSharedPtr<OSVRButton> buttons[] =
        {
            // left hand
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::SpecialLeft, "/controller/left/middle")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Left_Shoulder, "/controller/left/bumper")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Left_Thumbstick, "/controller/left/joystick/button")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Left_FaceButton1, "/controller/left/1")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Left_FaceButton2, "/controller/left/2")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Left_FaceButton3, "/controller/left/3")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Left_FaceButton4, "/controller/left/4")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::MotionController_Left_Thumbstick_X, "/controller/left/joystick/x")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_GT, defaultThreshold, FGamepadKeyNames::MotionController_Left_Thumbstick_Right, "/controller/left/joystick/x")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_LT, -defaultThreshold, FGamepadKeyNames::MotionController_Left_Thumbstick_Left, "/controller/left/joystick/x")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::MotionController_Left_Thumbstick_Y, "/controller/left/joystick/y")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_GT, defaultThreshold, FGamepadKeyNames::MotionController_Left_Thumbstick_Up, "/controller/left/joystick/y")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_LT, -defaultThreshold, FGamepadKeyNames::MotionController_Left_Thumbstick_Down, "/controller/left/joystick/y")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::MotionController_Left_TriggerAxis, "/controller/left/trigger")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, FGamepadKeyNames::MotionController_Left_Trigger, "/controller/left/trigger")),

            // right hand
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::SpecialRight, "/controller/right/middle")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Right_Shoulder, "/controller/right/bumper")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Right_Thumbstick, "/controller/right/joystick/button")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Right_FaceButton1, "/controller/right/1")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Right_FaceButton2, "/controller/right/2")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Right_FaceButton3, "/controller/right/3")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::MotionController_Right_FaceButton4, "/controller/right/4")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::MotionController_Right_Thumbstick_X, "/controller/right/joystick/x")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_GT, defaultThreshold, FGamepadKeyNames::MotionController_Right_Thumbstick_Right, "/controller/right/joystick/x")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_LT, -defaultThreshold, FGamepadKeyNames::MotionController_Right_Thumbstick_Left, "/controller/right/joystick/x")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::MotionController_Right_Thumbstick_Y, "/controller/right/joystick/y")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_GT, defaultThreshold, FGamepadKeyNames::MotionController_Right_Thumbstick_Up, "/controller/right/joystick/y")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_LT, -defaultThreshold, FGamepadKeyNames::MotionController_Right_Thumbstick_Down, "/controller/right/joystick/y")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::MotionController_Right_TriggerAxis, "/controller/right/trigger")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, FGamepadKeyNames::MotionController_Right_Trigger, "/controller/right/trigger")),

            // "controller" (like xbox360)
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::RightShoulder, "/controller/right/bumper")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::RightThumb, "/controller/right/joystick/button")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::FaceButtonBottom, "/controller/right/1")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::FaceButtonRight, "/controller/right/2")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::FaceButtonLeft, "/controller/right/3")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::FaceButtonTop, "/controller/right/4")),

            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::LeftShoulder, "/controller/left/bumper")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::LeftThumb, "/controller/left/joystick/button")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::DPadDown, "/controller/left/1")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::DPadRight, "/controller/left/2")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::DPadLeft, "/controller/left/3")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_DIGITAL, FGamepadKeyNames::DPadUp, "/controller/left/4")),

            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::LeftAnalogX, "/controller/left/joystick/x")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_GT, defaultThreshold, FGamepadKeyNames::LeftStickRight, "/controller/left/joystick/x")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_LT, -defaultThreshold, FGamepadKeyNames::LeftStickLeft, "/controller/left/joystick/x")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::LeftAnalogY, "/controller/left/joystick/y")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_GT, defaultThreshold, FGamepadKeyNames::LeftStickUp, "/controller/left/joystick/y")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_LT, -defaultThreshold, FGamepadKeyNames::LeftStickDown, "/controller/left/joystick/y")),

            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::RightAnalogX, "/controller/right/joystick/x")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_GT, defaultThreshold, FGamepadKeyNames::RightStickRight, "/controller/right/joystick/x")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_LT, -defaultThreshold, FGamepadKeyNames::RightStickLeft, "/controller/right/joystick/x")),

            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::RightAnalogY, "/controller/right/joystick/y")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_GT, defaultThreshold, FGamepadKeyNames::RightStickUp, "/controller/right/joystick/y")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, OSVR_THRESHOLD_TYPE_LT, -defaultThreshold, FGamepadKeyNames::RightStickDown, "/controller/right/joystick/y")),

            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::LeftTriggerAnalog, "/controller/left/trigger")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_ANALOG, FGamepadKeyNames::RightTriggerAnalog, "/controller/right/trigger")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, FGamepadKeyNames::LeftTriggerThreshold, "/controller/left/trigger")),
            MakeShareable(new OSVRButton(OSVR_BUTTON_TYPE_THRESHOLD, FGamepadKeyNames::RightTriggerThreshold, "/controller/right/trigger")),
        };
        osvrButtons.Append(buttons, ARRAY_COUNT(buttons));

        for (auto& button : osvrButtons)
        {
            auto ifaceItr = interfaces.Find(button->ifacePath);
            OSVR_ClientInterface iface = nullptr;
            if (!ifaceItr)
            {
                if (osvrClientGetInterface(context, TCHAR_TO_ANSI(*(button->ifacePath)), &iface) != OSVR_RETURN_SUCCESS)
                {
                    button->bIsValid = false;
                }
                else
                {
                    interfaces.Add(button->ifacePath, iface);
                }
            }
            else
            {
                iface = *ifaceItr;
            }

            if (button->bIsValid)
            {
                if (button->type == OSVR_BUTTON_TYPE_DIGITAL)
                {
                    if (osvrRegisterButtonCallback(iface, buttonCallback, button.Get()) == OSVR_RETURN_FAILURE)
                    {
                        button->bIsValid = false;
                    }
                }

                if (button->type == OSVR_BUTTON_TYPE_ANALOG ||
                    button->type == OSVR_BUTTON_TYPE_THRESHOLD)
                {
                    if (osvrRegisterAnalogCallback(iface, analogCallback, button.Get()) == OSVR_RETURN_FAILURE)
                    {
                        button->bIsValid = false;
                    }
                }
            }
        }

        bLeftHandValid = osvrClientGetInterface(context, "/me/hands/left", &leftHand)
            == OSVR_RETURN_SUCCESS;

        bRightHandValid = osvrClientGetInterface(context, "/me/hands/right", &rightHand)
            == OSVR_RETURN_SUCCESS;

        IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);

#if !OSVR_UNREAL_4_12
        // This may need to be removed in a future version of the engine.
        // From the SteamVR plugin: "construction of the controller happens after InitializeMotionControllers(), so we manually add this to the array here"
        GEngine->MotionControllerDevices.AddUnique(this);
#endif
    }
}

FOSVRInputDevice::~FOSVRInputDevice()
{
    FScopeLock lock(contextMutex);

    IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);

    if (context)
    {
        if (leftHand)
        {
            osvrClientFreeInterface(context, leftHand);
        }
        if (rightHand)
        {
            osvrClientFreeInterface(context, rightHand);
        }
        for (auto iface : interfaces)
        {
            if (iface.Value)
            {
                osvrClientFreeInterface(context, iface.Value);
            }
        }
    }
}

void FOSVRInputDevice::EventReport(const FKey& Key, const FVector& Translation, const FQuat& Orientation)
{
}

/**
* Returns the calibration-space orientation of the requested controller's hand.
*
* @param ControllerIndex	The Unreal controller (player) index of the contoller set
* @param DeviceHand		Which hand, within the controller set for the player, to get the orientation and position for
* @param OutOrientation	(out) If tracked, the orientation (in calibrated-space) of the controller in the specified hand
* @param OutPosition		(out) If tracked, the position (in calibrated-space) of the controller in the specified hand
* @param WorldToMetersScale The world scaling factor.
* @return					True if the device requested is valid and tracked, false otherwise
*/
bool FOSVRInputDevice::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
{
    bool bRet = false;
    if (ControllerIndex == 0)
    {
        FScopeLock lock(contextMutex);
        if (osvrClientCheckStatus(context) == OSVR_RETURN_SUCCESS
            && osvrClientUpdate(context) == OSVR_RETURN_SUCCESS)
        {
            auto iface = DeviceHand == EControllerHand::Left ? leftHand : rightHand;
            OSVR_PoseState state;
            OSVR_TimeValue tvalue;
            if (osvrGetPoseState(iface, &tvalue, &state) == OSVR_RETURN_SUCCESS)
            {
                OutPosition = OSVR2FVector(state.translation, WorldToMetersScale);
                OutOrientation = OSVR2FQuat(state.rotation).Rotator();
                bRet = true;
            }
        }
    }
    return bRet;
}

#if OSVR_UNREAL_4_11
ETrackingStatus FOSVRInputDevice::GetControllerTrackingStatus(const int32, const EControllerHand) const
{
    if (bContextValid && (bLeftHandValid || bRightHandValid))
    {
        return ETrackingStatus::Tracked;
    }
    return ETrackingStatus::NotTracked;
}
#endif

void FOSVRInputDevice::Tick(float DeltaTime)
{
}

void FOSVRInputDevice::SendControllerEvents()
{
    FScopeLock lock(contextMutex);
    if (osvrClientUpdate(context) == OSVR_RETURN_FAILURE)
    {
        UE_LOG(LogOSVRInputDevice, Warning, TEXT("FOSVRInputDevice::SendControllerEvents(): osvrClientUpdate failed."));
    }

    const int32 controllerId = 0;
    for (auto& button : osvrButtons)
    {
        if (button->bIsValid)
        {
            while (!button->digitalStateQueue.IsEmpty())
            {
                bool state = false;
                button->digitalStateQueue.Dequeue(state);
                if (state)
                {
                    MessageHandler->OnControllerButtonPressed(button->key, controllerId, false);
                }
                else
                {
                    MessageHandler->OnControllerButtonReleased(button->key, controllerId, false);
                }
            }
            while (!button->analogStateQueue.IsEmpty())
            {
                float state = 0.0f;
                button->analogStateQueue.Dequeue(state);
                MessageHandler->OnControllerAnalog(button->key, controllerId, state);
            }
        }
    }
}

void FOSVRInputDevice::SetMessageHandler(const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler)
{
    MessageHandler = InMessageHandler;
}

bool FOSVRInputDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
    return true;
}

void FOSVRInputDevice::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
}

void FOSVRInputDevice::SetChannelValues(int32 ControllerId, const FForceFeedbackValues& values)
{
}
