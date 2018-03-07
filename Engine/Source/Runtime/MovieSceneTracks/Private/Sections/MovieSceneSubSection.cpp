// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneSubSection.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSequence.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"

TWeakObjectPtr<UMovieSceneSubSection> UMovieSceneSubSection::TheRecordingSection;

float DeprecatedMagicNumber = TNumericLimits<float>::Lowest();

/* UMovieSceneSubSection structors
 *****************************************************************************/

UMovieSceneSubSection::UMovieSceneSubSection()
	: StartOffset_DEPRECATED(DeprecatedMagicNumber)
	, TimeScale_DEPRECATED(DeprecatedMagicNumber)
	, PrerollTime_DEPRECATED(DeprecatedMagicNumber)
{
}

FString UMovieSceneSubSection::GetPathNameInMovieScene() const
{
	UMovieScene* OuterMovieScene = GetTypedOuter<UMovieScene>();
	check(OuterMovieScene);
	return GetPathName(OuterMovieScene);
}

FMovieSceneSequenceID UMovieSceneSubSection::GetSequenceID() const
{
	FString FullPath = GetPathNameInMovieScene();
	if (SubSequence)
	{
		FullPath += TEXT(" / ");
		FullPath += SubSequence->GetPathName();
	}

	return FMovieSceneSequenceID(FCrc::Strihash_DEPRECATED(*FullPath));
}

void UMovieSceneSubSection::PostLoad()
{
	if (StartOffset_DEPRECATED != DeprecatedMagicNumber)
	{
		Parameters.StartOffset = StartOffset_DEPRECATED;
	}

	if (TimeScale_DEPRECATED != DeprecatedMagicNumber)
	{
		Parameters.TimeScale = TimeScale_DEPRECATED;
	}

	if (PrerollTime_DEPRECATED != DeprecatedMagicNumber)
	{
		Parameters.PrerollTime_DEPRECATED = PrerollTime_DEPRECATED;
	}

	// Pre and post roll is now supported generically
	if (Parameters.PrerollTime_DEPRECATED > 0.f)
	{
		SetPreRollTime(Parameters.PrerollTime_DEPRECATED);
	}

	if (Parameters.PostrollTime_DEPRECATED > 0.f)
	{
		SetPostRollTime(Parameters.PostrollTime_DEPRECATED);
	}

	Super::PostLoad();
}

void UMovieSceneSubSection::SetSequence(UMovieSceneSequence* Sequence)
{
	SubSequence = Sequence;

#if WITH_EDITOR
	OnSequenceChangedDelegate.ExecuteIfBound(SubSequence);
#endif
}

UMovieSceneSequence* UMovieSceneSubSection::GetSequence() const
{
	// when recording we need to act as if we have no sequence
	// the sequence is patched at the end of recording
	if(GetRecordingSection() == this)
	{
		return nullptr;
	}
	else
	{
		return SubSequence;
	}
}

UMovieSceneSubSection* UMovieSceneSubSection::GetRecordingSection()
{
	// check if the section is still valid and part of a track (i.e. it has not been deleted or GCed)
	if(TheRecordingSection.IsValid())
	{
		UMovieSceneTrack* TrackOuter = Cast<UMovieSceneTrack>(TheRecordingSection->GetOuter());
		if(TrackOuter)
		{
			if(TrackOuter->HasSection(*TheRecordingSection.Get()))
			{
				return TheRecordingSection.Get();
			}
		}
	}

	return nullptr;
}

void UMovieSceneSubSection::SetAsRecording(bool bRecord)
{
	if(bRecord)
	{
		TheRecordingSection = this;
	}
	else
	{
		TheRecordingSection = nullptr;
	}
}

bool UMovieSceneSubSection::IsSetAsRecording()
{
	return GetRecordingSection() != nullptr;
}

AActor* UMovieSceneSubSection::GetActorToRecord()
{
	UMovieSceneSubSection* RecordingSection = GetRecordingSection();
	if(RecordingSection)
	{
		return RecordingSection->ActorToRecord.Get();
	}

	return nullptr;
}

#if WITH_EDITOR
void UMovieSceneSubSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// recreate runtime instance when sequence is changed
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMovieSceneSubSection, SubSequence))
	{
		OnSequenceChangedDelegate.ExecuteIfBound(SubSequence);
	}
}
#endif

UMovieSceneSection* UMovieSceneSubSection::SplitSection( float SplitTime )
{
	if ( !IsTimeWithinSection( SplitTime ) )
	{
		return nullptr;
	}

	float InitialStartTime = GetStartTime();
	float InitialStartOffset = Parameters.StartOffset;
	float NewStartOffset = ( SplitTime - InitialStartTime ) / Parameters.TimeScale;
	NewStartOffset += InitialStartOffset;

	// Ensure start offset is not less than 0
	NewStartOffset = FMath::Max( NewStartOffset, 0.f );

	UMovieSceneSubSection* NewSection = Cast<UMovieSceneSubSection>( UMovieSceneSection::SplitSection( SplitTime ) );
	if ( NewSection )
	{
		NewSection->Parameters.StartOffset = NewStartOffset;
		return NewSection;
	}

	return nullptr;
}

void UMovieSceneSubSection::TrimSection( float TrimTime, bool bTrimLeft )
{
	if ( !IsTimeWithinSection( TrimTime ) )
	{
		return;
	}

	float InitialStartTime = GetStartTime();
	float InitialStartOffset = Parameters.StartOffset;

	UMovieSceneSection::TrimSection( TrimTime, bTrimLeft );

	// If trimming off the left, set the offset of the shot
	if ( bTrimLeft )
	{
		float NewStartOffset = ( TrimTime - InitialStartTime ) / Parameters.TimeScale;
		NewStartOffset += InitialStartOffset;

		// Ensure start offset is not less than 0
		Parameters.StartOffset = FMath::Max( NewStartOffset, 0.f );
	}
}

FMovieSceneEvaluationTemplate& UMovieSceneSubSection::GenerateTemplateForSubSequence(const FMovieSceneTrackCompilerArgs& InArgs) const
{
	return InArgs.SubSequenceStore.GetCompiledTemplate(*SubSequence);
}

FMovieSceneSubSequenceData UMovieSceneSubSection::GenerateSubSequenceData() const
{
	FMovieSceneSequenceTransform RootToSequenceTransform =
	FMovieSceneSequenceTransform(SubSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue() + Parameters.StartOffset) *		// Inner play offset
	FMovieSceneSequenceTransform(0.f, Parameters.TimeScale) *		// Inner play rate
	FMovieSceneSequenceTransform(-GetStartTime());					// Outer section start time

#if WITH_EDITORONLY_DATA
	TRange<float> InnerSectionRange(
		GetStartTime() * RootToSequenceTransform,
		GetEndTime() * RootToSequenceTransform
	);
	FMovieSceneSubSequenceData SubData(*SubSequence, GetSequenceID(), *GetPathNameInMovieScene(), InnerSectionRange);
#else
	FMovieSceneSubSequenceData SubData(*SubSequence, GetSequenceID());
#endif

	// Make sure pre/postroll ranges are in the inner sequence's time space
	if (GetPreRollTime() > 0)
	{
		SubData.PreRollRange = TRange<float>(GetStartTime() - GetPreRollTime(), TRangeBound<float>::Exclusive(GetStartTime())) * RootToSequenceTransform;
	}
	if (GetPostRollTime() > 0)
	{
		SubData.PostRollRange = TRange<float>(TRangeBound<float>::Exclusive(GetEndTime()), GetEndTime() + GetPostRollTime()) * RootToSequenceTransform;
	}

	// Construct the sub sequence data for this sub section
	SubData.RootToSequenceTransform = RootToSequenceTransform;
	SubData.SequenceKeyObject = SubSequence;

	SubData.HierarchicalBias = Parameters.HierarchicalBias;

	return SubData;
}
