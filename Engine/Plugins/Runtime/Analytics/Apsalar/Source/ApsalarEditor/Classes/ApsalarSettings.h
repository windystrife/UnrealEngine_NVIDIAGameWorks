// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnalyticsSettings.h"
#include "ApsalarSettings.generated.h"

USTRUCT()
struct FApsalarAnalyticsConfigSetting
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Apsalar, meta=(ConfigRestartRequired=true))
	FString ApiKey;

	UPROPERTY(EditAnywhere, Category=Apsalar, meta=(ConfigRestartRequired=true))
	FString ApiSecret;

	UPROPERTY(EditAnywhere, Category=Apsalar, meta=(ConfigRestartRequired=true))
	int32 SendInterval;

	UPROPERTY(EditAnywhere, Category=Apsalar, meta=(ConfigRestartRequired=true))
	int32 MaxBufferSize;

	UPROPERTY(EditAnywhere, Category=Apsalar, meta=(ConfigRestartRequired=true))
	bool ManuallyReportRevenue;
};

UCLASS()
class UApsalarSettings
	: public UAnalyticsSettingsBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Apsalar, meta=(ConfigRestartRequired=true))
	FApsalarAnalyticsConfigSetting Release;

	UPROPERTY(EditAnywhere, Category=Apsalar, meta=(ConfigRestartRequired=true))
	FApsalarAnalyticsConfigSetting Debug;

	UPROPERTY(EditAnywhere, Category=Apsalar, meta=(ConfigRestartRequired=true))
	FApsalarAnalyticsConfigSetting Test;

	UPROPERTY(EditAnywhere, Category=Apsalar, meta=(ConfigRestartRequired=true))
	FApsalarAnalyticsConfigSetting Development;

// UAnalyticsSettingsBase interface
protected:
	/**
	 * Provides a mechanism to read the section based information into this UObject's properties
	 */
	virtual void ReadConfigSettings();
	/**
	 * Provides a mechanism to save this object's properties to the section based ini values
	 */
	virtual void WriteConfigSettings();

private:
	void ReadConfigStruct(const FString& Section, FApsalarAnalyticsConfigSetting& Dest, FApsalarAnalyticsConfigSetting* Default = nullptr);
	void WriteConfigStruct(const FString& Section, FApsalarAnalyticsConfigSetting& Source);
};
