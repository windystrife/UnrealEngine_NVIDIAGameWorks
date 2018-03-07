// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimEncoding_PerTrackCompression.h: Per-track decompressor.
=============================================================================*/ 

#pragma once

#include "CoreMinimal.h"
#include "AnimEncoding.h"

class FMemoryArchive;

/**
 * Decompression codec for the per-track compressor.
 */
class AEFPerTrackCompressionCodec : public AnimEncoding
{
public:
	/**
	 * Handles Byte-swapping incoming animation data from a MemoryReader
	 *
	 * @param	Seq					An Animation Sequence to contain the read data.
	 * @param	MemoryReader		The MemoryReader object to read from.
	 * @param	SourceArVersion		The version of the archive that the data is coming from.
	 */
	virtual void ByteSwapIn(UAnimSequence& Seq, FMemoryReader& MemoryReader) override;

	/**
	 * Handles Byte-swapping outgoing animation data to an array of BYTEs
	 *
	 * @param	Seq					An Animation Sequence to write.
	 * @param	SerializedData		The output buffer.
	 * @param	ForceByteSwapping	true is byte swapping is not optional.
	 */
	virtual void ByteSwapOut(
		UAnimSequence& Seq,
		TArray<uint8>& SerializedData, 
		bool ForceByteSwapping) override;

	/**
	 * Extracts a single BoneAtom from an Animation Sequence.
	 *
	 * @param	OutAtom			The BoneAtom to fill with the extracted result.
	 * @param	Seq				An Animation Sequence to extract the BoneAtom from.
	 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
	 * @param	Time			The time (in seconds) to calculate the BoneAtom for.
	 */
	virtual void GetBoneAtom(
		FTransform& OutAtom,
		const UAnimSequence& Seq,
		int32 TrackIndex,
		float Time) override;

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
	virtual void GetPoseRotations(	
		FTransformArray& Atoms, 
		const BoneTrackArray& DesiredPairs,
		const UAnimSequence& Seq,
		float Time) override;

	/**
	 * Decompress all requested translation components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	Seq				The animation sequence to use.
	 * @param	Time			Current time to solve for.
	 * @return					None. 
	 */
	virtual void GetPoseTranslations(	
		FTransformArray& Atoms, 
		const BoneTrackArray& DesiredPairs,
		const UAnimSequence& Seq,
		float Time) override;

	/**
	 * Decompress all requested Scale components from an Animation Sequence
	 *
	 * @param	Atoms			The FTransform array to fill in.
	 * @param	DesiredPairs	Array of requested bone information
	 * @param	Seq				The animation sequence to use.
	 * @param	Time			Current time to solve for.
	 * @return					None. 
	 */
	virtual void GetPoseScales(	
		FTransformArray& Atoms, 
		const BoneTrackArray& DesiredPairs,
		const UAnimSequence& Seq,
		float Time) override;
#endif

protected:
	/**
	 * Handles Byte-swapping a single track of animation data from a MemoryReader or to a MemoryWriter
	 *
	 * @param	Seq					The Animation Sequence being operated on.
	 * @param	MemoryStream		The MemoryReader or MemoryWriter object to read from/write to.
	 * @param	Offset				The starting offset into the compressed byte stream for this track (can be INDEX_NONE to indicate an identity track)
	 */
	template<class TArchive>
	static void ByteSwapOneTrack(UAnimSequence& Seq, TArchive& MemoryStream, int32 Offset);

	/**
	 * Preserves 4 byte alignment within a stream
	 *
	 * @param	TrackData [inout]	The current data offset (will be returned four byte aligned from the start of the compressed byte stream)
	 * @param	MemoryStream		The MemoryReader or MemoryWriter object to read from/write to.
	 */
	static void PreservePadding(uint8*& TrackData, FMemoryArchive& MemoryStream);

	/**
	 * Decompress the Rotation component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	Seq				The animation sequence to use.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	Time			Current time to solve for.
	 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
	 * @return					None. 
	 */
	static void GetBoneAtomRotation(	
		FTransform& OutAtom,
		const UAnimSequence& Seq,
		int32 Offset,
		float Time,
		float RelativePos);

	/**
	 * Decompress the Translation component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	Seq				The animation sequence to use.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	Time			Current time to solve for.
	 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
	 * @return					None. 
	 */
	static void GetBoneAtomTranslation(	
		FTransform& OutAtom,
		const UAnimSequence& Seq,
		int32 Offset,
		float Time,
		float RelativePos);

	/**
	 * Decompress the Scale component of a BoneAtom
	 *
	 * @param	OutAtom			The FTransform to fill in.
	 * @param	Seq				The animation sequence to use.
	 * @param	Stream			The compressed animation data.
	 * @param	NumKeys			The number of keys present in Stream.
	 * @param	Time			Current time to solve for.
	 * @param	RelativePos		Current position within the animation to solve for in the range [0.0,1.0].
	 * @return					None. 
	 */
	static void GetBoneAtomScale(	
		FTransform& OutAtom,
		const UAnimSequence& Seq,
		int32 Offset,
		float Time,
		float RelativePos);
};

