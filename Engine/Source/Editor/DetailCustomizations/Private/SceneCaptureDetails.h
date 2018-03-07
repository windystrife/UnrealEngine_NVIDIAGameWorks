// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "IDetailCustomization.h"

class IPropertyHandle;
class IDetailLayoutBuilder;
class USceneCaptureComponent;
enum class ECheckBoxState : uint8;

class FSceneCaptureDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();
private:
	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

private:
	/** The show flags property on the SceneCaptureComponent */
	TSharedPtr<IPropertyHandle> ShowFlagSettingsProperty;

	/**
	* Gets the display state to send to a display filter check box
	*
	* @param The type of checkbox.
	* @return The desired checkbox state.
	*/
	ECheckBoxState OnGetDisplayCheckState(FString ShowFlagName) const;

	/** Show flag settings changed, so update the scene capture */
	void OnShowFlagCheckStateChanged(ECheckBoxState InNewRadioState, FString FlagName);
};
