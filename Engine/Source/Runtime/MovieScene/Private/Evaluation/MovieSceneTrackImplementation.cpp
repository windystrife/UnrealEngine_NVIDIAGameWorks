// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneTrackImplementation.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Evaluation/MovieSceneEvalTemplateSerializer.h"

bool FMovieSceneTrackImplementationPtr::Serialize(FArchive& Ar)
{
	return SerializeEvaluationTemplate(*this, Ar);
}
