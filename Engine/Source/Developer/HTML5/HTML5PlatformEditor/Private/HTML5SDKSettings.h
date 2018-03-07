// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "HTML5SDKSettings.generated.h"

class IHTML5TargetPlatformModule;
class ITargetPlatformManagerModule;

USTRUCT()
struct FHTML5DeviceMapping
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = HTML5_Devices, Meta = (DisplayName = "Browser name"))
	FString BrowserName;

	UPROPERTY(EditAnywhere, Category = HTML5_Devices, Meta = (DisplayName = "Browser filepath"))
	FFilePath BrowserPath;
};

/**
 * Implements the settings for the HTML5 SDK setup.
 */
UCLASS(config=Engine, globaluserconfig)
class HTML5PLATFORMEDITOR_API UHTML5SDKSettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	// Available browsers that can be used when launching HTML5 builds.
	TArray<FHTML5DeviceMapping> BrowserLauncher;

#if WITH_EDITOR

	// UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
	ITargetPlatformManagerModule * TargetManagerModule;
#endif
private:
	IHTML5TargetPlatformModule* TargetPlatformModule;
};
