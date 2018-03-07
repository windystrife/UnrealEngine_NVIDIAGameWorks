// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Math/Vector2D.h"
#include "Templates/SharedPointer.h"
#include "Misc/Optional.h"

class FGenericWindow;

namespace EMouseButtons
{
	enum Type
	{
		Left = 0,
		Middle,
		Right,
		Thumb01,
		Thumb02,

		Invalid,
	};
}

struct APPLICATIONCORE_API FGamepadKeyNames
{
	typedef FName Type;

	static const FName Invalid;

	static const FName LeftAnalogX;
	static const FName LeftAnalogY;
	static const FName RightAnalogX;
	static const FName RightAnalogY;
	static const FName LeftTriggerAnalog;
	static const FName RightTriggerAnalog;

	static const FName LeftThumb;
	static const FName RightThumb;
	static const FName SpecialLeft;
	static const FName SpecialLeft_X;
	static const FName SpecialLeft_Y;
	static const FName SpecialRight;
	static const FName FaceButtonBottom;
	static const FName FaceButtonRight;
	static const FName FaceButtonLeft;
	static const FName FaceButtonTop;
	static const FName LeftShoulder;
	static const FName RightShoulder;
	static const FName LeftTriggerThreshold;
	static const FName RightTriggerThreshold;
	static const FName DPadUp;
	static const FName DPadDown;
	static const FName DPadRight;
	static const FName DPadLeft;

	static const FName LeftStickUp;
	static const FName LeftStickDown;
	static const FName LeftStickRight;
	static const FName LeftStickLeft;

	static const FName RightStickUp;
	static const FName RightStickDown;
	static const FName RightStickRight;
	static const FName RightStickLeft;

	// Motion Controls
	//		Left Controller
	static const FName MotionController_Left_FaceButton1;
	static const FName MotionController_Left_FaceButton2;
	static const FName MotionController_Left_FaceButton3;
	static const FName MotionController_Left_FaceButton4;
	static const FName MotionController_Left_FaceButton5;
	static const FName MotionController_Left_FaceButton6;
	static const FName MotionController_Left_FaceButton7;
	static const FName MotionController_Left_FaceButton8;

	static const FName MotionController_Left_Shoulder;
	static const FName MotionController_Left_Trigger;

	static const FName MotionController_Left_Grip1;
	static const FName MotionController_Left_Grip2;

	static const FName MotionController_Left_Thumbstick;
	static const FName MotionController_Left_Thumbstick_Up;
	static const FName MotionController_Left_Thumbstick_Down;
	static const FName MotionController_Left_Thumbstick_Left;
	static const FName MotionController_Left_Thumbstick_Right;

	//		Right Controller
	static const FName MotionController_Right_FaceButton1;
	static const FName MotionController_Right_FaceButton2;
	static const FName MotionController_Right_FaceButton3;
	static const FName MotionController_Right_FaceButton4;
	static const FName MotionController_Right_FaceButton5;
	static const FName MotionController_Right_FaceButton6;
	static const FName MotionController_Right_FaceButton7;
	static const FName MotionController_Right_FaceButton8;

	static const FName MotionController_Right_Shoulder;
	static const FName MotionController_Right_Trigger;

	static const FName MotionController_Right_Grip1;
	static const FName MotionController_Right_Grip2;

	static const FName MotionController_Right_Thumbstick;
	static const FName MotionController_Right_Thumbstick_Up;
	static const FName MotionController_Right_Thumbstick_Down;
	static const FName MotionController_Right_Thumbstick_Left;
	static const FName MotionController_Right_Thumbstick_Right;

	//   Motion Controller Axes
	//		Left Controller
	static const FName MotionController_Left_Thumbstick_X;
	static const FName MotionController_Left_Thumbstick_Y;
	static const FName MotionController_Left_TriggerAxis;
	static const FName MotionController_Left_Grip1Axis;
	static const FName MotionController_Left_Grip2Axis;

	//		Right Controller
	static const FName MotionController_Right_Thumbstick_X;
	static const FName MotionController_Right_Thumbstick_Y;
	static const FName MotionController_Right_TriggerAxis;
	static const FName MotionController_Right_Grip1Axis;
	static const FName MotionController_Right_Grip2Axis;
};

enum class EWindowActivation : uint8
{
	Activate,
	ActivateByMouse,
	Deactivate
};

namespace EWindowZone
{
	/**
	 * The Window Zone is the window area we are currently over to send back to the operating system
	 * for operating system compliance.
	 */
	enum Type
	{
		NotInWindow			= 0,
		TopLeftBorder		= 1,
		TopBorder			= 2,
		TopRightBorder		= 3,
		LeftBorder			= 4,
		ClientArea			= 5,
		RightBorder			= 6,
		BottomLeftBorder	= 7,
		BottomBorder		= 8,
		BottomRightBorder	= 9,
		TitleBar			= 10,
		MinimizeButton		= 11,
		MaximizeButton		= 12,
		CloseButton			= 13,
		SysMenu				= 14,

		/** No zone specified */
		Unspecified	= 0,
	};
}


namespace EWindowAction
{
	enum Type
	{
		ClickedNonClientArea	= 1,
		Maximize				= 2,
		Restore					= 3,
		WindowMenu				= 4,
	};
}


/**
 * 
 */
namespace EDropEffect
{
	enum Type
	{
		None = 0,
		Copy = 1,
		Move = 2,
		Link = 3,
	};
}


enum class EGestureEvent : uint8
{
	None,
	Scroll,
	Magnify,
	Swipe,
	Rotate,
	LongPress,
	Count
};


/** Defines the minimum and maximum dimensions that a window can take on. */
struct FWindowSizeLimits
{
public:
	FWindowSizeLimits& SetMinWidth(TOptional<float> InValue){ MinWidth = InValue; return *this; }
	const TOptional<float>& GetMinWidth() const { return MinWidth; }

	FWindowSizeLimits& SetMinHeight(TOptional<float> InValue){ MinHeight = InValue; return *this; }
	const TOptional<float>& GetMinHeight() const { return MinHeight; }

	FWindowSizeLimits& SetMaxWidth(TOptional<float> InValue){ MaxWidth = InValue; return *this; }
	const TOptional<float>& GetMaxWidth() const { return MaxWidth; }

	FWindowSizeLimits& SetMaxHeight(TOptional<float> InValue){ MaxHeight = InValue; return *this; }
	const TOptional<float>& GetMaxHeight() const { return MaxHeight; }

private:
	TOptional<float> MinWidth;
	TOptional<float> MinHeight;
	TOptional<float> MaxWidth;
	TOptional<float> MaxHeight;
};


class FGenericApplicationMessageHandler
{
public:

	virtual ~FGenericApplicationMessageHandler() {}

	virtual bool ShouldProcessUserInputMessages( const TSharedPtr< FGenericWindow >& PlatformWindow ) const
	{
		return false;
	}

	virtual bool OnKeyChar( const TCHAR Character, const bool IsRepeat )
	{
		return false;
	}

	virtual bool OnKeyDown( const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat ) 
	{
		return false;
	}

	virtual bool OnKeyUp( const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat )
	{
		return false;
	}

	virtual bool OnMouseDown( const TSharedPtr< FGenericWindow >& Window, const EMouseButtons::Type Button )
	{
		return false;
	}

	virtual bool OnMouseDown( const TSharedPtr< FGenericWindow >& Window, const EMouseButtons::Type Button, const FVector2D CursorPos )
	{
		return false;
	}

	virtual bool OnMouseUp( const EMouseButtons::Type Button )
	{
		return false;
	}

	virtual bool OnMouseUp( const EMouseButtons::Type Button, const FVector2D CursorPos )
	{
		return false;
	}

	virtual bool OnMouseDoubleClick( const TSharedPtr< FGenericWindow >& Window, const EMouseButtons::Type Button )
	{
		return false;
	}

	virtual bool OnMouseDoubleClick( const TSharedPtr< FGenericWindow >& Window, const EMouseButtons::Type Button, const FVector2D CursorPos )
	{
		return false;
	}

	virtual bool OnMouseWheel( const float Delta )
	{
		return false;
	}

	virtual bool OnMouseWheel( const float Delta, const FVector2D CursorPos )
	{
		return false;
	}

	virtual bool OnMouseMove()
	{
		return false;
	}

	virtual bool OnRawMouseMove( const int32 X, const int32 Y )
	{
		return false;
	}

	virtual bool OnCursorSet()
	{
		return false;
	}

	virtual bool OnControllerAnalog( FGamepadKeyNames::Type KeyName, int32 ControllerId, float AnalogValue )
	{
		return false;
	}

	virtual bool OnControllerButtonPressed( FGamepadKeyNames::Type KeyName, int32 ControllerId, bool IsRepeat )
	{
		return false;
	}

	virtual bool OnControllerButtonReleased( FGamepadKeyNames::Type KeyName, int32 ControllerId, bool IsRepeat )
	{
		return false;
	}

    virtual void OnBeginGesture()
    {
    }

	virtual bool OnTouchGesture( EGestureEvent GestureType, const FVector2D& Delta, float WheelDelta, bool bIsDirectionInvertedFromDevice )
	{
		return false;
	}
    
    virtual void OnEndGesture()
    {
    }

	virtual bool OnTouchStarted( const TSharedPtr< FGenericWindow >& Window, const FVector2D& Location, int32 TouchIndex, int32 ControllerId )
	{
		return false;
	}

	virtual bool OnTouchMoved( const FVector2D& Location, int32 TouchIndex, int32 ControllerId )
	{
		return false;
	}

	virtual bool OnTouchEnded( const FVector2D& Location, int32 TouchIndex, int32 ControllerId )
	{
		return false;
	}

	virtual void ShouldSimulateGesture(EGestureEvent Gesture, bool bEnable)
	{

	}

	virtual bool OnMotionDetected( const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration, int32 ControllerId )
	{
		return false;
	}

	virtual bool OnSizeChanged( const TSharedRef< FGenericWindow >& Window, const int32 Width, const int32 Height, bool bWasMinimized = false )
	{
		return false;
	}

	virtual void OnOSPaint( const TSharedRef<FGenericWindow>& Window )
	{
	
	}

	virtual FWindowSizeLimits GetSizeLimitsForWindow( const TSharedRef<FGenericWindow>& Window ) const
	{
		return FWindowSizeLimits();
	}

	virtual void OnResizingWindow( const TSharedRef< FGenericWindow >& Window )
	{

	}

	virtual bool BeginReshapingWindow( const TSharedRef< FGenericWindow >& Window )
	{
		return true;
	}

	virtual void FinishedReshapingWindow( const TSharedRef< FGenericWindow >& Window )
	{

	}

	virtual void HandleDPIScaleChanged( const TSharedRef< FGenericWindow >& Window )
	{

	}

	virtual void OnMovedWindow( const TSharedRef< FGenericWindow >& Window, const int32 X, const int32 Y )
	{

	}

	virtual bool OnWindowActivationChanged( const TSharedRef< FGenericWindow >& Window, const EWindowActivation ActivationType )
	{
		return false;
	}

	virtual bool OnApplicationActivationChanged( const bool IsActive )
	{
		return false;
	}

	virtual bool OnConvertibleLaptopModeChanged()
	{
		return false;
	}

	virtual EWindowZone::Type GetWindowZoneForPoint( const TSharedRef< FGenericWindow >& Window, const int32 X, const int32 Y )
	{
		return EWindowZone::NotInWindow;
	}

	virtual void OnWindowClose( const TSharedRef< FGenericWindow >& Window )
	{

	}

	virtual EDropEffect::Type OnDragEnterText( const TSharedRef< FGenericWindow >& Window, const FString& Text )
	{
		return EDropEffect::None;
	}

	virtual EDropEffect::Type OnDragEnterFiles( const TSharedRef< FGenericWindow >& Window, const TArray< FString >& Files )
	{
		return EDropEffect::None;
	}

	virtual EDropEffect::Type OnDragEnterExternal( const TSharedRef< FGenericWindow >& Window, const FString& Text, const TArray< FString >& Files )
	{
		return EDropEffect::None;
	}

	virtual EDropEffect::Type OnDragOver( const TSharedPtr< FGenericWindow >& Window )
	{
		return EDropEffect::None;
	}

	virtual void OnDragLeave( const TSharedPtr< FGenericWindow >& Window )
	{

	}

	virtual EDropEffect::Type OnDragDrop( const TSharedPtr< FGenericWindow >& Window )
	{
		return EDropEffect::None;
	}

	virtual bool OnWindowAction( const TSharedRef< FGenericWindow >& Window, const EWindowAction::Type InActionType)
	{
		return true;
	}
};
