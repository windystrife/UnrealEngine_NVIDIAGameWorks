// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTrack.h"
#include "Evaluation/MovieSceneSegment.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneCompilerRules.h"

#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Evaluation/MovieSceneLegacyTrackInstanceTemplate.h"

#include "MovieSceneEvaluationCustomVersion.h"

UMovieSceneTrack::UMovieSceneTrack(const FObjectInitializer& InInitializer)
	: Super(InInitializer)
{
#if WITH_EDITORONLY_DATA
	TrackTint = FColor(127, 127, 127, 0);
#endif
}

void UMovieSceneTrack::PostInitProperties()
{
	// Propagate sub object flags from our outer (movie scene) to ourselves. This is required for tracks that are stored on blueprints (archetypes) so that they can be referenced in worlds.
	if (GetOuter()->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject))
	{
		SetFlags(GetOuter()->GetMaskedFlags(RF_PropagateToSubObjects));
	}
	
	Super::PostInitProperties();
}

void UMovieSceneTrack::PostLoad()
{
	Super::PostLoad();

	if (GetLinkerCustomVersion(FMovieSceneEvaluationCustomVersion::GUID) < FMovieSceneEvaluationCustomVersion::ChangeEvaluateNearestSectionDefault)
	{
		EvalOptions.bEvalNearestSection = EvalOptions.bEvaluateNearestSection_DEPRECATED;
	}
}

void UMovieSceneTrack::UpdateEasing()
{
	int32 MaxRows = GetMaxRowIndex();
	TArray<UMovieSceneSection*> RowSections;

	for (int32 RowIndex = 0; RowIndex <= MaxRows; ++RowIndex)
	{
		RowSections.Reset();

		for (UMovieSceneSection* Section : GetAllSections())
		{
			if (Section && Section->GetRowIndex() == RowIndex)
			{
				RowSections.Add(Section);
			}
		}

		for (int32 Index = 0; Index < RowSections.Num(); ++Index)
		{
			UMovieSceneSection* CurrentSection = RowSections[Index];

			// Check overlaps with exclusive ranges so that sections can butt up against each other
			UMovieSceneTrack* OuterTrack = CurrentSection->GetTypedOuter<UMovieSceneTrack>();
			float MaxEaseIn = 0.f;
			float MaxEaseOut = 0.f;
			bool bIsEntirelyUnderlapped = false;

			TRange<float> CurrentSectionRange = CurrentSection->GetRange();
			for (int32 OtherIndex = 0; OtherIndex < RowSections.Num(); ++OtherIndex)
			{
				if (OtherIndex == Index)
				{
					continue;
				}

				UMovieSceneSection* Other = RowSections[OtherIndex];
				TRange<float> OtherSectionRange = Other->GetRange();

				bIsEntirelyUnderlapped = OtherSectionRange.Contains(CurrentSectionRange);

				// Check the lower bound of the current section against the other section's upper bound
				const bool bSectionRangeContainsOtherUpperBound = !OtherSectionRange.GetUpperBound().IsOpen() && !CurrentSectionRange.GetLowerBound().IsOpen() && CurrentSectionRange.Contains(OtherSectionRange.GetUpperBoundValue());
				const bool bSectionRangeContainsOtherLowerBound = !OtherSectionRange.GetLowerBound().IsOpen() && !CurrentSectionRange.GetUpperBound().IsOpen() && CurrentSectionRange.Contains(OtherSectionRange.GetLowerBoundValue());
				if (bSectionRangeContainsOtherUpperBound && !bSectionRangeContainsOtherLowerBound)
				{
					MaxEaseIn = FMath::Max(MaxEaseIn, OtherSectionRange.GetUpperBoundValue() - CurrentSectionRange.GetLowerBoundValue());
				}

				if (bSectionRangeContainsOtherLowerBound &&!bSectionRangeContainsOtherUpperBound)
				{
					MaxEaseOut = FMath::Max(MaxEaseOut, CurrentSectionRange.GetUpperBoundValue() - OtherSectionRange.GetLowerBoundValue());
				}
			}

			const bool bIsFinite = CurrentSectionRange.HasLowerBound() && CurrentSectionRange.HasUpperBound();
			const float Max = bIsFinite ? CurrentSectionRange.Size<float>() : TNumericLimits<float>::Max();

			if (MaxEaseOut == 0.f && MaxEaseIn == 0.f && bIsEntirelyUnderlapped)
			{
				MaxEaseOut = MaxEaseIn = Max * 0.25f;
			}

			CurrentSection->Modify();
			CurrentSection->Easing.AutoEaseInTime = FMath::Clamp(MaxEaseIn, 0.f, Max);
			CurrentSection->Easing.AutoEaseOutTime = FMath::Clamp(MaxEaseOut, 0.f, Max);
		}
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedPtr<IMovieSceneTrackInstance> UMovieSceneTrack::CreateLegacyInstance() const
{
	// Ugly const cast required due to lack of const-correctness in old system
	return const_cast<UMovieSceneTrack*>(this)->CreateInstance();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

TInlineValue<FMovieSceneSegmentCompilerRules> UMovieSceneTrack::GetRowCompilerRules() const
{
	// By default we only evaluate the section with the highest Z-Order if they overlap on the same row
	struct FDefaultCompilerRules : FMovieSceneSegmentCompilerRules
	{
		virtual void BlendSegment(FMovieSceneSegment& Segment, const TArrayView<const FMovieSceneSectionData>& SourceData) const
		{
			MovieSceneSegmentCompiler::BlendSegmentHighPass(Segment, SourceData);
		}
	};
	return FDefaultCompilerRules();
}

TInlineValue<FMovieSceneSegmentCompilerRules> UMovieSceneTrack::GetTrackCompilerRules() const
{
	struct FRoundToNearstSectionRules : FMovieSceneSegmentCompilerRules
	{
		virtual TOptional<FMovieSceneSegment> InsertEmptySpace(const TRange<float>& Range, const FMovieSceneSegment* PreviousSegment, const FMovieSceneSegment* NextSegment) const
		{
			return MovieSceneSegmentCompiler::EvaluateNearestSegment(Range, PreviousSegment, NextSegment);
		}
	};

	// Evaluate according to bEvalNearestSection preference
	if (EvalOptions.bCanEvaluateNearestSection && EvalOptions.bEvalNearestSection)
	{
		return FRoundToNearstSectionRules();
	}
	else
	{
		return FMovieSceneSegmentCompilerRules();
	}
}

void UMovieSceneTrack::GenerateTemplate(const FMovieSceneTrackCompilerArgs& Args) const
{
	FMovieSceneEvaluationTrack NewTrackTemplate(Args.ObjectBindingId);

	// Legacy path
	if (CreateLegacyInstance().IsValid())
	{
		NewTrackTemplate.DefineAsSingleTemplate(FMovieSceneLegacyTrackInstanceTemplate(this));
		Args.Generator.AddLegacyTrack(MoveTemp(NewTrackTemplate), *this);
		return;
	}

	if (Compile(NewTrackTemplate, Args) != EMovieSceneCompileResult::Success)
	{
		return;
	}

	Args.Generator.AddOwnedTrack(MoveTemp(NewTrackTemplate), *this);
}

FMovieSceneEvaluationTrack UMovieSceneTrack::GenerateTrackTemplate() const
{
	FMovieSceneEvaluationTrack TrackTemplate = FMovieSceneEvaluationTrack(FGuid());

	// Legacy path
	if (CreateLegacyInstance().IsValid())
	{
		TrackTemplate.DefineAsSingleTemplate(FMovieSceneLegacyTrackInstanceTemplate(this));
	}
	else
	{
		// For this path, we don't have a generator, so we just pass through a stub
		struct FTemplateGeneratorStub : IMovieSceneTemplateGenerator
		{
			virtual void AddOwnedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack) override {}
			virtual void AddSharedTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, FMovieSceneSharedDataId SharedId, const UMovieSceneTrack& SourceTrack) override {}
			virtual void AddLegacyTrack(FMovieSceneEvaluationTrack&& InTrackTemplate, const UMovieSceneTrack& SourceTrack) override {}
			virtual void AddExternalSegments(TRange<float> RootRange, TArrayView<const FMovieSceneEvaluationFieldSegmentPtr> SegmentPtrs, ESectionEvaluationFlags Flags) override {}
			virtual FMovieSceneSequenceTransform GetSequenceTransform(FMovieSceneSequenceIDRef InSequenceID) const override { return FMovieSceneSequenceTransform(); }
			virtual void AddSubSequence(FMovieSceneSubSequenceData SequenceData, FMovieSceneSequenceIDRef ParentID, FMovieSceneSequenceID SpecificID) override { }
		} Generator;

		FMovieSceneSequenceTemplateStore Store;
		Compile(TrackTemplate, FMovieSceneTrackCompilerArgs(Generator, Store));
	}
	return TrackTemplate;
}

struct FSegmentRemapper
{
	explicit FSegmentRemapper(bool bInAllowEmptySegments)
		: bAllowEmptySegments(bInAllowEmptySegments)
	{}

	void ProcessSegments(const TArray<FMovieSceneSegment>& SourceSegments, FMovieSceneEvaluationTrack& OutTrack, const TFunctionRef<FMovieSceneEvalTemplatePtr(int32)>& TemplateFactory)
	{
		NewSegments.Reset(SourceSegments.Num());
		NewIndices.Reset();

		for (const FMovieSceneSegment& Segment : SourceSegments)
		{
			AddSegment(Segment, OutTrack, TemplateFactory);
		}

		OutTrack.SetSegments(MoveTemp(NewSegments));
	}

	void AddSegment(const FMovieSceneSegment& SourceSegment, FMovieSceneEvaluationTrack& OutTrack, const TFunctionRef<FMovieSceneEvalTemplatePtr(int32)>& TemplateFactory)
	{
		FMovieSceneSegment NewSegment;
		NewSegment.Range = SourceSegment.Range;

		for (const FSectionEvaluationData& EvalData : SourceSegment.Impls)
		{
			EnsureIndexIsValid(EvalData.ImplIndex);

			// Ensure all our templates have been added to the track
			if (NewIndices[EvalData.ImplIndex] == NotCreatedYet)
			{
				FMovieSceneEvalTemplatePtr NewTemplate = TemplateFactory(EvalData.ImplIndex);
				if (NewTemplate.IsValid())
				{
					NewIndices[EvalData.ImplIndex] = OutTrack.AddChildTemplate(MoveTemp(NewTemplate));
				}
				else
				{
					NewIndices[EvalData.ImplIndex] = NullTemplate;
				}
			}

			if (NewIndices[EvalData.ImplIndex] == NullTemplate)
			{
				continue;
			}

			FSectionEvaluationData NewData = EvalData;
			NewData.ImplIndex = NewIndices[EvalData.ImplIndex];
			NewSegment.Impls.Add(NewData);
		}

		if (bAllowEmptySegments || NewSegment.Impls.Num())
		{
			NewSegments.Add(NewSegment);
		}
	}

private:
	bool bAllowEmptySegments;

	static const int32 NotCreatedYet;
	static const int32 NullTemplate;

	TArray<FMovieSceneSegment> NewSegments;
	TArray<int32> NewIndices;

	void EnsureIndexIsValid(int32 InSourceIndex)
	{
		NewIndices.Reserve(InSourceIndex + 1);
		for (int32 Index = NewIndices.Num(); Index < InSourceIndex + 1; ++Index)
		{
			NewIndices.Add(NotCreatedYet);
		}
	}
};

const int32 FSegmentRemapper::NotCreatedYet = -1;
const int32 FSegmentRemapper::NullTemplate = -2;

EMovieSceneCompileResult UMovieSceneTrack::Compile(FMovieSceneEvaluationTrack& OutTrack, const FMovieSceneTrackCompilerArgs& Args) const
{
	OutTrack.SetPreAndPostrollConditions(EvalOptions.bEvaluateInPreroll, EvalOptions.bEvaluateInPostroll);

	EMovieSceneCompileResult Result = CustomCompile(OutTrack, Args);

	if (Result == EMovieSceneCompileResult::Unimplemented)
	{
		// Default implementation
		const TArray<UMovieSceneSection*>& AllSections = GetAllSections();
		
		TInlineValue<FMovieSceneSegmentCompilerRules> RowCompilerRules = GetRowCompilerRules();
		FMovieSceneTrackCompiler::FRows TrackRows(AllSections, RowCompilerRules.GetPtr(nullptr));

		FMovieSceneTrackCompiler Compiler;
		TInlineValue<FMovieSceneSegmentCompilerRules> Rules = GetTrackCompilerRules();
		FMovieSceneTrackEvaluationField EvaluationField = Compiler.Compile(TrackRows.Rows, Rules.GetPtr(nullptr));

		const bool bAllowEmptySegments = Rules.IsValid() && Rules->AllowEmptySegments(); 

		FSegmentRemapper Remapper(bAllowEmptySegments);
		Remapper.ProcessSegments(EvaluationField.Segments, OutTrack,
			[&](int32 SectionIndex){
				FMovieSceneEvalTemplatePtr NewTemplate = CreateTemplateForSection(*AllSections[SectionIndex]);
				if (NewTemplate.IsValid())
				{
					NewTemplate->SetCompletionMode(AllSections[SectionIndex]->EvalOptions.CompletionMode);
					NewTemplate->SetSourceSection(AllSections[SectionIndex]);
				}
				return NewTemplate;
			}
		);

		Result = EMovieSceneCompileResult::Success;
	}

	if (Result == EMovieSceneCompileResult::Success)
	{
		PostCompile(OutTrack, Args);
	}

	return Result;
}

FMovieSceneEvalTemplatePtr UMovieSceneTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return InSection.GenerateTemplate();
}

int32 UMovieSceneTrack::GetMaxRowIndex() const
{
	int32 MaxRowIndex = 0;
	for (UMovieSceneSection* Section : GetAllSections())
	{
		MaxRowIndex = FMath::Max(MaxRowIndex, Section->GetRowIndex());
	}

	return MaxRowIndex;
}

bool UMovieSceneTrack::FixRowIndices()
{
	bool bFixesMade = false;
	TArray<UMovieSceneSection*> Sections = GetAllSections();
	if (SupportsMultipleRows())
	{
		// remove any empty track rows by waterfalling down sections to be as compact as possible
		TArray<TArray<UMovieSceneSection*>> RowIndexToSectionsMap;
		RowIndexToSectionsMap.AddZeroed(GetMaxRowIndex() + 1);

		for (UMovieSceneSection* Section : Sections)
		{
			RowIndexToSectionsMap[Section->GetRowIndex()].Add(Section);
		}

		int32 NewIndex = 0;
		for (const TArray<UMovieSceneSection*>& SectionsForIndex : RowIndexToSectionsMap)
		{
			if (SectionsForIndex.Num() > 0)
			{
				for (UMovieSceneSection* SectionForIndex : SectionsForIndex)
				{
					if (SectionForIndex->GetRowIndex() != NewIndex)
					{
						SectionForIndex->Modify();
						SectionForIndex->SetRowIndex(NewIndex);
						bFixesMade = true;
					}
				}
				++NewIndex;
			}
		}
	}
	else
	{
		for (int32 i = 0; i < Sections.Num(); ++i)
		{
			if (Sections[i]->GetRowIndex() != 0)
			{
				Sections[i]->Modify();
				Sections[i]->SetRowIndex(0);
				bFixesMade = true;
			}
		}
	}
	return bFixesMade;
}
