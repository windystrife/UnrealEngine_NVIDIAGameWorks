// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "STimeRange.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "STimeRange"

void STimeRange::Construct( const STimeRange::FArguments& InArgs, TSharedRef<ITimeSliderController> InTimeSliderController, TSharedRef<INumericTypeInterface<float>> NumericTypeInterface )
{
	TimeSliderController = InTimeSliderController;
	ShowFrameNumbers = InArgs._ShowFrameNumbers;

	TSharedRef<SWidget> WorkingRangeStart = SNullWidget::NullWidget, WorkingRangeEnd = SNullWidget::NullWidget;
	if (InArgs._ShowWorkingRange)
	{
		WorkingRangeStart = SNew(SSpinBox<float>)
		.Value(this, &STimeRange::WorkingStartTime)
		.ToolTipText(LOCTEXT("WorkingRangeStart", "Working Range Start"))
		.OnValueCommitted( this, &STimeRange::OnWorkingStartTimeCommitted )
		.OnValueChanged( this, &STimeRange::OnWorkingStartTimeChanged )
		.MinValue(TOptional<float>())
		.MaxValue(this, &STimeRange::MaxWorkingStartTime)
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true);

		WorkingRangeEnd = SNew(SSpinBox<float>)
		.Value(this, &STimeRange::WorkingEndTime)
		.ToolTipText(LOCTEXT("WorkingRangeEnd", "Working Range End"))
		.OnValueCommitted( this, &STimeRange::OnWorkingEndTimeCommitted )
		.OnValueChanged( this, &STimeRange::OnWorkingEndTimeChanged )
		.MinValue(this, &STimeRange::MinWorkingEndTime)
		.MaxValue(TOptional<float>())
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true);
	}

	TSharedRef<SWidget> ViewRangeStart = SNullWidget::NullWidget, ViewRangeEnd = SNullWidget::NullWidget;
	if (InArgs._ShowViewRange)
	{
		ViewRangeStart = SNew(SSpinBox<float>)
		.Value(this, &STimeRange::ViewStartTime)
		.ToolTipText(ViewStartTimeTooltip())
		.OnValueCommitted( this, &STimeRange::OnViewStartTimeCommitted )
		.OnValueChanged( this, &STimeRange::OnViewStartTimeChanged )
		.MinValue(TOptional<float>())
		.MaxValue(this, &STimeRange::MaxViewStartTime)
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true);

		ViewRangeEnd = SNew(SSpinBox<float>)
		.Value(this, &STimeRange::ViewEndTime)
		.ToolTipText(ViewEndTimeTooltip())
		.OnValueCommitted( this, &STimeRange::OnViewEndTimeCommitted )
		.OnValueChanged( this, &STimeRange::OnViewEndTimeChanged )
		.MinValue(this, &STimeRange::MinViewEndTime)
		.MaxValue(TOptional<float>())
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true);
	}

	TSharedRef<SWidget> PlaybackRangeStart = SNullWidget::NullWidget, PlaybackRangeEnd = SNullWidget::NullWidget;
	{
		PlaybackRangeStart = SNew(SSpinBox<float>)
		.Value(this, &STimeRange::PlayStartTime)
		.ToolTipText(PlayStartTimeTooltip())
		.OnValueCommitted( this, &STimeRange::OnPlayStartTimeCommitted )
		.OnValueChanged( this, &STimeRange::OnPlayStartTimeChanged )
		.MinValue(this, &STimeRange::MinPlayStartTime)
		.MaxValue(this, &STimeRange::MaxPlayStartTime)
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true);

		PlaybackRangeEnd = SNew(SSpinBox<float>)
		.Value(this, &STimeRange::PlayEndTime)
		.ToolTipText(PlayEndTimeTooltip())
		.OnValueCommitted( this, &STimeRange::OnPlayEndTimeCommitted )
		.OnValueChanged( this, &STimeRange::OnPlayEndTimeChanged )
		.MinValue(this, &STimeRange::MinPlayEndTime)
		.MaxValue(this, &STimeRange::MaxPlayEndTime)
		.Style(&FEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.TypeInterface(NumericTypeInterface)
		.ClearKeyboardFocusOnCommit(true);
	}

	this->ChildSlot
	.HAlign(HAlign_Fill)
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.Visibility(InArgs._ShowWorkingRange ? EVisibility::Visible : EVisibility::Collapsed)
			.WidthOverride(48)
			.HAlign(HAlign_Center)
			[
				WorkingRangeStart
			]
		]

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.WidthOverride(48)
			.HAlign(HAlign_Center)
			.Visibility(InArgs._ShowPlaybackRange ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(nullptr)
				.ForegroundColor(FLinearColor::Green)
				[
					PlaybackRangeStart
				]
			]
		]

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.Visibility(InArgs._ShowViewRange ? EVisibility::Visible : EVisibility::Collapsed)
			.WidthOverride(48)
			.HAlign(HAlign_Center)
			[
				ViewRangeStart
			]
		]

		+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(2.0f, 4.0f)
			.VAlign(VAlign_Center)
			[
				InArgs._CenterContent.Widget
			]
		
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.Visibility(InArgs._ShowViewRange ? EVisibility::Visible : EVisibility::Collapsed)
			.WidthOverride(48)
			.HAlign(HAlign_Center)
			[
				ViewRangeEnd
			]
		]

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.WidthOverride(48)
			.HAlign(HAlign_Center)
			.Visibility(InArgs._ShowPlaybackRange ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(nullptr)
				.ForegroundColor(FLinearColor::Red)
				[
					PlaybackRangeEnd
				]
			]
		]

		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(2.f)
		[
			SNew(SBox)
			.WidthOverride(48)
			.HAlign(HAlign_Center)
			.Visibility(InArgs._ShowWorkingRange ? EVisibility::Visible : EVisibility::Collapsed)
			[
				WorkingRangeEnd
			]
		]
	];
}

float STimeRange::WorkingStartTime() const
{
	return TimeSliderController.IsValid() ? TimeSliderController.Get()->GetClampRange().GetLowerBoundValue() : 0.f;
}

float STimeRange::WorkingEndTime() const
{
	return TimeSliderController.IsValid() ? TimeSliderController.Get()->GetClampRange().GetUpperBoundValue() : 0.f;
}

float STimeRange::ViewStartTime() const
{
	return TimeSliderController.IsValid() ? TimeSliderController.Get()->GetViewRange().GetLowerBoundValue() : 0.f;
}

float STimeRange::ViewEndTime() const
{
	return TimeSliderController.IsValid() ? TimeSliderController.Get()->GetViewRange().GetUpperBoundValue() : 0.f;
}

float STimeRange::PlayStartTime() const
{
	return TimeSliderController.IsValid() ? TimeSliderController.Get()->GetPlayRange().GetLowerBoundValue() : 0.f;
}

float STimeRange::PlayEndTime() const
{
	return TimeSliderController.IsValid() ? TimeSliderController.Get()->GetPlayRange().GetUpperBoundValue() : 0.f;
}

TOptional<float> STimeRange::MaxViewStartTime() const
{
	return ViewEndTime();
}

TOptional<float> STimeRange::MinViewEndTime() const
{
	return ViewStartTime();
}

TOptional<float> STimeRange::MinPlayStartTime() const
{
	return WorkingStartTime();
}

TOptional<float> STimeRange::MaxPlayStartTime() const
{
	return PlayEndTime();
}

TOptional<float> STimeRange::MinPlayEndTime() const
{
	return PlayStartTime();
}

TOptional<float> STimeRange::MaxPlayEndTime() const
{
	return WorkingEndTime();
}

TOptional<float> STimeRange::MaxWorkingStartTime() const
{
	return ViewEndTime();
}

TOptional<float> STimeRange::MinWorkingEndTime() const
{
	return ViewStartTime();
}

void STimeRange::OnWorkingStartTimeCommitted(float NewValue, ETextCommit::Type InTextCommit)
{
	OnWorkingStartTimeChanged(NewValue);
}

void STimeRange::OnWorkingEndTimeCommitted(float NewValue, ETextCommit::Type InTextCommit)
{
	OnWorkingEndTimeChanged(NewValue);
}

void STimeRange::OnViewStartTimeCommitted(float NewValue, ETextCommit::Type InTextCommit)
{
	OnViewStartTimeChanged(NewValue);
}

void STimeRange::OnViewEndTimeCommitted(float NewValue, ETextCommit::Type InTextCommit)
{
	OnViewEndTimeChanged(NewValue);
}

void STimeRange::OnPlayStartTimeCommitted(float NewValue, ETextCommit::Type InTextCommit)
{
	OnPlayStartTimeChanged(NewValue);
}

void STimeRange::OnPlayEndTimeCommitted(float NewValue, ETextCommit::Type InTextCommit)
{
	OnPlayEndTimeChanged(NewValue);
}

void STimeRange::OnWorkingStartTimeChanged(float NewValue)
{
	if (auto* Controller = TimeSliderController.Get())
	{
		Controller->SetClampRange(NewValue, Controller->GetClampRange().GetUpperBoundValue());

		if (NewValue > Controller->GetViewRange().GetLowerBoundValue())
		{
			Controller->SetViewRange(NewValue, Controller->GetViewRange().GetUpperBoundValue(), EViewRangeInterpolation::Immediate);
		}
	}
}

void STimeRange::OnWorkingEndTimeChanged(float NewValue)
{
	if (auto* Controller = TimeSliderController.Get())
	{
		Controller->SetClampRange(Controller->GetClampRange().GetLowerBoundValue(), NewValue);

		if (NewValue < Controller->GetViewRange().GetUpperBoundValue())
		{
			Controller->SetViewRange(Controller->GetViewRange().GetLowerBoundValue(), NewValue, EViewRangeInterpolation::Immediate);
		}
	}
}

void STimeRange::OnViewStartTimeChanged(float NewValue)
{
	if (auto* Controller = TimeSliderController.Get())
	{
		if (NewValue < TimeSliderController.Get()->GetClampRange().GetLowerBoundValue())
		{
			Controller->SetClampRange(NewValue, Controller->GetClampRange().GetUpperBoundValue());
		}

		Controller->SetViewRange(NewValue, Controller->GetViewRange().GetUpperBoundValue(), EViewRangeInterpolation::Immediate);
	}
}

void STimeRange::OnViewEndTimeChanged(float NewValue)
{
	if (auto* Controller = TimeSliderController.Get())
	{
		if (NewValue > Controller->GetClampRange().GetUpperBoundValue())
		{
			Controller->SetClampRange(Controller->GetClampRange().GetLowerBoundValue(), NewValue);
		}

		Controller->SetViewRange(Controller->GetViewRange().GetLowerBoundValue(), NewValue, EViewRangeInterpolation::Immediate);
	}
}

void STimeRange::OnPlayStartTimeChanged(float NewValue)
{
	if (auto* Controller = TimeSliderController.Get())
	{
		if (NewValue < TimeSliderController.Get()->GetClampRange().GetLowerBoundValue())
		{
			Controller->SetClampRange(NewValue, Controller->GetClampRange().GetUpperBoundValue());
		}

		Controller->SetPlayRange(NewValue, Controller->GetPlayRange().GetUpperBoundValue());
	}
}

void STimeRange::OnPlayEndTimeChanged(float NewValue)
{
	if (auto* Controller = TimeSliderController.Get())
	{
		if (NewValue > Controller->GetClampRange().GetUpperBoundValue())
		{
			Controller->SetClampRange(Controller->GetClampRange().GetLowerBoundValue(), NewValue);
		}

		Controller->SetPlayRange(Controller->GetPlayRange().GetLowerBoundValue(), NewValue);
	}
}

FText STimeRange::PlayStartTimeTooltip() const
{
	bool bShowFrameNumbers = ShowFrameNumbers.IsBound() ? ShowFrameNumbers.Get() : false;
	if (bShowFrameNumbers)
	{
		return LOCTEXT("PlayStartFrameTooltip", "In Frame");
	}
	else
	{
		return LOCTEXT("PlayStartTimeTooltip", "In Time");
	}
}

FText STimeRange::PlayEndTimeTooltip() const
{
	bool bShowFrameNumbers = ShowFrameNumbers.IsBound() ? ShowFrameNumbers.Get() : false;
	if (bShowFrameNumbers)
	{
		return LOCTEXT("PlayEndFrameTooltip", "Out Frame");
	}
	else
	{
		return LOCTEXT("PlayEndTimeTooltip", "Out Time");
	}
}

FText STimeRange::ViewStartTimeTooltip() const
{
	bool bShowFrameNumbers = ShowFrameNumbers.IsBound() ? ShowFrameNumbers.Get() : false;
	if (bShowFrameNumbers)
	{
		return LOCTEXT("ViewStartFrameTooltip", "View Range Start Frame");
	}
	else
	{
		return LOCTEXT("ViewStartTimeTooltip", "View Range Start Time");
	}
}

FText STimeRange::ViewEndTimeTooltip() const
{
	bool bShowFrameNumbers = ShowFrameNumbers.IsBound() ? ShowFrameNumbers.Get() : false;
	if (bShowFrameNumbers)
	{
		return LOCTEXT("ViewEndFrameTooltip", "View Range End Frame");
	}
	else
	{
		return LOCTEXT("ViewEndTimeTooltip", "View Range End Time");
	}
}

#undef LOCTEXT_NAMESPACE
