// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneSequenceTemplateStore.h"
#include "MovieSceneSequence.h"
#include "UObject/ObjectKey.h"

FMovieSceneEvaluationTemplate& FMovieSceneSequenceTemplateStore::GetCompiledTemplate(UMovieSceneSequence& Sequence)
{
	return GetCompiledTemplate(Sequence, FObjectKey(&Sequence));
}

FMovieSceneEvaluationTemplate& FMovieSceneSequenceTemplateStore::GetCompiledTemplate(UMovieSceneSequence& Sequence, FObjectKey InSequenceKey)
{
	UObject* ObjectKey = InSequenceKey.ResolveObjectPtr();
#if WITH_EDITORONLY_DATA
	if (AreTemplatesVolatile())
	{
		// Lookup into map here if we have a key that is not the sequence
		if (ObjectKey == &Sequence)
		{
			Sequence.EvaluationTemplate.Regenerate(Sequence.TemplateParameters);
		}
		else
		{
			// assume this is an instanced subsequence
			FCachedMovieSceneEvaluationTemplate* EvaluationTemplatePtr = Sequence.InstancedSubSequenceEvaluationTemplates.Find(ObjectKey);
			if (EvaluationTemplatePtr == nullptr)
			{
				EvaluationTemplatePtr = &Sequence.InstancedSubSequenceEvaluationTemplates.Add(ObjectKey);
				EvaluationTemplatePtr->Initialize(Sequence, this);
			}

			EvaluationTemplatePtr->Regenerate(Sequence.TemplateParameters);
		}
	}
#endif

	// Lookup into map here if we have a key that is not the sequence
	if (ObjectKey == &Sequence)
	{
		return Sequence.EvaluationTemplate;
	}
	else
	{
		// assume this is an instanced subsequence
		return Sequence.InstancedSubSequenceEvaluationTemplates.FindChecked(ObjectKey);
	}
}
