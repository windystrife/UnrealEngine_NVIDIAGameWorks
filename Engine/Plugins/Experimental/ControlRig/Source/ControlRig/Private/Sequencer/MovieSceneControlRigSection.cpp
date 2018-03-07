// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneControlRigSection.h"
#include "Animation/AnimSequence.h"
#include "MessageLog.h"
#include "MovieScene.h"
#include "ControlRigSequence.h"
#include "ControlRigBindingTemplate.h"

#define LOCTEXT_NAMESPACE "MovieSceneControlRigSection"

UMovieSceneControlRigSection::UMovieSceneControlRigSection()
{
	// Section template relies on always restoring state for objects when they are no longer animating. This is how it releases animation control.
	EvalOptions.CompletionMode = EMovieSceneCompletionMode::RestoreState;

	Weight.SetDefaultValue(1.0f);
}


void UMovieSceneControlRigSection::MoveSection(float DeltaTime, TSet<FKeyHandle>& KeyHandles)
{
	Super::MoveSection(DeltaTime, KeyHandles);

	Weight.ShiftCurve(DeltaTime, KeyHandles);
}


void UMovieSceneControlRigSection::DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
{
	Parameters.TimeScale /= DilationFactor;

	Super::DilateSection(DilationFactor, Origin, KeyHandles);

	Weight.ScaleCurve(Origin, DilationFactor, KeyHandles);
}

void UMovieSceneControlRigSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (auto It(Weight.GetKeyHandleIterator()); It; ++It)
	{
		float Time = Weight.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
}

FMovieSceneEvaluationTemplate& UMovieSceneControlRigSection::GenerateTemplateForSubSequence(const FMovieSceneTrackCompilerArgs& InArgs) const
{
	// Use our section as the object key here
	FMovieSceneEvaluationTemplate& Template = InArgs.SubSequenceStore.GetCompiledTemplate(*SubSequence, FObjectKey(this));

	for (TPair<FMovieSceneTrackIdentifier, FMovieSceneEvaluationTrack>& TrackPair : Template.GetTracks())
	{
		// iterate child templates
		for (FMovieSceneEvalTemplatePtr& ChildTemplate : TrackPair.Value.GetChildTemplates())
		{
			if (&(*ChildTemplate).GetScriptStruct() == FControlRigBindingTemplate::StaticStruct())
			{
				// set section curves into binding track
				FControlRigBindingTemplate& BindingTemplate = *static_cast<FControlRigBindingTemplate*>(&(*ChildTemplate));

				BindingTemplate.SetObjectBindingId(InArgs.ObjectBindingId, MovieSceneSequenceID::Root);
				BindingTemplate.SetWeightCurve(Weight, -GetStartTime(), Parameters.TimeScale == 0.0f ? 0.0f : 1.0f / Parameters.TimeScale);
				BindingTemplate.SetPerBoneBlendFilter(bApplyBoneFilter, BoneFilter);
				BindingTemplate.SetAdditive(bAdditive);
			}
		}
	}

	return Template;
}

FMovieSceneSubSequenceData UMovieSceneControlRigSection::GenerateSubSequenceData() const
{
	FMovieSceneSubSequenceData SubData = UMovieSceneSubSection::GenerateSubSequenceData();

	SubData.SequenceKeyObject = this;

	return SubData;
}

#undef LOCTEXT_NAMESPACE 