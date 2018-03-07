// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatformApplicationMisc.h"
#include "GenericApplication.h"
#include "Misc/OutputDeviceAnsiError.h"
#include "HAL/FeedbackContextAnsi.h"
#include "Math/Color.h"

#include "PlatformApplicationMisc.h"

/** Hooks for moving ClipboardCopy and ClipboardPaste into FPlatformApplicationMisc */
CORE_API extern void (*ClipboardCopyShim)(const TCHAR* Text);
CORE_API extern void (*ClipboardPasteShim)(FString& Dest);

bool FGenericPlatformApplicationMisc::CachedPhysicalScreenData = false;
EScreenPhysicalAccuracy FGenericPlatformApplicationMisc::CachedPhysicalScreenAccuracy = EScreenPhysicalAccuracy::Unknown;
int32 FGenericPlatformApplicationMisc::CachedPhysicalScreenDensity = 0;

void FGenericPlatformApplicationMisc::PreInit()
{
}

void FGenericPlatformApplicationMisc::Init()
{
	ClipboardCopyShim = &FPlatformApplicationMisc::ClipboardCopy;
	ClipboardPasteShim = &FPlatformApplicationMisc::ClipboardPaste;
}

void FGenericPlatformApplicationMisc::PostInit()
{
}

void FGenericPlatformApplicationMisc::TearDown()
{
	ClipboardCopyShim = nullptr;
	ClipboardPasteShim = nullptr;
}

class FOutputDeviceConsole* FGenericPlatformApplicationMisc::CreateConsoleOutputDevice()
{
	return nullptr; // normally only used for PC
}

class FOutputDeviceError* FGenericPlatformApplicationMisc::GetErrorOutputDevice()
{
	static FOutputDeviceAnsiError Singleton;
	return &Singleton;
}

class FFeedbackContext* FGenericPlatformApplicationMisc::GetFeedbackContext()
{
	static FFeedbackContextAnsi Singleton;
	return &Singleton;
}

GenericApplication* FGenericPlatformApplicationMisc::CreateApplication()
{
	return new GenericApplication( nullptr );
}

void FGenericPlatformApplicationMisc::RequestMinimize()
{
}

bool FGenericPlatformApplicationMisc::IsThisApplicationForeground()
{
	UE_LOG(LogHAL, Fatal, TEXT("FGenericPlatformProcess::IsThisApplicationForeground not implemented on this platform"));
	return false;
}

FLinearColor FGenericPlatformApplicationMisc::GetScreenPixelColor(const struct FVector2D& InScreenPos, float InGamma)
{ 
	return FLinearColor::Black;
}

void FGenericPlatformApplicationMisc::ClipboardCopy(const TCHAR* Str)
{

}
void FGenericPlatformApplicationMisc:: ClipboardPaste(class FString& Dest)
{
	Dest = FString();
}

EScreenPhysicalAccuracy FGenericPlatformApplicationMisc::GetPhysicalScreenDensity(int32& ScreenDensity)
{
	if ( !CachedPhysicalScreenData )
	{
		CachedPhysicalScreenData = true;
		CachedPhysicalScreenAccuracy = FPlatformApplicationMisc::ComputePhysicalScreenDensity(CachedPhysicalScreenDensity);
	}

	ScreenDensity = CachedPhysicalScreenDensity;
	return CachedPhysicalScreenAccuracy;
}

EScreenPhysicalAccuracy FGenericPlatformApplicationMisc::ConvertInchesToPixels(float Inches, float& OutPixels)
{
	int32 ScreenDensity;
	EScreenPhysicalAccuracy Accuracy = GetPhysicalScreenDensity(ScreenDensity);
	
	if ( Accuracy != EScreenPhysicalAccuracy::Unknown )
	{
		OutPixels = Inches * ScreenDensity;
	}
	else
	{
		OutPixels = 0;
	}

	return Accuracy;
}

EScreenPhysicalAccuracy FGenericPlatformApplicationMisc::ConvertPixelsToInches(float Pixels, float& OutInches)
{
	int32 ScreenDensity;
	EScreenPhysicalAccuracy Accuracy = GetPhysicalScreenDensity(ScreenDensity);

	if ( Accuracy != EScreenPhysicalAccuracy::Unknown )
	{
		OutInches = Pixels / (float)ScreenDensity;
	}
	else
	{
		OutInches = 0;
	}

	return Accuracy;
}

EScreenPhysicalAccuracy FGenericPlatformApplicationMisc::ComputePhysicalScreenDensity(int32& ScreenDensity)
{
	ScreenDensity = 0;
	return EScreenPhysicalAccuracy::Unknown;
}

