// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Sequencer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "SequencerCommonHelpers.h"
#include "IKeyArea.h"
#include "MovieSceneCommonHelpers.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "Arse"

/**
 * A widget for navigating between keys on a sequencer track
 */
class SKeyNavigationButtons
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SKeyNavigationButtons){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FSequencerDisplayNode>& InDisplayNode)
	{
		DisplayNode = InDisplayNode;

		const FSlateBrush* NoBorder = FEditorStyle::GetBrush( "NoBorder" );

		TAttribute<FLinearColor> HoverTint(this, &SKeyNavigationButtons::GetHoverTint);

		ChildSlot
		[
			SNew(SHorizontalBox)
			
			// Previous key slot
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(3, 0, 0, 0)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(NoBorder)
				.ColorAndOpacity(HoverTint)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton")
					.ToolTipText(LOCTEXT("PreviousKeyButton", "Set the time to the previous key"))
					.OnClicked(this, &SKeyNavigationButtons::OnPreviousKeyClicked)
					.ForegroundColor( FSlateColor::UseForeground() )
					.ContentPadding(0)
					.IsFocusable(false)
					[
						SNew(STextBlock)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.7"))
						.Text(FText::FromString(FString(TEXT("\xf060"))) /*fa-arrow-left*/)
					]
				]
			]
			// Add key slot
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(NoBorder)
				.ColorAndOpacity(HoverTint)
				.IsEnabled(!InDisplayNode->GetSequencer().IsReadOnly())
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton")
					.ToolTipText(LOCTEXT("AddKeyButton", "Add a new key at the current time"))
					.OnClicked(this, &SKeyNavigationButtons::OnAddKeyClicked)
					.ForegroundColor( FSlateColor::UseForeground() )
					.ContentPadding(0)
					.IsFocusable(false)
					[
						SNew(STextBlock)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.7"))
						.Text(FText::FromString(FString(TEXT("\xf055"))) /*fa-plus-circle*/)
					]
				]
			]
			// Next key slot
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(NoBorder)
				.ColorAndOpacity(HoverTint)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton")
					.ToolTipText(LOCTEXT("NextKeyButton", "Set the time to the next key"))
					.OnClicked(this, &SKeyNavigationButtons::OnNextKeyClicked)
					.ContentPadding(0)
					.ForegroundColor( FSlateColor::UseForeground() )
					.IsFocusable(false)
					[
						SNew(STextBlock)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.7"))
						.Text(FText::FromString(FString(TEXT("\xf061"))) /*fa-arrow-right*/)
					]
				]
			]
		];
	}

	FLinearColor GetHoverTint() const
	{
		return DisplayNode->IsHovered() ? FLinearColor(1,1,1,0.9f) : FLinearColor(1,1,1,0.4f);
	}

	FReply OnPreviousKeyClicked()
	{
		FSequencer& Sequencer = DisplayNode->GetSequencer();
		float ClosestPreviousKeyDistance = MAX_FLT;
		float CurrentTime = Sequencer.GetLocalTime();
		float PreviousTime = 0;
		bool PreviousKeyFound = false;

		TArray<float> AllTimes;

		TSet<TSharedPtr<IKeyArea>> KeyAreas;
		SequencerHelpers::GetAllKeyAreas( DisplayNode, KeyAreas );
		for ( TSharedPtr<IKeyArea> KeyArea : KeyAreas )
		{
			for ( FKeyHandle& KeyHandle : KeyArea->GetUnsortedKeyHandles() )
			{
				float KeyTime = KeyArea->GetKeyTime( KeyHandle );
				AllTimes.Add(KeyTime);
			}
		}

		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections( DisplayNode.ToSharedRef(), Sections );
		for ( TWeakObjectPtr<UMovieSceneSection> Section : Sections )
		{
			if (Section.IsValid() && !Section->IsInfinite())
			{
				AllTimes.Add(Section->GetStartTime());
				AllTimes.Add(Section->GetEndTime());
			}
		}

		for (float Time : AllTimes)
		{
			if (Time < CurrentTime && CurrentTime - Time < ClosestPreviousKeyDistance)
			{
				PreviousTime = Time;
				ClosestPreviousKeyDistance = CurrentTime - Time;
				PreviousKeyFound = true;
			}
		}

		if (PreviousKeyFound)
		{
			Sequencer.SetLocalTime(PreviousTime);
		}
		return FReply::Handled();
	}


	FReply OnNextKeyClicked()
	{
		FSequencer& Sequencer = DisplayNode->GetSequencer();
		float ClosestNextKeyDistance = MAX_FLT;
		float CurrentTime = Sequencer.GetLocalTime();
		float NextTime = 0;
		bool NextKeyFound = false;

		TArray<float> AllTimes;

		TSet<TSharedPtr<IKeyArea>> KeyAreas;
		SequencerHelpers::GetAllKeyAreas( DisplayNode, KeyAreas );
		for ( TSharedPtr<IKeyArea> KeyArea : KeyAreas )
		{
			for ( FKeyHandle& KeyHandle : KeyArea->GetUnsortedKeyHandles() )
			{
				float KeyTime = KeyArea->GetKeyTime( KeyHandle );
				AllTimes.Add(KeyTime);
			}
		}

		TSet<TWeakObjectPtr<UMovieSceneSection> > Sections;
		SequencerHelpers::GetAllSections( DisplayNode.ToSharedRef(), Sections );
		for ( TWeakObjectPtr<UMovieSceneSection> Section : Sections )
		{
			if (Section.IsValid() && !Section->IsInfinite())
			{
				AllTimes.Add(Section->GetStartTime());
				AllTimes.Add(Section->GetEndTime());
			}
		}

		for (float Time : AllTimes)
		{
			if (Time > CurrentTime && Time - CurrentTime < ClosestNextKeyDistance)
			{
				NextTime = Time;
				ClosestNextKeyDistance = Time - CurrentTime;
				NextKeyFound = true;
			}
		}

		if (NextKeyFound)
		{
			Sequencer.SetLocalTime(NextTime);
		}

		return FReply::Handled();
	}


	FReply OnAddKeyClicked()
	{
		FSequencer& Sequencer = DisplayNode->GetSequencer();
		float CurrentTime = Sequencer.GetLocalTime();

		TSet<TSharedPtr<IKeyArea>> KeyAreas;
		SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);

		// Prune out any key areas that do not exist at the current time, or are overlapped
		TMap<FName, TArray<TSharedPtr<IKeyArea>>> NameToKeyAreas;
		for (TSharedPtr<IKeyArea> KeyArea : KeyAreas)
		{
			NameToKeyAreas.FindOrAdd(KeyArea->GetName()).Add(KeyArea);
		}

		FScopedTransaction Transaction(LOCTEXT("AddKeys", "Add Keys at Current Time"));
		for (auto& Pair : NameToKeyAreas)
		{
			TArray<UMovieSceneSection*> AllSections;
			for (TSharedPtr<IKeyArea>& KeyArea : Pair.Value)
			{
				AllSections.Add(KeyArea->GetOwningSection());
			}

			int32 KeyAreaIndex = SequencerHelpers::GetSectionFromTime(AllSections, CurrentTime);

			if (KeyAreaIndex != INDEX_NONE)
			{
				UMovieSceneSection* OwningSection = AllSections[KeyAreaIndex];

				OwningSection->SetFlags(RF_Transactional);
				if (OwningSection->TryModify())
				{
					Pair.Value[KeyAreaIndex]->AddKeyUnique(CurrentTime, Sequencer.GetKeyInterpolation());
					Sequencer.NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
				}
			}
		}

		Sequencer.UpdatePlaybackRange();

		return FReply::Handled();
	}

	TSharedPtr<FSequencerDisplayNode> DisplayNode;
};

#undef LOCTEXT_NAMESPACE
