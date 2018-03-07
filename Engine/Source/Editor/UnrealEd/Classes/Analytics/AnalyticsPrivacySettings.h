// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// AnalyticsPrivacySettings
//
// A configuration class that holds information for the user's privacy settings.
// Supplied so that the editor 'remembers' the last setup the user had.
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/ImportantToggleSettingInterface.h"
#include "AnalyticsPrivacySettings.generated.h"

UCLASS(MinimalAPI, hidecategories=Object, config=EditorSettings)
class UAnalyticsPrivacySettings : public UObject, public IImportantToggleSettingInterface
{
	GENERATED_UCLASS_BODY()

	/** Determines whether the editor sends usage information to Epic Games in order to improve Unreal Engine. Your information will never be shared with 3rd parties. */
	UPROPERTY(EditAnywhere, config, Category=Options)
	bool bSendUsageData;

public:

	// BEGIN IImportantToggleSettingInterface
	virtual void GetToogleCategoryAndPropertyNames(FName& OutCategory, FName& OutProperty) const override;
	virtual FText GetFalseStateLabel() const override;
	virtual FText GetFalseStateTooltip() const override;
	virtual FText GetFalseStateDescription() const override;
	virtual FText GetTrueStateLabel() const override;
	virtual FText GetTrueStateTooltip() const override;
	virtual FText GetTrueStateDescription() const override;
	virtual FString GetAdditionalInfoUrl() const override;
	virtual FText GetAdditionalInfoUrlLabel() const override;
	// END IImportantToggleSettingInterface

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	void OnSendFullUsageDataChanged();
#endif
};
