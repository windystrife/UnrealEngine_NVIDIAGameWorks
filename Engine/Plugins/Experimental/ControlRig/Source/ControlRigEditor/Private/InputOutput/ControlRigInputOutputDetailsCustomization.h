// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "SlateTypes.h"
#include "UObject/WeakObjectPtr.h"

class IDetailLayoutBuilder;
class UK2Node_ControlRig;

class FControlRigInputOutputDetailsCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	ECheckBoxState IsAnimationPinChecked(FName InName, bool bInput) const;

	void HandleAnimationPinCheckStateChanged(ECheckBoxState CheckBoxState, FName InName, bool bInput);

	void CustomizeHierarchicalDetails(IDetailLayoutBuilder& DetailLayout);

private:
	/** Whether to show input parameters */
	bool bShowInputs;

	/** Whether to show output parameters */
	bool bShowOutputs;

	/** The ControlRigs we are currently editing */
	TArray<TWeakObjectPtr<UK2Node_ControlRig>> ControlRigs;

	/** Undo multicaster for skeleton tree */
	FSimpleMulticastDelegate OnUndoRedo;
};
