// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Animation/AnimSequence.h"
#include "Evaluation/MovieSceneSkeletalAnimationTemplate.h"
#include "MessageLog.h"
#include "MovieScene.h"
#include "SequencerObjectVersion.h"

#define LOCTEXT_NAMESPACE "MovieSceneSkeletalAnimationSection"

namespace
{
	FName DefaultSlotName( "DefaultSlot" );
}

FMovieSceneSkeletalAnimationParams::FMovieSceneSkeletalAnimationParams()
{
	Animation = nullptr;
	StartOffset = 0.f;
	EndOffset = 0.f;
	PlayRate = 1.f;
	bReverse = false;
	SlotName = DefaultSlotName;
	Weight.SetDefaultValue(1.f);
}

UMovieSceneSkeletalAnimationSection::UMovieSceneSkeletalAnimationSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	AnimSequence_DEPRECATED = nullptr;
	Animation_DEPRECATED = nullptr;
	StartOffset_DEPRECATED = 0.f;
	EndOffset_DEPRECATED = 0.f;
	PlayRate_DEPRECATED = 1.f;
	bReverse_DEPRECATED = false;
	SlotName_DEPRECATED = DefaultSlotName;

	BlendType = EMovieSceneBlendType::Absolute;
	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);

#if WITH_EDITOR
	PreviousPlayRate = Params.PlayRate;
#endif
}

void UMovieSceneSkeletalAnimationSection::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FSequencerObjectVersion::GUID);
	Super::Serialize(Ar);
}

void UMovieSceneSkeletalAnimationSection::PostLoad()
{
	if (AnimSequence_DEPRECATED)
	{
		Params.Animation = AnimSequence_DEPRECATED;
	}

	if (Animation_DEPRECATED != nullptr)
	{
		Params.Animation = Animation_DEPRECATED;
	}

	if (StartOffset_DEPRECATED != 0.f)
	{
		Params.StartOffset = StartOffset_DEPRECATED;
	}

	if (EndOffset_DEPRECATED != 0.f)
	{
		Params.EndOffset = EndOffset_DEPRECATED;
	}

	if (PlayRate_DEPRECATED != 1.f)
	{
		Params.PlayRate = PlayRate_DEPRECATED;
	}

	if (bReverse_DEPRECATED != false)
	{
		Params.bReverse = bReverse_DEPRECATED;
	}

	if (SlotName_DEPRECATED != DefaultSlotName)
	{
		Params.SlotName = SlotName_DEPRECATED;
	}

	// if version is less than this
	if (GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::ConvertEnableRootMotionToForceRootLock)
	{
		UAnimSequence* AnimSeq = Cast<UAnimSequence>(Params.Animation);
		if (AnimSeq && AnimSeq->bEnableRootMotion && !AnimSeq->bForceRootLock)
		{
			// this is not ideal, but previously single player node was using this flag to whether or not to extract root motion
			// with new anim sequencer instance, this would break because we use the instance flag to extract root motion or not
			// so instead of setting that flag, we use bForceRootLock flag to asset
			// this can have side effect, where users didn't want that to be on to start with
			// So we'll notify users to let them know this has to be saved
			AnimSeq->bForceRootLock = true;
			AnimSeq->MarkPackageDirty();
			// warning to users
#if WITH_EDITOR			
			if (!IsRunningGame())
			{
				static FName NAME_LoadErrors("LoadErrors");
				FMessageLog LoadErrors(NAME_LoadErrors);

				TSharedRef<FTokenizedMessage> Message = LoadErrors.Warning();
				Message->AddToken(FTextToken::Create(LOCTEXT("RootMotionFixUp1", "The Animation ")));
				Message->AddToken(FAssetNameToken::Create(AnimSeq->GetPathName(), FText::FromString(GetNameSafe(AnimSeq))));
				Message->AddToken(FTextToken::Create(LOCTEXT("RootMotionFixUp2", "will be set to ForceRootLock on. Please save the animation if you want to keep this change.")));
				Message->SetSeverity(EMessageSeverity::Warning);
				LoadErrors.Notify();
			}
#endif // WITH_EDITOR

			UE_LOG(LogMovieScene, Warning, TEXT("%s Animation has set ForceRootLock to be used in Sequencer. If this animation is used in anywhere else using root motion, that will cause conflict."), *AnimSeq->GetName());
		}
	}

	Super::PostLoad();
}

FMovieSceneEvalTemplatePtr UMovieSceneSkeletalAnimationSection::GenerateTemplate() const
{
	return FMovieSceneSkeletalAnimationSectionTemplate(*this);
}

void UMovieSceneSkeletalAnimationSection::MoveSection( float DeltaTime, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection(DeltaTime, KeyHandles);

	Params.Weight.ShiftCurve(DeltaTime, KeyHandles);
}


void UMovieSceneSkeletalAnimationSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	Params.PlayRate /= DilationFactor;

	Super::DilateSection(DilationFactor, Origin, KeyHandles);
	
	Params.Weight.ScaleCurve(Origin, DilationFactor, KeyHandles);
}

UMovieSceneSection* UMovieSceneSkeletalAnimationSection::SplitSection(float SplitTime)
{
	float AnimPlayRate = FMath::IsNearlyZero(Params.PlayRate) ? 1.0f : Params.PlayRate;
	float AnimPosition = (SplitTime - GetStartTime()) * AnimPlayRate;
	float SeqLength = Params.GetSequenceLength() - (Params.StartOffset + Params.EndOffset);

	float NewOffset = FMath::Fmod(AnimPosition, SeqLength);
	NewOffset += Params.StartOffset;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime);
	if (NewSection != nullptr)
	{
		UMovieSceneSkeletalAnimationSection* NewSkeletalSection = Cast<UMovieSceneSkeletalAnimationSection>(NewSection);
		NewSkeletalSection->Params.StartOffset = NewOffset;
	}
	return NewSection;
}


void UMovieSceneSkeletalAnimationSection::GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const
{
	if (!TimeRange.Overlaps(GetRange()))
	{
		return;
	}

	for (auto It(Params.Weight.GetKeyHandleIterator()); It; ++It)
	{
		float Time = Params.Weight.GetKeyTime(It.Key());
		if (TimeRange.Contains(Time))
		{
			OutKeyHandles.Add(It.Key());
		}
	}
}


void UMovieSceneSkeletalAnimationSection::GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const
{
	Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);

	float CurrentTime = GetStartTime();
	float AnimPlayRate = FMath::IsNearlyZero(Params.PlayRate) ? 1.0f : Params.PlayRate;
	float SeqLength = (Params.GetSequenceLength() - (Params.StartOffset + Params.EndOffset)) / AnimPlayRate;

	// Snap to the repeat times
	while (CurrentTime <= GetEndTime() && !FMath::IsNearlyZero(SeqLength, KINDA_SMALL_NUMBER) && SeqLength > 0)
	{
		if (CurrentTime >= GetStartTime())
		{
			OutSnapTimes.Add(CurrentTime);
		}

		CurrentTime += SeqLength;
	}
}

#if WITH_EDITOR
void UMovieSceneSkeletalAnimationSection::PreEditChange(UProperty* PropertyAboutToChange)
{
	// Store the current play rate so that we can compute the amount to compensate the section end time when the play rate changes
	PreviousPlayRate = Params.PlayRate;

	Super::PreEditChange(PropertyAboutToChange);
}

void UMovieSceneSkeletalAnimationSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Adjust the duration automatically if the play rate changes
	if (PropertyChangedEvent.Property != nullptr &&
		PropertyChangedEvent.Property->GetFName() == TEXT("PlayRate"))
	{
		float NewPlayRate = Params.PlayRate;

		if (!FMath::IsNearlyZero(NewPlayRate))
		{
			float CurrentDuration = GetEndTime() - GetStartTime();
			float NewDuration = CurrentDuration * (PreviousPlayRate / NewPlayRate);
			float NewEndTime = GetStartTime() + NewDuration;
			SetEndTime(NewEndTime);

			PreviousPlayRate = NewPlayRate;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#undef LOCTEXT_NAMESPACE 