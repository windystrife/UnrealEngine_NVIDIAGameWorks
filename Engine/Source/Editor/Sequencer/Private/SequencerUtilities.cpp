// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerUtilities.h"
#include "Misc/Paths.h"
#include "Layout/Margin.h"
#include "Fonts/SlateFontInfo.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "SequencerTrackNode.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "MultiBoxBuilder.h"
#include "ISequencerTrackEditor.h"
#include "ISequencer.h"
#include "ScopedTransaction.h"
#include "Package.h"

#define LOCTEXT_NAMESPACE "FSequencerUtilities"

static EVisibility GetRolloverVisibility(TAttribute<bool> HoverState, TWeakPtr<SComboButton> WeakComboButton)
{
	TSharedPtr<SComboButton> ComboButton = WeakComboButton.Pin();
	if (HoverState.Get() || ComboButton->IsOpen())
	{
		return EVisibility::SelfHitTestInvisible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

TSharedRef<SWidget> FSequencerUtilities::MakeAddButton(FText HoverText, FOnGetContent MenuContent, const TAttribute<bool>& HoverState)
{
	FSlateFontInfo SmallLayoutFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8 );

	TSharedRef<STextBlock> ComboButtonText = SNew(STextBlock)
		.Text(HoverText)
		.Font(SmallLayoutFont)
		.ColorAndOpacity( FSlateColor::UseForeground() );

	TSharedRef<SComboButton> ComboButton =

		SNew(SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
		.ForegroundColor( FSlateColor::UseForeground() )
		.OnGetMenuContent(MenuContent)
		.ContentPadding(FMargin(5, 2))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ButtonContent()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0,0,2,0))
			[
				SNew(SImage)
				.ColorAndOpacity( FSlateColor::UseForeground() )
				.Image(FEditorStyle::GetBrush("Plus"))
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				ComboButtonText
			]
		];

	TAttribute<EVisibility> Visibility = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(GetRolloverVisibility, HoverState, TWeakPtr<SComboButton>(ComboButton)));
	ComboButtonText->SetVisibility(Visibility);

	return ComboButton;
}

void FSequencerUtilities::PopulateMenu_CreateNewSection(FMenuBuilder& MenuBuilder, int32 RowIndex, UMovieSceneTrack* Track, TWeakPtr<ISequencer> InSequencer)
{
	if (!Track)
	{
		return;
	}
	
	auto CreateNewSection = [Track, InSequencer, RowIndex](EMovieSceneBlendType BlendType)
	{
		TSharedPtr<ISequencer> Sequencer = InSequencer.Pin();
		if (!Sequencer.IsValid())
		{
			return;
		}

		const float StartAtTime = Sequencer->GetLocalTime();
		TRange<float> VisibleRange = Sequencer->GetViewRange();

		FScopedTransaction Transaction(LOCTEXT("AddSectionTransactionText", "Add Section"));
		if (UMovieSceneSection* NewSection = Track->CreateNewSection())
		{
			int32 OverlapPriority = 0;
			for (UMovieSceneSection* Section : Track->GetAllSections())
			{
				OverlapPriority = FMath::Max(Section->GetOverlapPriority() + 1, OverlapPriority);
			}

			Track->Modify();

			NewSection->SetIsInfinite(false);
			NewSection->SetStartTime(StartAtTime);
			NewSection->SetOverlapPriority(OverlapPriority);
			NewSection->SetEndTime(VisibleRange.GetUpperBoundValue() - (VisibleRange.GetUpperBoundValue() - StartAtTime)*0.25f);
			NewSection->SetRowIndex(RowIndex);
			NewSection->SetBlendType(BlendType);

			Track->AddSection(*NewSection);
			Track->UpdateEasing();

			Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
		}
		else
		{
			Transaction.Cancel();
		}
	};

	FText NameOverride		= Track->GetSupportedBlendTypes().Num() == 1 ? LOCTEXT("AddSectionText", "Add New Section") : FText();
	FText TooltipOverride	= Track->GetSupportedBlendTypes().Num() == 1 ? LOCTEXT("AddSectionToolTip", "Adds a new section at the current time") : FText();

	const UEnum* MovieSceneBlendType = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMovieSceneBlendType"));
	for (EMovieSceneBlendType BlendType : Track->GetSupportedBlendTypes())
	{
		FText DisplayName = MovieSceneBlendType->GetDisplayNameTextByValue((int64)BlendType);
		FName EnumValueName = MovieSceneBlendType->GetNameByValue((int64)BlendType);
		MenuBuilder.AddMenuEntry(
			NameOverride.IsEmpty() ? DisplayName : NameOverride,
			TooltipOverride.IsEmpty() ? FText::Format(LOCTEXT("AddSectionFormatToolTip", "Adds a new {0} section at the current time"), DisplayName) : TooltipOverride,
			FSlateIcon("EditorStyle", EnumValueName),
			FUIAction(FExecuteAction::CreateLambda(CreateNewSection, BlendType))
		);
	}
}

void FSequencerUtilities::PopulateMenu_SetBlendType(FMenuBuilder& MenuBuilder, UMovieSceneSection* Section)
{
	PopulateMenu_SetBlendType(MenuBuilder, TArray<TWeakObjectPtr<UMovieSceneSection>>({ Section }));
}

void FSequencerUtilities::PopulateMenu_SetBlendType(FMenuBuilder& MenuBuilder, const TArray<TWeakObjectPtr<UMovieSceneSection>>& InSections)
{
	auto Execute = [InSections](EMovieSceneBlendType BlendType)
	{
		FScopedTransaction Transaction(LOCTEXT("SetBlendType", "Set Blend Type"));
		for (TWeakObjectPtr<UMovieSceneSection> WeakSection : InSections)
		{
			if (UMovieSceneSection* Section = WeakSection.Get())
			{
				Section->Modify();
				Section->SetBlendType(BlendType);
			}
		}
	};

	const UEnum* MovieSceneBlendType = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("EMovieSceneBlendType"));
	for (int32 NameIndex = 0; NameIndex < MovieSceneBlendType->NumEnums() - 1; ++NameIndex)
	{
		EMovieSceneBlendType BlendType = (EMovieSceneBlendType)MovieSceneBlendType->GetValueByIndex(NameIndex);

		// Don't include this if it's not supported
		for (TWeakObjectPtr<UMovieSceneSection> WeakSection : InSections)
		{
			UMovieSceneSection* Section = WeakSection.Get();
			if (Section && !Section->GetSupportedBlendTypes().Contains(BlendType))
			{
				return;
			}
		}

		FName EnumValueName = MovieSceneBlendType->GetNameByIndex(NameIndex);
		MenuBuilder.AddMenuEntry(
			MovieSceneBlendType->GetDisplayNameTextByIndex(NameIndex),
			MovieSceneBlendType->GetToolTipTextByIndex(NameIndex),
			FSlateIcon("EditorStyle", EnumValueName),
			FUIAction(FExecuteAction::CreateLambda(Execute, BlendType))
		);
	}
}


FName FSequencerUtilities::GetUniqueName( FName CandidateName, const TArray<FName>& ExistingNames )
{
	FString CandidateNameString = CandidateName.ToString();
	FString BaseNameString = CandidateNameString;
	if ( CandidateNameString.Len() >= 3 && CandidateNameString.Right(3).IsNumeric() )
	{
		BaseNameString = CandidateNameString.Left( CandidateNameString.Len() - 3 );
	}

	FName UniqueName = FName(*BaseNameString);
	int32 NameIndex = 1;
	while ( ExistingNames.Contains( UniqueName ) )
	{
		UniqueName = FName( *FString::Printf(TEXT("%s%i"), *BaseNameString, NameIndex ) );
		NameIndex++;
	}

	return UniqueName;
}

#undef LOCTEXT_NAMESPACE