// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Framework/Commands/InputChord.h"
#include "Input/Events.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KismetInputLibrary.generated.h"

UENUM(BlueprintType)
enum class ESlateGesture : uint8
{
	None,
	Scroll,
	Magnify,
	Swipe,
	Rotate,
	LongPress
};

UCLASS()
class ENGINE_API UKismetInputLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Calibrate the tilt for the input device */
	UFUNCTION(BlueprintCallable, Category="Input")
	static void CalibrateTilt();

	/**
	 * Test if the input key are equal (A == B)
	 * @param A - The key to compare against
	 * @param B - The key to compare
	 * @returns True if the key are equal, false otherwise
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Equal (Key)", CompactNodeTitle = "=="), Category="Utilities|Key")
	static bool EqualEqual_KeyKey(FKey A, FKey B);

	/**
	* Test if the input chords are equal (A == B)
	* @param A - The chord to compare against
	* @param B - The chord to compare
	* @returns True if the chords are equal, false otherwise
	*/
	UFUNCTION( BlueprintPure, meta = ( DisplayName = "Equal (Input Chord)", CompactNodeTitle = "==" ), Category = "Utilities|Key" )
	static bool EqualEqual_InputChordInputChord( FInputChord A, FInputChord B );
	
	/**
	 * @returns True if the key is a modifier key: Ctrl, Command, Alt, Shift
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Is Modifier Key"), Category="Utilities|Key")
	static bool Key_IsModifierKey(const FKey& Key);
	
	/**
	 * @returns True if the key is a gamepad button
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Is Gamepad Key"), Category="Utilities|Key")
	static bool Key_IsGamepadKey(const FKey& Key);
	
	/**
	 * @returns True if the key is a mouse button
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Is Mouse Button"), Category="Utilities|Key")
	static bool Key_IsMouseButton(const FKey& Key);
	
	/**
	 * @returns True if the key is a keyboard button
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Is Keyboard Key"), Category="Utilities|Key")
	static bool Key_IsKeyboardKey(const FKey& Key);
	
	/**
	 * @returns True if the key is a float axis
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Is Float Axis"), Category="Utilities|Key")
	static bool Key_IsFloatAxis(const FKey& Key);
	
	/**
	 * @returns True if the key is a vector axis
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Is Vector Axis"), Category="Utilities|Key")
	static bool Key_IsVectorAxis(const FKey& Key);
	
	/**
	 * @returns The display name of the key.
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Key Display Name"), Category="Utilities|Key")
	static FText Key_GetDisplayName(const FKey& Key);

	/**
	 * Returns whether or not this character is an auto-repeated keystroke
	 *
	 * @return  True if this character is a repeat
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Repeat" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsRepeat(const FInputEvent& Input);

	/**
	 * Returns true if either shift key was down when this event occurred
	 *
	 * @return  True if shift is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Shift Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsShiftDown(const FInputEvent& Input);

	/**
	 * Returns true if left shift key was down when this event occurred
	 *
	 * @return True if left shift is pressed.
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Left Shift Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsLeftShiftDown(const FInputEvent& Input);

	/**
	 * Returns true if right shift key was down when this event occurred
	 *
	 * @return True if right shift is pressed.
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Right Shift Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsRightShiftDown(const FInputEvent& Input);

	/**
	 * Returns true if either control key was down when this event occurred
	 *
	 * @return  True if control is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Control Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsControlDown(const FInputEvent& Input);

	/**
	 * Returns true if left control key was down when this event occurred
	 *
	 * @return  True if left control is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Left Control Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsLeftControlDown(const FInputEvent& Input);

	/**
	 * Returns true if left control key was down when this event occurred
	 *
	 * @return  True if left control is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Right Control Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsRightControlDown(const FInputEvent& Input);

	/**
	 * Returns true if either alt key was down when this event occurred
	 *
	 * @return  True if alt is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Alt Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsAltDown(const FInputEvent& Input);

	/**
	 * Returns true if left alt key was down when this event occurred
	 *
	 * @return  True if left alt is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Left Alt Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsLeftAltDown(const FInputEvent& Input);

	/**
	 * Returns true if right alt key was down when this event occurred
	 *
	 * @return  True if right alt is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Right Alt Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsRightAltDown(const FInputEvent& Input);

	/**
	 * Returns true if either command key was down when this event occurred
	 *
	 * @return  True if command is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Command Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsCommandDown(const FInputEvent& Input);

	/**
	 * Returns true if left command key was down when this event occurred
	 *
	 * @return  True if left command is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Left Command Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsLeftCommandDown(const FInputEvent& Input);

	/**
	 * Returns true if right command key was down when this event occurred
	 *
	 * @return  True if right command is pressed
	 */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Right Command Down" ), Category="Utilities|InputEvent")
	static bool InputEvent_IsRightCommandDown(const FInputEvent& Input);


	/**
	 * Returns the key for this event.
	 *
	 * @return  Key name
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|KeyEvent")
	static FKey GetKey(const FKeyEvent& Input);

	UFUNCTION(BlueprintPure, Category = "Utilities|KeyEvent")
	static int32 GetUserIndex(const FKeyEvent& Input);

	UFUNCTION(BlueprintPure, Category = "Utilities|FAnalogInputEvent")
	static float GetAnalogValue(const FAnalogInputEvent& Input);

	/** @return The position of the cursor in screen space */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Get Screen Space Position" ), Category="Utilities|PointerEvent")
	static FVector2D PointerEvent_GetScreenSpacePosition(const FPointerEvent& Input);

	/** @return The position of the cursor in screen space last time we handled an input event */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Get Last Screen Space Position" ), Category="Utilities|PointerEvent")
	static FVector2D PointerEvent_GetLastScreenSpacePosition(const FPointerEvent& Input);

	/** @return the distance the mouse traveled since the last event was handled. */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Get Cursor Delta" ), Category="Utilities|PointerEvent")
	static FVector2D PointerEvent_GetCursorDelta(const FPointerEvent& Input);

	/** Mouse buttons that are currently pressed */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Mouse Button Down" ), Category="Utilities|PointerEvent")
	static bool PointerEvent_IsMouseButtonDown(const FPointerEvent& Input, FKey MouseButton);

	/** Mouse button that caused this event to be raised (possibly EB_None) */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Get Effecting Button" ), Category="Utilities|PointerEvent")
	static FKey PointerEvent_GetEffectingButton(const FPointerEvent& Input);

	/** How much did the mouse wheel turn since the last mouse event */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Get Wheel Delta" ), Category="Utilities|PointerEvent")
	static float PointerEvent_GetWheelDelta(const FPointerEvent& Input);

	/** @return The index of the user that caused the event */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Get User Index" ), Category="Utilities|PointerEvent")
	static int32 PointerEvent_GetUserIndex(const FPointerEvent& Input);

	/** @return The unique identifier of the pointer (e.g., finger index) */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Get Pointer Index" ), Category="Utilities|PointerEvent")
	static int32 PointerEvent_GetPointerIndex(const FPointerEvent& Input);

	/** @return The index of the touch pad that generated this event (for platforms with multiple touch pads per user) */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Get Touchpad Index" ), Category="Utilities|PointerEvent")
	static int32 PointerEvent_GetTouchpadIndex(const FPointerEvent& Input);

	/** @return Is this event a result from a touch (as opposed to a mouse) */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Is Touch Event" ), Category="Utilities|PointerEvent")
	static bool PointerEvent_IsTouchEvent(const FPointerEvent& Input);

	/** @return The type of touch gesture */
	UFUNCTION(BlueprintPure, Category="Utilities|PointerEvent")
	static ESlateGesture PointerEvent_GetGestureType(const FPointerEvent& Input);

	/** @return The change in gesture value since the last gesture event of the same type. */
	UFUNCTION(BlueprintPure, meta=( DisplayName = "Get Gesture Delta" ), Category="Utilities|PointerEvent")
	static FVector2D PointerEvent_GetGestureDelta(const FPointerEvent& Input);
};
