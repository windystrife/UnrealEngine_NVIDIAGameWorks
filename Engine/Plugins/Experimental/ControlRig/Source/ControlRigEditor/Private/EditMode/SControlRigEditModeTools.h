// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "IDetailKeyframeHandler.h"
#include "DeclarativeSyntaxSupport.h"

class IDetailsView;
class ISequencer;
class SControlManipulatorPicker;
class SExpandableArea;

class SControlRigEditModeTools : public SCompoundWidget, public IDetailKeyframeHandler
{
public:
	SLATE_BEGIN_ARGS(SControlRigEditModeTools) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

	/** Set the objects to be displayed in the details panel */
	void SetDetailsObjects(const TArray<TWeakObjectPtr<>>& InObjects);

	/** Set the sequencer we are bound to */
	void SetSequencer(TSharedPtr<ISequencer> InSequencer);

	// IDetailKeyframeHandler interface
	virtual bool IsPropertyKeyable(UClass* InObjectClass, const class IPropertyHandle& PropertyHandle) const override;
	virtual bool IsPropertyKeyingEnabled() const override;
	virtual void OnKeyPropertyClicked(const IPropertyHandle& KeyedPropertyHandle) override;

private:
	/** Sequencer we are currently bound to */
	TWeakPtr<ISequencer> WeakSequencer;

	/** The details view we do most of our work within */
	TSharedPtr<IDetailsView> DetailsView;

	/** Special picker for controls */
	TSharedPtr<SControlManipulatorPicker> ControlPicker;

	TSharedPtr<SExpandableArea> PickerExpander;

	/** Display or edit set up for property */
	bool ShouldShowPropertyOnDetailCustomization(const struct FPropertyAndParent& InPropertyAndParent) const;
	bool IsReadOnlyPropertyOnDetailCustomization(const struct FPropertyAndParent& InPropertyAndParent) const;

	/** Called when a manipulator is selected in the picker */
	void OnManipulatorsPicked(const TArray<FName>& Manipulators);
	/** Called when edit mode selection set changes */
	void OnSelectionSetChanged(const TArray<FName>& SelectedManipulators);
};
