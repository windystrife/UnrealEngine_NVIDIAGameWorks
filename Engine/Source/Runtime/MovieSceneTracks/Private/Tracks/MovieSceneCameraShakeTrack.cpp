// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneCameraShakeTrack.h"
#include "Sections/MovieSceneCameraShakeSection.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "Evaluation/MovieSceneCameraAnimTemplate.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneCompilerRules.h"

#define LOCTEXT_NAMESPACE "MovieSceneCameraShakeTrack"

void UMovieSceneCameraShakeTrack::AddNewCameraShake(float KeyTime, TSubclassOf<UCameraShake> ShakeClass)
{
	UMovieSceneCameraShakeSection* const NewSection = Cast<UMovieSceneCameraShakeSection>(CreateNewSection());
	if (NewSection)
	{
		// #fixme get length
		NewSection->InitialPlacement(CameraShakeSections, KeyTime, KeyTime + 5.f /*AnimSequence->SequenceLength*/, SupportsMultipleRows());
		NewSection->ShakeData.ShakeClass = ShakeClass;
		
		AddSection(*NewSection);
	}
}

void UMovieSceneCameraShakeTrack::PostCompile(FMovieSceneEvaluationTrack& OutTrack, const FMovieSceneTrackCompilerArgs& Args) const
{
	FMovieSceneSharedDataId UniqueId = FMovieSceneAdditiveCameraAnimationTrackTemplate::SharedDataId;

	// Add a new shared track for the additive camera anim. There will only be one of these, and it will apply all the additive camera animations for this object.
	FMovieSceneEvaluationTrack SharedTrackTemplate(Args.ObjectBindingId);
	SharedTrackTemplate.DefineAsSingleTemplate(FMovieSceneAdditiveCameraAnimationTrackTemplate());
	SharedTrackTemplate.SetEvaluationPriority(0xF);
	
	Args.Generator.AddSharedTrack(MoveTemp(SharedTrackTemplate), UniqueId, *this);
}

TInlineValue<FMovieSceneSegmentCompilerRules> UMovieSceneCameraShakeTrack::GetTrackCompilerRules() const
{
	return FMovieSceneAdditiveCameraRules(this);
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneCameraShakeTrack::GetDisplayName() const
{
	return LOCTEXT("TrackName", "Camera Shake");
}
#endif




/* UMovieSceneTrack interface
*****************************************************************************/


const TArray<UMovieSceneSection*>& UMovieSceneCameraShakeTrack::GetAllSections() const
{
	return CameraShakeSections;
}


UMovieSceneSection* UMovieSceneCameraShakeTrack::CreateNewSection()
{
	return NewObject<UMovieSceneCameraShakeSection>(this);
}


void UMovieSceneCameraShakeTrack::RemoveAllAnimationData()
{
	CameraShakeSections.Empty();
}


bool UMovieSceneCameraShakeTrack::HasSection(const UMovieSceneSection& Section) const
{
	return CameraShakeSections.Contains(&Section);
}


void UMovieSceneCameraShakeTrack::AddSection(UMovieSceneSection& Section)
{
	CameraShakeSections.Add(&Section);
}


void UMovieSceneCameraShakeTrack::RemoveSection(UMovieSceneSection& Section)
{
	CameraShakeSections.Remove(&Section);
}


bool UMovieSceneCameraShakeTrack::IsEmpty() const
{
	return CameraShakeSections.Num() == 0;
}


TRange<float> UMovieSceneCameraShakeTrack::GetSectionBoundaries() const
{
	TArray<TRange<float>> Bounds;

	for (auto Section : CameraShakeSections)
	{
		Bounds.Add(Section->GetRange());
	}

	return TRange<float>::Hull(Bounds);
}


#undef LOCTEXT_NAMESPACE
