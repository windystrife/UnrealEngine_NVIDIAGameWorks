// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SControlManipulatorPicker.h"
#include "SCanvas.h"
#include "SBox.h"
#include "SImage.h"
#include "SBorder.h"
#include "SButton.h"
#include "SOverlay.h"
#include "SNumericEntryBox.h"
#include "SScaleBox.h"
#include "Rigs/ControlManipulator.h"
#include "Rigs/HierarchicalRig.h"
#include "Rigs/HumanRig.h"
#include "EditorStyleSet.h"
#include "SButton.h"
#include "ScopedTransaction.h"
#include "ControlRigEditMode.h"
#include "EditorModeManager.h"


#define LOCTEXT_NAMESPACE "ControlManipulatorPicker"

static bool bShowButtonEditing = false;

static FName LeftArmLimbName(TEXT("LeftArm"));
static FName RightArmLimbName(TEXT("RightArm"));
static FName LeftLegLimbName(TEXT("LeftLeg"));
static FName RightLegLimbName(TEXT("RightLeg"));
static FName SpineControlName(TEXT("Spine"));

static FText IKText = LOCTEXT("IK", "IK");
static FText FKText = LOCTEXT("FK", "FK");

void SControlKinematicButton::Construct(const FArguments& InArgs, TSharedPtr<SControlManipulatorPicker> Picker, FName InControlName)
{
	PickerPtr = Picker;
	ControlName = InControlName;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(1.f)
		.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.f))
		.ToolTipText(this, &SControlKinematicButton::GetButtonTooltip)
		[
			SNew(SOverlay)

			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image(&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Button").Normal)
				.ColorAndOpacity(this, &SControlKinematicButton::GetButtonColor)
			]

			+ SOverlay::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(FEditorStyle::GetFontStyle("BoldFont"))
				.ColorAndOpacity(FLinearColor::Black)
				.Text(this, &SControlKinematicButton::GetButtonText)
			]
		]
	];
}

FSlateColor SControlKinematicButton::GetButtonColor() const
{
	TSharedPtr<SControlManipulatorPicker> Picker = PickerPtr.Pin();
	bool bIsIK = Picker->IsControlIK(ControlName);
	return bIsIK ? FLinearColor(0.9f, 0.8f, 0.2f, 1.f) : FLinearColor(0.9f, 0.2f, 0.8f, 1.f);
}

FText SControlKinematicButton::GetButtonText() const
{
	TSharedPtr<SControlManipulatorPicker> Picker = PickerPtr.Pin();
	bool bIsIK = Picker->IsControlIK(ControlName);
	return bIsIK ? IKText : FKText;
}

FText SControlKinematicButton::GetButtonTooltip() const
{
	TSharedPtr<SControlManipulatorPicker> Picker = PickerPtr.Pin();
	bool bIsIK = Picker->IsControlIK(ControlName);
	return FText::Format(LOCTEXT("FailedTemplateCopy", "Switch {0} to {1}"), FText::FromName(ControlName), bIsIK ? FKText : IKText);
}

FReply SControlKinematicButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<SControlManipulatorPicker> Picker = PickerPtr.Pin();
	Picker->ToggleControlKinematicMode(ControlName);
	return FReply::Handled();
}

//////////////////////////////////////////////////////////////////////////

void SControlManipulatorButton::Construct(const FArguments& InArgs, TSharedPtr<SControlManipulatorPicker> Picker, FName InManipulatorName)
{
	PickerPtr = Picker;
	ManipulatorName = InManipulatorName;
	Color = InArgs._Color;
	SelectedColor = InArgs._SelectedColor;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(1.f)
		.BorderImage(FEditorStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.f))
		.ToolTipText(InArgs._DisplayName)
		[
			SNew(SImage)
			.Image(&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Button").Normal)
			.ColorAndOpacity(this, &SControlManipulatorButton::GetButtonColor)
		]
	];
}


FSlateColor SControlManipulatorButton::GetButtonColor() const
{
	TSharedPtr<SControlManipulatorPicker> Picker = PickerPtr.Pin();

	bool bSelected = Picker->IsManipulatorSelected(ManipulatorName);

	bool bEnabled = false;
	UHierarchicalRig* Rig = Picker->GetRig();
	if (Rig)
	{
		UControlManipulator* Manip = Rig->FindManipulator(ManipulatorName);
		if (Manip)
		{
			bEnabled = Rig->IsManipulatorEnabled(Manip);
		}
	}

	// Get regular or selected color
	FLinearColor DesiredColor = bSelected ? SelectedColor : Color;
	// Dim if disabled
	return bEnabled ? DesiredColor : DesiredColor * 0.4f;
}


FReply SControlManipulatorButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<SControlManipulatorPicker> Picker = PickerPtr.Pin();
	Picker->SelectManipulator(ManipulatorName, MouseEvent.IsControlDown() || MouseEvent.IsShiftDown());
	return FReply::Handled();
}

//////////////////////////////////////////////////////////////////////////

void SControlManipulatorPickerCanvas::Construct(const FArguments& InArgs, TSharedPtr<SControlManipulatorPicker> Picker)
{
	PickerPtr = Picker;

	ChildSlot
	[
		SAssignNew(ScaleBox, SScaleBox)
		.Stretch(EStretch::ScaleToFit)
		.StretchDirection(EStretchDirection::DownOnly)
		[
			SNew(SBox)
			.WidthOverride(300)
			.HeightOverride(400)
			.Content()
			[
				SAssignNew(Canvas, SCanvas)
			]
		]
	];
}

FReply SControlManipulatorPickerCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<SControlManipulatorPicker> Picker = PickerPtr.Pin();
	Picker->ClearSelection();
	return FReply::Handled();
}

FVector2D SControlManipulatorPickerCanvas::GetButtonPosition(FName ControlName) const
{
	TSharedPtr<SControlManipulatorPicker> Picker = PickerPtr.Pin();

	if (FLimbControl* Limb = Picker->GetLimbControl(ControlName))
	{
		return Limb->PickerIKTogglePos;
	}
	else if (FSpineControl* Spine = Picker->GetSpineControl(ControlName))
	{
		return Spine->PickerIKTogglePos;
	}
	else
	{
		return FVector2D(0,0);
	}
}

void SControlManipulatorPickerCanvas::MakeIKButtonForControl(FName ControlName)
{
	TSharedPtr<SControlManipulatorPicker> Picker = PickerPtr.Pin();

	Canvas->AddSlot()
	.Position(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateSP(this, &SControlManipulatorPickerCanvas::GetButtonPosition, ControlName)))
	.Size(FVector2D(30, 20))
	[
		SNew(SControlKinematicButton, Picker, ControlName)
	];
}

void SControlManipulatorPickerCanvas::MakeButtonsForRig(UHierarchicalRig* Rig)
{
	// Remove any existing buttons
	Canvas->ClearChildren();

	if (Rig)
	{
		TSharedPtr<SControlManipulatorPicker> Picker = PickerPtr.Pin();

		// Add button for each manipulator
		for (const UControlManipulator* Manipulator : Rig->Manipulators)
		{
			// Grab color from manipulator
			FLinearColor Color = FLinearColor::White;
			FLinearColor SelectedColor = FLinearColor::Red;
			const UColoredManipulator* ColoredManip = Cast<UColoredManipulator>(Manipulator);
			if (ColoredManip)
			{
				Color = ColoredManip->Color;
				SelectedColor = ColoredManip->SelectedColor;
			}

			Canvas->AddSlot()
			.HAlign(HAlign_Center)
			.Position(Manipulator->PickerPos)
			.Size(Manipulator->PickerSize)
			[
				SNew(SControlManipulatorButton, Picker, Manipulator->Name)
				.DisplayName(Manipulator->DisplayName)
				.Color(Color)
				.SelectedColor(SelectedColor)
			];
		}

		// Add 'select all' button in top left
		Canvas->AddSlot()
		.Position(FVector2D(20, 10))
		.Size(FVector2D(70, 20))
		[
			SNew(SButton)
			.OnPressed(Picker.Get(), &SControlManipulatorPicker::SelectAll)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SelectAll","Select All"))
			]
		];

		// If a human rig, make limb IK/FK switch buttons
		UHumanRig* HumanRig = Cast<UHumanRig>(Rig);
		if (HumanRig)
		{
			MakeIKButtonForControl(LeftArmLimbName);
			MakeIKButtonForControl(RightArmLimbName);
			MakeIKButtonForControl(LeftLegLimbName);
			MakeIKButtonForControl(RightLegLimbName);
			MakeIKButtonForControl(SpineControlName);
		}
	}
}


//////////////////////////////////////////////////////////////////////////

void SControlManipulatorPicker::Construct(const FArguments& InArgs)
{
	EditedName = NAME_None;
	OnManipulatorsPicked = InArgs._OnManipulatorsPicked;

	ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(PickerCanvas, SControlManipulatorPickerCanvas, SharedThis(this))
			.Visibility(this, &SControlManipulatorPicker::ShowPickerCanvas)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.Visibility(this, &SControlManipulatorPicker::ShowButtonEditingUI)
			.Content()
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Pos:", "Pos:"))
				]

				+SHorizontalBox::Slot()
				.Padding(2.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value(this, &SControlManipulatorPicker::GetManipPosX)
					.OnValueCommitted(this, &SControlManipulatorPicker::SetManipPosX)
				]

				+ SHorizontalBox::Slot()
				.Padding(2.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value(this, &SControlManipulatorPicker::GetManipPosY)
					.OnValueCommitted(this, &SControlManipulatorPicker::SetManipPosY)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Size:", "Size:"))
				]

				+ SHorizontalBox::Slot()
				.Padding(2.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value(this, &SControlManipulatorPicker::GetManipSizeX)
					.OnValueCommitted(this, &SControlManipulatorPicker::SetManipSizeX)
				]

				+ SHorizontalBox::Slot()
				.Padding(2.f)
				[
					SNew(SNumericEntryBox<float>)
					.Value(this, &SControlManipulatorPicker::GetManipSizeY)
					.OnValueCommitted(this, &SControlManipulatorPicker::SetManipSizeY)
				]
			]
		]
	];

}

void SControlManipulatorPicker::SetHierarchicalRig(UHierarchicalRig* InRig)
{
	if (InRig != RigPtr.Get())
	{
		RigPtr = InRig;
		PickerCanvas->MakeButtonsForRig(GetRig());
	}
}

void SControlManipulatorPicker::SetSelectedManipulators(const TArray<FName>& Manipulators)
{
	SelectedManipulators = Manipulators;
}


void SControlManipulatorPicker::SelectManipulator(FName ManipulatorName, bool bAddToSelection)
{
	EditedName = ManipulatorName;

	// If not adding to selection, empty selection first
	if (!bAddToSelection)
	{
		SelectedManipulators.Empty();
	}
	// Add name to selection
	SelectedManipulators.AddUnique(ManipulatorName);

	OnManipulatorsPicked.ExecuteIfBound(SelectedManipulators);
}

void SControlManipulatorPicker::ClearSelection()
{
	EditedName = NAME_None;
	SelectedManipulators.Empty();
	OnManipulatorsPicked.ExecuteIfBound(SelectedManipulators);
}

void SControlManipulatorPicker::SelectAll()
{
	UHierarchicalRig* Rig = GetRig();
	if (Rig)
	{
		// Make array of all manipulator names
		SelectedManipulators.Empty();
		for (UControlManipulator* Manip : Rig->Manipulators)
		{
			SelectedManipulators.Add(Manip->Name);
		}
		OnManipulatorsPicked.ExecuteIfBound(SelectedManipulators);
	}
}


bool SControlManipulatorPicker::IsManipulatorSelected(FName ManipulatorName)
{
	return SelectedManipulators.Contains(ManipulatorName);
}

bool SControlManipulatorPicker::IsControlIK(FName ControlName) const
{
	if (FLimbControl* Limb = GetLimbControl(ControlName))
	{
		return Limb->IKSpaceMode == EIKSpaceMode::IKMode;
	}
	else if (FSpineControl* Spine = GetSpineControl(ControlName))
	{
		return Spine->IKSpaceMode == EIKSpaceMode::IKMode;
	}
	else
	{
		return false;
	}
}

static void KeyManipulatorForNode(UHumanRig* HumanRig, FName Node, FControlRigEditMode* ControlRigEditMode)
{
	UControlManipulator* Manipulator = HumanRig->FindManipulatorForNode(Node);
	if (Manipulator)
	{
		ControlRigEditMode->SetKeyForManipulator(HumanRig, Manipulator);
	}

}

void SControlManipulatorPicker::ToggleControlKinematicMode(FName ControlName)
{
	UHumanRig* HumanRig = Cast<UHumanRig>(GetRig());
	FLimbControl* Limb = GetLimbControl(ControlName);
	FSpineControl* Spine = GetSpineControl(ControlName);
	if (HumanRig && (Limb || Spine))
	{
		const FScopedTransaction Transaction(LOCTEXT("ToggleKinematicMode", "Toggle IK/FK"));
		HumanRig->Modify(true);
		// Need to build a property chain and call PreEditChange
		FEditPropertyChain EditPropertyChain;

		// Get property of control
		UStructProperty* ControlProperty = CastChecked<UStructProperty>(UHumanRig::StaticClass()->FindPropertyByName(ControlName));
		EditPropertyChain.AddTail(ControlProperty);

		// Then get IKSpaceMode member of that struct
		static FName IKSpaceModeName(TEXT("IKSpaceMode"));
		UProperty* IKSpaceModeProperty = ControlProperty->Struct->FindPropertyByName(IKSpaceModeName);
		EditPropertyChain.AddTail(IKSpaceModeProperty);

		EditPropertyChain.SetActiveMemberPropertyNode(ControlProperty);
		EditPropertyChain.SetActivePropertyNode(IKSpaceModeProperty);

		HumanRig->PreEditChange(EditPropertyChain);

		// Change IK space property
		if (Limb)
		{
			Limb->IKSpaceMode = (Limb->IKSpaceMode == EIKSpaceMode::IKMode) ? EIKSpaceMode::FKMode : EIKSpaceMode::IKMode;

			// Update IK space here, so that we know it's up to date
			HumanRig->CorrectIKSpace(*Limb);
		}
		else
		{
			Spine->IKSpaceMode = (Spine->IKSpaceMode == EIKSpaceMode::IKMode) ? EIKSpaceMode::FKMode : EIKSpaceMode::IKMode;

			// Update IK space here, so that we know it's up to date
			HumanRig->CorrectIKSpace(*Spine);
		}

		// now we have to update properties, so that it pushes code changes to properties			
		HumanRig->UpdateManipulatorToNode(true);

		// Build a PropertyChangedEvent and call PoseEditChangeProperty
		FPropertyChangedEvent PropertyChangedEvent(IKSpaceModeProperty);
		PropertyChangedEvent.ChangeType = EPropertyChangeType::ValueSet;
		HumanRig->PostEditChangeProperty(PropertyChangedEvent);

		// Create keys for all manipulators for this limb
		FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName));
		if (ControlRigEditMode)
		{
			if (Limb)
			{	
				KeyManipulatorForNode(HumanRig, Limb->FKChainName[0], ControlRigEditMode);
				KeyManipulatorForNode(HumanRig, Limb->FKChainName[1], ControlRigEditMode);
				KeyManipulatorForNode(HumanRig, Limb->FKChainName[2], ControlRigEditMode);

				KeyManipulatorForNode(HumanRig, Limb->IKEffectorName, ControlRigEditMode);
				KeyManipulatorForNode(HumanRig, Limb->IKJointTargetName, ControlRigEditMode);
			}
			else
			{
				// @TODO Key Spine
			}
		}
	}
}


UHierarchicalRig* SControlManipulatorPicker::GetRig() const
{
	return RigPtr.Get();
}

FSpineControl* SControlManipulatorPicker::GetSpineControl(FName SpineName) const
{
	FSpineControl* Spine = nullptr;

	UHumanRig* HumanRig = Cast<UHumanRig>(GetRig());
	if (HumanRig)
	{
		if (SpineName == SpineControlName)
		{
			Spine = &HumanRig->Spine;
		}
	}

	return Spine;
}

FLimbControl* SControlManipulatorPicker::GetLimbControl(FName LimbName) const
{
	FLimbControl* Limb = nullptr;

	UHumanRig* HumanRig = Cast<UHumanRig>(GetRig());
	if (HumanRig)
	{
		if (LimbName == LeftArmLimbName)
		{
			Limb = &HumanRig->LeftArm;
		}
		else if (LimbName == RightArmLimbName)
		{
			Limb = &HumanRig->RightArm;
		}
		else if (LimbName == LeftLegLimbName)
		{
			Limb = &HumanRig->LeftLeg;
		}
		else if (LimbName == RightLegLimbName)
		{
			Limb = &HumanRig->RightLeg;
		}
	}

	return Limb;
}


UControlManipulator* SControlManipulatorPicker::GetEditedManipulator() const
{
	UControlManipulator* Manipulator = nullptr;

	UHierarchicalRig* Rig = RigPtr.Get();
	if (Rig)
	{
		Manipulator = Rig->FindManipulator(EditedName);
	}

	return Manipulator;
}

UControlManipulator* SControlManipulatorPicker::GetEditedManipulatorDefaults() const
{
	UControlManipulator* Manipulator = nullptr;

	UHierarchicalRig* Rig = RigPtr.Get();
	UHierarchicalRig* RigCDO = (Rig) ? Rig->GetClass()->GetDefaultObject<UHierarchicalRig>() : nullptr;

	if (RigCDO)
	{
		Manipulator = RigCDO->FindManipulator(EditedName); // Calling func on CDO should be ok
	}

	return Manipulator;
}


TOptional<float> SControlManipulatorPicker::GetManipPosX() const
{
	UControlManipulator* Manipulator = GetEditedManipulator();
	return (Manipulator) ? Manipulator->PickerPos.X : 0.f;
}

TOptional<float> SControlManipulatorPicker::GetManipPosY() const
{
	UControlManipulator* Manipulator = GetEditedManipulator();
	return (Manipulator) ? Manipulator->PickerPos.Y : 0.f;
}

TOptional<float> SControlManipulatorPicker::GetManipSizeX() const
{
	UControlManipulator* Manipulator = GetEditedManipulator();
	return (Manipulator) ? Manipulator->PickerSize.X : 0.f;
}

TOptional<float> SControlManipulatorPicker::GetManipSizeY() const
{
	UControlManipulator* Manipulator = GetEditedManipulator();
	return (Manipulator) ? Manipulator->PickerSize.Y : 0.f;
}

void SControlManipulatorPicker::SetManipPosX(float InPosX, ETextCommit::Type CommitType)
{
	UControlManipulator* Manipulator = GetEditedManipulator();
	UControlManipulator* ManipulatorDef = GetEditedManipulatorDefaults();
	if (Manipulator && ManipulatorDef)
	{
		ManipulatorDef->PickerPos.X = Manipulator->PickerPos.X = InPosX;
		PickerCanvas->MakeButtonsForRig(GetRig());
	}
}

void SControlManipulatorPicker::SetManipPosY(float InPosY, ETextCommit::Type CommitType)
{
	UControlManipulator* Manipulator = GetEditedManipulator();
	UControlManipulator* ManipulatorDef = GetEditedManipulatorDefaults();
	if (Manipulator && ManipulatorDef)
	{
		ManipulatorDef->PickerPos.Y = Manipulator->PickerPos.Y = InPosY;
		PickerCanvas->MakeButtonsForRig(GetRig());
	}
}

void SControlManipulatorPicker::SetManipSizeX(float InSizeX, ETextCommit::Type CommitType)
{
	UControlManipulator* Manipulator = GetEditedManipulator();
	UControlManipulator* ManipulatorDef = GetEditedManipulatorDefaults();
	if (Manipulator && ManipulatorDef)
	{
		ManipulatorDef->PickerSize.X = Manipulator->PickerSize.X = InSizeX;
		PickerCanvas->MakeButtonsForRig(GetRig());
	}
}

void SControlManipulatorPicker::SetManipSizeY(float InSizeY, ETextCommit::Type CommitType)
{
	UControlManipulator* Manipulator = GetEditedManipulator();
	UControlManipulator* ManipulatorDef = GetEditedManipulatorDefaults();
	if (Manipulator && ManipulatorDef)
	{
		ManipulatorDef->PickerSize.Y = Manipulator->PickerSize.Y = InSizeY;
		PickerCanvas->MakeButtonsForRig(GetRig());
	}
}

EVisibility SControlManipulatorPicker::ShowButtonEditingUI() const
{
	return (bShowButtonEditing) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SControlManipulatorPicker::ShowPickerCanvas() const
{
	return (GetRig() != nullptr) ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
