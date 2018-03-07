// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneSequenceID.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "Evaluation/MovieSceneEvaluationState.h"
#include "Evaluation/MovieScenePlayback.h"
#include "Evaluation/MovieSceneSequenceTemplateStore.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Evaluation/MovieSceneExecutionTokens.h"

class UMovieSceneSequence;
struct FDelayedPreAnimatedStateRestore;

/**
 * An instance of an evaluation template. Fast to initialize and evaluate.
 */
struct FMovieSceneEvaluationTemplateInstance
{
	FMovieSceneEvaluationTemplateInstance() : Template(nullptr), HierarchicalBias(0), SequenceID(MovieSceneSequenceID::Invalid) {}

	/**
	 * Constructor for root instances
	 *
	 * @param InSequence			The source master sequence we're playing back at the root level
	 * @param InTemplate			The template to use when playing back the specified sequence. A pointer to which will be held within this instance.
	 */
	FMovieSceneEvaluationTemplateInstance(UMovieSceneSequence& InSequence, const FMovieSceneEvaluationTemplate& InTemplate);

	/**
	 * Constructor for sub sequence instances. SubData must point to a valid sequence
	 *
	 * @param SubData				Information about the sub sequence we're instantiating
	 * @param InTemplate			The template to use when playing back the sub sequence. A pointer to which will be held within this instance.
	 * @param InSequenceID			The unique identifier within the master sequence
	 */
	FMovieSceneEvaluationTemplateInstance(const FMovieSceneSubSequenceData& SubData, const FMovieSceneEvaluationTemplate& InTemplate, FMovieSceneSequenceIDRef InSequenceID);

	FMovieSceneEvaluationTemplateInstance(const FMovieSceneEvaluationTemplateInstance&) = delete;
	FMovieSceneEvaluationTemplateInstance& operator=(const FMovieSceneEvaluationTemplateInstance&) = delete;

	FMovieSceneEvaluationTemplateInstance(FMovieSceneEvaluationTemplateInstance&&) = default;
	FMovieSceneEvaluationTemplateInstance& operator=(FMovieSceneEvaluationTemplateInstance&&) = default;

public:

	/** The sequence that the sub section references */
	TWeakObjectPtr<UMovieSceneSequence> Sequence;

	/** Transform that transforms a given time from the sequences outer space, to its authored space. */
	FMovieSceneSequenceTransform RootToSequenceTransform;

	/** Pointer to the evaluation template we're evaluating */
	const FMovieSceneEvaluationTemplate* Template;

	/** Pre and Post roll ranges in the inner-sequence's time space */
	TRange<float> PreRollRange, PostRollRange;

	/** The hierarchical bias for this template instance */
	int32 HierarchicalBias;

	/** ID of the sequence within the master sequence */
	FMovieSceneSequenceID SequenceID;

	/** Legacy sequence instance required for old track instances */
	TSharedPtr<FMovieSceneSequenceInstance> LegacySequenceInstance;
};

/**
 * Root evaluation template instance used to play back any sequence
 */
struct FMovieSceneRootEvaluationTemplateInstance
{
public:
	MOVIESCENE_API FMovieSceneRootEvaluationTemplateInstance();
	MOVIESCENE_API ~FMovieSceneRootEvaluationTemplateInstance();

	FMovieSceneRootEvaluationTemplateInstance(const FMovieSceneRootEvaluationTemplateInstance&) = delete;
	FMovieSceneRootEvaluationTemplateInstance& operator=(const FMovieSceneRootEvaluationTemplateInstance&) = delete;

	FMovieSceneRootEvaluationTemplateInstance(FMovieSceneRootEvaluationTemplateInstance&&) = delete;
	FMovieSceneRootEvaluationTemplateInstance& operator=(FMovieSceneRootEvaluationTemplateInstance&&) = delete;

	/**
	 * Check if this instance has been initialized correctly
	 */
	bool IsValid() const
	{
		return RootSequence.Get() && RootInstance.Template;
	}

	/**
	 * Initialize this template instance with the specified sequence
	 *
	 * @param RootSequence				The sequence play back
	 * @param Player					The player responsible for playback
	 */
	MOVIESCENE_API void Initialize(UMovieSceneSequence& RootSequence, IMovieScenePlayer& Player);

	/**
	 * Initialize this template instance with the specified sequence
	 *
	 * @param RootSequence				The sequence play back
	 * @param Player					The player responsible for playback
	 * @param TemplateStore				Template store responsible for supplying templates for a given sequence
	 */
	MOVIESCENE_API void Initialize(UMovieSceneSequence& RootSequence, IMovieScenePlayer& Player, TSharedRef<FMovieSceneSequenceTemplateStore> TemplateStore);

	/**
	 * Evaluate this sequence
	 *
	 * @param Context				Evaluation context containing the time (or range) to evaluate
	 * @param Player				The player responsible for playback
	 * @param OverrideRootID		The ID of the sequence from which to evaluate.
	 */
	MOVIESCENE_API void Evaluate(FMovieSceneContext Context, IMovieScenePlayer& Player, FMovieSceneSequenceID OverrideRootID = MovieSceneSequenceID::Root);

	/**
	 * Indicate that we're not going to evaluate this instance again, and that we should tear down any current state
	 *
	 * @param Player				The player responsible for playback
	 */
	MOVIESCENE_API void Finish(IMovieScenePlayer& Player);

public:

	class FOnUpdated : public TBaseMulticastDelegate<void>
	{
		friend FMovieSceneRootEvaluationTemplateInstance;
	};

	/**
	 * Event that is triggered when this sequence has been updated
	 */
	FOnUpdated& OnUpdated() { return OnUpdatedEvent; }

public:

	/**
	 * Retrieve a specific template instance given a sequence ID
	 *
	 * @param SequenceID			The sequence ID to find a template for
	 * @return Pointer to a template instance, or nullptr if the ID was not found
	 */
	FORCEINLINE const FMovieSceneEvaluationTemplateInstance* GetInstance(FMovieSceneSequenceIDRef SequenceID) const
	{
		return SequenceID == MovieSceneSequenceID::Root ? &RootInstance : SubInstances.Find(SequenceID);
	}

	/**
	 * Access the underlying map of sub template instances
	 */
	const TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplateInstance>& GetSubInstances() const
	{
		return SubInstances;
	}

	/**
	 * Attempt to locate the underlying sequence given a sequence ID
	 *
	 * @param SequenceID 			ID of the sequence to locate
	 * @return The sequence, or nullptr if the ID was not found
	 */
	UMovieSceneSequence* GetSequence(FMovieSceneSequenceIDRef SequenceID) const
	{
		if (const FMovieSceneEvaluationTemplateInstance* Instance = GetInstance(SequenceID))
		{
			return Instance->Sequence.Get();
		}
		return nullptr;
	}


	/**
	 * Access the master sequence's hierarchy data
	 */
	const FMovieSceneSequenceHierarchy& GetHierarchy() const
	{
		return RootInstance.Template->Hierarchy;
	}

	/**
 	 * Cache of everything that is evaluated this frame 
	 */
	const FMovieSceneEvaluationMetaData& GetThisFrameMetaData() const
	{
		return ThisFrameMetaData;
	}

	/**
	 * Check whether this instance is dirty (one of its referenced templates has changed)
	 */
	bool IsDirty() const
	{
		return bIsDirty;
	}

	/**
	 * Copy any actuators from this template instance into the specified accumulator
	 *
	 * @param Accumulator 			The accumulator to copy actuators into
	 */
	MOVIESCENE_API void CopyActuators(FMovieSceneBlendingAccumulator& Accumulator) const;

private:

	/**
	 * Process entities that are newly evaluated, and those that are no longer being evaluated
	 */
	void CallSetupTearDown(IMovieScenePlayer& Player, FDelayedPreAnimatedStateRestore* DelayedRestore = nullptr);

	/**
	 * Evaluate a particular group of a segment
	 */
	void EvaluateGroup(const FMovieSceneEvaluationGroup& Group, const FMovieSceneContext& Context, IMovieScenePlayer& Player);

	/**
	 * Remap the specified sequence ID based on the currently evaluating sequence path, to the Root
	 *
	 * @param SequenceID			The sequence ID to find a template for
	 * @return Pointer to a template instance, or nullptr if the ID was not found
	 */
	FORCEINLINE_DEBUGGABLE FMovieSceneSequenceID GetSequenceIdForRoot(FMovieSceneSequenceID SequenceID) const
	{
		if (!ReverseOverrideRootPath.Num())
		{
			return SequenceID;
		}

		for (FMovieSceneSequenceID Parent : ReverseOverrideRootPath)
		{
			SequenceID = SequenceID.AccumulateParentID(Parent);
		}
		return SequenceID;
	}

	/**
	 * Retrieve a specific template instance given a sequence ID. Assumes it exists.
	 *
	 * @param SequenceID			The sequence ID to find a template for
	 * @return The template instance
	 */
	FORCEINLINE_DEBUGGABLE const FMovieSceneEvaluationTemplateInstance& GetInstanceChecked(FMovieSceneSequenceIDRef SequenceID) const
	{
		return SequenceID == MovieSceneSequenceID::Root ? RootInstance : SubInstances.FindChecked(SequenceID);
	}

	/**
	 * Attempt to retrieve a specific template instance given a sequence ID
	 *
	 * @param SequenceID			The sequence ID to find a template for
	 * @return Pointer to a template instance, or nullptr if the ID was not found
	 */
	FORCEINLINE_DEBUGGABLE const FMovieSceneEvaluationTemplateInstance* FindInstance(FMovieSceneSequenceIDRef SequenceID) const
	{
		return SequenceID == MovieSceneSequenceID::Root ? &RootInstance : SubInstances.Find(SequenceID);
	}

private:

	/** Called whenever any of the sequences contained within this instance is changed */
	void OnSequenceChanged();

	/** Reset this template instance */
	void Reset();

private:

	TWeakObjectPtr<UMovieSceneSequence> RootSequence;

	FMovieSceneEvaluationTemplateInstance RootInstance;

	/** Cache of everything that was evaluated last frame */
	FMovieSceneEvaluationMetaData LastFrameMetaData;
	/** Cache of everything that is evaluated this frame */
	FMovieSceneEvaluationMetaData ThisFrameMetaData;

	/** Map of all sub instances, arranged by sequence ID */
	TMap<FMovieSceneSequenceID, FMovieSceneEvaluationTemplateInstance> SubInstances;

	/** Template store responsible for supplying templates for a given sequence */
	TSharedPtr<FMovieSceneSequenceTemplateStore> TemplateStore;

	/** A reverse path of deterministic sequence IDs required to accumulate from local -> root */
	TArray<FMovieSceneSequenceID, TInlineAllocator<8>> ReverseOverrideRootPath;

	/** Execution tokens that are used to apply animated state */
	FMovieSceneExecutionTokens ExecutionTokens;

	/** True when any of our templates are out of date, and need reinitializing */
	bool bIsDirty;

	/** Event that is triggered on update */
	FOnUpdated OnUpdatedEvent;
};
