// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSRuntimeSettings.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/EngineBuildSettings.h"
#include "HAL/IConsoleManager.h"

UIOSRuntimeSettings::UIOSRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bEnableGameCenterSupport = true;
	bEnableCloudKitSupport = false;
	bSupportsPortraitOrientation = true;
	BundleDisplayName = TEXT("UE4 Game");
	BundleName = TEXT("MyUE4Game");
	BundleIdentifier = TEXT("com.YourCompany.GameNameNoSpaces");
	VersionInfo = TEXT("1.0.0");
    FrameRateLock = EPowerUsageFrameRateLock::PUFRL_30;
	bSupportsIPad = true;
	bSupportsIPhone = true;
	MinimumiOSVersion = EIOSVersion::IOS_9;
	EnableRemoteShaderCompile = false;
	bGeneratedSYMFile = false;
	bGeneratedSYMBundle = false;
	bGenerateXCArchive = false;
	bDevForArmV7 = false;
	bDevForArm64 = true;
	bDevForArmV7S = false;
	bShipForArmV7 = false;
	bShipForArm64 = true;
	bShipForArmV7S = false;
	bShipForBitcode = false;
	bUseRSync = true;
	AdditionalPlistData = TEXT("");
	AdditionalLinkerFlags = TEXT("");
	AdditionalShippingLinkerFlags = TEXT("");
	bTreatRemoteAsSeparateController = false;
	bAllowRemoteRotation = true;
	bUseRemoteAsVirtualJoystick = true;
	bUseRemoteAbsoluteDpadValues = false;
    bEnableRemoteNotificationsSupport = false;
	bSupportsOpenGLES2 = false;
	bSupportsMetal = true;
	bSupportsMetalMRT = false;
}

#if WITH_EDITOR
void UIOSRuntimeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Ensure that at least one orientation is supported
	if (!bSupportsPortraitOrientation && !bSupportsUpsideDownOrientation && !bSupportsLandscapeLeftOrientation && !bSupportsLandscapeRightOrientation)
	{
		bSupportsPortraitOrientation = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bSupportsPortraitOrientation)), GetDefaultConfigFilename());
	}

	// Ensure that at least one API is supported
	if (!bSupportsMetal && !bSupportsMetalMRT)
	{
		bSupportsMetal = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bSupportsMetal)), GetDefaultConfigFilename());
	}
	if (bSupportsOpenGLES2)
	{
		bSupportsOpenGLES2 = false;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bSupportsOpenGLES2)), GetDefaultConfigFilename());
	}

	// Ensure that at least arm64 is selected for shipping and dev
	if (!bDevForArm64)
	{
		bDevForArm64 = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bDevForArm64)), GetDefaultConfigFilename());
	}
	if (bDevForArmV7)
	{
		bDevForArmV7 = false;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bDevForArmV7)), GetDefaultConfigFilename());
	}
	if (bDevForArmV7S)
	{
		bDevForArmV7S = false;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bDevForArmV7S)), GetDefaultConfigFilename());
	}
	if (!bShipForArm64)
	{
		bShipForArm64 = true;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bShipForArm64)), GetDefaultConfigFilename());
	}
	if (bShipForArmV7)
	{
		bShipForArmV7 = false;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bShipForArmV7)), GetDefaultConfigFilename());
	}
	if (bShipForArmV7S)
	{
		bShipForArmV7S = false;
		UpdateSinglePropertyInConfigFile(GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UIOSRuntimeSettings, bShipForArmV7S)), GetDefaultConfigFilename());
	}
}


void UIOSRuntimeSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// We can have a look for potential keys
	if (!RemoteServerName.IsEmpty() && !RSyncUsername.IsEmpty())
	{
		SSHPrivateKeyLocation = TEXT("");

		const FString DefaultKeyFilename = TEXT("RemoteToolChainPrivate.key");
		const FString RelativeFilePathLocation = FPaths::Combine(TEXT("SSHKeys"), *RemoteServerName, *RSyncUsername, *DefaultKeyFilename);
		TCHAR Path[4096];
		FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"), Path, ARRAY_COUNT(Path));

		TArray<FString> PossibleKeyLocations;
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::ProjectDir(), TEXT("Build"), TEXT("NotForLicensees"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::ProjectDir(), TEXT("Build"), TEXT("NoRedist"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::ProjectDir(), TEXT("Build"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::EngineDir(), TEXT("Build"), TEXT("NotForLicensees"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::EngineDir(), TEXT("Build"), TEXT("NoRedist"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(*FPaths::EngineDir(), TEXT("Build"), *RelativeFilePathLocation));
		PossibleKeyLocations.Add(FPaths::Combine(Path, TEXT("Unreal Engine"), TEXT("UnrealBuildTool"), *RelativeFilePathLocation));

		// Find a potential path that we will use if the user hasn't overridden.
		// For information purposes only
		for (const FString& NextLocation : PossibleKeyLocations)
		{
			if (IFileManager::Get().FileSize(*NextLocation) > 0)
			{
				SSHPrivateKeyLocation = NextLocation;
				break;
			}
		}
	}

	// switch IOS_6.1, IOS_7, and IOS_8 to IOS_9
	if (MinimumiOSVersion < EIOSVersion::IOS_9)
	{
		MinimumiOSVersion = EIOSVersion::IOS_9;
	}
	if (bSupportsOpenGLES2)
	{
		bSupportsOpenGLES2 = false;
	}
	if (bDevForArmV7)
	{
		bDevForArmV7 = false;
	}
	if (bDevForArmV7S)
	{
		bDevForArmV7S = false;
	}
	if (bShipForArmV7)
	{
		bShipForArmV7 = false;
	}
	if (bShipForArmV7S)
	{
		bShipForArmV7S = false;
	}
	if (!bSupportsMetal && !bSupportsMetalMRT)
	{
		bSupportsMetal = true;
	}
	if (!bDevForArm64)
	{
		bDevForArm64 = true;
	}
	if (!bShipForArm64)
	{
		bShipForArm64 = true;
	}
	
	// Due to a driver bug on A8 devices running iOS 9 we can only support the global clip-plane when running iOS 10+
	static IConsoleVariable* ClipPlaneCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowGlobalClipPlane"));
	if (ClipPlaneCVar && ClipPlaneCVar->GetInt() != 0 && MinimumiOSVersion < EIOSVersion::IOS_10)
	{
		MinimumiOSVersion = EIOSVersion::IOS_10;
	}
}
#endif
