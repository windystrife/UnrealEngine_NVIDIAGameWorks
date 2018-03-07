// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

extern TAutoConsoleVariable<float> CVarCrushThem;
extern TAutoConsoleVariable<float> CVarStopCrushWhenAbove;
extern TAutoConsoleVariable<float> CVarStartCrushWhenBelow;

#define LOCTEXT_NAMESPACE "SRotatorInputBox"

void SRotatorInputBox::Construct( const SRotatorInputBox::FArguments& InArgs )
{
	bCanBeCrushed = InArgs._AllowResponsiveLayout;

	const FLinearColor LabelColorX = InArgs._bColorAxisLabels ? SNumericEntryBox<float>::RedLabelBackgroundColor : FLinearColor(0.0f, 0.0f, 0.0f, .5f);
	const FLinearColor LabelColorY = InArgs._bColorAxisLabels ? SNumericEntryBox<float>::GreenLabelBackgroundColor : FLinearColor(0.0f, 0.0f, 0.0f, .5f);
	const FLinearColor LabelColorZ = InArgs._bColorAxisLabels ? SNumericEntryBox<float>::BlueLabelBackgroundColor : FLinearColor(0.0f, 0.0f, 0.0f, .5f);

	
	this->ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.VAlign( VAlign_Center )
		.FillWidth( 1.0f )
		.Padding( 0.0f, 1.0f, 2.0f, 1.0f )
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(InArgs._AllowSpin)
			.MinSliderValue(0.0f)
			.MaxSliderValue(359.999f)
			.LabelPadding(0)
			.Label()
			[
				BuildDecoratorLabel(LabelColorX, FLinearColor::White, LOCTEXT("Roll_Label", "X"))
			]
			.Font( InArgs._Font )
			.Value( InArgs._Roll )
			.OnValueChanged( InArgs._OnRollChanged )
			.OnValueCommitted( InArgs._OnRollCommitted )
			.OnBeginSliderMovement( InArgs._OnBeginSliderMovement )
			.OnEndSliderMovement( InArgs._OnEndSliderMovement )
			.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
			.ToolTipText( LOCTEXT("Roll_ToolTip", "Roll Value") )
			.TypeInterface(InArgs._TypeInterface)
		]
		+SHorizontalBox::Slot()
		.VAlign( VAlign_Center )
		.FillWidth( 1.0f )
		.Padding( 0.0f, 1.0f, 2.0f, 1.0f )
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(InArgs._AllowSpin)
			.MinSliderValue(0.0f)
			.MaxSliderValue(359.999f)
			.LabelPadding(0)
			.Label()
			[
				BuildDecoratorLabel(LabelColorY, FLinearColor::White, LOCTEXT("Pitch_Label", "Y"))
			]
			.Font( InArgs._Font )
			.Value( InArgs._Pitch )
			.OnValueChanged( InArgs._OnPitchChanged )
			.OnValueCommitted( InArgs._OnPitchCommitted )
			.OnBeginSliderMovement( InArgs._OnBeginSliderMovement )
			.OnEndSliderMovement( InArgs._OnEndSliderMovement )
			.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
			.ToolTipText( LOCTEXT("Pitch_ToolTip", "Pitch Value") )
			.TypeInterface(InArgs._TypeInterface)
		]
		+SHorizontalBox::Slot()
		.VAlign( VAlign_Center )
		.FillWidth( 1.0f )
		.Padding( 0.0f, 1.0f, 0.0f, 1.0f )
		[
			SNew(SNumericEntryBox<float>)
			.AllowSpin(InArgs._AllowSpin)
			.MinSliderValue(0.0f)
			.MaxSliderValue(359.999f)
			.LabelPadding(0)
			.Label()
			[
				BuildDecoratorLabel(LabelColorZ, FLinearColor::White, LOCTEXT("Yaw_Label", "Z"))
			]
			.Font( InArgs._Font )
			.Value( InArgs._Yaw )
			.OnValueChanged( InArgs._OnYawChanged )
			.OnValueCommitted( InArgs._OnYawCommitted )
			.OnBeginSliderMovement( InArgs._OnBeginSliderMovement )
			.OnEndSliderMovement( InArgs._OnEndSliderMovement )
			.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
			.ToolTipText( LOCTEXT("Yaw_ToolTip", "Yaw Value") )
			.TypeInterface(InArgs._TypeInterface)
		]
	];

}

TSharedRef<SWidget> SRotatorInputBox::BuildDecoratorLabel(FLinearColor BackgroundColor, FLinearColor InForegroundColor, FText Label)
{
	TSharedRef<SWidget> LabelWidget = SNumericEntryBox<float>::BuildLabel(Label, InForegroundColor, BackgroundColor);

	TSharedRef<SWidget> ResultWidget = LabelWidget;

	if (bCanBeCrushed)
	{
		ResultWidget =
			SNew(SWidgetSwitcher)
			.WidgetIndex(this, &SRotatorInputBox::GetLabelActiveSlot)
			+SWidgetSwitcher::Slot()
			[
				LabelWidget
			]
			+SWidgetSwitcher::Slot()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("NumericEntrySpinBox.NarrowDecorator"))
				.BorderBackgroundColor(BackgroundColor)
				.ForegroundColor(InForegroundColor)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
			];
	}

	return ResultWidget;
}

int32 SRotatorInputBox::GetLabelActiveSlot() const
{
	return bIsBeingCrushed ? 1 : 0;
}

FMargin SRotatorInputBox::GetTextMargin() const
{
	return bIsBeingCrushed ? FMargin(1.0f, 2.0f) : FMargin(4.0f, 2.0f);
}

void SRotatorInputBox::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	bool bFoop = bCanBeCrushed && (CVarCrushThem.GetValueOnAnyThread() > 0.0f);

	if (bFoop)
	{
		const float AlottedWidth = AllottedGeometry.GetLocalSize().X;

		const float CrushBelow = CVarStartCrushWhenBelow.GetValueOnAnyThread();
		const float StopCrushing = CVarStopCrushWhenAbove.GetValueOnAnyThread();

		if (bIsBeingCrushed)
		{
			bIsBeingCrushed = AlottedWidth < StopCrushing;
		}
		else
		{
			bIsBeingCrushed = AlottedWidth < CrushBelow;
		}
	}
	else
	{
		bIsBeingCrushed = false;
	}

	SCompoundWidget::OnArrangeChildren(AllottedGeometry, ArrangedChildren);
}

#undef LOCTEXT_NAMESPACE

