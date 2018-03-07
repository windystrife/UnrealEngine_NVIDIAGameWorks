// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSegmentCompilerTests.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "MovieSceneTestsCommon.h"
#include "Package.h"

namespace Impl
{
	static const TRangeBound<float> Inf = TRangeBound<float>::Open();
	static const FOptionalMovieSceneBlendType NoBlending;

	/** Compiler rules to sort by priority */
	struct FSortByPriorityCompilerRules : FMovieSceneSegmentCompilerRules
	{
		int32 MaxImplIndex;

		FSortByPriorityCompilerRules(int32 InMaxImplIndex, bool bInAllowEmptySegments)
			: MaxImplIndex(InMaxImplIndex)
		{
			bAllowEmptySegments = bInAllowEmptySegments;
		}

		virtual TOptional<FMovieSceneSegment> InsertEmptySpace(const TRange<float>& Range, const FMovieSceneSegment* PreviousSegment, const FMovieSceneSegment* NextSegment) const override
		{
			return bAllowEmptySegments ? FMovieSceneSegment(Range) : TOptional<FMovieSceneSegment>();
		}

		virtual void BlendSegment(FMovieSceneSegment& Segment, const TArrayView<const FMovieSceneSectionData>& SourceData) const
		{
			Segment.Impls.Sort(
				[&](const FSectionEvaluationData& A, const FSectionEvaluationData& B)
				{
					return SourceData[A.ImplIndex].Priority > SourceData[B.ImplIndex].Priority;
				}
			);
		}

		virtual void PostProcessSegments(TArray<FMovieSceneSegment>& Segments, const TArrayView<const FMovieSceneSectionData>& SourceData) const override
		{
			for (FMovieSceneSegment& Segment : Segments)
			{
				for (FSectionEvaluationData EvalData : Segment.Impls)
				{
					ensureMsgf(SourceData.IsValidIndex(EvalData.ImplIndex),
						TEXT("Compiled segment data does not correctly map to the source data array"));

					const int32 ThisImpl = SourceData[EvalData.ImplIndex].EvalData.ImplIndex;
					ensureMsgf(ThisImpl >= 0 && ThisImpl <= MaxImplIndex,
						TEXT("Compiled segment data does not correctly map to either the designated implementation range"));
				}
			}
		}

	};
	
	FORCEINLINE void AssertSegmentValues(FAutomationTestBase* Test, TArrayView<const FMovieSceneSegment> Expected, TArrayView<const FMovieSceneSegment> Actual)
	{
		auto Join = [](TArrayView<const FSectionEvaluationData> Stuff) -> FString
		{
			FString Result;
			for (const FSectionEvaluationData& Thing : Stuff)
			{
				if (Result.Len())
				{
					Result += TEXT(", ");
				}

				Result += FString::Printf(TEXT("(Impl: %d, ForcedTime: %.7f, Flags: %u)"), Thing.ImplIndex, Thing.ForcedTime, (uint8)Thing.Flags);
			}

			return Result;
		};

		using namespace Lex;
		if (Actual.Num() != Expected.Num())
		{
			Test->AddError(FString::Printf(TEXT("Wrong number of compiled segments. Expected %d, actual %d."), Expected.Num(), Actual.Num()));
		}
		else for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			const FMovieSceneSegment& ExpectedSegment = Expected[Index];
			const FMovieSceneSegment& ActualSegment = Actual[Index];

			if (ExpectedSegment.Range != ActualSegment.Range)
			{
				Test->AddError(FString::Printf(TEXT("Incorrect compiled segment range at segment index %d. Expected:\n%s\nActual:\n%s"), Index, *Lex::ToString(ExpectedSegment.Range), *Lex::ToString(ActualSegment.Range)));
			}
			else if (ExpectedSegment.Impls.Num() != ActualSegment.Impls.Num())
			{
				Test->AddError(FString::Printf(TEXT("Incorrect number of implementation references compiled into segment index %d. Expected %d, actual %d."), Index, ExpectedSegment.Impls.Num(), ActualSegment.Impls.Num()));
			}
			else 
			{
				FString ActualImpls = Join(ActualSegment.Impls);
				FString ExpectedImpls = Join(ExpectedSegment.Impls);

				if (ActualImpls != ExpectedImpls)
				{
					Test->AddError(FString::Printf(TEXT("Compiled data does not match for segment index %d.\nExpected: %s\nActual:   %s."), Index, *ExpectedImpls, *ActualImpls));
				}
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieSceneCompilerBasicTest, "System.Engine.Sequencer.Compiler.Basic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieSceneCompilerBasicTest::RunTest(const FString& Parameters)
{
	using namespace Impl;

	// Specify descending priorities on the segments so we always sort the compiled segments in the order they're defined within the data
	//	This is our test layout
	//	Time				-inf				10							20				25					30							inf
	//											[============= 0 ===========]
	//																		[============== 1 ==================]
	//											[========================== 2 ==================================]
	//						[================== 3 ==========================]
	//																						[================== 4 ==========================]
	//----------------------------------------------------------------------------------------------------------------------------------------
	//	Expected Impls		[ 		3			|			0,2,3			|		1,2		|		1,2,4		|			4				]

	const FMovieSceneSectionData SegmentData[] = {
		FMovieSceneSectionData(TRange<float>(10.f,	20.f), 									FSectionEvaluationData(0), 	NoBlending,		4),
		FMovieSceneSectionData(TRange<float>(20.f,	30.f), 									FSectionEvaluationData(1), 	NoBlending,		3),
		FMovieSceneSectionData(TRange<float>(10.f,	30.f), 									FSectionEvaluationData(2), 	NoBlending,		2),
		FMovieSceneSectionData(TRange<float>(Inf,	TRangeBound<float>::Exclusive(20.f)), 	FSectionEvaluationData(3), 	NoBlending,		1),
		FMovieSceneSectionData(TRange<float>(25.f,	Inf), 									FSectionEvaluationData(4), 	NoBlending,		0)
	};

	FSortByPriorityCompilerRules DefaultRules(4, false);
	TArray<FMovieSceneSegment> Segments = FMovieSceneSegmentCompiler().Compile(SegmentData, &DefaultRules);

	FMovieSceneSegment Expected[] = {
		FMovieSceneSegment(FFloatRange(Inf, 	TRangeBound<float>::Exclusive(10.f)), { FSectionEvaluationData(3) }),
		FMovieSceneSegment(FFloatRange(10.f,	20.f), { FSectionEvaluationData(0), FSectionEvaluationData(2), FSectionEvaluationData(3) }),
		FMovieSceneSegment(FFloatRange(20.f,	25.f), { FSectionEvaluationData(1), FSectionEvaluationData(2) }),
		FMovieSceneSegment(FFloatRange(25.f,	30.f), { FSectionEvaluationData(1), FSectionEvaluationData(2), FSectionEvaluationData(4) }),
		FMovieSceneSegment(FFloatRange(30.f,	Inf), { FSectionEvaluationData(4) })
	};
	AssertSegmentValues(this, Expected, Segments);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieSceneCompilerEmptySpaceTest, "System.Engine.Sequencer.Compiler.Empty Space", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieSceneCompilerEmptySpaceTest::RunTest(const FString& Parameters)
{
	using namespace Impl;

	// Specify descending priorities on the segments so we always sort the compiled segments in the order they're defined within the data
	//	This is our test layout
	//	Time			-Inf			10				20				30				40				Inf
	//									[====== 0 ======]				[====== 1 ======]
	//----------------------------------------------------------------------------------------------------------------------------------------
	//	Expected Impls	[	Empty		|		0		|	Empty		|		1		|	Empty		]

	const FMovieSceneSectionData SegmentData[] = {
		FMovieSceneSectionData(TRange<float>(10.f,	20.f), 									FSectionEvaluationData(0), NoBlending, 	1),
		FMovieSceneSectionData(TRange<float>(30.f,	40.f), 									FSectionEvaluationData(1), NoBlending, 	0)
	};

	FSortByPriorityCompilerRules DefaultRules(1, true);
	TArray<FMovieSceneSegment> Segments = FMovieSceneSegmentCompiler().Compile(SegmentData, &DefaultRules);

	FMovieSceneSegment Expected[] = {
		FMovieSceneSegment(FFloatRange(Inf, 	TRangeBound<float>::Exclusive(10.f)), { }),
		FMovieSceneSegment(FFloatRange(10.f,	20.f), { FSectionEvaluationData(0) }),
		FMovieSceneSegment(FFloatRange(20.f,	30.f), {  }),
		FMovieSceneSegment(FFloatRange(30.f,	40.f), { FSectionEvaluationData(1) }),
		FMovieSceneSegment(FFloatRange(40.f,	Inf), {  })
	};
	AssertSegmentValues(this, Expected, Segments);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieSceneCustomCompilerTest, "System.Engine.Sequencer.Compiler.Custom", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieSceneCustomCompilerTest::RunTest(const FString& Parameters)
{
	using namespace Impl;

	// Specify descending priorities on the segments so we always sort the compiled segments in the order they're defined within the data
	//	This is our test layout
	//	Time			-Inf			10			15			20			25			30						40				Inf
	//									[===== 0 (preroll) =====]
	//												[========== 0 ==========]
	//															[========== 0 ==========][========== 0 =========]
	//----------------------------------------------------------------------------------------------------------------------------------------
	//	Expected Impls					[	0 (p)	| (0, 0(p)) |						0						]

	const FMovieSceneSectionData SegmentData[] = {
		FMovieSceneSectionData(TRange<float>(TRangeBound<float>::Inclusive(10.f),	TRangeBound<float>::Exclusive(20.f)), FSectionEvaluationData(0, ESectionEvaluationFlags::PreRoll),	NoBlending,	4),
		FMovieSceneSectionData(TRange<float>(TRangeBound<float>::Inclusive(15.f),	TRangeBound<float>::Exclusive(25.f)), FSectionEvaluationData(0),									NoBlending,	1),
		FMovieSceneSectionData(TRange<float>(TRangeBound<float>::Inclusive(20.f),	TRangeBound<float>::Exclusive(30.f)), FSectionEvaluationData(0),									NoBlending,	1),
		FMovieSceneSectionData(TRange<float>(TRangeBound<float>::Inclusive(30.f),	TRangeBound<float>::Exclusive(40.f)), FSectionEvaluationData(0),									NoBlending,	1),
	};

	FSortByPriorityCompilerRules DefaultRules(0, false);
	TArray<FMovieSceneSegment> Segments = FMovieSceneSegmentCompiler().Compile(SegmentData, &DefaultRules);

	FMovieSceneSegment Expected[] = {
		FMovieSceneSegment(TRange<float>(TRangeBound<float>::Inclusive(10.f),	TRangeBound<float>::Exclusive(15.f)), { FSectionEvaluationData(0, ESectionEvaluationFlags::PreRoll) }),
		FMovieSceneSegment(TRange<float>(TRangeBound<float>::Inclusive(15.f),	TRangeBound<float>::Exclusive(20.f)), { FSectionEvaluationData(0, ESectionEvaluationFlags::PreRoll), FSectionEvaluationData(0) }),
		FMovieSceneSegment(TRange<float>(TRangeBound<float>::Inclusive(20.f),	TRangeBound<float>::Exclusive(40.f)), { FSectionEvaluationData(0) }),
	};

	AssertSegmentValues(this, Expected, Segments);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieSceneTrackCompilerTest, "System.Engine.Sequencer.Compiler.Tracks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieSceneTrackCompilerTest::RunTest(const FString& Parameters)
{
	using namespace Impl;

	//	Track 0 test layout:
	//	Time								-inf				10							20				25					30							inf
	//	------------------------------------------------------------------------------------------------------------------------------------------------------
	//	Track 0:												[====================== 0 ==================]
	//																						[=============== 1 =================]
	//	------------------------------------------------------------------------------------------------------------------------------------------------------
	//	Additive Camera Rules Expected		[ 					|			   0			|		(0,1)	|		 1			|							]
	//	Nearest Section Expected			[ 		0 (10.f)	|			   0			|		(0,1)	|		 1			|			1 (30.f)		]
	//	No Nearest Section Expected			[ 					|			   0			|		(0,1)	|		 1			|							]
	//	High-pass Filter Expected			[ 					|						0					|		 1			|							]

	{
		UMovieSceneSegmentCompilerTestTrack* Track = NewObject<UMovieSceneSegmentCompilerTestTrack>(GetTransientPackage());
		Track->EvalOptions.bCanEvaluateNearestSection = true;

		UMovieSceneSegmentCompilerTestSection* Section0 = NewObject<UMovieSceneSegmentCompilerTestSection>(Track);
		Section0->SetStartTime(10.f);
		Section0->SetEndTime(25.f);
		Section0->SetRowIndex(0);

		UMovieSceneSegmentCompilerTestSection* Section1 = NewObject<UMovieSceneSegmentCompilerTestSection>(Track);
		Section1->SetStartTime(20.f);
		Section1->SetEndTime(30.f);
		Section1->SetRowIndex(1);
		
		Track->SectionArray.Add(Section0);
		Track->SectionArray.Add(Section1);

		TInlineValue<FMovieSceneSegmentCompilerRules> RowCompilerRules = Track->GetRowCompilerRules();
		FMovieSceneTrackCompiler::FRows Rows(Track->SectionArray, RowCompilerRules.GetPtr(nullptr));

		// Test compiling the track with the additive camera rules
		{
			FMovieSceneAdditiveCameraRules AdditiveCameraRules(Track);
			FMovieSceneTrackEvaluationField Field = FMovieSceneTrackCompiler().Compile(Rows.Rows, &AdditiveCameraRules);

			FMovieSceneSegment Expected[] = {
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Inclusive(10.f),	TRangeBound<float>::Exclusive(20.f)), { FSectionEvaluationData(0) }),
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Inclusive(20.f),	TRangeBound<float>::Inclusive(25.f)), { FSectionEvaluationData(0), FSectionEvaluationData(1) }),
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Exclusive(25.f),	TRangeBound<float>::Inclusive(30.f)), { FSectionEvaluationData(1) }),
			};
			AssertSegmentValues(this, Expected, Field.Segments);
		}

		// Test compiling with 'evaluate nearest section' enabled
		{
			Track->EvalOptions.bEvalNearestSection = true;
			FMovieSceneTrackEvaluationField Field = FMovieSceneTrackCompiler().Compile(Rows.Rows, Track->GetTrackCompilerRules().GetPtr(nullptr));

			FMovieSceneSegment Expected[] = {
				FMovieSceneSegment(FFloatRange(Inf,									TRangeBound<float>::Exclusive(10.f)),	{ FSectionEvaluationData(0, 10.f) }),
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Inclusive(10.f),	TRangeBound<float>::Exclusive(20.f)),	{ FSectionEvaluationData(0) }),
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Inclusive(20.f),	TRangeBound<float>::Inclusive(25.f)),	{ FSectionEvaluationData(0), FSectionEvaluationData(1) }),
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Exclusive(25.f),	TRangeBound<float>::Inclusive(30.f)),	{ FSectionEvaluationData(1) }),
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Exclusive(30.f),	Inf), 									{ FSectionEvaluationData(1, 30.f) }),
			};
			AssertSegmentValues(this, Expected, Field.Segments);
		}

		// Test compiling without 'evaluate nearest section' enabled
		{
			Track->EvalOptions.bEvalNearestSection = false;
			FMovieSceneTrackEvaluationField Field = FMovieSceneTrackCompiler().Compile(Rows.Rows, Track->GetTrackCompilerRules().GetPtr(nullptr));

			FMovieSceneSegment Expected[] = {
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Inclusive(10.f),	TRangeBound<float>::Exclusive(20.f)), { FSectionEvaluationData(0) }),
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Inclusive(20.f),	TRangeBound<float>::Inclusive(25.f)), { FSectionEvaluationData(0), FSectionEvaluationData(1) }),
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Exclusive(25.f),	TRangeBound<float>::Inclusive(30.f)), { FSectionEvaluationData(1) }),
			};
			AssertSegmentValues(this, Expected, Field.Segments);
		}

		// Test high-pass filter
		{
			Track->EvalOptions.bEvalNearestSection = false;
			Track->bHighPassFilter = true;
			FMovieSceneTrackEvaluationField Field = FMovieSceneTrackCompiler().Compile(Rows.Rows, Track->GetTrackCompilerRules().GetPtr(nullptr));

			FMovieSceneSegment Expected[] = {
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Inclusive(10.f),	TRangeBound<float>::Inclusive(25.f)), { FSectionEvaluationData(0) }),
				FMovieSceneSegment(FFloatRange(TRangeBound<float>::Exclusive(25.f),	TRangeBound<float>::Inclusive(30.f)), { FSectionEvaluationData(1) }),
			};
			AssertSegmentValues(this, Expected, Field.Segments);
		}
	}

	// Track 1 test layout:
	//	Time								-inf				10			15			20				25					30							inf
	//	-----------------------------------------------------------------------------------------------------------------------------------------------------
	//	Track 1:															[==== 3 ====(==== 3,2 ======)======= 2 =========]
	//															[============================ 0 ============================]
	//										[================================================ 1 ========================================================]
	//	-----------------------------------------------------------------------------------------------------------------------------------------------------
	//	Additive Camera Rules Expected		[ 		1			|	(1,0)	|			(1,0,3)			|		(1,0,2)		|			1				]
	//	Nearest Section Expected			[ 		1			|	(0,1)	|			(3,0,1)			|		(2,0,1)		|			1				]
	//	No Nearest Section Expected			[ 		1			|	(0,1)	|			(3,0,1)			|		(2,0,1)		|			1				]
	//	High-Pass Filter Expected			[ 		1			|	0		|			3				|		2			|			1				]
	{
		UMovieSceneSegmentCompilerTestTrack* Track = NewObject<UMovieSceneSegmentCompilerTestTrack>(GetTransientPackage());

		UMovieSceneSegmentCompilerTestSection* Section0 = NewObject<UMovieSceneSegmentCompilerTestSection>(Track);
		Section0->SetStartTime(10.f);
		Section0->SetEndTime(30.f);
		Section0->SetRowIndex(1);

		UMovieSceneSegmentCompilerTestSection* Section1 = NewObject<UMovieSceneSegmentCompilerTestSection>(Track);
		Section1->SetIsInfinite(true);
		Section1->SetRowIndex(2);
		
		UMovieSceneSegmentCompilerTestSection* Section2 = NewObject<UMovieSceneSegmentCompilerTestSection>(Track);
		Section2->SetStartTime(20.f);
		Section2->SetEndTime(30.f);
		Section2->SetRowIndex(0);

		UMovieSceneSegmentCompilerTestSection* Section3 = NewObject<UMovieSceneSegmentCompilerTestSection>(Track);
		Section3->SetStartTime(15.f);
		Section3->SetEndTime(25.f);
		Section3->SetRowIndex(0);
		Section3->SetOverlapPriority(100.f);

		Track->SectionArray.Add(Section0);
		Track->SectionArray.Add(Section1);
		Track->SectionArray.Add(Section2);
		Track->SectionArray.Add(Section3);

		auto RowCompilerRules = Track->GetRowCompilerRules();
		FMovieSceneTrackCompiler::FRows Rows(Track->SectionArray, RowCompilerRules.GetPtr(nullptr));

		// Additive camera rules prescribe that they are evaluated in order of start time
		FMovieSceneSegment Expected[] = {
			FMovieSceneSegment(FFloatRange(Inf,											TRangeBound<float>::Exclusive(10.f)),	{ }),
			FMovieSceneSegment(FFloatRange(TRangeBound<float>::Inclusive(10.f),			TRangeBound<float>::Exclusive(15.f)),	{ }),
			FMovieSceneSegment(FFloatRange(TRangeBound<float>::Inclusive(15.f),			TRangeBound<float>::Inclusive(25.f)),	{ }),
			FMovieSceneSegment(FFloatRange(TRangeBound<float>::Exclusive(25.f),			TRangeBound<float>::Inclusive(30.f)),	{ }),
			FMovieSceneSegment(FFloatRange(TRangeBound<float>::Exclusive(30.f),			Inf),									{ }),
		};

		// Test compiling the track with the additive camera rules
		{
			FMovieSceneAdditiveCameraRules AdditiveCameraRules(Track);
			FMovieSceneTrackEvaluationField Field = FMovieSceneTrackCompiler().Compile(Rows.Rows, &AdditiveCameraRules);

			// Additive camera rules prescribe that they are evaluated in order of start time
			Expected[0].Impls = { FSectionEvaluationData(1) };
			Expected[1].Impls = { FSectionEvaluationData(1), FSectionEvaluationData(0) };
			Expected[2].Impls = { FSectionEvaluationData(1), FSectionEvaluationData(0), FSectionEvaluationData(3) };
			Expected[3].Impls = { FSectionEvaluationData(1), FSectionEvaluationData(0), FSectionEvaluationData(2) };
			Expected[4].Impls = { FSectionEvaluationData(1) };

			AssertSegmentValues(this, Expected, Field.Segments);
		}

		// Test compiling with 'evaluate nearest section' enabled
		{
			Track->EvalOptions.bEvalNearestSection = true;
			FMovieSceneTrackEvaluationField Field = FMovieSceneTrackCompiler().Compile(Rows.Rows, Track->GetTrackCompilerRules().GetPtr(nullptr));

			Expected[0].Impls = { FSectionEvaluationData(1) };
			Expected[1].Impls = { FSectionEvaluationData(0), FSectionEvaluationData(1) };
			Expected[2].Impls = { FSectionEvaluationData(3), FSectionEvaluationData(0), FSectionEvaluationData(1) };
			Expected[3].Impls = { FSectionEvaluationData(2), FSectionEvaluationData(0), FSectionEvaluationData(1) };
			Expected[4].Impls = { FSectionEvaluationData(1) };

			AssertSegmentValues(this, Expected, Field.Segments);
		}

		// Test compiling without 'evaluate nearest section' enabled
		{
			Track->EvalOptions.bEvalNearestSection = false;
			FMovieSceneTrackEvaluationField Field = FMovieSceneTrackCompiler().Compile(Rows.Rows, Track->GetTrackCompilerRules().GetPtr(nullptr));

			Expected[0].Impls = { FSectionEvaluationData(1) };
			Expected[1].Impls = { FSectionEvaluationData(0), FSectionEvaluationData(1) };
			Expected[2].Impls = { FSectionEvaluationData(3), FSectionEvaluationData(0), FSectionEvaluationData(1) };
			Expected[3].Impls = { FSectionEvaluationData(2), FSectionEvaluationData(0), FSectionEvaluationData(1) };
			Expected[4].Impls = { FSectionEvaluationData(1) };

			AssertSegmentValues(this, Expected, Field.Segments);
		}

		// Test compiling with high pass filter
		{
			Track->EvalOptions.bEvalNearestSection = false;
			Track->bHighPassFilter = true;
			FMovieSceneTrackEvaluationField Field = FMovieSceneTrackCompiler().Compile(Rows.Rows, Track->GetTrackCompilerRules().GetPtr(nullptr));

			Expected[0].Impls = { FSectionEvaluationData(1) };
			Expected[1].Impls = { FSectionEvaluationData(0) };
			Expected[2].Impls = { FSectionEvaluationData(3) };
			Expected[3].Impls = { FSectionEvaluationData(2) };
			Expected[4].Impls = { FSectionEvaluationData(1) };

			AssertSegmentValues(this, Expected, Field.Segments);
		}
	}

	return true;
}


TInlineValue<FMovieSceneSegmentCompilerRules> UMovieSceneSegmentCompilerTestTrack::GetTrackCompilerRules() const
{
	struct FRules : FMovieSceneSegmentCompilerRules
	{
		bool bHighPass;
		bool bEvaluateNearest;

		FRules(bool bInHighPassFilter, bool bInEvaluateNearest)
			: bHighPass(bInHighPassFilter)
			, bEvaluateNearest(bInEvaluateNearest)
		{}

		virtual void BlendSegment(FMovieSceneSegment& Segment, const TArrayView<const FMovieSceneSectionData>& SourceData) const
		{
			if (bHighPass)
			{
				MovieSceneSegmentCompiler::BlendSegmentHighPass(Segment, SourceData);
			}

			// Always sort by priority
			Segment.Impls.Sort(
				[&](const FSectionEvaluationData& A, const FSectionEvaluationData& B)
				{
					return SourceData[A.ImplIndex].Priority > SourceData[B.ImplIndex].Priority;
				}
			);
		}
		virtual TOptional<FMovieSceneSegment> InsertEmptySpace(const TRange<float>& Range, const FMovieSceneSegment* PreviousSegment, const FMovieSceneSegment* NextSegment) const
		{
			return bEvaluateNearest ? MovieSceneSegmentCompiler::EvaluateNearestSegment(Range, PreviousSegment, NextSegment) : TOptional<FMovieSceneSegment>();
		}
	};

	// Evaluate according to bEvalNearestSection preference
	return FRules(bHighPassFilter, EvalOptions.bCanEvaluateNearestSection && EvalOptions.bEvalNearestSection);
}