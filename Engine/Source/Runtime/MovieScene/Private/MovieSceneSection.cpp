// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Generators/MovieSceneEasingCurves.h"


UMovieSceneSection::UMovieSceneSection(const FObjectInitializer& ObjectInitializer)
	: Super( ObjectInitializer )
	, StartTime(0.0f)
	, EndTime(0.0f)
	, RowIndex(0)
	, OverlapPriority(0)
	, bIsActive(true)
	, bIsLocked(false)
	, bIsInfinite(false)
	, PreRollTime(0)
	, PostRollTime(0)
{
	UMovieSceneBuiltInEasingFunction* DefaultEaseIn = ObjectInitializer.CreateDefaultSubobject<UMovieSceneBuiltInEasingFunction>(this, "EaseInFunction");
	DefaultEaseIn->SetFlags(RF_Public); //@todo Need to be marked public. GLEO occurs when transform sections are added to actor sequence blueprints. Are these not being duplicated properly?
	DefaultEaseIn->Type = EMovieSceneBuiltInEasing::CubicInOut;
	Easing.EaseIn = DefaultEaseIn;

	UMovieSceneBuiltInEasingFunction* DefaultEaseOut = ObjectInitializer.CreateDefaultSubobject<UMovieSceneBuiltInEasingFunction>(this, "EaseOutFunction");
	DefaultEaseOut->SetFlags(RF_Public); //@todo Need to be marked public. GLEO occurs when transform sections are added to actor sequence blueprints. Are these not being duplicated properly?
	DefaultEaseOut->Type = EMovieSceneBuiltInEasing::CubicInOut;
	Easing.EaseOut = DefaultEaseOut;
}


void UMovieSceneSection::PostInitProperties()
{
	// Propagate sub object flags from our outer (track) to ourselves. This is required for sections that are stored on blueprints (archetypes) so that they can be referenced in worlds.
	if (GetOuter()->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject))
	{
		SetFlags(GetOuter()->GetMaskedFlags(RF_PropagateToSubObjects));
	}
	
	Super::PostInitProperties();
}


FMovieSceneBlendTypeField UMovieSceneSection::GetSupportedBlendTypes() const
{
	UMovieSceneTrack* Track = GetTypedOuter<UMovieSceneTrack>();
	return Track ? Track->GetSupportedBlendTypes() : FMovieSceneBlendTypeField::None();
}


bool UMovieSceneSection::TryModify(bool bAlwaysMarkDirty)
{
	if (IsLocked())
	{
		return false;
	}

	Modify(bAlwaysMarkDirty);

	return true;
}


void UMovieSceneSection::GetOverlappingSections(TArray<UMovieSceneSection*>& OutSections, bool bSameRow, bool bIncludeThis)
{
	UMovieSceneTrack* Track = GetTypedOuter<UMovieSceneTrack>();
	if (!Track)
	{
		return;
	}

	TRange<float> ThisRange = GetRange();
	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		if (!Section || (!bIncludeThis && Section == this))
		{
			continue;
		}

		if (bSameRow && Section->GetRowIndex() != GetRowIndex())
		{
			continue;
		}

		if (Section->GetRange().Overlaps(ThisRange))
		{
			OutSections.Add(Section);
		}
	}
}


const UMovieSceneSection* UMovieSceneSection::OverlapsWithSections(const TArray<UMovieSceneSection*>& Sections, int32 TrackDelta, float TimeDelta) const
{
	// Check overlaps with exclusive ranges so that sections can butt up against each other
	int32 NewTrackIndex = RowIndex + TrackDelta;
	TRange<float> NewSectionRange = TRange<float>(TRange<float>::BoundsType::Exclusive(StartTime + TimeDelta), TRange<float>::BoundsType::Exclusive(EndTime + TimeDelta));

	for (const auto Section : Sections)
	{
		check(Section);
		if ((this != Section) && (Section->GetRowIndex() == NewTrackIndex))
		{
			TRange<float> ExclusiveSectionRange = TRange<float>(TRange<float>::BoundsType::Exclusive(Section->GetRange().GetLowerBoundValue()), TRange<float>::BoundsType::Exclusive(Section->GetRange().GetUpperBoundValue()));
			if (NewSectionRange.Overlaps(ExclusiveSectionRange))
			{
				return Section;
			}
		}
	}

	return nullptr;
}


void UMovieSceneSection::InitialPlacement(const TArray<UMovieSceneSection*>& Sections, float InStartTime, float InEndTime, bool bAllowMultipleRows)
{
	check(StartTime <= EndTime);

	StartTime = InStartTime;
	EndTime = InEndTime;
	RowIndex = 0;

	for (UMovieSceneSection* OtherSection : Sections)
	{
		OverlapPriority = FMath::Max(OtherSection->GetOverlapPriority()+1, OverlapPriority);
	}

	if (bAllowMultipleRows)
	{
		while (OverlapsWithSections(Sections) != nullptr)
		{
			++RowIndex;
		}
	}
	else
	{
		for (;;)
		{
			const UMovieSceneSection* OverlappedSection = OverlapsWithSections(Sections);

			if (OverlappedSection == nullptr)
			{
				break;
			}

			TSet<FKeyHandle> KeyHandles;
			MoveSection(OverlappedSection->GetEndTime() - StartTime, KeyHandles);
		}
	}

	UMovieSceneTrack* Track = GetTypedOuter<UMovieSceneTrack>();
	if (Track)
	{
		Track->UpdateEasing();
	}
}


UMovieSceneSection* UMovieSceneSection::SplitSection(float SplitTime)
{
	if (!IsTimeWithinSection(SplitTime))
	{
		return nullptr;
	}

	SetFlags(RF_Transactional);

	if (TryModify())
	{
		float SectionEndTime = GetEndTime();
				
		// Trim off the right
		SetEndTime(SplitTime);

		// Create a new section
		UMovieSceneTrack* Track = CastChecked<UMovieSceneTrack>(GetOuter());
		Track->Modify();

		UMovieSceneSection* NewSection = DuplicateObject<UMovieSceneSection>(this, Track);
		check(NewSection);

		NewSection->SetStartTime(SplitTime);
		NewSection->SetEndTime(SectionEndTime);
		Track->AddSection(*NewSection);

		return NewSection;
	}

	return nullptr;
}


void UMovieSceneSection::TrimSection(float TrimTime, bool bTrimLeft)
{
	if (IsTimeWithinSection(TrimTime))
	{
		SetFlags(RF_Transactional);
		if (TryModify())
		{
			if (bTrimLeft)
			{
				SetStartTime(TrimTime);
			}
			else
			{
				SetEndTime(TrimTime);
			}
		}
	}
}


void UMovieSceneSection::AddKeyToCurve(FRichCurve& InCurve, float Time, float Value, EMovieSceneKeyInterpolation Interpolation, const bool bUnwindRotation)
{
	if (IsTimeWithinSection(Time))
	{
		if (TryModify())
		{
			FKeyHandle ExistingKeyHandle = InCurve.FindKey(Time);
			FKeyHandle NewKeyHandle = InCurve.UpdateOrAddKey(Time, Value, bUnwindRotation);

			if (!InCurve.IsKeyHandleValid(ExistingKeyHandle) && InCurve.IsKeyHandleValid(NewKeyHandle))
			{
				MovieSceneHelpers::SetKeyInterpolation(InCurve, NewKeyHandle, Interpolation);
			}
		}
	}
}


void UMovieSceneSection::SetCurveDefault(FRichCurve& InCurve, float Value)
{
	if (InCurve.GetDefaultValue() != Value && TryModify())
	{
		InCurve.SetDefaultValue(Value);
	}
}


FMovieSceneEvalTemplatePtr UMovieSceneSection::GenerateTemplate() const
{
	return FMovieSceneEvalTemplatePtr();
}


float UMovieSceneSection::EvaluateEasing(float InTime) const
{
	TRange<float> CurrentRange = GetRange();

	float EaseInValue = 1.f;
	float EaseOutValue = 1.f;

	if (!CurrentRange.GetLowerBound().IsOpen() && Easing.GetEaseInTime() > 0.f && Easing.EaseIn.GetObject())
	{
		float EaseInTime = (InTime - CurrentRange.GetLowerBoundValue()) / Easing.GetEaseInTime();
		if (EaseInTime <= 0.f)
		{
			EaseInValue = 0.f;
		}
		else if (EaseInTime >= 1.f)
		{
			EaseInValue = 1.f;
		}
		else
		{
			EaseInValue = IMovieSceneEasingFunction::EvaluateWith(Easing.EaseIn, EaseInTime);
		}
	}


	if (!CurrentRange.GetUpperBound().IsOpen() && Easing.GetEaseOutTime() > 0.f && Easing.EaseOut.GetObject())
	{
		float EaseOutTime = (InTime - (CurrentRange.GetUpperBoundValue() - Easing.GetEaseOutTime())) / Easing.GetEaseOutTime();
		if (EaseOutTime <= 0.f)
		{
			EaseOutValue = 1.f;
		}
		else if (EaseOutTime >= 1.f)
		{
			EaseOutValue = 0.f;
		}
		else
		{
			EaseOutValue = 1.f - IMovieSceneEasingFunction::EvaluateWith(Easing.EaseOut, EaseOutTime);
		}
	}

	return EaseInValue * EaseOutValue;
}


void UMovieSceneSection::EvaluateEasing(float InTime, TOptional<float>& OutEaseInValue, TOptional<float>& OutEaseOutValue, float* OutEaseInInterp, float* OutEaseOutInterp) const
{
	TRange<float> CurrentRange = GetRange();

	if (!CurrentRange.GetLowerBound().IsOpen() && Easing.EaseIn.GetObject() && GetEaseInRange().Contains(InTime))
	{
		float EaseInTime = (InTime - CurrentRange.GetLowerBoundValue()) / Easing.GetEaseInTime();
		OutEaseInValue = IMovieSceneEasingFunction::EvaluateWith(Easing.EaseIn, EaseInTime);

		if (OutEaseInInterp)
		{
			*OutEaseInInterp = EaseInTime;
		}
	}

	if (!CurrentRange.GetUpperBound().IsOpen() && Easing.EaseOut.GetObject() && GetEaseOutRange().Contains(InTime))
	{
		float EaseOutTime = (InTime - (CurrentRange.GetUpperBoundValue() - Easing.GetEaseOutTime())) / Easing.GetEaseOutTime();
		OutEaseOutValue = 1.f - IMovieSceneEasingFunction::EvaluateWith(Easing.EaseOut, EaseOutTime);

		if (OutEaseOutInterp)
		{
			*OutEaseOutInterp = EaseOutTime;
		}
	}
}


TRange<float> UMovieSceneSection::GetEaseInRange() const
{
	const float MaxTime = FMath::Min(GetStartTime() + Easing.GetEaseInTime(), GetEndTime());
	if (!bIsInfinite && Easing.GetEaseInTime() > 0.f)
	{
		return TRange<float>(GetStartTime(), TRangeBound<float>::Inclusive(MaxTime));
	}
	return TRange<float>::Empty();
}


TRange<float> UMovieSceneSection::GetEaseOutRange() const
{
	const float MinTime = FMath::Max(GetEndTime() - Easing.GetEaseOutTime(), GetStartTime());
	if (!bIsInfinite && Easing.GetEaseOutTime() > 0.f)
	{
		return TRange<float>(MinTime, TRangeBound<float>::Inclusive(GetEndTime()));
	}
	return TRange<float>::Empty();
}