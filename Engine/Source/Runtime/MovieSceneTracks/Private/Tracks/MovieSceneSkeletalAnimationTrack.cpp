// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "MovieSceneEvaluationCustomVersion.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneSkeletalAnimationTemplate.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"

#define LOCTEXT_NAMESPACE "MovieSceneSkeletalAnimationTrack"


/* UMovieSceneSkeletalAnimationTrack structors
 *****************************************************************************/

UMovieSceneSkeletalAnimationTrack::UMovieSceneSkeletalAnimationTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUseLegacySectionIndexBlend(false)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(124, 15, 124, 65);
#endif

	SupportedBlendTypes.Add(EMovieSceneBlendType::Absolute);

	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
}


/* UMovieSceneSkeletalAnimationTrack interface
 *****************************************************************************/

void UMovieSceneSkeletalAnimationTrack::AddNewAnimation(float KeyTime, UAnimSequenceBase* AnimSequence)
{
	UMovieSceneSkeletalAnimationSection* NewSection = Cast<UMovieSceneSkeletalAnimationSection>(CreateNewSection());
	{
		NewSection->InitialPlacement(AnimationSections, KeyTime, KeyTime + AnimSequence->SequenceLength, SupportsMultipleRows());
		NewSection->Params.Animation = AnimSequence;
	}

	AddSection(*NewSection);
}


TArray<UMovieSceneSection*> UMovieSceneSkeletalAnimationTrack::GetAnimSectionsAtTime(float Time)
{
	TArray<UMovieSceneSection*> Sections;
	for (auto Section : AnimationSections)
	{
		if (Section->IsTimeWithinSection(Time))
		{
			Sections.Add(Section);
		}
	}

	return Sections;
}


/* UMovieSceneTrack interface
 *****************************************************************************/

void UMovieSceneSkeletalAnimationTrack::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerCustomVersion(FMovieSceneEvaluationCustomVersion::GUID) < FMovieSceneEvaluationCustomVersion::AddBlendingSupport)
	{
		bUseLegacySectionIndexBlend = true;
	}
}

const TArray<UMovieSceneSection*>& UMovieSceneSkeletalAnimationTrack::GetAllSections() const
{
	return AnimationSections;
}


bool UMovieSceneSkeletalAnimationTrack::SupportsMultipleRows() const
{
	return true;
}


UMovieSceneSection* UMovieSceneSkeletalAnimationTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSkeletalAnimationSection>(this);
}


void UMovieSceneSkeletalAnimationTrack::RemoveAllAnimationData()
{
	AnimationSections.Empty();
}


bool UMovieSceneSkeletalAnimationTrack::HasSection(const UMovieSceneSection& Section) const
{
	return AnimationSections.Contains(&Section);
}


void UMovieSceneSkeletalAnimationTrack::AddSection(UMovieSceneSection& Section)
{
	AnimationSections.Add(&Section);
}


void UMovieSceneSkeletalAnimationTrack::RemoveSection(UMovieSceneSection& Section)
{
	AnimationSections.Remove(&Section);
}


bool UMovieSceneSkeletalAnimationTrack::IsEmpty() const
{
	return AnimationSections.Num() == 0;
}


TRange<float> UMovieSceneSkeletalAnimationTrack::GetSectionBoundaries() const
{
	TArray<TRange<float>> Bounds;

	for (auto Section : AnimationSections)
	{
		Bounds.Add(Section->GetRange());
	}

	return TRange<float>::Hull(Bounds);
}

#if WITH_EDITORONLY_DATA

FText UMovieSceneSkeletalAnimationTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Animation");
}

#endif

TInlineValue<FMovieSceneSegmentCompilerRules> UMovieSceneSkeletalAnimationTrack::GetRowCompilerRules() const
{
	// Apply an upper bound exclusive blend
	struct FSkeletalAnimationRowCompilerRules : FMovieSceneSegmentCompilerRules
	{
		bool bUseLegacySectionIndexBlend;
		FSkeletalAnimationRowCompilerRules(bool bInUseLegacySectionIndexBlend) : bUseLegacySectionIndexBlend(bInUseLegacySectionIndexBlend) {}

		virtual void BlendSegment(FMovieSceneSegment& Segment, const TArrayView<const FMovieSceneSectionData>& SourceData) const
		{
			// Run the default high pass filter for overlap priority
			MovieSceneSegmentCompiler::BlendSegmentHighPass(Segment, SourceData);

			if (bUseLegacySectionIndexBlend)
			{
				// Weed out based on array index (legacy behaviour)
				MovieSceneSegmentCompiler::BlendSegmentLegacySectionOrder(Segment, SourceData);
			}
		}
	};
	return FSkeletalAnimationRowCompilerRules(bUseLegacySectionIndexBlend);
}

#undef LOCTEXT_NAMESPACE
