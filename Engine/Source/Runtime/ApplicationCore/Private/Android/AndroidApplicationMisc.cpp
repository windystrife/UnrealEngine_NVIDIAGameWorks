// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidApplicationMisc.h"

#include "AndroidApplication.h"
#include "AndroidErrorOutputDevice.h"
#include "AndroidInputInterface.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Regex.h"
#include "Modules/ModuleManager.h"

void FAndroidApplicationMisc::LoadPreInitModules()
{
	FModuleManager::Get().LoadModule(TEXT("OpenGLDrv"));
	FModuleManager::Get().LoadModule(TEXT("AndroidAudio"));
	FModuleManager::Get().LoadModule(TEXT("AudioMixerAndroid"));
}

class FOutputDeviceError* FAndroidApplicationMisc::GetErrorOutputDevice()
{
	static FAndroidErrorOutputDevice Singleton;
	return &Singleton;
}


GenericApplication* FAndroidApplicationMisc::CreateApplication()
{
	return FAndroidApplication::CreateAndroidApplication();
}

extern void AndroidThunkCpp_Minimize();

void FAndroidApplicationMisc::RequestMinimize()
{
	AndroidThunkCpp_Minimize();
}


extern void AndroidThunkCpp_KeepScreenOn(bool Enable);

bool FAndroidApplicationMisc::ControlScreensaver(EScreenSaverAction Action)
{
	switch (Action)
	{
		case EScreenSaverAction::Disable:
			// Prevent display sleep.
			AndroidThunkCpp_KeepScreenOn(true);
			break;

		case EScreenSaverAction::Enable:
			// Stop preventing display sleep now that we are done.
			AndroidThunkCpp_KeepScreenOn(false);
			break;
	}
	return true;
}

void FAndroidApplicationMisc::ResetGamepadAssignments()
{
	FAndroidInputInterface::ResetGamepadAssignments();
}

void FAndroidApplicationMisc::ResetGamepadAssignmentToController(int32 ControllerId)
{
	FAndroidInputInterface::ResetGamepadAssignmentToController(ControllerId);
}

bool FAndroidApplicationMisc::IsControllerAssignedToGamepad(int32 ControllerId)
{
	return FAndroidInputInterface::IsControllerAssignedToGamepad(ControllerId);
}

void FAndroidApplicationMisc::ClipboardCopy(const TCHAR* Str)
{
	//@todo Android
}

void FAndroidApplicationMisc::ClipboardPaste(class FString& Result)
{
	Result = TEXT("");
	//@todo Android
}

struct FScreenDensity
{
	FString Model;
	bool IsRegex;
	int32 Density;
	
	FScreenDensity()
		: Model()
		, IsRegex(false)
		, Density(0)
	{
	}

	bool InitFromString(const FString& InSourceString)
	{
		Model = TEXT("");
		Density = 0;
		IsRegex = false;

		// The initialization is only successful if the Model and Density values can all be parsed from the string
		const bool bSuccessful = FParse::Value(*InSourceString, TEXT("Model="), Model) && FParse::Value(*InSourceString, TEXT("Density="), Density);

		// Regex= is optional, it lets us know if this model requires regular expression matching, which is much more expensive.
		FParse::Bool(*InSourceString, TEXT("IsRegex="), IsRegex);

		return bSuccessful;
	}

	bool IsMatch(const FString& InDeviceModel) const
	{
		if ( IsRegex )
		{
			const FRegexPattern RegexPattern(Model);
			FRegexMatcher RegexMatcher(RegexPattern, InDeviceModel);

			return RegexMatcher.FindNext();
		}
		else
		{
			return Model == InDeviceModel;
		}
	}
};

static float GetWindowUpscaleFactor()
{
	// Determine the difference between the native resolution of the device, and the size of our window,
	// and return that scalar.

	int32_t SurfaceWidth, SurfaceHeight;
	FAndroidWindow::CalculateSurfaceSize(FAndroidWindow::GetHardwareWindow(), SurfaceWidth, SurfaceHeight);

	FPlatformRect ScreenRect = FAndroidWindow::GetScreenRect();

	const float CalculatedScaleFactor = FVector2D(ScreenRect.Right - ScreenRect.Left, ScreenRect.Bottom - ScreenRect.Top).Size() / FVector2D(SurfaceWidth, SurfaceHeight).Size();

	return CalculatedScaleFactor;
}

extern FString AndroidThunkCpp_GetMetaDataString(const FString& Key);

EScreenPhysicalAccuracy FAndroidApplicationMisc::ComputePhysicalScreenDensity(int32& OutScreenDensity)
{
	FString MyDeviceModel = FPlatformMisc::GetDeviceModel();

	TArray<FString> DeviceStrings;
	GConfig->GetArray(TEXT("DeviceScreenDensity"), TEXT("Devices"), DeviceStrings, GEngineIni);

	TArray<FScreenDensity> Devices;
	for ( const FString& DeviceString : DeviceStrings )
	{
		FScreenDensity DensityEntry;
		if ( DensityEntry.InitFromString(DeviceString) )
		{
			Devices.Add(DensityEntry);
		}
	}

	for ( const FScreenDensity& Device : Devices )
	{
		if ( Device.IsMatch(MyDeviceModel) )
		{
			OutScreenDensity = Device.Density * GetWindowUpscaleFactor();
			return EScreenPhysicalAccuracy::Truth;
		}
	}

	FString DPIStrings = AndroidThunkCpp_GetMetaDataString(TEXT("ue4.displaymetrics.dpi"));
	TArray<FString> DPIValues;
	DPIStrings.ParseIntoArray(DPIValues, TEXT(","));

	float xdpi, ydpi;
	LexicalConversion::FromString(xdpi, *DPIValues[0]);
	LexicalConversion::FromString(ydpi, *DPIValues[1]);

	OutScreenDensity = ( xdpi + ydpi ) / 2.0f;

	if ( OutScreenDensity <= 0 || OutScreenDensity > 2000 )
	{
		return EScreenPhysicalAccuracy::Unknown;
	}

	OutScreenDensity *= GetWindowUpscaleFactor();
	return EScreenPhysicalAccuracy::Approximation;
}
