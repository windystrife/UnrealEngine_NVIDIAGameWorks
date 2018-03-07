// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformApplicationMisc.h"

struct APPLICATIONCORE_API FWindowsPlatformApplicationMisc : public FGenericPlatformApplicationMisc
{
	static void LoadPreInitModules();
	static void LoadStartupModules();
	static class FOutputDeviceConsole* CreateConsoleOutputDevice();
	static class FOutputDeviceError* GetErrorOutputDevice();
	static class FFeedbackContext* GetFeedbackContext();
	static class GenericApplication* CreateApplication();
	static void RequestMinimize();
	static bool IsThisApplicationForeground();
	static int32 GetAppIcon();
	static void PumpMessages(bool bFromMainLoop);
	static void PreventScreenSaver();
	static struct FLinearColor GetScreenPixelColor(const FVector2D& InScreenPos, float InGamma = 1.0f);
	static bool GetWindowTitleMatchingText(const TCHAR* TitleStartsWith, FString& OutTitle);
	static float GetDPIScaleFactorAtPoint(float X, float Y);
	static void ClipboardCopy(const TCHAR* Str);
	static void ClipboardPaste(class FString& Dest);
};

typedef FWindowsPlatformApplicationMisc FPlatformApplicationMisc;
