// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSequence.h"
#include "Compilation/MovieSceneEvaluationTemplateGenerator.h"
#include "Evaluation/MovieSceneEvaluationCustomVersion.h"
#include "MovieScene.h"
#include "EditorObjectVersion.h"

UMovieSceneSequence::UMovieSceneSequence(const FObjectInitializer& Init)
	: Super(Init)
{
	bParentContextsAreSignificant = false;

	TemplateParameters.bForEditorPreview = false;
#if WITH_EDITORONLY_DATA
	EvaluationTemplate.Initialize(*this);
#endif
}

#if WITH_EDITORONLY_DATA
void UMovieSceneSequence::PostDuplicate(bool bDuplicateForPIE)
{
	if (bDuplicateForPIE)
	{
		EvaluationTemplate.Regenerate(TemplateParameters);
	}

	Super::PostDuplicate(bDuplicateForPIE);
}
#endif // WITH_EDITORONLY_DATA

void UMovieSceneSequence::PostLoad()
{
#if WITH_EDITORONLY_DATA
	// Wipe compiled data on editor load to ensure we don't try and iteratively compile previously saved content. In a cooked game, this will contain our up-to-date compiled template.
	EvaluationTemplate = FCachedMovieSceneEvaluationTemplate(*this);
#endif

	Super::PostLoad();
}

void UMovieSceneSequence::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FMovieSceneEvaluationCustomVersion::GUID);
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);

#if WITH_EDITORONLY_DATA
	if (Ar.IsCooking() && !HasAnyFlags(RF_ClassDefaultObject))
	{
		EvaluationTemplate.Regenerate(TemplateParameters);
	}
	else if (Ar.IsSaving())
	{
		// Don't save template data unless we're cooking
		EvaluationTemplate = FCachedMovieSceneEvaluationTemplate(*this);
	}
#endif
	
	Super::Serialize(Ar);
}

void UMovieSceneSequence::GenerateEvaluationTemplate(FMovieSceneEvaluationTemplate& Template, const FMovieSceneTrackCompilationParams& Params, FMovieSceneSequenceTemplateStore& Store)
{
	FMovieSceneEvaluationTemplateGenerator(*this, Template, Store).Generate(Params);
}

FGuid UMovieSceneSequence::FindPossessableObjectId(UObject& Object, UObject* Context) const
{
	UMovieScene* MovieScene = GetMovieScene();
	if (!MovieScene)
	{
		return FGuid();
	}

	// Search all possessables
	for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
	{
		FGuid ThisGuid = MovieScene->GetPossessable(Index).GetGuid();
		if (LocateBoundObjects(ThisGuid, Context).Contains(&Object))
		{
			return ThisGuid;
		}
	}
	return FGuid();
}
