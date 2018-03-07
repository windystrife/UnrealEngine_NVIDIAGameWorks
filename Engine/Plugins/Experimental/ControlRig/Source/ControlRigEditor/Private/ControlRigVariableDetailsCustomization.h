// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "SlateTypes.h"

class IDetailLayoutBuilder;
class IBlueprintEditor;
class UBlueprint;

class FControlRigVariableDetailsCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedPtr<IDetailCustomization> MakeInstance(TSharedPtr<IBlueprintEditor> InBlueprintEditor);

	FControlRigVariableDetailsCustomization(TSharedPtr<IBlueprintEditor> InBlueprintEditor, UBlueprint* Blueprint)
		: BlueprintEditorPtr(InBlueprintEditor)
		, BlueprintPtr(Blueprint)
	{}

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	bool IsAnimationFlagEnabled(UProperty* PropertyBeingCustomized) const;

	ECheckBoxState IsAnimationOutputChecked(UProperty* PropertyBeingCustomized) const;

	void HandleAnimationOutputCheckStateChanged(ECheckBoxState CheckBoxState, UProperty* PropertyBeingCustomized);

	ECheckBoxState IsAnimationInputChecked(UProperty* PropertyBeingCustomized) const;

	void HandleAnimationInputCheckStateChanged(ECheckBoxState CheckBoxState, UProperty* PropertyBeingCustomized);

private:
	/** The Blueprint editor we are embedded in */
	TWeakPtr<IBlueprintEditor> BlueprintEditorPtr;

	/** The blueprint we are editing */
	TWeakObjectPtr<UBlueprint> BlueprintPtr;
};
