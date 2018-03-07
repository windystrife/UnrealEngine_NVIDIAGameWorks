// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Baseclass for animation compression algorithms.
 *
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Animation/AnimSequence.h"
#include "AnimationUtils.h"
#include "AnimEnums.h"
#include "AnimCompress.generated.h"

//Helper function for ddc key generation
uint8 MakeBitForFlag(uint32 Item, uint32 Position);

class ENGINE_API FCompressionMemorySummary
{
public:
	FCompressionMemorySummary(bool bInEnabled);

	void GatherPreCompressionStats(UAnimSequence* Seq, int32 ProgressNumerator, int32 ProgressDenominator);

	void GatherPostCompressionStats(UAnimSequence* Seq, TArray<FBoneData>& BoneData);

	~FCompressionMemorySummary();

private:
	bool bEnabled;
	bool bUsed;
	int32 TotalRaw;
	int32 TotalBeforeCompressed;
	int32 TotalAfterCompressed;

	float ErrorTotal;
	float ErrorCount;
	float AverageError;
	float MaxError;
	float MaxErrorTime;
	int32 MaxErrorBone;
	FName MaxErrorBoneName;
	FName MaxErrorAnimName;
};

//////////////////////////////////////////////////////////////////////////
// FAnimCompressContext - Context information / storage for use during
// animation compression
struct ENGINE_API FAnimCompressContext
{
	FCompressionMemorySummary	CompressionSummary;
	uint32						AnimIndex;
	uint32						MaxAnimations;
	bool						bAllowAlternateCompressor;
	bool						bOutput;

	FAnimCompressContext(bool bInAllowAlternateCompressor, bool bInOutput, uint32 InMaxAnimations = 1) : CompressionSummary(bInOutput), AnimIndex(0), MaxAnimations(InMaxAnimations), bAllowAlternateCompressor(bInAllowAlternateCompressor), bOutput(bInOutput) {}

	void GatherPreCompressionStats(UAnimSequence* Seq);

	void GatherPostCompressionStats(UAnimSequence* Seq, TArray<FBoneData>& BoneData);
};

UCLASS(abstract, hidecategories=Object, MinimalAPI, EditInlineNew)
class UAnimCompress : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Name of Compression Scheme used for this asset*/
	UPROPERTY(Category=Compression, VisibleAnywhere)
	FString Description;

	/** Compression algorithms requiring a skeleton should set this value to true. */
	UPROPERTY()
	uint32 bNeedsSkeleton:1;

	/** Format for bitwise compression of translation data. */
	UPROPERTY()
	TEnumAsByte<AnimationCompressionFormat> TranslationCompressionFormat;

	/** Format for bitwise compression of rotation data. */
	UPROPERTY()
	TEnumAsByte<AnimationCompressionFormat> RotationCompressionFormat;

	/** Format for bitwise compression of scale data. */
	UPROPERTY()
	TEnumAsByte<AnimationCompressionFormat> ScaleCompressionFormat;

	/** Max error for compression of curves using remove redundant keys */
	UPROPERTY(Category = Compression, EditAnywhere)
	float MaxCurveError;

#if WITH_EDITOR
public:
	/**
	 * Reduce the number of keyframes and bitwise compress the specified sequence.
	 *
	 * @param	AnimSeq		The animation sequence to compress.
	 * @param	bOutput		If false don't generate output or compute memory savings.
	 * @return				false if a skeleton was needed by the algorithm but not provided.
	 */
	ENGINE_API bool Reduce(class UAnimSequence* AnimSeq, bool bOutput);

	/**
	 * Reduce the number of keyframes and bitwise compress all sequences in the specified array.
	 *
	 * @param	AnimSequences	The animations to compress.
	 * @param	bOutput			If false don't generate output or compute memory savings.
	 * @return					false if a skeleton was needed by the algorithm but not provided.
	 */
	ENGINE_API bool Reduce(class UAnimSequence* AnimSeq, FAnimCompressContext& Context);
#endif // WITH_EDITOR
protected:
#if WITH_EDITOR
	/**
	 * Implemented by child classes, this function reduces the number of keyframes in
	 * the specified sequence, given the specified skeleton (if needed).
	 *
	 * @return		true if the keyframe reduction was successful.
	 */
	virtual void DoReduction(class UAnimSequence* AnimSeq, const TArray<class FBoneData>& BoneData) PURE_VIRTUAL(UAnimCompress::DoReduction,);
#endif // WITH_EDITOR
	/**
	 * Common compression utility to remove 'redundant' position keys based on the provided delta threshold
	 *
	 * @param	Track			Position tracks to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 */
	static void FilterTrivialPositionKeys(
		TArray<struct FTranslationTrack>& Track,
		float MaxPosDelta);

	/**
	 * Common compression utility to remove 'redundant' position keys in a single track based on the provided delta threshold
	 *
	 * @param	Track			Track to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 */
	static void FilterTrivialPositionKeys(
		struct FTranslationTrack& Track,
		float MaxPosDelta);

	/**
	 * Common compression utility to remove 'redundant' rotation keys in a set of tracks based on the provided delta threshold
	 *
	 * @param	InputTracks		Array of rotation track elements to reduce
	 * @param	MaxRotDelta		Maximum angle threshold to consider stationary motion
	 */
	static void FilterTrivialRotationKeys(
		TArray<struct FRotationTrack>& InputTracks,
		float MaxRotDelta);

	/**
	 * Common compression utility to remove 'redundant' rotation keys in a set of tracks based on the provided delta threshold
	 *
	 * @param	Track			Track to reduce
	 * @param	MaxRotDelta		Maximum angle threshold to consider stationary motion
	 */
	static void FilterTrivialRotationKeys(
		struct FRotationTrack& Track,
		float MaxRotDelta);

	/**
	 * Common compression utility to remove 'redundant' Scale keys based on the provided delta threshold
	 *
	 * @param	Track			Scale tracks to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 */
	static void FilterTrivialScaleKeys(
		TArray<struct FScaleTrack>& Track,
		float MaxScaleDelta);

	/**
	 * Common compression utility to remove 'redundant' Scale keys in a single track based on the provided delta threshold
	 *
	 * @param	Track			Track to reduce
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 */
	static void FilterTrivialScaleKeys(
		struct FScaleTrack& Track,
		float MaxScaleDelta);

	
	/**
	 * Common compression utility to remove 'redundant' keys based on the provided delta thresholds
	 *
	 * @param	PositionTracks	Array of position track elements to reduce
	 * @param	RotationTracks	Array of rotation track elements to reduce
	 * @param	ScaleTracks		Array of scale track elements to reduce	 
	 * @param	MaxPosDelta		Maximum local-space threshold for stationary motion
	 * @param	MaxRotDelta		Maximum angle threshold to consider stationary motion
	 * @param	MaxScaleDelta	Maximum scale threshold to consider stationary motion	 
	 */
	static void FilterTrivialKeys(
		TArray<struct FTranslationTrack>& PositionTracks,
		TArray<struct FRotationTrack>& RotationTracks,
		TArray<struct FScaleTrack>& ScaleTracks,
		float MaxPosDelta,
		float MaxRotDelta, 
		float MaxScaleDelta);

	/** 
	 * Remove translation keys from tracks marked bAnimRotationOnly.
	 *
	 * @param PositionTracks	Array of position track elements to reduce
	 * @param AnimSeq			AnimSequence the track is from.
	 */
	static void FilterAnimRotationOnlyKeys(TArray<FTranslationTrack>& PositionTracks, UAnimSequence* AnimSeq);

	/**
	 * Common compression utility to retain only intermittent position keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	PositionTracks	Array of position track elements to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentPositionKeys(
		TArray<struct FTranslationTrack>& PositionTracks,
		int32 StartIndex,
		int32 Interval);

	/**
	 * Common compression utility to retain only intermittent position keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	Track			Track to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentPositionKeys(
		struct FTranslationTrack& Track,
		int32 StartIndex,
		int32 Interval);

	/**
	 * Common compression utility to retain only intermittent rotation keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	RotationTracks	Array of rotation track elements to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentRotationKeys(
		TArray<struct FRotationTrack>& RotationTracks,
		int32 StartIndex,
		int32 Interval);

	/**
	 * Common compression utility to retain only intermittent rotation keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	Track			Track to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentRotationKeys(
		struct FRotationTrack& Track,
		int32 StartIndex,
		int32 Interval);

	/**
	 * Common compression utility to retain only intermittent animation keys. For example,
	 * calling with an Interval of 3 would keep every third key in the set and discard the rest
	 *
	 * @param	PositionTracks	Array of position track elements to reduce
	 * @param	RotationTracks	Array of rotation track elements to reduce
	 * @param	StartIndex		Index at which to begin reduction
	 * @param	Interval		Interval of keys to retain
	 */
	static void FilterIntermittentKeys(
		TArray<struct FTranslationTrack>& PositionTracks,
		TArray<struct FRotationTrack>& RotationTracks,
		int32 StartIndex,
		int32 Interval);

	/**
	 * Common compression utility to populate individual rotation and translation track
	 * arrays from a set of raw animation tracks. Used as a precurser to animation compression.
	 *
	 * @param	RawAnimData			Array of raw animation tracks
	 * @param	SequenceLength		The duration of the animation in seconds
	 * @param	OutTranslationData	Translation tracks to fill
	 * @param	OutRotationData		Rotation tracks to fill
	 * @param	OutScaleData		Scale tracks to fill
	 */
	static void SeparateRawDataIntoTracks(
		const TArray<struct FRawAnimSequenceTrack>& RawAnimData,
		float SequenceLength,
		TArray<struct FTranslationTrack>& OutTranslationData,
		TArray<struct FRotationTrack>& OutRotationData, 
		TArray<struct FScaleTrack>& OutScaleData);

	/**
	 * Common compression utility to walk an array of rotation tracks and enforce
	 * that all adjacent rotation keys are represented by shortest-arc quaternion pairs.
	 *
	 * @param	RotationData	Array of rotation track elements to reduce.
	 */
	static void PrecalculateShortestQuaternionRoutes(TArray<struct FRotationTrack>& RotationData);

public:

	/**
	 * Encodes individual key arrays into an AnimSequence using the desired bit packing formats.
	 *
	 * @param	Seq							Pointer to an Animation Sequence which will contain the bit-packed data .
	 * @param	TargetTranslationFormat		The format to use when encoding translation keys.
	 * @param	TargetRotationFormat		The format to use when encoding rotation keys.
	 * @param	TargetScaleFormat			The format to use when encoding scale keys.	 
	 * @param	TranslationData				Translation Tracks to bit-pack into the Animation Sequence.
	 * @param	RotationData				Rotation Tracks to bit-pack into the Animation Sequence.
	 * @param	ScaleData					Scale Tracks to bit-pack into the Animation Sequence.	 
	 * @param	IncludeKeyTable				true if the compressed data should also contain a table of frame indices for each key. (required by some codecs)
	 */
	static void BitwiseCompressAnimationTracks(
		class UAnimSequence* Seq, 
		AnimationCompressionFormat TargetTranslationFormat, 
		AnimationCompressionFormat TargetRotationFormat,
		AnimationCompressionFormat TargetScaleFormat,
		const TArray<FTranslationTrack>& TranslationData,
		const TArray<FRotationTrack>& RotationData,
		const TArray<FScaleTrack>& ScaleData,
		bool IncludeKeyTable = false);

#if WITH_EDITOR
	FString MakeDDCKey();

protected:
	virtual void PopulateDDCKey(FArchive& Ar);
#endif // WITH_EDITOR
};



