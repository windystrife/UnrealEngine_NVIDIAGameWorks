// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MovieSceneExecutionToken.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieScenePreAnimatedState.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"

#define LOCTEXT_NAMESPACE "MovieScenePreAnimatedStateTests"

namespace Impl
{
	struct FPreAnimatedToken : IMovieScenePreAnimatedGlobalToken
	{
		int32* Ptr;
		int32 Value;

		FPreAnimatedToken(int32* InPtr, int32 InValue) : Ptr(InPtr), Value(InValue) {}

		virtual void RestoreState(IMovieScenePlayer& Player) override
		{
			*Ptr = Value;
		}
	};

	struct FPreAnimatedTokenProducer : IMovieScenePreAnimatedGlobalTokenProducer
	{
		int32* Ptr;
		mutable int32 InitializeCount;

		FPreAnimatedTokenProducer(int32* InPtr) : Ptr(InPtr), InitializeCount(0) {}

		virtual void InitializeForAnimation() const override
		{
			++InitializeCount;
			*Ptr = 0;
		}

		virtual IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState() const override
		{
			return FPreAnimatedToken(Ptr, *Ptr);
		}
	};

	struct FTestMovieScenePlayer : IMovieScenePlayer
	{
		FMovieSceneRootEvaluationTemplateInstance Template;
		virtual FMovieSceneRootEvaluationTemplateInstance& GetEvaluationTemplate() override { return Template; }
		virtual void UpdateCameraCut(UObject* CameraObject, UObject* UnlockIfCameraObject = nullptr, bool bJumpCut = false) override {}
		virtual void SetViewportSettings(const TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) override {}
		virtual void GetViewportSettings(TMap<FViewportClient*, EMovieSceneViewportParams>& ViewportParamsMap) const override {}
		virtual EMovieScenePlayerStatus::Type GetPlaybackStatus() const override { return EMovieScenePlayerStatus::Playing; }
		virtual void SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus) override {}
	};

	static const int32 TestMagicNumber = 0xdeadbeef;

	int32 TestValue1 = TestMagicNumber;
	int32 TestValue2 = TestMagicNumber;

	FTestMovieScenePlayer TestPlayer;

	FMovieSceneTrackIdentifier TrackID = FMovieSceneTrackIdentifier::Invalid();

	FMovieSceneEvaluationKey TrackKey1(MovieSceneSequenceID::Root, ++TrackID);
	FMovieSceneEvaluationKey SectionKey1(MovieSceneSequenceID::Root, TrackID, 0);

	FMovieSceneEvaluationKey TrackKey2(MovieSceneSequenceID::Root, ++TrackID);
	FMovieSceneEvaluationKey SectionKey2(MovieSceneSequenceID::Root, TrackID, 0);

	FMovieSceneAnimTypeID AnimType1 = FMovieSceneAnimTypeID::Unique();
	FMovieSceneAnimTypeID AnimType2 = FMovieSceneAnimTypeID::Unique();

	void Assert(FAutomationTestBase* Test, int32 Actual, int32 Expected, const TCHAR* Message)
	{
		if (Actual != Expected)
		{
			Test->AddError(FString::Printf(TEXT("%s. Expected %d, actual %d."), Message, Expected, Actual));
		}
	}

	void ResetValues()
	{
		TestValue1 = TestMagicNumber;
		TestValue2 = TestMagicNumber;
	}
}

/** Tests that multiple calls to SavePreAnimated state works correctly */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieScenePreAnimatedStateGlobalTest, "System.Engine.Sequencer.Pre-Animated State.Global", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieScenePreAnimatedStateGlobalTest::RunTest(const FString& Parameters)
{
	using namespace Impl;

	ResetValues();

	FMovieScenePreAnimatedState State;
	State.EnableGlobalCapture();

	FPreAnimatedTokenProducer Producer(&TestValue1);

	// Save the global state of TestValue1.
	State.SavePreAnimatedState(AnimType1, Producer);
	State.SavePreAnimatedState(AnimType1, Producer);
	State.SavePreAnimatedState(AnimType1, Producer);

	Assert(this, Producer.InitializeCount, 1, TEXT("Should have called FPreAnimatedTokenProducer::InitializeForAnimation exactly once."));
	Assert(this, TestValue1, 0, TEXT("TestValue1 did not initialize correctly."));

	TestValue1 = 50;

	State.RestorePreAnimatedState(TestPlayer);

	Assert(this, TestValue1, TestMagicNumber, TEXT("TestValue1 did not restore correctly."));

	return true;
}

/** This specifically tests that a single call to SavePreAnimatedState while capturing for an entity, and globally, produces the correct results when restoring the entity */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieScenePreAnimatedStateEntityTest, "System.Engine.Sequencer.Pre-Animated State.Entity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieScenePreAnimatedStateEntityTest::RunTest(const FString& Parameters)
{
	using namespace Impl;

	ResetValues();

	FMovieScenePreAnimatedState State;
	State.EnableGlobalCapture();

	FPreAnimatedTokenProducer Producer(&TestValue1);
	
	State.SetCaptureEntity(SectionKey1, EMovieSceneCompletionMode::RestoreState);
	State.SavePreAnimatedState(AnimType1, Producer);

	Assert(this, Producer.InitializeCount, 1, TEXT("Should have called FPreAnimatedTokenProducer::InitializeForAnimation exactly once."));
	Assert(this, TestValue1, 0, TEXT("TestValue1 did not initialize correctly."));

	TestValue1 = 50;

	State.RestorePreAnimatedState(TestPlayer, SectionKey1);
	Assert(this, TestValue1, TestMagicNumber, TEXT("Section did not restore correctly."));

	TestValue1 = 100;
	State.RestorePreAnimatedState(TestPlayer);
	Assert(this, TestValue1, 100, TEXT("Global state should not still exist (it should have been cleared with the entity)."));
	return true;
}

/** Tests that overlapping entities that restore in different orders than they saved correctly restores to the original state */ 
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieScenePreAnimatedStateOverlappingEntitiesTest, "System.Engine.Sequencer.Pre-Animated State.Overlapping Entities", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieScenePreAnimatedStateOverlappingEntitiesTest::RunTest(const FString& Parameters)
{
	using namespace Impl;

	ResetValues();

	FMovieScenePreAnimatedState State;
	State.EnableGlobalCapture();

	FPreAnimatedTokenProducer Producer(&TestValue1);

	// 1. Save a global token
	{
		State.SavePreAnimatedState(AnimType1, Producer);

		Assert(this, Producer.InitializeCount, 1, TEXT("Should have called FPreAnimatedTokenProducer::InitializeForAnimation exactly once."));
		Assert(this, TestValue1, 0, TEXT("TestValue1 did not initialize correctly."));
	}

	// 2. Save a token for the track's evaluation
	{
		State.SetCaptureEntity(TrackKey1, EMovieSceneCompletionMode::RestoreState);
		State.SavePreAnimatedState(AnimType1, Producer);

		TestValue1 = 50;
		Assert(this, Producer.InitializeCount, 1, TEXT("Should not have called Initialize when capturing for the track."));
	}

	// 3. Save a token for the section's evaluation
	{
		State.SetCaptureEntity(SectionKey1, EMovieSceneCompletionMode::RestoreState);
		State.SavePreAnimatedState(AnimType1, Producer);

		TestValue1 = 100;
		Assert(this, Producer.InitializeCount, 1, TEXT("Should not have called Initialize when capturing for the section."));
	}

	// 4. Save a token for another section's evaluation
	{
		State.SetCaptureEntity(SectionKey2, EMovieSceneCompletionMode::RestoreState);
		State.SavePreAnimatedState(AnimType1, Producer);

		TestValue1 = 150;
		Assert(this, Producer.InitializeCount, 1, TEXT("Should not have called Initialize when capturing for the section."));
	}

	// Restore the section first - ensure it does not restore the value (because the track is still animating it)
	State.RestorePreAnimatedState(TestPlayer, SectionKey1);
	Assert(this, TestValue1, 150, TEXT("Section 1 should not have restored."));

	// Restore the track - it should not restore either, because section 2 is still active
	State.RestorePreAnimatedState(TestPlayer, TrackKey1);
	Assert(this, TestValue1, 150, TEXT("Track should not have restored."));

	// Restore the section - since it's the last entity animating the object with 'RestoreState' it should restore to the orignal value
	State.RestorePreAnimatedState(TestPlayer, SectionKey2);
	Assert(this, TestValue1, 0, TEXT("Section 2 did not restore correctly."));

	// Restore globally - ensure that test value goes back to the original value
	State.RestorePreAnimatedState(TestPlayer);
	Assert(this, TestValue1, TestMagicNumber, TEXT("Global state did not restore correctly."));

	return true;
}

/** Tests an edge case where one section keeps state, while another subsequent section restores state. the second section should restore to its starting value, not the original state before the first section. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieScenePreAnimatedStateKeepThenRestoreEntityTest, "System.Engine.Sequencer.Pre-Animated State.Keep Then Restore Entity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMovieScenePreAnimatedStateKeepThenRestoreEntityTest::RunTest(const FString& Parameters)
{
	using namespace Impl;

	ResetValues();

	FMovieScenePreAnimatedState State;
	State.EnableGlobalCapture();

	FPreAnimatedTokenProducer Producer(&TestValue1);

	// Indicate that the entity should not capture state
	State.SetCaptureEntity(SectionKey1, EMovieSceneCompletionMode::KeepState);
	// Save state - this will only save globally
	State.SavePreAnimatedState(AnimType1, Producer);

	Assert(this, Producer.InitializeCount, 1, TEXT("Should have called FPreAnimatedTokenProducer::InitializeForAnimation exactly once."));
	Assert(this, TestValue1, 0, TEXT("TestValue1 did not initialize correctly."));

	TestValue1 = 50;

	// Restore state for the entity only - this should not do anything since we specified KeepState above
	State.RestorePreAnimatedState(TestPlayer, SectionKey1);
	Assert(this, TestValue1, 50, TEXT("Section should not have restored state."));

	// Indicate that SectionKey2 is now animating, and wants to restore state
	State.SetCaptureEntity(SectionKey2, EMovieSceneCompletionMode::RestoreState);
	State.SavePreAnimatedState(AnimType1, Producer);

	Assert(this, Producer.InitializeCount, 1, TEXT("Should not have called FPreAnimatedTokenProducer::InitializeForAnimation a second time."));

	TestValue1 = 100;

	// Restoring section key 2 here should result in the test value being the same value that was set while section 1 was evaluating (50)
	State.RestorePreAnimatedState(TestPlayer, SectionKey2);
	Assert(this, TestValue1, 50, TEXT("Section 2 did not restore to the correct value. It should restore back to the value that was set in section 1 (it doesn't restore state)."));

	// We should still have the global state of the object cached which will restore it to the original state
	State.RestorePreAnimatedState(TestPlayer);
	Assert(this, TestValue1, TestMagicNumber, TEXT("Global state did not restore correctly."));

	return true;
}

/** Tests an edge case where one section keeps state, while another subsequent section restores state. the second section should restore to its starting value, not the original state before the first section. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovieScenePreAnimatedStatePerformanceTest, "System.Engine.Sequencer.Pre-Animated State.Performance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::Disabled)
bool FMovieScenePreAnimatedStatePerformanceTest::RunTest(const FString& Parameters)
{
	using namespace Impl;

	ResetValues();

	FMovieScenePreAnimatedState State;
	State.EnableGlobalCapture();

	FPreAnimatedTokenProducer Producer(&TestValue1);

	// Indicate that the entity should not capture state
	State.SetCaptureEntity(SectionKey1, EMovieSceneCompletionMode::KeepState);

	for (int32 Iteration = 0; Iteration < 1000000; ++Iteration)
	{
		State.SavePreAnimatedState(AnimType1, Producer);
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
