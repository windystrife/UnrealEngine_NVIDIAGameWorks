// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceEditorActorBinding.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "Engine/Selection.h"
#include "SlateIconFinder.h"
#include "SceneOutlinerPublicTypes.h"
#include "SceneOutlinerModule.h"
#include "MultiBoxBuilder.h"
#include "Engine/Engine.h"
#include "SlateApplication.h"
#include "ModuleManager.h"
#include "Editor.h"
#include "SBox.h"

#define LOCTEXT_NAMESPACE "LevelSequenceEditorActorBinding"

FLevelSequenceEditorActorBinding::FLevelSequenceEditorActorBinding(TSharedRef<ISequencer> InSequencer)
	: Sequencer(InSequencer)
{
}

void FLevelSequenceEditorActorBinding::BuildSequencerAddMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddSubMenu(
		LOCTEXT("AddActor_Label", "Actor To Sequencer"),
		LOCTEXT("AddActor_ToolTip", "Allow sequencer to possess an actor that already exists in the current level"),
		FNewMenuDelegate::CreateRaw(this, &FLevelSequenceEditorActorBinding::AddPossessActorMenuExtensions),
		false /*bInOpenSubMenuOnClick*/,
		FSlateIcon("LevelSequenceEditorStyle", "LevelSequenceEditor.PossessNewActor")
		);
}

bool FLevelSequenceEditorActorBinding::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return InSequence->GetClass() == ULevelSequence::StaticClass();
}

void FLevelSequenceEditorActorBinding::AddPossessActorMenuExtensions(FMenuBuilder& MenuBuilder)
{
	auto IsActorValidForPossession = [=](const AActor* InActor, TWeakPtr<ISequencer> InWeakSequencer)
	{
		TSharedPtr<ISequencer> SequencerPtr = InWeakSequencer.Pin();
		bool bCreateHandleIfMissing = false;
		return SequencerPtr.IsValid() && !SequencerPtr->GetHandleToObject((UObject*)InActor, bCreateHandleIfMissing).IsValid();
	};

	// Set up a menu entry to add the selected actor(s) to the sequencer
	TArray<AActor*> ActorsValidForPossession;
	GEditor->GetSelectedActors()->GetSelectedObjects(ActorsValidForPossession);
	ActorsValidForPossession.RemoveAll([&](AActor* In){ return !IsActorValidForPossession(In, Sequencer); });

	FText SelectedLabel;
	FSlateIcon ActorIcon = FSlateIconFinder::FindIconForClass(AActor::StaticClass());
	if (ActorsValidForPossession.Num() == 1)
	{
		SelectedLabel = FText::Format(LOCTEXT("AddSpecificActor", "Add '{0}'"), FText::FromString(ActorsValidForPossession[0]->GetActorLabel()));
		ActorIcon = FSlateIconFinder::FindIconForClass(ActorsValidForPossession[0]->GetClass());
	}
	else if (ActorsValidForPossession.Num() > 1)
	{
		SelectedLabel = FText::Format(LOCTEXT("AddSpecificActor", "Add Current Selection ({0} actors)"), FText::AsNumber(ActorsValidForPossession.Num()));
	}

	if (!SelectedLabel.IsEmpty())
	{
		// Copy the array into the lambda - probably not that big a deal
		MenuBuilder.AddMenuEntry(SelectedLabel, FText(), ActorIcon, FExecuteAction::CreateLambda([=]{
			FSlateApplication::Get().DismissAllMenus();
			AddActorsToSequencer(ActorsValidForPossession.GetData(), ActorsValidForPossession.Num());
		}));
	}
	
	MenuBuilder.BeginSection("ChooseActorSection", LOCTEXT("ChooseActor", "Choose Actor:"));

	using namespace SceneOutliner;

	// Set up a menu entry to add any arbitrary actor to the sequencer
	FInitializationOptions InitOptions;
	{
		InitOptions.Mode = ESceneOutlinerMode::ActorPicker;

		// We hide the header row to keep the UI compact.
		InitOptions.bShowHeaderRow = false;
		InitOptions.bShowSearchBox = true;
		InitOptions.bShowCreateNewFolder = false;
		InitOptions.bFocusSearchBoxWhenOpened = true;

		// Only want the actor label column
		InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));

		// Only display actors that are not possessed already
		InitOptions.Filters->AddFilterPredicate(FActorFilterPredicate::CreateLambda(IsActorValidForPossession, Sequencer));
	}

	// actor selector to allow the user to choose an actor
	FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
	TSharedRef< SWidget > MiniSceneOutliner =
		SNew(SBox)
		.MaxDesiredHeight(400.0f)
		.WidthOverride(300.0f)
		[
			SceneOutlinerModule.CreateSceneOutliner(
				InitOptions,
				FOnActorPicked::CreateLambda([=](AActor* Actor){
					// Create a new binding for this actor
					FSlateApplication::Get().DismissAllMenus();
					AddActorsToSequencer(&Actor, 1);
				})
			)
		];

	MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
	MenuBuilder.EndSection();
}

void FLevelSequenceEditorActorBinding::AddActorsToSequencer(AActor*const* InActors, int32 NumActors)
{
	TArray<TWeakObjectPtr<AActor>> Actors;

	while (NumActors--)
	{
		AActor* ThisActor = *InActors;
		Actors.Add(ThisActor);

		InActors++;
	}

	Sequencer.Pin()->AddActors(Actors);
}

#undef LOCTEXT_NAMESPACE