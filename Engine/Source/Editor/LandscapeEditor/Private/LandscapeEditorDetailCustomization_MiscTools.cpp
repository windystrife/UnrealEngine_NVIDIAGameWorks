// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailCustomization_MiscTools.h"
#include "Widgets/Text/STextBlock.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "LandscapeEditorObject.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"

#include "ScopedTransaction.h"
#include "IDetailPropertyRow.h"
#include "SNumericEntryBox.h"
#include "SFlattenHeightEyeDropperButton.h"
#include "LandscapeEdModeTools.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.Tools"

TSharedRef<IDetailCustomization> FLandscapeEditorDetailCustomization_MiscTools::MakeInstance()
{
	return MakeShareable(new FLandscapeEditorDetailCustomization_MiscTools);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FLandscapeEditorDetailCustomization_MiscTools::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& ToolsCategory = DetailBuilder.EditCategory("Tool Settings");

	if (IsBrushSetActive("BrushSet_Component"))
	{
		ToolsCategory.AddCustomRow(LOCTEXT("Component.ClearSelection", "Clear Component Selection"))
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&FLandscapeEditorDetailCustomization_MiscTools::GetClearComponentSelectionVisibility)))
		[
			SNew(SButton)
			.Text(LOCTEXT("Component.ClearSelection", "Clear Component Selection"))
			.HAlign(HAlign_Center)
			.OnClicked_Static(&FLandscapeEditorDetailCustomization_MiscTools::OnClearComponentSelectionButtonClicked)
		];
	}

	//if (IsToolActive("Mask"))
	{
		ToolsCategory.AddCustomRow(LOCTEXT("Mask.ClearSelection", "Clear Region Selection"))
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(&FLandscapeEditorDetailCustomization_MiscTools::GetClearRegionSelectionVisibility)))
		[
			SNew(SButton)
			.Text(LOCTEXT("Mask.ClearSelection", "Clear Region Selection"))
			.HAlign(HAlign_Center)
			.OnClicked_Static(&FLandscapeEditorDetailCustomization_MiscTools::OnClearRegionSelectionButtonClicked)
		];
	}

	if (IsToolActive("Flatten"))
	{
		TSharedRef<IPropertyHandle> FlattenValueProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, FlattenTarget));
		IDetailPropertyRow& FlattenValueRow = ToolsCategory.AddProperty(FlattenValueProperty);
		FlattenValueRow.CustomWidget()
			.NameContent()
			[
				FlattenValueProperty->CreatePropertyNameWidget()
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(0.0f, 2.0f, 5.0f, 2.0f)
				.FillWidth(1.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SNumericEntryBox<float>)
					.AllowSpin(true)
					.Font(DetailBuilder.GetDetailFont())
					.Value(this, &FLandscapeEditorDetailCustomization_MiscTools::GetFlattenValue)
					.OnValueChanged_Static(&FLandscapeEditorDetailCustomization_Base::OnValueChanged<float>, FlattenValueProperty)
					.OnValueCommitted_Static(&FLandscapeEditorDetailCustomization_Base::OnValueCommitted<float>, FlattenValueProperty)
					.MinValue(-32768.0f)
					.MaxValue(32768.0f)
					.SliderExponentNeutralValue(0.0f)
					.SliderExponent(5.0f)
					.ShiftMouseMovePixelPerDelta(20)
					.MinSliderValue(-32768.0f)
					.MaxSliderValue(32768.0f)
					.MinDesiredValueWidth(75.0f)
					.ToolTipText(LOCTEXT("FlattenToolTips", "Target height to flatten towards (in Unreal Units)"))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 2.0f, 5.0f, 2.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SFlattenHeightEyeDropperButton)
					.OnBegin(this, &FLandscapeEditorDetailCustomization_MiscTools::OnBeginFlattenToolEyeDrop)
					.OnComplete(this, &FLandscapeEditorDetailCustomization_MiscTools::OnCompletedFlattenToolEyeDrop)
				]		
			];
	}

	if (IsToolActive("Splines"))
	{
		ToolsCategory.AddCustomRow(LOCTEXT("ApplySplinesLabel", "Apply Splines"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0, 6, 0, 0)
			[
				SNew(STextBlock)
				.Font(DetailBuilder.GetDetailFont())
				.ShadowOffset(FVector2D::UnitVector)
				.Text(LOCTEXT("Spline.ApplySplines", "Deform Landscape to Splines:"))
			]
		];
		ToolsCategory.AddCustomRow(LOCTEXT("ApplySplinesLabel", "Apply Splines"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("Spline.ApplySplines.All.Tooltip", "Deforms and paints the landscape to fit all the landscape spline segments and controlpoints."))
				.Text(LOCTEXT("Spline.ApplySplines.All", "All Splines"))
				.HAlign(HAlign_Center)
				.OnClicked_Static(&FLandscapeEditorDetailCustomization_MiscTools::OnApplyAllSplinesButtonClicked)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("Spline.ApplySplines.Tooltip", "Deforms and paints the landscape to fit only the selected landscape spline segments and controlpoints."))
				.Text(LOCTEXT("Spline.ApplySplines.Selected", "Only Selected"))
				.HAlign(HAlign_Center)
				.OnClicked_Static(&FLandscapeEditorDetailCustomization_MiscTools::OnApplySelectedSplinesButtonClicked)
			]
		];
		ToolsCategory.AddCustomRow(LOCTEXT("Spline.bUseAutoRotateControlPoint.Selected", "Use Auto Rotate Control Point"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0, 6, 0, 0)
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &FLandscapeEditorDetailCustomization_MiscTools::OnbUseAutoRotateControlPointChanged)
				.IsChecked(this, &FLandscapeEditorDetailCustomization_MiscTools::GetbUseAutoRotateControlPoint)
				.Content()
				[
					SNew(STextBlock).Text(LOCTEXT("Spline.bUseAutoRotateControlPoint.Selected", "Use Auto Rotate Control Point"))
				]
			]
		];
	}


	if (IsToolActive("Ramp"))
	{
		ToolsCategory.AddCustomRow(LOCTEXT("RampLabel", "Ramp"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0, 6, 0, 0)
			[
				SNew(STextBlock)
				.Font(DetailBuilder.GetDetailFont())
				.ShadowOffset(FVector2D::UnitVector)
				.Text(LOCTEXT("Ramp.Hint", "Click to add ramp points, then press \"Add Ramp\"."))
			]
		];
		ToolsCategory.AddCustomRow(LOCTEXT("ApplyRampLabel", "Apply Ramp"))
		[
			SNew(SBox)
			.Padding(FMargin(0, 0, 12, 0)) // Line up with the other properties due to having no reset to default button
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0, 0, 3, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("Ramp.Reset", "Reset"))
					.HAlign(HAlign_Center)
					.OnClicked_Static(&FLandscapeEditorDetailCustomization_MiscTools::OnResetRampButtonClicked)
				]
				+ SHorizontalBox::Slot()
				.Padding(3, 0, 0, 0)
				[
					SNew(SButton)
					.IsEnabled_Static(&FLandscapeEditorDetailCustomization_MiscTools::GetApplyRampButtonIsEnabled)
					.Text(LOCTEXT("Ramp.Apply", "Add Ramp"))
					.HAlign(HAlign_Center)
					.OnClicked_Static(&FLandscapeEditorDetailCustomization_MiscTools::OnApplyRampButtonClicked)
				]
			]
		];
	}

	if (IsToolActive("Mirror"))
	{
		ToolsCategory.AddProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, MirrorPoint));
		ToolsCategory.AddProperty(GET_MEMBER_NAME_CHECKED(ULandscapeEditorObject, MirrorOp));
		ToolsCategory.AddCustomRow(LOCTEXT("ApplyMirrorLabel", "Apply Mirror"))
		[
			SNew(SBox)
			.Padding(FMargin(0, 0, 12, 0)) // Line up with the other properties due to having no reset to default button
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0, 0, 3, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("Mirror.Reset", "Recenter"))
					.HAlign(HAlign_Center)
					.OnClicked_Lambda(&FLandscapeEditorDetailCustomization_MiscTools::OnResetMirrorPointButtonClicked)
				]
				+ SHorizontalBox::Slot()
				.Padding(3, 0, 0, 0)
				[
					SNew(SButton)
					//.IsEnabled_Static(&FLandscapeEditorDetailCustomization_MiscTools::GetApplyMirrorButtonIsEnabled)
					.Text(LOCTEXT("Mirror.Apply", "Apply"))
					.HAlign(HAlign_Center)
					.OnClicked_Static(&FLandscapeEditorDetailCustomization_MiscTools::OnApplyMirrorButtonClicked)
				]
			]
		];
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

EVisibility FLandscapeEditorDetailCustomization_MiscTools::GetClearComponentSelectionVisibility()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentTool)
	{
		const FName CurrentToolName = LandscapeEdMode->CurrentTool->GetToolName();
		if (CurrentToolName == FName("Select"))
		{
			return EVisibility::Visible;
		}
		else if (LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid() && LandscapeEdMode->CurrentToolTarget.LandscapeInfo->GetSelectedComponents().Num() > 0)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

FReply FLandscapeEditorDetailCustomization_MiscTools::OnClearComponentSelectionButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		ULandscapeInfo* LandscapeInfo = LandscapeEdMode->CurrentToolTarget.LandscapeInfo.Get();
		if (LandscapeInfo)
		{
			FScopedTransaction Transaction(LOCTEXT("Component.Undo_ClearSelected", "Clearing Selection"));
			LandscapeInfo->Modify();
			LandscapeInfo->ClearSelectedRegion(true);
		}
	}

	return FReply::Handled();
}

EVisibility FLandscapeEditorDetailCustomization_MiscTools::GetClearRegionSelectionVisibility()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentTool)
	{
		const FName CurrentToolName = LandscapeEdMode->CurrentTool->GetToolName();
		if (CurrentToolName == FName("Mask"))
		{
			return EVisibility::Visible;
		}
		else if (LandscapeEdMode->CurrentTool && LandscapeEdMode->CurrentTool->SupportsMask() &&
			LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid() && LandscapeEdMode->CurrentToolTarget.LandscapeInfo->SelectedRegion.Num() > 0)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

FReply FLandscapeEditorDetailCustomization_MiscTools::OnClearRegionSelectionButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		ULandscapeInfo* LandscapeInfo = LandscapeEdMode->CurrentToolTarget.LandscapeInfo.Get();
		if (LandscapeInfo)
		{
			FScopedTransaction Transaction(LOCTEXT("Region.Undo_ClearSelected", "Clearing Region Selection"));
			LandscapeInfo->Modify();
			LandscapeInfo->ClearSelectedRegion(false);
		}
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_MiscTools::OnApplyAllSplinesButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		LandscapeEdMode->CurrentToolTarget.LandscapeInfo->ApplySplines(false);
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_MiscTools::OnApplySelectedSplinesButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentToolTarget.LandscapeInfo.IsValid())
	{
		LandscapeEdMode->CurrentToolTarget.LandscapeInfo->ApplySplines(true);
	}

	return FReply::Handled();
}

void FLandscapeEditorDetailCustomization_MiscTools::OnbUseAutoRotateControlPointChanged(ECheckBoxState NewState)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		LandscapeEdMode->SetbUseAutoRotateOnJoin(NewState == ECheckBoxState::Checked);
	}
}

ECheckBoxState FLandscapeEditorDetailCustomization_MiscTools::GetbUseAutoRotateControlPoint() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode)
	{
		return LandscapeEdMode->GetbUseAutoRotateOnJoin() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

FReply FLandscapeEditorDetailCustomization_MiscTools::OnApplyRampButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL && IsToolActive(FName("Ramp")))
	{
		LandscapeEdMode->ApplyRampTool();
	}

	return FReply::Handled();
}

bool FLandscapeEditorDetailCustomization_MiscTools::GetApplyRampButtonIsEnabled()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL && IsToolActive(FName("Ramp")))
	{
		return LandscapeEdMode->CanApplyRampTool();
	}

	return false;
}

FReply FLandscapeEditorDetailCustomization_MiscTools::OnResetRampButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL && IsToolActive(FName("Ramp")))
	{
		LandscapeEdMode->ResetRampTool();
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_MiscTools::OnApplyMirrorButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL && IsToolActive(FName("Mirror")))
	{
		LandscapeEdMode->ApplyMirrorTool();
	}

	return FReply::Handled();
}

FReply FLandscapeEditorDetailCustomization_MiscTools::OnResetMirrorPointButtonClicked()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL && IsToolActive(FName("Mirror")))
	{
		LandscapeEdMode->CenterMirrorTool();
	}

	return FReply::Handled();
}

TOptional<float> FLandscapeEditorDetailCustomization_MiscTools::GetFlattenValue() const
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr && IsToolActive(FName("Flatten")))
	{
		if (LandscapeEdMode->UISettings->bFlattenEyeDropperModeActivated)
		{
			return LandscapeEdMode->UISettings->FlattenEyeDropperModeDesiredTarget;
		}

		return LandscapeEdMode->UISettings->FlattenTarget;
	}

	return 0.0f;
}

void FLandscapeEditorDetailCustomization_MiscTools::OnBeginFlattenToolEyeDrop()
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr && IsToolActive(FName("Flatten")))
	{
		LandscapeEdMode->UISettings->bFlattenEyeDropperModeActivated = true;
		LandscapeEdMode->CurrentTool->SetCanToolBeActivated(false);
	}
}

void FLandscapeEditorDetailCustomization_MiscTools::OnCompletedFlattenToolEyeDrop(bool Canceled)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != nullptr && IsToolActive(FName("Flatten")))
	{
		LandscapeEdMode->UISettings->bFlattenEyeDropperModeActivated = false;
		LandscapeEdMode->CurrentTool->SetCanToolBeActivated(true);

		if (!Canceled)
		{
			LandscapeEdMode->UISettings->FlattenTarget = LandscapeEdMode->UISettings->FlattenEyeDropperModeDesiredTarget;
		}
	}
}

#undef LOCTEXT_NAMESPACE
