// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericApplication.h"

/**
 * The accuracy when dealing with physical characteristics of the monitor/screen of the device we're running on.
 */
enum class EScreenPhysicalAccuracy
{
	Unknown,
	Approximation,
	Truth
};

struct APPLICATIONCORE_API FGenericPlatformApplicationMisc
{
	static void PreInit();

	static void Init();

	static void PostInit();

	static void TearDown();

	/**
	 * Load the preinit modules required by this platform, typically they are the renderer modules
	 */
	static void LoadPreInitModules()
	{
	}

	/**
	 * Load the platform-specific startup modules
	 */
	static void LoadStartupModules()
	{
	}

	/**
	 * Creates a console output device for this platform. Should only be called once.
	 */
	static FOutputDeviceConsole* CreateConsoleOutputDevice();

	/**
	 * Gets a pointer to the platform error output device singleton.
	 */
	static FOutputDeviceError* GetErrorOutputDevice();

	/**
	 * Gets a pointer to the default platform feedback context implementation.
	 */
	static FFeedbackContext* GetFeedbackContext();

	/**
	 * Creates an application instance.
	 */
	static class GenericApplication* CreateApplication();

	/** Request application to minimize (goto background). **/
	static void RequestMinimize();

	/** Returns true if the specified application has a visible window, and that window is active/has focus/is selected */
	static bool IsThisApplicationForeground();	

	/**
	* Returns whether the platform wants to use a touch screen for a virtual keyboard.
	*/
	static bool RequiresVirtualKeyboard()
	{
		return PLATFORM_HAS_TOUCH_MAIN_SCREEN;
	}

	/**
	 *	Pumps Windows messages.
	 *	@param bFromMainLoop if true, this is from the main loop, otherwise we are spinning waiting for the render thread
	 */
	FORCEINLINE static void PumpMessages(bool bFromMainLoop)
	{
	}

	/**
	 * Prevents screen-saver from kicking in by moving the mouse by 0 pixels. This works even on
	 * Vista in the presence of a group policy for password protected screen saver.
	 */
	static void PreventScreenSaver()
	{
	}

	enum EScreenSaverAction
	{
		Disable,
		Enable
	};

	/**
	 * Disables screensaver (if platform supports such an API)
	 *
	 * @param Action enable or disable
	 * @return true if succeeded, false if platform does not have this API and PreventScreenSaver() hack is needed
	 */
	static bool ControlScreensaver(EScreenSaverAction Action)
	{
		return false;
	}

	/**
	 * Sample the displayed pixel color from anywhere on the screen using the OS
	 *
	 * @param	InScreenPos		The screen coords to sample for current pixel color
	 * @param	InGamma			Optional gamma correction to apply to the screen color
	 * @return					The color of the pixel displayed at the chosen location
	 */
	static struct FLinearColor GetScreenPixelColor(const struct FVector2D& InScreenPos, float InGamma = 1.0f);

	/**
	 * Searches for a window that matches the window name or the title starts with a particular text. When
	 * found, it returns the title text for that window
	 *
	 * @param TitleStartsWith an alternative search method that knows part of the title text
	 * @param OutTitle the string the data is copied into
	 *
	 * @return whether the window was found and the text copied or not
	 */
	static bool GetWindowTitleMatchingText(const TCHAR* TitleStartsWith, FString& OutTitle)
	{
		return false;
	}

	/**
	* Returns monitor's DPI scale factor at given screen coordinates (expressed in pixels)
	* @return Monitor's DPI scale factor at given point
	*/
	static float GetDPIScaleFactorAtPoint(float X, float Y)
	{
		return 1.0f;
	}

	/*
	 * Resets the gamepad to player controller id assignments
	 */
	static void ResetGamepadAssignments()
	{}

	/*
	* Resets the gamepad assignment to player controller id
	*/
	static void ResetGamepadAssignmentToController(int32 ControllerId)
	{}

	/*
	 * Returns true if controller id assigned to a gamepad
	 */
	static bool IsControllerAssignedToGamepad(int32 ControllerId)
	{
		return (ControllerId == 0);
	}

	/** Copies text to the operating system clipboard. */
	static void ClipboardCopy(const TCHAR* Str);

	/** Pastes in text from the operating system clipboard. */
	static void ClipboardPaste(class FString& Dest);

	/**
	 * Gets the physical size of the screen if possible.  Some platforms lie, some platforms don't know.
	 */
	static EScreenPhysicalAccuracy GetPhysicalScreenDensity(int32& OutScreenDensity);

	/**
	 * Gets the physical size of the screen if possible.  Some platforms lie, some platforms don't know.
	 */
	static EScreenPhysicalAccuracy ComputePhysicalScreenDensity(int32& OutScreenDensity);

	/**
	 * If we know or can approximate the pixel density of the screen we will convert the incoming inches
	 * to pixels on the device.  If the accuracy is unknown OutPixels will be set to 0.
	 */
	static EScreenPhysicalAccuracy ConvertInchesToPixels(float Inches, float& OutPixels);

	/**
	 * If we know or can approximate the pixel density of the screen we will convert the incoming pixels
	 * to inches on the device.  If the accuracy is unknown OutInches will be set to 0.
	 */
	static EScreenPhysicalAccuracy ConvertPixelsToInches(float Pixels, float& OutInches);

protected:
	static bool CachedPhysicalScreenData;
	static EScreenPhysicalAccuracy CachedPhysicalScreenAccuracy;
	static int32 CachedPhysicalScreenDensity;
};
