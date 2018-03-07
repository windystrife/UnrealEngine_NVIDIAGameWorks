// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "LocalizationTargetTypes.h"
#include "LocalizationSettings.generated.h"

struct FPropertyChangedEvent;
class ULocalizationTargetSet;

// Class for loading/saving configuration settings and the details view objects needed for localization dashboard functionality.
UCLASS(Config=Editor, defaultconfig)
class LOCALIZATION_API ULocalizationSettings : public UObject
{
	GENERATED_BODY()

public:
	ULocalizationSettings(const FObjectInitializer& ObjectInitializer);

private:
	UPROPERTY()
	ULocalizationTargetSet* EngineTargetSet;

	UPROPERTY(config)
	TArray<FLocalizationTargetSettings> EngineTargetsSettings;

	UPROPERTY()
	ULocalizationTargetSet* GameTargetSet;

	UPROPERTY(config)
	TArray<FLocalizationTargetSettings> GameTargetsSettings;

public:
#if WITH_EDITOR
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	static ULocalizationTargetSet* GetEngineTargetSet();
	static ULocalizationTargetSet* GetGameTargetSet();
};

/** Struct containing util functions for getting/setting the SCC settings for the localization dashboard */
struct LOCALIZATION_API FLocalizationSourceControlSettings
{
public:
	/** Checks to see whether source control is available based upon the current editor SCC settings. */
	static bool IsSourceControlAvailable();

	/** Check to see whether we should use SCC when running the localization commandlets. This should be used to optionally pass "-EnableSCC" to the commandlet. */
	static bool IsSourceControlEnabled();

	/** Check to see whether we should automatically submit changed files after running the commandlet. This should be used to optionally pass "-DisableSCCSubmit" to the commandlet. */
	static bool IsSourceControlAutoSubmitEnabled();

	/** Set whether we should use SCC when running the localization commandlets. */
	static void SetSourceControlEnabled(const bool bIsEnabled);

	/** Set whether we should automatically submit changed files after running the commandlet. */
	static void SetSourceControlAutoSubmitEnabled(const bool bIsEnabled);

private:
	static const FString LocalizationSourceControlSettingsCategoryName;
	static const FString SourceControlEnabledSettingName;
	static const FString SourceControlAutoSubmitEnabledSettingName;
};
