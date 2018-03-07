// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMovieSceneSection;
class FSequencerDisplayNode;
class IKeyArea;

enum class EFindKeyDirection
{
	Forwards, Backwards
};

struct FSequencerKeyCollectionSignature
{
	FSequencerKeyCollectionSignature()
	{}

	/** Initialize this key collection from the specified nodes. Only gathers keys from those explicitly specified. */
	SEQUENCER_API static FSequencerKeyCollectionSignature FromNodes(const TArray<FSequencerDisplayNode*>& InNodes, float InDuplicateThresholdTime);

	/** Initialize this key collection from the specified nodes. Gathers keys from all child nodes. */
	SEQUENCER_API static FSequencerKeyCollectionSignature FromNodesRecursive(const TArray<FSequencerDisplayNode*>& InNodes, float InDuplicateThresholdTime);

	/** Initialize this key collection from the specified node and section index. */
	SEQUENCER_API static FSequencerKeyCollectionSignature FromNodeRecursive(FSequencerDisplayNode& InNode, UMovieSceneSection* InSection, float InDuplicateThresholdTime);

	/** Compare this signature for inequality with another */
	SEQUENCER_API friend bool operator!=(const FSequencerKeyCollectionSignature& A, const FSequencerKeyCollectionSignature& B);

	/** Compare this signature for equality with another */
	SEQUENCER_API friend bool operator==(const FSequencerKeyCollectionSignature& A, const FSequencerKeyCollectionSignature& B);

	/** Access the map of keyareas and signatures this signature was generated for */
	const TMap<TSharedRef<IKeyArea>, FGuid>& GetKeyAreas() const
	{
		return KeyAreaToSignature;
	}

	/** Access duplicate threshold that this signature was generated for */
	float GetDuplicateThreshold() const
	{
		return DuplicateThresholdTime;
	}

private:

	/** Check whether this signature contains content that cannot be cached (such content causes this signature to never compare equal with another) */
	bool HasUncachableContent() const;

	/** The time at which proximal keys are considered duplicates */
	float DuplicateThresholdTime;

	/** Map of key areas to the section signature with with this signature was generated */
	TMap<TSharedRef<IKeyArea>, FGuid> KeyAreaToSignature;
};

/**
 * A collection of keys gathered recursively from a particular node or nodes
 */
class ISequencerKeyCollection
{
public:
	virtual ~ISequencerKeyCollection() {}

public:

	/** Iterate the keys contained within this collection */
	virtual void IterateKeys(const TFunctionRef<bool(float)>& Iter) = 0;

	/** Get a value specifying how close keys need to be in order to be considered equal by this collection */
	virtual float GetKeyGroupingThreshold() const = 0;

	/** Find the first key in the given range */
	virtual TOptional<float> FindFirstKeyInRange(const TRange<float>& InRange, EFindKeyDirection Direction) const = 0;

public:
	/**
	 * Initialize this key collection from the specified nodes. Only gathers keys from those explicitly specified.
	 * @return true if this collection was (re)initialized, false if it did not need updating
	 */
	virtual bool InitializeExplicit(const TArray<FSequencerDisplayNode*>& InNodes, float DuplicateThreshold = SMALL_NUMBER) = 0;

	/**
	 * Initialize this key collection from the specified nodes. Gathers keys from all child nodes.
	 * @return true if this collection was (re)initialized, false if it did not need updating
	 */
	virtual bool InitializeRecursive(const TArray<FSequencerDisplayNode*>& InNodes, float DuplicateThreshold = SMALL_NUMBER) = 0;
	
	/**
	 * Initialize this key collection from the specified node and section index.
	 * @return true if this collection was (re)initialized, false if it did not need updating
	 */
	virtual bool InitializeRecursive(FSequencerDisplayNode& InNode, UMovieSceneSection* InSection, float DuplicateThreshold = SMALL_NUMBER) = 0;
};
