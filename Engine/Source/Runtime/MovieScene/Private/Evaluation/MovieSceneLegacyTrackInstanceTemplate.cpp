// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneLegacyTrackInstanceTemplate.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "IMovieScenePlayer.h"


/** Legacy tracks just get a new ID each time (so they always save/restore state, regardless of whether a similar track has already animated the object. That was the old behaviour) */
struct FMovieSceneLegacyAnimTypeID : FMovieSceneAnimTypeID
{
	FMovieSceneLegacyAnimTypeID(uint32 InSeed)
	{
		ID = GenerateHash(&UnusedStatic, InSeed);
	}
	static uint64 UnusedStatic;
};

uint64 FMovieSceneLegacyAnimTypeID::UnusedStatic = 0;

static uint32 LegacyTrackIdSeed = 0;
IMovieSceneTrackInstance::IMovieSceneTrackInstance()
	: AnimTypeID(FMovieSceneLegacyAnimTypeID(LegacyTrackIdSeed++))
{
}

struct FLegacyTrackData : IPersistentEvaluationData
{
	TSharedPtr<IMovieSceneTrackInstance> TrackInstance;
};

struct FRestoreStateToken : IMovieScenePreAnimatedGlobalToken
{
	TArray<TWeakObjectPtr<>> RuntimeObjects;
	TSharedPtr<FMovieSceneSequenceInstance> LegacySequence;
	TSharedPtr<IMovieSceneTrackInstance> LegacyTrackInstance;

	virtual void RestoreState(IMovieScenePlayer& InPlayer) override
	{
		LegacyTrackInstance->RestoreState(RuntimeObjects, InPlayer, *LegacySequence);
	}
};

struct FLegacyPreAnimatedStateProducer : IMovieScenePreAnimatedGlobalTokenProducer
{
	FLegacyPreAnimatedStateProducer(TFunctionRef<IMovieScenePreAnimatedGlobalTokenPtr()> In) : Producer(MoveTemp(In)) {}

	TFunctionRef<IMovieScenePreAnimatedGlobalTokenPtr()> Producer;

	virtual IMovieScenePreAnimatedGlobalTokenPtr CacheExistingState() const override
	{
		return Producer();
	}
};

struct FLegacyExecutionToken : IMovieSceneExecutionToken
{
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		const FMovieSceneEvaluationTemplateInstance* TemplateInstance = Player.GetEvaluationTemplate().GetInstance(Operand.SequenceID);

		TSharedPtr<FMovieSceneSequenceInstance> LegacySequence = TemplateInstance ? TemplateInstance->LegacySequenceInstance : nullptr;
		TSharedPtr<IMovieSceneTrackInstance> LegacyTrackInstance = PersistentData.GetSectionData<FLegacyTrackData>().TrackInstance;
		
		if (!ensure(LegacyTrackInstance.IsValid() && LegacySequence.IsValid()))
		{
			return;
		}

		EMovieSceneUpdateData UpdateData(Context.GetTime(), Context.GetPreviousTime());
		UpdateData.bJumpCut = Context.HasJumped();

		TArray<TWeakObjectPtr<>> RuntimeObjects;
		for (TWeakObjectPtr<> Obj : Player.FindBoundObjects(Operand))
		{
			RuntimeObjects.Add(Obj);
		}

		FLegacyPreAnimatedStateProducer Producer([&]() -> IMovieScenePreAnimatedGlobalTokenPtr{
			LegacyTrackInstance->SaveState(RuntimeObjects, Player, *LegacySequence);

			FRestoreStateToken Token;
			Token.RuntimeObjects = RuntimeObjects;
			Token.LegacySequence = LegacySequence;
			Token.LegacyTrackInstance = LegacyTrackInstance;
			return MoveTemp(Token);
		});
		
		Player.SavePreAnimatedState(LegacyTrackInstance->AnimTypeID, Producer);

		UpdateData.UpdatePass = MSUP_PreUpdate;
		if (LegacyTrackInstance->HasUpdatePasses() & UpdateData.UpdatePass)
		{
			LegacyTrackInstance->Update(UpdateData, RuntimeObjects, Player, *LegacySequence);
		}

		UpdateData.UpdatePass = MSUP_Update;
		if (LegacyTrackInstance->HasUpdatePasses() & UpdateData.UpdatePass)
		{
			LegacyTrackInstance->Update(UpdateData, RuntimeObjects, Player, *LegacySequence);
		}

		UpdateData.UpdatePass = MSUP_PostUpdate;
		if (LegacyTrackInstance->HasUpdatePasses() & UpdateData.UpdatePass)
		{
			LegacyTrackInstance->Update(UpdateData, RuntimeObjects, Player, *LegacySequence);
		}
	}
};

FMovieSceneLegacyTrackInstanceTemplate::FMovieSceneLegacyTrackInstanceTemplate(const UMovieSceneTrack* InTrack)
	: Track(InTrack)
{
}

void FMovieSceneLegacyTrackInstanceTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	ExecutionTokens.Add(FLegacyExecutionToken());
}

void FMovieSceneLegacyTrackInstanceTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	PersistentData.AddSectionData<FLegacyTrackData>().TrackInstance = Track->CreateLegacyInstance();
}
