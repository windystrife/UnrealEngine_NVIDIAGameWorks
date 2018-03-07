// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerGotoBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SNumericEntryBox.h"


#define LOCTEXT_NAMESPACE "Sequencer"


void SSequencerGotoBox::Construct(const FArguments& InArgs, const TSharedRef<FSequencer>& InSequencer, USequencerSettings& InSettings, const TSharedRef<INumericTypeInterface<float>>& InNumericTypeInterface)
{
	SequencerPtr = InSequencer;
	Settings = &InSettings;
	NumericTypeInterface = InNumericTypeInterface;

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
						SNew(STextBlock)
							.Text(LOCTEXT("GotoLabel", "Go to:"))
					]

				+ SHorizontalBox::Slot()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					.AutoWidth()
					[
						SAssignNew(EntryBox, SNumericEntryBox<float>)
							.MinDesiredValueWidth(64.0f)
							.OnValueCommitted(this, &SSequencerGotoBox::HandleEntryBoxValueCommitted)
							.TypeInterface(NumericTypeInterface)
							.Value_Lambda([this](){ return SequencerPtr.Pin()->GetLocalTime(); })
					]
			]
	];
}


void SSequencerGotoBox::ToggleVisibility()
{
	FSlateApplication& SlateApplication = FSlateApplication::Get();

	if (Border->GetVisibility() == EVisibility::Visible)
	{
		SlateApplication.SetAllUserFocus(LastFocusedWidget.Pin(), EFocusCause::Navigation);
		Border->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		Border->SetVisibility(EVisibility::Visible);
		LastFocusedWidget = SlateApplication.GetUserFocusedWidget(0);
		SlateApplication.SetAllUserFocus(EntryBox, EFocusCause::Navigation);
	}
}


void SSequencerGotoBox::HandleEntryBoxValueCommitted(float Value, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter)
	{
		return;
	}

	ToggleVisibility();

	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	// scroll view range if new value is not visible
	const FAnimatedRange ViewRange = Sequencer->GetViewRange();

	if (!ViewRange.Contains(Value))
	{
		const float HalfViewWidth = 0.5f * ViewRange.Size<float>();
		const TRange<float> NewRange = TRange<float>(Value - HalfViewWidth, Value + HalfViewWidth);
		Sequencer->SetViewRange(NewRange);
	}

	Sequencer->SetLocalTimeDirectly(Value);
}


#undef LOCTEXT_NAMESPACE
