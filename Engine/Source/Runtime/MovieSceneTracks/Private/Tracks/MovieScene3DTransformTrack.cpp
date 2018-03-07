// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieScene3DTransformTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Compilation/MovieSceneTemplateInterrogation.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "MovieSceneEvaluationTrack.h"
#include "Algo/BinarySearch.h"

UMovieScene3DTransformTrack::UMovieScene3DTransformTrack( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	static FName Transform("Transform");
	SetPropertyNameAndPath(Transform, Transform.ToString());

	SupportedBlendTypes = FMovieSceneBlendTypeField::All();

#if WITH_EDITORONLY_DATA
	TrackTint = FColor(65, 173, 164, 65);
#endif

	EvalOptions.bEvaluateNearestSection_DEPRECATED = EvalOptions.bCanEvaluateNearestSection = true;
}

TArray<FTrajectoryKey> UMovieScene3DTransformTrack::GetTrajectoryData(float Time, int32 MaxNumDataPoints) const
{
	struct FCurveKeyIterator
	{
		FCurveKeyIterator(UMovieScene3DTransformSection* InSection, EMovieSceneTransformChannel InType, const FRichCurve& InCurve, float StartTime)
			: Section(InSection), Type(InType), SortedKeys(InCurve.GetConstRefOfKeys()), SectionRange(InSection->IsInfinite() ? TRange<float>::All() : InSection->GetRange())
		{
			CurrentIndex = Algo::LowerBoundBy(SortedKeys, StartTime, &FRichCurveKey::Time, TLess<>());

			bIsLowerBound = false;
			bIsUpperBound = !SortedKeys.IsValidIndex(CurrentIndex) && SectionRange.GetUpperBound().IsClosed();
		}

		FCurveKeyIterator& operator--()
		{
			if (bIsLowerBound)
			{
				bIsLowerBound = false;
				CurrentIndex = -1;
			}
			else
			{
				if (bIsUpperBound)
				{
					bIsUpperBound = false;
					CurrentIndex = Algo::LowerBoundBy(SortedKeys, SectionRange.GetUpperBoundValue(), &FRichCurveKey::Time, TLess<>()) - 1;
				}
				else
				{
					--CurrentIndex;
				}

				bIsLowerBound = SectionRange.GetLowerBound().IsClosed() && (!SortedKeys.IsValidIndex(CurrentIndex) || !SectionRange.Contains(SortedKeys[CurrentIndex].Time) );
			}
			return *this;
		}

		FCurveKeyIterator& operator++()
		{
			if (bIsUpperBound)
			{
				bIsUpperBound = false;
				CurrentIndex = -1;
			}
			else
			{
				if (bIsLowerBound)
				{
					bIsLowerBound = false;
					CurrentIndex = Algo::UpperBoundBy(SortedKeys, SectionRange.GetLowerBoundValue(), &FRichCurveKey::Time, TLess<>());
				}
				else 
				{
					++CurrentIndex;
				}

				bIsUpperBound = SectionRange.GetUpperBound().IsClosed() && (!SortedKeys.IsValidIndex(CurrentIndex) || !SectionRange.Contains(SortedKeys[CurrentIndex].Time) );
			}
			return *this;
		}

		explicit operator bool() const
		{
			return ( bIsLowerBound || bIsUpperBound ) || ( SortedKeys.IsValidIndex(CurrentIndex) && SectionRange.Contains(SortedKeys[CurrentIndex].Time) );
		}

		float GetTime() const
		{
			return bIsLowerBound ? SectionRange.GetLowerBoundValue() : bIsUpperBound ? SectionRange.GetUpperBoundValue() : SortedKeys[CurrentIndex].Time;
		}

		ERichCurveInterpMode GetInterpMode() const
		{
			return ( bIsLowerBound || bIsUpperBound ) ? RCIM_None : SortedKeys[CurrentIndex].InterpMode.GetValue();
		}

		EMovieSceneTransformChannel GetType() const
		{
			return Type;
		}

		UMovieScene3DTransformSection* GetSection() const
		{
			return Section;
		}

	private:
		UMovieScene3DTransformSection* Section;
		EMovieSceneTransformChannel Type;
		const TArray<FRichCurveKey>& SortedKeys;
		TRange<float> SectionRange;
		int32 CurrentIndex;
		bool bIsUpperBound, bIsLowerBound;
	};

	TArray<FCurveKeyIterator> ForwardIters;
	TArray<FCurveKeyIterator> BackwardIters;

	for (UMovieSceneSection* Section : Sections)
	{
		UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Section);
		if (TransformSection)
		{
			EMovieSceneTransformChannel Mask = TransformSection->GetMask().GetChannels();
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::TranslationX))
			{
				ForwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::TranslationX, TransformSection->GetTranslationCurve(EAxis::X), Time);
				BackwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::TranslationX, TransformSection->GetTranslationCurve(EAxis::X), Time);
			}
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::TranslationY))
			{
				ForwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::TranslationY, TransformSection->GetTranslationCurve(EAxis::Y), Time);
				BackwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::TranslationY, TransformSection->GetTranslationCurve(EAxis::Y), Time);
			}
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::TranslationZ))
			{
				ForwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::TranslationZ, TransformSection->GetTranslationCurve(EAxis::Z), Time);
				BackwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::TranslationZ, TransformSection->GetTranslationCurve(EAxis::Z), Time);
			}
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::RotationX))
			{
				ForwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::RotationX, TransformSection->GetRotationCurve(EAxis::X), Time);
				BackwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::RotationX, TransformSection->GetRotationCurve(EAxis::X), Time);
			}
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::RotationY))
			{
				ForwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::RotationY, TransformSection->GetRotationCurve(EAxis::Y), Time);
				BackwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::RotationY, TransformSection->GetRotationCurve(EAxis::Y), Time);
			}
			if (EnumHasAnyFlags(Mask, EMovieSceneTransformChannel::RotationZ))
			{
				ForwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::RotationZ, TransformSection->GetRotationCurve(EAxis::Z), Time);
				BackwardIters.Emplace(TransformSection, EMovieSceneTransformChannel::RotationZ, TransformSection->GetRotationCurve(EAxis::Z), Time);
			}
		}
	}

	auto HasAnyValidIterators = [](const FCurveKeyIterator& It)
	{
		return It;
	};

	for (FCurveKeyIterator& Bck : BackwardIters)
	{
		--Bck;
	}

	TArray<FTrajectoryKey> Result;
	while (ForwardIters.ContainsByPredicate(HasAnyValidIterators) || BackwardIters.ContainsByPredicate(HasAnyValidIterators))
	{
		if (MaxNumDataPoints != 0 && Result.Num() >= MaxNumDataPoints)
		{
			break;
		}

		FTrajectoryKey& NewKey = Result[Result.Emplace(TNumericLimits<float>::Max())];

		// Find the closest key time
		for (const FCurveKeyIterator& Fwd : ForwardIters)
		{
			if (Fwd && ( FMath::Abs(Time - Fwd.GetTime()) < FMath::Abs(Time - NewKey.Time) ) )
			{
				NewKey.Time = Fwd.GetTime();
			}
		}
		for (const FCurveKeyIterator& Bck : BackwardIters)
		{
			if (Bck && ( FMath::Abs(Time - Bck.GetTime()) < FMath::Abs(Time - NewKey.Time) ) )
			{
				NewKey.Time = Bck.GetTime();
			}
		}

		for (FCurveKeyIterator& Fwd : ForwardIters)
		{
			if (Fwd && FMath::IsNearlyEqual(Fwd.GetTime(), NewKey.Time))
			{
				// Add this key to the trajectory key
				NewKey.KeyData.Emplace(Fwd.GetSection(), Fwd.GetInterpMode(), Fwd.GetType());
				// Move onto the next key in this curve
				++Fwd;
			}
		}

		for (FCurveKeyIterator& Bck : BackwardIters)
		{
			if (Bck && FMath::IsNearlyEqual(Bck.GetTime(), NewKey.Time))
			{
				// Add this key to the trajectory key
				NewKey.KeyData.Emplace(Bck.GetSection(), Bck.GetInterpMode(), Bck.GetType());
				// Move onto the next key in this curve
				--Bck;
			}
		}
	}

	Result.Sort(
		[](const FTrajectoryKey& A, const FTrajectoryKey& B)
		{
			return A.Time < B.Time;
		}
	);

	return Result;
}

UMovieSceneSection* UMovieScene3DTransformTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieScene3DTransformSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneInterrogationKey UMovieScene3DTransformTrack::GetInterrogationKey()
{
	static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}