// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimEncoding_ConstantKeyLerp.h: Constant key compression.
=============================================================================*/ 

#pragma once

#include "CoreMinimal.h"
#include "AnimEncoding.h"
#include "AnimationCompression.h"

class FMemoryWriter;

/**
 * Base class for all Animation Encoding Formats using consistently-spaced key interpolation.
 */
class AEFConstantKeyLerpShared : public AnimEncodingLegacyBase
{
public:
	/**
	 * Handles the ByteSwap of compressed rotation data on import
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryReader	The FMemoryReader to read from.
	 * @param	RotTrackData	The compressed rotation data stream.
	 * @param	NumKeysRot		The number of keys present in the stream.
	 */
	virtual void ByteSwapRotationIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		uint8*& RotTrackData,
		int32 NumKeysRot) override;

	/**
	 * Handles the ByteSwap of compressed translation data on import
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryReader	The FMemoryReader to read from.
	 * @param	TransTrackData	The compressed translation data stream.
	 * @param	NumKeysTrans	The number of keys present in the stream.
	 */
	virtual void ByteSwapTranslationIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		uint8*& TransTrackData,
		int32 NumKeysTrans) override;

	/**
	 * Handles the ByteSwap of compressed Scale data on import
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryReader	The FMemoryReader to read from.
	 * @param	ScaleTrackData	The compressed Scale data stream.
	 * @param	NumKeysScale	The number of keys present in the stream.
	 */
	virtual void ByteSwapScaleIn(
		UAnimSequence& Seq, 
		FMemoryReader& MemoryReader,
		uint8*& ScaleTrackData,
		int32 NumKeysScale) override;


	/**
	 * Handles the ByteSwap of compressed rotation data on export
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryWriter	The FMemoryWriter to write to.
	 * @param	RotTrackData	The compressed rotation data stream.
	 * @param	NumKeysRot		The number of keys to write to the stream.
	 */
	virtual void ByteSwapRotationOut(
		UAnimSequence& Seq, 
		FMemoryWriter& MemoryWriter,
		uint8*& RotTrackData,
		int32 NumKeysRot) override;

	/**
	 * Handles the ByteSwap of compressed translation data on export
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryWriter	The FMemoryWriter to write to.
	 * @param	TransTrackData	The compressed translation data stream.
	 * @param	NumKeysTrans	The number of keys to write to the stream.
	 */
	virtual void ByteSwapTranslationOut(
		UAnimSequence& Seq, 
		FMemoryWriter& MemoryWriter,
		uint8*& TransTrackData,
		int32 NumKeysTrans) override;

	/**
	 * Handles the ByteSwap of compressed Scale data on export
	 *
	 * @param	Seq				The UAnimSequence container.
	 * @param	MemoryWriter	The FMemoryWriter to write to.
	 * @param	TransTrackData	The compressed Scale data stream.
	 * @param	NumKeysTrans	The number of keys to write to the stream.
	 */
	virtual void ByteSwapScaleOut(
		UAnimSequence& Seq, 
		FMemoryWriter& MemoryWriter,
		uint8*& ScaleTrackData,
		int32 NumKeysScale) override;
};

template<int32 FORMAT>
class AEFConstantKeyLerp : public AEFConstantKeyLerpShared
{
public:
	/**
	 * Decompress the Rotation component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	Time			Current time to solve for.
	 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
	 * @return					None. 
	 */
	void GetBoneAtomRotation(	
		FTransform& OutAtom,
		const UAnimSequence& Seq,
		const uint8* RESTRICT Stream,
		int32 NumKeys,
		float Time,
		float RelativePos);

	/**
	 * Decompress the Translation component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	Time			Current time to solve for.
	 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
	 * @return					None. 
	 */
	void GetBoneAtomTranslation(	
		FTransform& OutAtom,
		const UAnimSequence& Seq,
		const uint8* RESTRICT Stream,
		int32 NumKeys,
		float Time,
		float RelativePos);

	/**
	 * Decompress the Scale component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	Time			Current time to solve for.
	 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
	 * @return					None. 
	 */
	void GetBoneAtomScale(	
		FTransform& OutAtom,
		const UAnimSequence& Seq,
		const uint8* RESTRICT Stream,
		int32 NumKeys,
		float Time,
		float RelativePos);
#if USE_ANIMATION_CODEC_BATCH_SOLVER

	/**
	 * Decompress all requested rotation components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	Seq				The animation sequence to use.
	 * @param	Time			Current time to solve for.
	 * @return					None. 
	 */
	void GetPoseRotations(	
		FTransformArray& Atoms, 
		const BoneTrackArray& DesiredPairs,
		const UAnimSequence& Seq,
		float RelativePos);

	/**
	 * Decompress all requested translation components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	Seq				The animation sequence to use.
	 * @param	Time			Current time to solve for.
	 * @return					None. 
	 */
	void GetPoseTranslations(	
		FTransformArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		const UAnimSequence& Seq,
		float RelativePos);

	/**
	 * Decompress all requested Scale components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	Seq				The animation sequence to use.
	 * @param	Time			Current time to solve for.
	 * @return					None. 
	 */
	void GetPoseScales(	
		FTransformArray& Atoms,
		const BoneTrackArray& DesiredPairs,
		const UAnimSequence& Seq,
		float RelativePos);
#endif

};



/**
 * Decompress the Rotation component of a BoneAtom
 *
 * @param	OutAtom			The FTransform to fill in.
 * @param	Stream			The compressed animation data.
 * @param	NumKeys			The number of keys present in Stream.
 * @param	Time			Current time to solve for.
 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
 * @return					None. 
 */
template<int32 FORMAT>
FORCEINLINE void AEFConstantKeyLerp<FORMAT>::GetBoneAtomRotation(	
	FTransform& OutAtom,
	const UAnimSequence& Seq,
	const uint8* RESTRICT RotStream,
	int32 NumRotKeys,
	float Time,
	float RelativePos)
{
	if (NumRotKeys == 1)
	{
		// For a rotation track of n=1 keys, the single key is packed as an FQuatFloat96NoW.
		FQuat R0;
		DecompressRotation<ACF_Float96NoW>( R0, RotStream, RotStream );
		OutAtom.SetRotation(R0);
	}
	else
	{
		int32 Index0;
		int32 Index1;
		float Alpha = TimeToIndex(Seq,RelativePos,NumRotKeys,Index0,Index1);

		const int32 RotationStreamOffset = (FORMAT == ACF_IntervalFixed32NoW) ? (sizeof(float)*6) : 0; // offset past Min and Range data

		if (Index0 != Index1)
		{

			// unpack and lerp between the two nearest keys
			const uint8* RESTRICT KeyData0= RotStream + RotationStreamOffset +(Index0*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);
			const uint8* RESTRICT KeyData1= RotStream + RotationStreamOffset +(Index1*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);
			FQuat R0;
			FQuat R1;
			DecompressRotation<FORMAT>( R0, RotStream, KeyData0 );
			DecompressRotation<FORMAT>( R1, RotStream, KeyData1 );

			// Fast linear quaternion interpolation.
			FQuat BlendedQuat = FQuat::FastLerp(R0, R1, Alpha);
			BlendedQuat.Normalize();
			OutAtom.SetRotation( BlendedQuat );
		}
		else // (Index0 == Index1)
		{
			
			// unpack a single key
			const uint8* RESTRICT KeyData= RotStream + RotationStreamOffset +(Index0*CompressedRotationStrides[FORMAT]*CompressedRotationNum[FORMAT]);
			FQuat R0;
			DecompressRotation<FORMAT>( R0, RotStream, KeyData );
			OutAtom.SetRotation( R0 );
		}
	}
}

/**
 * Decompress the Translation component of a BoneAtom
 *
 * @param	OutAtom			The FTransform to fill in.
 * @param	Stream			The compressed animation data.
 * @param	NumKeys			The number of keys present in Stream.
 * @param	Time			Current time to solve for.
 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
 * @return					None. 
 */
template<int32 FORMAT>
FORCEINLINE void AEFConstantKeyLerp<FORMAT>::GetBoneAtomTranslation(	
	FTransform& OutAtom,
	const UAnimSequence& Seq,
	const uint8* RESTRICT TransStream,
	int32 NumTransKeys,
	float Time,
	float RelativePos)
{
	int32 Index0;
	int32 Index1;
	float Alpha = TimeToIndex(Seq,RelativePos,NumTransKeys,Index0,Index1);

	const int32 TransStreamOffset = ((FORMAT == ACF_IntervalFixed32NoW) && NumTransKeys > 1) ? (sizeof(float)*6) : 0; // offset past Min and Range data

	if (Index0 != Index1)
	{
		const uint8* RESTRICT KeyData0 = TransStream + TransStreamOffset + Index0*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		const uint8* RESTRICT KeyData1 = TransStream + TransStreamOffset + Index1*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		FVector P0;
		FVector P1;
		DecompressTranslation<FORMAT>( P0, TransStream, KeyData0 );
		DecompressTranslation<FORMAT>( P1, TransStream, KeyData1 );
		OutAtom.SetTranslation( FMath::Lerp( P0, P1, Alpha ) );
	}
	else // (Index0 == Index1)
	{
		// unpack a single key
		const uint8* RESTRICT KeyData = TransStream + TransStreamOffset + Index0*CompressedTranslationStrides[FORMAT]*CompressedTranslationNum[FORMAT];
		FVector P0;
		DecompressTranslation<FORMAT>( P0, TransStream, KeyData);
		OutAtom.SetTranslation(P0);
	}
}

/**
 * Decompress the Scale component of a BoneAtom
 *
 * @param	OutAtom			The FScaleform to fill in.
 * @param	Stream			The compressed animation data.
 * @param	NumKeys			The number of keys present in Stream.
 * @param	Time			Current time to solve for.
 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
 * @return					None. 
 */
template<int32 FORMAT>
FORCEINLINE void AEFConstantKeyLerp<FORMAT>::GetBoneAtomScale(	
	FTransform& OutAtom,
	const UAnimSequence& Seq,
	const uint8* RESTRICT ScaleStream,
	int32 NumScaleKeys,
	float Time,
	float RelativePos)
{
	int32 Index0;
	int32 Index1;
	float Alpha = TimeToIndex(Seq,RelativePos,NumScaleKeys,Index0,Index1);

	const int32 ScaleStreamOffset = ((FORMAT == ACF_IntervalFixed32NoW) && NumScaleKeys > 1) ? (sizeof(float)*6) : 0; // offset past Min and Range data

	if (Index0 != Index1)
	{
		const uint8* RESTRICT KeyData0 = ScaleStream + ScaleStreamOffset + Index0*CompressedScaleStrides[FORMAT]*CompressedScaleNum[FORMAT];
		const uint8* RESTRICT KeyData1 = ScaleStream + ScaleStreamOffset + Index1*CompressedScaleStrides[FORMAT]*CompressedScaleNum[FORMAT];
		FVector P0;
		FVector P1;
		DecompressScale<FORMAT>( P0, ScaleStream, KeyData0 );
		DecompressScale<FORMAT>( P1, ScaleStream, KeyData1 );
		OutAtom.SetScale3D( FMath::Lerp( P0, P1, Alpha ) );
	}
	else // (Index0 == Index1)
	{
		// unpack a single key
		const uint8* RESTRICT KeyData = ScaleStream + ScaleStreamOffset + Index0*CompressedScaleStrides[FORMAT]*CompressedScaleNum[FORMAT];
		FVector P0;
		DecompressScale<FORMAT>( P0, ScaleStream, KeyData);
		OutAtom.SetScale3D(P0);
	}
}

#if USE_ANIMATION_CODEC_BATCH_SOLVER

/**
 * Decompress all requested rotation components from an Animation Sequence
 *
 * @param	Atoms			The FTransform array to fill in.
 * @param	DesiredPairs	Array of requested bone information
 * @param	Seq				The animation sequence to use.
 * @param	Time			Current time to solve for.
 * @return					None. 
 */
template<int32 FORMAT>
inline void AEFConstantKeyLerp<FORMAT>::GetPoseRotations(	
	FTransformArray& Atoms, 
	const BoneTrackArray& DesiredPairs,
	const UAnimSequence& Seq,
	float Time)
{
	const int32 PairCount = DesiredPairs.Num();
	const float RelativePos = Time / (float)Seq.SequenceLength;

	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		const int32 TrackIndex = Pair.TrackIndex;
		const int32 AtomIndex = Pair.AtomIndex;
		FTransform& BoneAtom = Atoms[AtomIndex];

		const int32* RESTRICT TrackData = Seq.CompressedTrackOffsets.GetData() + (TrackIndex*4);
		const int32 RotKeysOffset	= *(TrackData+2);
		const int32 NumRotKeys	= *(TrackData+3);
		const uint8* RESTRICT RotStream		= Seq.CompressedByteStream.GetData()+RotKeysOffset;

		// call the decoder directly (not through the vtable)
		AEFConstantKeyLerp<FORMAT>::GetBoneAtomRotation(BoneAtom, Seq, RotStream, NumRotKeys, Time, RelativePos);
	}
}

/**
 * Decompress all requested translation components from an Animation Sequence
 *
 * @param	Atoms			The FTransform array to fill in.
 * @param	DesiredPairs	Array of requested bone information
 * @param	Seq				The animation sequence to use.
 * @param	Time			Current time to solve for.
 * @return					None. 
 */
template<int32 FORMAT>
inline void AEFConstantKeyLerp<FORMAT>::GetPoseTranslations(	
	FTransformArray& Atoms, 
	const BoneTrackArray& DesiredPairs,
	const UAnimSequence& Seq,
	float Time)
{
	const int32 PairCount= DesiredPairs.Num();
	const float RelativePos = Time / (float)Seq.SequenceLength;

	//@TODO: Verify that this prefetch is helping
	// Prefetch the desired pairs array and 2 destination spots; the loop will prefetch one 2 out each iteration
	FPlatformMisc::Prefetch(&(DesiredPairs[0]));
	const int32 PrefetchCount = FMath::Min(PairCount, 1);
	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		FPlatformMisc::Prefetch(Atoms.GetData() + Pair.AtomIndex);
	}

	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		int32 PrefetchIndex = PairIndex + PrefetchCount;
		if (PrefetchIndex < PairCount)
		{
			FPlatformMisc::Prefetch(Atoms.GetData() + DesiredPairs[PrefetchIndex].AtomIndex);
		}

		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		const int32 TrackIndex = Pair.TrackIndex;
		const int32 AtomIndex = Pair.AtomIndex;
		FTransform& BoneAtom = Atoms[AtomIndex];

		const int32* RESTRICT TrackData = Seq.CompressedTrackOffsets.GetData() + (TrackIndex*4);
		const int32 TransKeysOffset	= *(TrackData+0);
		const int32 NumTransKeys		= *(TrackData+1);
		const uint8* RESTRICT TransStream = Seq.CompressedByteStream.GetData()+TransKeysOffset;

		// call the decoder directly (not through the vtable)
		AEFConstantKeyLerp<FORMAT>::GetBoneAtomTranslation(BoneAtom, Seq, TransStream, NumTransKeys, Time, RelativePos);
	}
}

/**
 * Decompress all requested Scale components from an Animation Sequence
 *
 * @param	Atoms			The FTransform array to fill in.
 * @param	DesiredPairs	Array of requested bone information
 * @param	Seq				The animation sequence to use.
 * @param	Time			Current time to solve for.
 * @return					None. 
 */
template<int32 FORMAT>
inline void AEFConstantKeyLerp<FORMAT>::GetPoseScales(	
	FTransformArray& Atoms, 
	const BoneTrackArray& DesiredPairs,
	const UAnimSequence& Seq,
	float Time)
{
	check (Seq.CompressedScaleOffsets.IsValid());

	const int32 PairCount= DesiredPairs.Num();
	const float RelativePos = Time / (float)Seq.SequenceLength;

	//@TODO: Verify that this prefetch is helping
	// Prefetch the desired pairs array and 2 destination spots; the loop will prefetch one 2 out each iteration
	FPlatformMisc::Prefetch(&(DesiredPairs[0]));
	const int32 PrefetchCount = FMath::Min(PairCount, 1);
	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		FPlatformMisc::Prefetch(Atoms.GetData() + Pair.AtomIndex);
	}

	for (int32 PairIndex=0; PairIndex<PairCount; ++PairIndex)
	{
		int32 PrefetchIndex = PairIndex + PrefetchCount;
		if (PrefetchIndex < PairCount)
		{
			FPlatformMisc::Prefetch(Atoms.GetData() + DesiredPairs[PrefetchIndex].AtomIndex);
		}

		const BoneTrackPair& Pair = DesiredPairs[PairIndex];
		const int32 TrackIndex = Pair.TrackIndex;
		const int32 AtomIndex = Pair.AtomIndex;
		FTransform& BoneAtom = Atoms[AtomIndex];

		const int32 ScaleKeysOffset		= Seq.CompressedScaleOffsets.GetOffsetData(TrackIndex, 0);
		const int32 NumScaleKeys		= Seq.CompressedScaleOffsets.GetOffsetData(TrackIndex, 1);
		const uint8* RESTRICT ScaleStream = Seq.CompressedByteStream.GetData()+ScaleKeysOffset;

		// call the decoder directly (not through the vtable)
		AEFConstantKeyLerp<FORMAT>::GetBoneAtomScale(BoneAtom, Seq, ScaleStream, NumScaleKeys, Time, RelativePos);
	}
}
#endif
