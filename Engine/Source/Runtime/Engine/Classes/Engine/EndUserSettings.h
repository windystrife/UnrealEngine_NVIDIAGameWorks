// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// EndUserSettings.h
//
// A configuration class that holds information for the end-user of a game.
// You may want to expose these values in your game's UI.
// Supplied so that the engine 'remembers' the last setup the user had.
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/ImportantToggleSettingInterface.h"
#include "EndUserSettings.generated.h"

UCLASS(MinimalAPI, hidecategories=Object, config=Engine, defaultconfig)
class UEndUserSettings : public UObject, public IImportantToggleSettingInterface
{
	GENERATED_UCLASS_BODY()

	/** Determines whether the engine sends anonymous usage information about game sessions to Epic Games in order to improve Unreal Engine. Information will never be shared with 3rd parties. */
	UPROPERTY(EditAnywhere, config, Category=Privacy)
	bool bSendAnonymousUsageDataToEpic;

	/** Determines whether the engine sends anonymous crash/abnormal-shutdown data about game sessions to Epic Games in order to improve Unreal Engine. Information will never be shared with 3rd parties. */
	UPROPERTY(EditAnywhere, config, Category=PrivacyDetails)
	bool bSendMeanTimeBetweenFailureDataToEpic;

	/** If enabled, adds user identifying data to the otherwise anonymous reports sent to Epic Games. */
	UPROPERTY(EditAnywhere, config, Category = PrivacyDetails)
	bool bAllowUserIdInUsageData;

public:
	/** Sets bSendAnonymousUsageDataToEpic. It MUST be set this way when the end user changes the value to trigger the correct state change in the analytics system. */
	void SetSendAnonymousUsageDataToEpic(bool bEnable);

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

private:
	void OnSendAnonymousUsageDataToEpicChanged();
};
