// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneEventTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieSceneEventSection.h"
#include "Evaluation/MovieSceneEventTemplate.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"
#include "IMovieSceneTracksModule.h"

#define LOCTEXT_NAMESPACE "MovieSceneEventTrack"


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneEventTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
}


UMovieSceneSection* UMovieSceneEventTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneEventSection::StaticClass(), NAME_None, RF_Transactional);
}


const TArray<UMovieSceneSection*>& UMovieSceneEventTrack::GetAllSections() const
{
	return Sections;
}


TRange<float> UMovieSceneEventTrack::GetSectionBoundaries() const
{
	TRange<float> SectionBoundaries = TRange<float>::Empty();

	for (auto& Section : Sections)
	{
		SectionBoundaries = TRange<float>::Hull(SectionBoundaries, Section->GetRange());
	}

	return SectionBoundaries;
}


bool UMovieSceneEventTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}


bool UMovieSceneEventTrack::IsEmpty() const
{
	return (Sections.Num() == 0);
}


void UMovieSceneEventTrack::RemoveAllAnimationData()
{
	Sections.Empty();
}


void UMovieSceneEventTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
}

FMovieSceneEvalTemplatePtr UMovieSceneEventTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneEventSectionTemplate(*CastChecked<UMovieSceneEventSection>(&InSection), *this);
}

void UMovieSceneEventTrack::PostCompile(FMovieSceneEvaluationTrack& Track, const FMovieSceneTrackCompilerArgs& Args) const
{
	switch (EventPosition)
	{
	case EFireEventsAtPosition::AtStartOfEvaluation:
		Track.SetEvaluationGroup(IMovieSceneTracksModule::GetEvaluationGroupName(EBuiltInEvaluationGroup::PreEvaluation));
		break;

	case EFireEventsAtPosition::AtEndOfEvaluation:
		Track.SetEvaluationGroup(IMovieSceneTracksModule::GetEvaluationGroupName(EBuiltInEvaluationGroup::PostEvaluation));
		break;

	default:
		Track.SetEvaluationGroup(IMovieSceneTracksModule::GetEvaluationGroupName(EBuiltInEvaluationGroup::SpawnObjects));
		Track.SetEvaluationPriority(UMovieSceneSpawnTrack::GetEvaluationPriority() - 100);
		break;
	}

	Track.SetEvaluationMethod(EEvaluationMethod::Swept);
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneEventTrack::GetDefaultDisplayName() const
{ 
	return LOCTEXT("TrackName", "Events"); 
}

#endif


#undef LOCTEXT_NAMESPACE
