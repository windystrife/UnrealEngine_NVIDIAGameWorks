// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/LevelSequenceFactoryNew.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneToolsProjectSettings.h"

#define LOCTEXT_NAMESPACE "MovieSceneFactory"


/* ULevelSequenceFactory structors
 *****************************************************************************/

ULevelSequenceFactoryNew::ULevelSequenceFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = ULevelSequence::StaticClass();
}


/* UFactory interface
 *****************************************************************************/

UObject* ULevelSequenceFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	auto NewLevelSequence = NewObject<ULevelSequence>(InParent, Name, Flags|RF_Transactional);
	NewLevelSequence->Initialize();
	
	// Set up some sensible defaults
	const UMovieSceneToolsProjectSettings* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();
	NewLevelSequence->GetMovieScene()->SetPlaybackRange(ProjectSettings->DefaultStartTime, ProjectSettings->DefaultStartTime + ProjectSettings->DefaultDuration);

	return NewLevelSequence;
}


bool ULevelSequenceFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}


#undef LOCTEXT_NAMESPACE
