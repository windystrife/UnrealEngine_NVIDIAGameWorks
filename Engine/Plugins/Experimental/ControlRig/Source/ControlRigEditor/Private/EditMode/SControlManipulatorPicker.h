// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"
#include "Delegates/Delegate.h"
#include "UObject/WeakObjectPtr.h"

class UHierarchicalRig;
class SControlManipulatorPicker;
class SCanvas;
class SScaleBox;
class UControlManipulator;
struct FLimbControl;
struct FSpineControl;

/** Delegate executed when manipulator is picked */
DECLARE_DELEGATE_OneParam(FOnManipulatorsPicked, const TArray<FName>& /* ManipulatorNames */);


//////////////////////////////////////////////////////////////////////////

/** Widget to draw for each manipulator in picker */
class SControlManipulatorButton : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SControlManipulatorButton) {}
	SLATE_ARGUMENT(FText, DisplayName)
		SLATE_ARGUMENT(FLinearColor, Color)
		SLATE_ARGUMENT(FLinearColor, SelectedColor)
		SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedPtr<SControlManipulatorPicker> Picker, FName InManipulatorName);

	/** Pointer back to owning Picker */
	TWeakPtr<SControlManipulatorPicker> PickerPtr;
	/** Name of manipulator we represent */
	FName ManipulatorName;
	/** Desired color for this button, dimmed if not active */
	FLinearColor Color;
	/** Selected color for this button */
	FLinearColor SelectedColor;

	/** Return color to draw button */
	FSlateColor GetButtonColor() const;

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
};


//////////////////////////////////////////////////////////////////////////

/** Button for IK/FK switch on a control (limb, spine) */
class SControlKinematicButton : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SControlKinematicButton) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedPtr<SControlManipulatorPicker> Picker, FName InControlName);

	/** Pointer back to owning Picker */
	TWeakPtr<SControlManipulatorPicker> PickerPtr;
	/** Name of control we represent */
	FName ControlName;

	/** Return color to draw button */
	FSlateColor GetButtonColor() const;
	/** Return text to put on button */
	FText GetButtonText() const;
	/** Return tooltip for button */
	FText GetButtonTooltip() const;


	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
};

//////////////////////////////////////////////////////////////////////////

/** Widget that contains picker canvas, used to grab clicks on background etc. */
class SControlManipulatorPickerCanvas : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SControlManipulatorPickerCanvas) {}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedPtr<SControlManipulatorPicker> Picker);

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	/** Repopulate canvas with buttons from the supplied rig */
	void MakeButtonsForRig(UHierarchicalRig* Rig);

	/** Make a button for toggling IK on a control */
	void MakeIKButtonForControl(FName ControlName);

	/** Get position for IK/FK button, given a control name */
	FVector2D GetButtonPosition(FName ControlName) const;

	/** Owning picker */
	TWeakPtr<SControlManipulatorPicker> PickerPtr;
	/** Canvas wiget */
	TSharedPtr<SCanvas> Canvas;
	/** Scale box for resizing to fit */
	TSharedPtr<SScaleBox> ScaleBox;
};

//////////////////////////////////////////////////////////////////////////

/** 2D visual picker for picking control manipulators within a rig  */
class SControlManipulatorPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SControlManipulatorPicker) {}
		SLATE_EVENT(FOnManipulatorsPicked, OnManipulatorsPicked)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);

	/** Set the rig to display manipulators for */
	void SetHierarchicalRig(UHierarchicalRig* InRig);

	/** Set the manipulators that are currently selected */
	void SetSelectedManipulators(const TArray<FName>& Manipulators);

	/** Call when a button is clicked, will fire OnManipulatorsPicked */
	void SelectManipulator(FName ManipulatorName, bool bAddToSelection);
	/** Call when background clicked, will fired OnManipulatorsPicked */
	void ClearSelection();
	/** Select all manipulators */
	void SelectAll();

	/** Returns whether a particular manipulator is selected */
	bool IsManipulatorSelected(FName ManipulatorName);

	/** Returns the rig we are displaying controls for */
	UHierarchicalRig* GetRig() const;

	/** Get limb control by name */
	FLimbControl* GetLimbControl(FName LimbName) const;
	/** Get spine control by name */
	FSpineControl* GetSpineControl(FName SpineName) const;

	/** See if the limb/spine is in IK mode, or FK */
	bool IsControlIK(FName ControlName) const;

	/** Toggle kinematic mode for a limb/spine, by name */
	void ToggleControlKinematicMode(FName ControlName);

protected:

	// For editing UI
	TOptional<float> GetManipPosX() const;
	TOptional<float> GetManipPosY() const;
	TOptional<float> GetManipSizeX() const;
	TOptional<float> GetManipSizeY() const;
	void SetManipPosX(float InPosX, ETextCommit::Type CommitType);
	void SetManipPosY(float InPosY, ETextCommit::Type CommitType);
	void SetManipSizeX(float InSizeX, ETextCommit::Type CommitType);
	void SetManipSizeY(float InSizeY, ETextCommit::Type CommitType);

	EVisibility ShowPickerCanvas() const;
	EVisibility ShowButtonEditingUI() const;

	/** Get pointer to the selected manipulator object */
	UControlManipulator* GetEditedManipulator() const;
	/** Get the default object of the selected manipulator object */
	UControlManipulator* GetEditedManipulatorDefaults() const;

	/** Set of selected manipulator names */
	TArray<FName> SelectedManipulators;

	/** Name of manipulator that we are currently editing */
	FName EditedName;

	/** Canvas widget used to lay out picker buttons */
	TSharedPtr<SControlManipulatorPickerCanvas> PickerCanvas;

	/** Rig we are showing controls for */
	TWeakObjectPtr<UHierarchicalRig> RigPtr;

	/** Delegate to call when control is selected */
	FOnManipulatorsPicked OnManipulatorsPicked;

};