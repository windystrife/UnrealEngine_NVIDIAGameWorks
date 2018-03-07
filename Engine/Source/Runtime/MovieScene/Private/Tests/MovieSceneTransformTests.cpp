// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "Evaluation/MovieSceneSectionParameters.h"
#include "Containers/ArrayView.h"
#include "Misc/AutomationTest.h"
#include "MovieSceneTestsCommon.h"

#define LOCTEXT_NAMESPACE "MovieSceneTransformTests"

bool IsNearly(TRangeBound<float> A, TRangeBound<float> B)
{
	if (A.IsOpen() || B.IsOpen())
	{
		return A.IsOpen() == B.IsOpen();
	}
	else if (A.IsInclusive() != B.IsInclusive())
	{
		return false;
	}
	
	return FMath::IsNearlyEqual(A.GetValue(), B.GetValue());
}

bool IsNearly(TRange<float> A, TRange<float> B)
{
	return IsNearly(A.GetLowerBound(), B.GetLowerBound()) && IsNearly(A.GetUpperBound(), B.GetUpperBound());
}

namespace Lex
{
	FString ToString(const TRange<float>& InRange)
	{
		TRangeBound<float> SourceLower = InRange.GetLowerBound();
		TRangeBound<float> SourceUpper = InRange.GetUpperBound();

		return *FString::Printf(TEXT("%s-%s"),
			SourceLower.IsOpen() ? 
				TEXT("[...") : 
				SourceLower.IsInclusive() ?
					*FString::Printf(TEXT("[%.5f"), SourceLower.GetValue()) :
					*FString::Printf(TEXT("(%.5f"), SourceLower.GetValue()),

			SourceUpper.IsOpen() ? 
				TEXT("...]") : 
				SourceUpper.IsInclusive() ?
					*FString::Printf(TEXT("%.5f]"), SourceUpper.GetValue()) :
					*FString::Printf(TEXT("%.5f)"), SourceUpper.GetValue())
			);
	}
}

bool TestTransform(FAutomationTestBase& Test, FMovieSceneSequenceTransform Transform, TArrayView<TRange<float>> InSource, TArrayView<TRange<float>> InExpected, const TCHAR* TestName)
{
	using namespace Lex;

	check(InSource.Num() == InExpected.Num());

	bool bSuccess = true;
	for (int32 Index = 0; Index < InSource.Num(); ++Index)
	{
		TRange<float> Result = InSource[Index] * Transform;
		if (!IsNearly(Result, InExpected[Index]))
		{
			Test.AddError(FString::Printf(TEXT("Test '%s' failed (Index %d). Transform (Scale %.3f, Offset %.3f) did not apply correctly (%s != %s)"),
				TestName,
				Index,
				Transform.TimeScale,
				Transform.Offset,
				*ToString(Result),
				*ToString(InExpected[Index])));

			bSuccess = false;
		}
	}

	return bSuccess;
}

// Calculate the transform that transforms from range A to range B
FMovieSceneSequenceTransform TransformRange(float StartA, float EndA, float StartB, float EndB)
{
	return FMovieSceneSequenceTransform(StartB, (EndB - StartB) / (EndA - StartA)) * FMovieSceneSequenceTransform(-StartA);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieSceneSubSectionCoreTransformsTest, "System.Engine.Sequencer.Core.Transforms", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieSceneSubSectionCoreTransformsTest::RunTest(const FString& Parameters)
{
	// We test using ranges since that implicitly tests float transformation
	static const TRangeBound<float> OpenBound;

	TRange<float> InfiniteRange(OpenBound, OpenBound);
	TRange<float> OpenLowerRange(OpenBound, 200.f);
	TRange<float> OpenUpperRange(100.f, OpenBound);
	TRange<float> ClosedRange(100.f, 200.f);

	TRange<float> SourceRanges[] = {
		InfiniteRange, OpenLowerRange, OpenUpperRange, ClosedRange
	};

	bool bSuccess = true;

	{
		// Test Multiplication with an identity transform
		FMovieSceneSequenceTransform IdentityTransform;

		TRange<float> Expected[] = {
			InfiniteRange, OpenLowerRange, OpenUpperRange, ClosedRange
		};
		
		bSuccess = TestTransform(*this, IdentityTransform, SourceRanges, Expected, TEXT("IdentityTransform")) && bSuccess;
	}

	{
		// Test a simple translation
		FMovieSceneSequenceTransform Transform(100.f, 1);

		TRange<float> Expected[] = {
			InfiniteRange, TRange<float>(OpenBound, 300.f), TRange<float>(200.f, OpenBound), TRange<float>(200.f, 300.f)
		};

		bSuccess = TestTransform(*this, Transform, SourceRanges, Expected, TEXT("Simple Translation")) && bSuccess;
	}

	{
		// Test a simple translation + time scale

		// Transform 100 - 200 to -200 - 1000
		FMovieSceneSequenceTransform Transform = TransformRange(100, 200, -200, 1000);

		TRange<float> Expected[] = {
			InfiniteRange, TRange<float>(OpenBound, 1000.f), TRange<float>(-200.f, OpenBound), TRange<float>(-200.f, 1000.f)
		};

		bSuccess = TestTransform(*this, Transform, SourceRanges, Expected, TEXT("Simple Translation + half speed")) && bSuccess;
	}

	{
		// Test that transforming a float by the same transform multiple times, does the same as the equivalent accumulated transform

		// scales by 2, then offsets by 100
		FMovieSceneSequenceTransform SeedTransform = FMovieSceneSequenceTransform(100.f, 0.5f);
		FMovieSceneSequenceTransform AccumulatedTransform;

		float SeedValue = 10.f;
		for (int32 i = 0; i < 5; ++i)
		{
			AccumulatedTransform = SeedTransform * AccumulatedTransform;

			SeedValue = SeedValue * SeedTransform;
		}

		float AccumValue = 10.f * AccumulatedTransform;
		if (!FMath::IsNearlyEqual(AccumValue, SeedValue))
		{
			AddError(FString::Printf(TEXT("Accumulated transform does not have the same effect as separate transformations (%.7f != %.7f)"), AccumValue, SeedValue));
		}

		FMovieSceneSequenceTransform InverseTransform = AccumulatedTransform.Inverse();

		float InverseValue = AccumValue * InverseTransform;
		if (!FMath::IsNearlyEqual(InverseValue, 10.f))
		{
			AddError(FString::Printf(TEXT("Inverse accumulated transform does not return value back to its original value (%.7f != 10.f)"), InverseValue));
		}
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
