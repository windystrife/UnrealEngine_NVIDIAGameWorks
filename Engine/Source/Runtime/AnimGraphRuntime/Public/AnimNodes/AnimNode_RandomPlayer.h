// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Math/RandomStream.h"
#include "Animation/AnimationAsset.h"
#include "AlphaBlend.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_RandomPlayer.generated.h"

enum class ERandomDataIndexType
{
	Current,
	Next,
};

struct FRandomAnimPlayData
{
	FRandomAnimPlayData()
		: PreviousTimeAccumulator(0.0f)
		, InternalTimeAccumulator(0.0f)
		, PlayRate(1.0f)
		, BlendWeight(0.0f)
		, RemainingLoops(0)
	{

	}

	// Prev time, used to track loops (prev > current)
	float PreviousTimeAccumulator;

	// Current time through the sequence
	float InternalTimeAccumulator;

	// Calculated play rate 
	float PlayRate;

	// Current blend weight
	float BlendWeight;

	// Calculated loops remaining
	int32 RemainingLoops;

	// Marker tick record for this play through
	FMarkerTickRecord MarkerTickRecord;
};

/** The random player node holds a list of sequences and parameter ranges which will be played continuously
  * In a random order. If shuffle mode is enabled then each entry will be played once before repeating any
  */
USTRUCT(BlueprintInternalUseOnly)
struct FRandomPlayerSequenceEntry
{
	GENERATED_BODY()

	FRandomPlayerSequenceEntry()
	: Sequence(nullptr)
	, ChanceToPlay(1.0f)
	, MinLoopCount(0)
	, MaxLoopCount(0)
	, MinPlayRate(1.0f)
	, MaxPlayRate(1.0f)
	{

	}

	/** Sequence to play when this entry is picked */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	UAnimSequence* Sequence;

	/** When not in shuffle mode, this is the chance this entry will play (normalized against all other sample chances) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	float ChanceToPlay;

	/** Minimum number of times this entry will loop before ending */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta=(UIMin="0", ClampMin="0"))
	int32 MinLoopCount;

	/** Maximum number of times this entry will loop before ending */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta=(UIMin="0", ClampMin="0"))
	int32 MaxLoopCount;

	/** Minimum playrate for this entry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta=(UIMin="0", ClampMin="0"))
	float MinPlayRate;

	/** Maximum playrate for this entry */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta=(UIMin="0", ClampMin="0"))
	float MaxPlayRate;

	/** Blending properties used when this entry is blending in ontop of another entry */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAlphaBlend BlendIn;
};

USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_RandomPlayer : public FAnimNode_Base
{
	GENERATED_BODY()

	FAnimNode_RandomPlayer();

public:

	/** When shuffle mode is active we will never loop a sequence beyond MaxLoopCount
	  * without visiting each sequence in turn (no repeats). Enabling this will ignore
	  * ChanceToPlay for each entry
	  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bShuffleMode;

	/** List of sequences to randomly step through */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TArray<FRandomPlayerSequenceEntry> Entries;

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	// End of FAnimNode_Base interface

private:

	int32 GetNextEntryIndex();
	int32 GetDataIndex(const ERandomDataIndexType& Type);

	void SwitchNextToCurrent();

	void BuildShuffleList();

	// Normalized list of play chances when we aren't using shuffle mode
	TArray<float> NormalizedPlayChances;

	// The currently playing entry in the entries list
	int32 CurrentEntry;

	// The next entry to play, we need this early so we can handle the crossfade correctly as entries
	// can all have different blend in times.
	int32 NextEntry;

	// List to store transient shuffle stack in shuffle mode.
	TArray<int32> ShuffleList;

	// Index of the 'current' data set in the PlayData array
	int32 CurrentDataIndex;

	// Play data for the current and next sequence
	TArray<FRandomAnimPlayData> PlayData;

	// Random number source
	FRandomStream RandomStream;
};
