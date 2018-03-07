// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerTransformBox.h"
#include "Sequencer.h"
#include "SequencerSettings.h"
#include "SequencerCommonHelpers.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "Sequencer"

void SSequencerTransformBox::Construct(const FArguments& InArgs, const TSharedRef<FSequencer>& InSequencer, USequencerSettings& InSettings, const TSharedRef<INumericTypeInterface<float>>& InNumericTypeInterface)
{
	SequencerPtr = InSequencer;
	Settings = &InSettings;
	NumericTypeInterface = InNumericTypeInterface;
	
	DeltaTime = 1.f;
	ScaleFactor = 1.f;

	// Initialize to 10 frames if showing frame numbers
	float TimeSnapInterval = SequencerPtr.Pin()->GetFixedFrameInterval();
	if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSnapInterval))
	{	
		float FrameRate = 1.0f / TimeSnapInterval;
		DeltaTime = SequencerHelpers::FrameToTime(10, FrameRate);
	}

	const FDockTabStyle* GenericTabStyle = &FCoreStyle::Get().GetWidgetStyle<FDockTabStyle>("Docking.Tab");
	const FButtonStyle* const CloseButtonStyle = &GenericTabStyle->CloseButtonStyle;

	ChildSlot
	[
		SAssignNew(Border, SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(6.0f)
			.Visibility(EVisibility::Collapsed)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
							.Text(LOCTEXT("PlusLabel", "+"))
							.OnClicked(this, &SSequencerTransformBox::OnPlusButtonClicked)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
							.Text(LOCTEXT("MinusLabel", "-"))
							.OnClicked(this, &SSequencerTransformBox::OnMinusButtonClicked)
					]

				+ SHorizontalBox::Slot()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					.AutoWidth()
					[
						SAssignNew(EntryBox, SNumericEntryBox<float>)
							.MinDesiredValueWidth(32.0f)
							.TypeInterface(NumericTypeInterface)
							.ToolTipText(LOCTEXT("Delta_Tooltip", "The amount to offset the keys/sections by"))
							.OnValueCommitted(this, &SSequencerTransformBox::OnDeltaChanged)
							.Value_Lambda([this](){ return DeltaTime; })
					]

				+ SHorizontalBox::Slot()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
							.Text(LOCTEXT("MultiplyLabel", "*"))
							.OnClicked(this, &SSequencerTransformBox::OnMultiplyButtonClicked)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
							.Text(LOCTEXT("DivideLabel", "/"))
							.OnClicked(this, &SSequencerTransformBox::OnDivideButtonClicked)
					]

				+ SHorizontalBox::Slot()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					.AutoWidth()
					[
						SAssignNew(EntryBox, SNumericEntryBox<float>)
							.MinDesiredValueWidth(32.0f)
							.ToolTipText(LOCTEXT("Scale_Tooltip", "The amount to scale the keys/section by (about the local time)"))
							.OnValueCommitted(this, &SSequencerTransformBox::OnScaleChanged)
							.Value_Lambda([this](){ return ScaleFactor; })
					]

				+ SHorizontalBox::Slot()
				.Padding(3.0f, 0.0f, 0.0f, 0.0f)
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle( CloseButtonStyle )
					.OnClicked( this, &SSequencerTransformBox::OnCloseButtonClicked )
					.ContentPadding( 0 )
					[
						SNew(SSpacer)
						.Size( CloseButtonStyle->Normal.ImageSize )
					]
				]
			]
	];
}


void SSequencerTransformBox::ToggleVisibility()
{
	FSlateApplication& SlateApplication = FSlateApplication::Get();

	if (Border->GetVisibility() == EVisibility::Visible)
	{
		if (LastFocusedWidget.IsValid())
		{
			SlateApplication.SetAllUserFocus(LastFocusedWidget.Pin(), EFocusCause::Navigation);
		}
		
		Border->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		Border->SetVisibility(EVisibility::Visible);
		LastFocusedWidget = SlateApplication.GetUserFocusedWidget(0);
		SlateApplication.SetAllUserFocus(EntryBox, EFocusCause::Navigation);
	}
}

void SSequencerTransformBox::OnDeltaChanged(float Value, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter)
	{
		return;
	}

	DeltaTime = Value;
}

void SSequencerTransformBox::OnScaleChanged(float Value, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter)
	{
		return;
	}

	ScaleFactor = Value;
}

FReply SSequencerTransformBox::OnPlusButtonClicked()
{
	if (DeltaTime != 0.f)
	{
		TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

		Sequencer->TransformSelectedKeysAndSections(DeltaTime, 1.f);
	}

	return FReply::Handled();
}

FReply SSequencerTransformBox::OnMinusButtonClicked()
{
	if (DeltaTime != 0.f)
	{
		TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
	
		Sequencer->TransformSelectedKeysAndSections(-DeltaTime, 1.f);
	}

	return FReply::Handled();
}

FReply SSequencerTransformBox::OnMultiplyButtonClicked()
{
	if (ScaleFactor != 0.f && ScaleFactor != 1.f)
	{
		TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();
		
		Sequencer->TransformSelectedKeysAndSections(0, ScaleFactor);
	}

	return FReply::Handled();
}

FReply SSequencerTransformBox::OnDivideButtonClicked()
{
	if (ScaleFactor != 0.f && ScaleFactor != 1.f)
	{
		TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

		Sequencer->TransformSelectedKeysAndSections(0, 1.f/ScaleFactor);
	}
	return FReply::Handled();
}

FReply SSequencerTransformBox::OnCloseButtonClicked()
{
	ToggleVisibility();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
