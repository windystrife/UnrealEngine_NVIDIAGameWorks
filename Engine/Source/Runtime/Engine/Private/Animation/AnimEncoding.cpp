// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimEncoding.cpp: Skeletal mesh animation functions.
=============================================================================*/ 

#include "AnimEncoding.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "AnimationCompression.h"

// known codecs
#include "AnimEncoding_ConstantKeyLerp.h"
#include "AnimEncoding_VariableKeyLerp.h"
#include "AnimEncoding_PerTrackCompression.h"

/** Each CompresedTranslationData track's ByteStream will be byte swapped in chunks of this size. */
const int32 CompressedTranslationStrides[ACF_MAX] =
{
	sizeof(float),						// ACF_None					(float X, float Y, float Z)
	sizeof(float),						// ACF_Float96NoW			(float X, float Y, float Z)
	sizeof(float),						// ACF_Fixed48NoW			(Illegal value for translation)
	sizeof(FVectorIntervalFixed32NoW),	// ACF_IntervalFixed32NoW	(compressed to 11-11-10 per-component interval fixed point)
	sizeof(float),						// ACF_Fixed32NoW			(Illegal value for translation)
	sizeof(float),						// ACF_Float32NoW			(Illegal value for translation)
	0									// ACF_Identity
};

/** Number of swapped chunks per element. */
const int32 CompressedTranslationNum[ACF_MAX] =
{
	3,	// ACF_None					(float X, float Y, float Z)
	3,	// ACF_Float96NoW			(float X, float Y, float Z)
	3,	// ACF_Fixed48NoW			(Illegal value for translation)
	1,	// ACF_IntervalFixed32NoW	(compressed to 11-11-10 per-component interval fixed point)
	3,	// ACF_Fixed32NoW			(Illegal value for translation)
	3,	// ACF_Float32NoW			(Illegal value for translation)
	0	// ACF_Identity
};

/** Each CompresedRotationData track's ByteStream will be byte swapped in chunks of this size. */
const int32 CompressedRotationStrides[ACF_MAX] =
{
	sizeof(float),						// ACF_None					(FQuats are serialized per element hence sizeof(float) rather than sizeof(FQuat).
	sizeof(float),						// ACF_Float96NoW			(FQuats with one component dropped and the remaining three uncompressed at 32bit floating point each
	sizeof(uint16),						// ACF_Fixed48NoW			(FQuats with one component dropped and the remaining three compressed to 16-16-16 fixed point.
	sizeof(FQuatIntervalFixed32NoW),	// ACF_IntervalFixed32NoW	(FQuats with one component dropped and the remaining three compressed to 11-11-10 per-component interval fixed point.
	sizeof(FQuatFixed32NoW),			// ACF_Fixed32NoW			(FQuats with one component dropped and the remaining three compressed to 11-11-10 fixed point.
	sizeof(FQuatFloat32NoW),			// ACF_Float32NoW			(FQuats with one component dropped and the remaining three compressed to 11-11-10 floating point.
	0	// ACF_Identity
};

/** Number of swapped chunks per element. */
const int32 CompressedRotationNum[ACF_MAX] =
{
	4,	// ACF_None					(FQuats are serialized per element hence sizeof(float) rather than sizeof(FQuat).
	3,	// ACF_Float96NoW			(FQuats with one component dropped and the remaining three uncompressed at 32bit floating point each
	3,	// ACF_Fixed48NoW			(FQuats with one component dropped and the remaining three compressed to 16-16-16 fixed point.
	1,	// ACF_IntervalFixed32NoW	(FQuats with one component dropped and the remaining three compressed to 11-11-10 per-component interval fixed point.
	1,	// ACF_Fixed32NoW			(FQuats with one component dropped and the remaining three compressed to 11-11-10 fixed point.
	1,  // ACF_Float32NoW			(FQuats with one component dropped and the remaining three compressed to 11-11-10 floating point.
	0	// ACF_Identity
};

/** Number of swapped chunks per element, split out per component (high 3 bits) and flags (low 3 bits)
  *
  * Note: The entry for ACF_IntervalFixed32NoW is special, and actually indicates how many fixed components there are!
  **/
const uint8 PerTrackNumComponentTable[ACF_MAX * 8] =
{
	4,4,4,4,4,4,4,4,	// ACF_None
	3,1,1,2,1,2,2,3,	// ACF_Float96NoW (0 is special, as uncompressed rotation gets 'mis'encoded with 0 instead of 7, so it's treated as a 3; a genuine 0 would use ACF_Identity)
	3,1,1,2,1,2,2,3,	// ACF_Fixed48NoW (ditto)
	6,2,2,4,2,4,4,6,	// ACF_IntervalFixed32NoW (special, indicates number of interval pairs stored in the fixed track)
	1,1,1,1,1,1,1,1,	// ACF_Fixed32NoW
	1,1,1,1,1,1,1,1,	// ACF_Float32NoW
	0,0,0,0,0,0,0,0		// ACF_Identity
};

/** Each CompresedScaleData track's ByteStream will be byte swapped in chunks of this size. */
const int32 CompressedScaleStrides[ACF_MAX] =
{
	sizeof(float),						// ACF_None					(float X, float Y, float Z)
	sizeof(float),						// ACF_Float96NoW			(float X, float Y, float Z)
	sizeof(float),						// ACF_Fixed48NoW			(Illegal value for Scale)
	sizeof(FVectorIntervalFixed32NoW),	// ACF_IntervalFixed32NoW	(compressed to 11-11-10 per-component interval fixed point)
	sizeof(float),						// ACF_Fixed32NoW			(Illegal value for Scale)
	sizeof(float),						// ACF_Float32NoW			(Illegal value for Scale)
	0									// ACF_Identity
};

/** Number of swapped chunks per element. */
const int32 CompressedScaleNum[ACF_MAX] =
{
	3,	// ACF_None					(float X, float Y, float Z)
	3,	// ACF_Float96NoW			(float X, float Y, float Z)
	3,	// ACF_Fixed48NoW			(Illegal value for Scale)
	1,	// ACF_IntervalFixed32NoW	(compressed to 11-11-10 per-component interval fixed point)
	3,	// ACF_Fixed32NoW			(Illegal value for Scale)
	3,	// ACF_Float32NoW			(Illegal value for Scale)
	0	// ACF_Identity
};
/**
 * Compressed translation data will be byte swapped in chunks of this size.
 */
inline int32 GetCompressedTranslationStride(AnimationCompressionFormat TranslationCompressionFormat)
{
	return CompressedTranslationStrides[TranslationCompressionFormat];
}

/**
 * Compressed rotation data will be byte swapped in chunks of this size.
 */
inline int32 GetCompressedRotationStride(AnimationCompressionFormat RotationCompressionFormat)
{
	return CompressedRotationStrides[RotationCompressionFormat];
}

/**
 * Compressed Scale data will be byte swapped in chunks of this size.
 */
inline int32 GetCompressedScaleStride(AnimationCompressionFormat ScaleCompressionFormat)
{
	// @todo fixme change this?
	return CompressedScaleStrides[ScaleCompressionFormat];
}

/**
 * Compressed translation data will be byte swapped in chunks of this size.
 */
inline int32 GetCompressedTranslationStride(const UAnimSequence* Seq)
{
	return CompressedTranslationStrides[static_cast<AnimationCompressionFormat>(Seq->TranslationCompressionFormat)];
}

/**
 * Compressed rotation data will be byte swapped in chunks of this size.
 */
inline int32 GetCompressedRotationStride(const UAnimSequence* Seq)
{
	return CompressedRotationStrides[static_cast<AnimationCompressionFormat>(Seq->RotationCompressionFormat)];
}

/**
 * Compressed Scale data will be byte swapped in chunks of this size.
 */
inline int32 GetCompressedScaleStride(const UAnimSequence* Seq)
{
	// @todo fixme change this?
	return CompressedScaleStrides[static_cast<AnimationCompressionFormat>(Seq->ScaleCompressionFormat)];
}


/**
 * Pads a specified number of bytes to the memory writer to maintain alignment
 */
void PadMemoryWriter(FMemoryWriter* MemoryWriter, uint8*& TrackData, const int32 Alignment)
{
	const PTRINT ByteStreamLoc = (PTRINT) TrackData;
	const int32 Pad = static_cast<int32>( Align( ByteStreamLoc, Alignment ) - ByteStreamLoc );
	const uint8 PadSentinel = 85; // (1<<1)+(1<<3)+(1<<5)+(1<<7)
	
	for ( int32 PadByteIndex = 0; PadByteIndex < Pad; ++PadByteIndex )
	{
		MemoryWriter->Serialize( (void*)&PadSentinel, sizeof(uint8) );
	}
	TrackData += Pad;
}

/**
 * Skips a specified number of bytes in the memory reader to maintain alignment
 */
void PadMemoryReader(FMemoryReader* MemoryReader, uint8*& TrackData, const int32 Alignment)
{
	const PTRINT ByteStreamLoc = (PTRINT) TrackData;
	const int32 Pad = static_cast<int32>( Align( ByteStreamLoc, Alignment ) - ByteStreamLoc );
	MemoryReader->Serialize( TrackData, Pad );
	TrackData += Pad;
}

/**
 * Extracts a single BoneAtom from an Animation Sequence.
 *
 * @param	OutAtom			The BoneAtom to fill with the extracted result.
 * @param	Seq				An Animation Sequence to extract the BoneAtom from.
 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
 * @param	Time			The time (in seconds) to calculate the BoneAtom for.
 */
void AnimationFormat_GetBoneAtom(	
	FTransform& OutAtom,
	const UAnimSequence& Seq,
	int32 TrackIndex,
	float Time)
{
	checkSlow(Seq.RotationCodec != NULL);
	((AnimEncoding*)Seq.RotationCodec)->GetBoneAtom(OutAtom, Seq, TrackIndex, Time);
}

#if USE_ANIMATION_CODEC_BATCH_SOLVER

/**
 * Extracts an array of BoneAtoms from an Animation Sequence representing an entire pose of the skeleton.
 *
 * @param	Atoms				The BoneAtoms to fill with the extracted result.
 * @param	RotationTracks		A BoneTrackArray element for each bone requesting rotation data. 
 * @param	TranslationTracks	A BoneTrackArray element for each bone requesting translation data. 
 * @param	Seq					An Animation Sequence to extract the BoneAtom from.
 * @param	Time				The time (in seconds) to calculate the BoneAtom for.
 */
void AnimationFormat_GetAnimationPose(	
	FTransformArray& Atoms, 
	const BoneTrackArray& RotationPairs,
	const BoneTrackArray& TranslationPairs,
	const BoneTrackArray& ScalePairs,
	const UAnimSequence& Seq,
	float Time)
{
	// decompress the translation component using the proper method
	checkSlow(Seq.TranslationCodec != NULL);
	if (TranslationPairs.Num() > 0)
	{
		((AnimEncoding*)Seq.TranslationCodec)->GetPoseTranslations(Atoms, TranslationPairs, Seq, Time);
	}

	// decompress the rotation component using the proper method
	checkSlow(Seq.RotationCodec != NULL);
	((AnimEncoding*)Seq.RotationCodec)->GetPoseRotations(Atoms, RotationPairs, Seq, Time);

	checkSlow(Seq.ScaleCodec != NULL);
	// we allow scale key to be empty
	if (Seq.CompressedScaleOffsets.IsValid())
	{
		((AnimEncoding*)Seq.ScaleCodec)->GetPoseScales(Atoms, ScalePairs, Seq, Time);
	}
}
#endif

/**
 * Extracts a single BoneAtom from an Animation Sequence.
 *
 * @param	OutAtom			The BoneAtom to fill with the extracted result.
 * @param	Seq				An Animation Sequence to extract the BoneAtom from.
 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
 * @param	Time			The time (in seconds) to calculate the BoneAtom for.
 */
void AnimEncodingLegacyBase::GetBoneAtom(
	FTransform& OutAtom,
	const UAnimSequence& Seq,
	int32 TrackIndex,
	float Time)
{
	// Initialize to identity to set the scale and in case of a missing rotation or translation codec
	OutAtom.SetIdentity();

	// Use the CompressedTrackOffsets stream to find the data addresses
	const int32* RESTRICT TrackData= Seq.CompressedTrackOffsets.GetData() + (TrackIndex*4);
	int32 TransKeysOffset = *(TrackData+0);
	int32 NumTransKeys	= *(TrackData+1);
	int32 RotKeysOffset	= *(TrackData+2);
	int32 NumRotKeys		= *(TrackData+3);
	const uint8* RESTRICT TransStream	= Seq.CompressedByteStream.GetData()+TransKeysOffset;
	const uint8* RESTRICT RotStream		= Seq.CompressedByteStream.GetData()+RotKeysOffset;

	const float RelativePos = Time / (float)Seq.SequenceLength;

	// decompress the translation component using the proper method
	checkSlow(Seq.TranslationCodec != NULL);
	((AnimEncodingLegacyBase*)Seq.TranslationCodec)->GetBoneAtomTranslation(OutAtom, Seq, TransStream, NumTransKeys, Time, RelativePos);

	// decompress the rotation component using the proper method
	checkSlow(Seq.RotationCodec != NULL);
	((AnimEncodingLegacyBase*)Seq.RotationCodec)->GetBoneAtomRotation(OutAtom, Seq, RotStream, NumRotKeys, Time, RelativePos);

	// we assume scale keys can be empty, so only extrace if we have valid keys
	bool bHasValidScale = Seq.CompressedScaleOffsets.IsValid();
	if (bHasValidScale)
	{
		int32 ScaleKeyOffset = Seq.CompressedScaleOffsets.GetOffsetData(TrackIndex, 0);
		int32 NumScaleKeys = Seq.CompressedScaleOffsets.GetOffsetData(TrackIndex, 1);
		const uint8* RESTRICT ScaleStream		= Seq.CompressedByteStream.GetData()+ScaleKeyOffset;
		// decompress the rotation component using the proper method
		checkSlow(Seq.ScaleCodec != NULL);
		((AnimEncodingLegacyBase*)Seq.ScaleCodec)->GetBoneAtomScale(OutAtom, Seq, ScaleStream, NumScaleKeys, Time, RelativePos);
	}
}


/**
 * Handles Byte-swapping incoming animation data from a MemoryReader
 *
 * @param	Seq					An Animation Sequence to contain the read data.
 * @param	MemoryReader		The MemoryReader object to read from.
 */
//@todo.VC10: Apparent VC10 compiler bug here causes an access violation in optimized builds
#ifdef _MSC_VER
	PRAGMA_DISABLE_OPTIMIZATION
#endif
void AnimEncodingLegacyBase::ByteSwapIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader)
{
	const int32 NumTracks = Seq.CompressedTrackOffsets.Num() / 4;

	int32 OriginalNumBytes = MemoryReader.TotalSize();
	Seq.CompressedByteStream.Empty(OriginalNumBytes);
	Seq.CompressedByteStream.AddUninitialized(OriginalNumBytes);

	// Read and swap
	uint8* StreamBase = Seq.CompressedByteStream.GetData();
	bool bHasValidScale = Seq.CompressedScaleOffsets.IsValid();

	for ( int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		const int32 OffsetTrans	= Seq.CompressedTrackOffsets[TrackIndex*4+0];
		const int32 NumKeysTrans	= Seq.CompressedTrackOffsets[TrackIndex*4+1];
		const int32 OffsetRot		= Seq.CompressedTrackOffsets[TrackIndex*4+2];
		const int32 NumKeysRot	= Seq.CompressedTrackOffsets[TrackIndex*4+3];

		// Translation data.
		checkSlow( (OffsetTrans % 4) == 0 && "CompressedByteStream not aligned to four bytes" );
		uint8* TransTrackData = StreamBase + OffsetTrans;
		checkSlow(Seq.TranslationCodec != NULL);
		((AnimEncodingLegacyBase*)Seq.TranslationCodec)->ByteSwapTranslationIn(Seq, MemoryReader, TransTrackData, NumKeysTrans);

		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, TransTrackData, 4); 

		// Rotation data.
		checkSlow( (OffsetRot % 4) == 0 && "CompressedByteStream not aligned to four bytes" );
		uint8* RotTrackData = StreamBase + OffsetRot;
		checkSlow(Seq.RotationCodec != NULL);
		((AnimEncodingLegacyBase*)Seq.RotationCodec)->ByteSwapRotationIn(Seq, MemoryReader, RotTrackData, NumKeysRot);

		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, RotTrackData, 4); 

		if (bHasValidScale)
		{
			const int32 OffsetScale		= Seq.CompressedScaleOffsets.GetOffsetData(TrackIndex, 0);
			const int32 NumKeysScale		= Seq.CompressedScaleOffsets.GetOffsetData(TrackIndex, 1);

			// Scale data.
			checkSlow( (OffsetScale % 4) == 0 && "CompressedByteStream not aligned to four bytes" );
			uint8* ScaleTrackData = StreamBase + OffsetScale;
			checkSlow(Seq.ScaleCodec != NULL);
			((AnimEncodingLegacyBase*)Seq.ScaleCodec)->ByteSwapScaleIn(Seq, MemoryReader, ScaleTrackData, NumKeysScale);

			// Like the compressed byte stream, pad the serialization stream to four bytes.
			// As a sanity check, each pad byte can be checked to be the PadSentinel.
			PadMemoryReader(&MemoryReader, ScaleTrackData, 4); 
		}
	}
}

#ifdef _MSC_VER
PRAGMA_ENABLE_OPTIMIZATION
#endif


/**
 * Handles Byte-swapping outgoing animation data to an array of BYTEs
 *
 * @param	Seq					An Animation Sequence to write.
 * @param	SerializedData		The output buffer.
 * @param	ForceByteSwapping	true is byte swapping is not optional.
 */
void AnimEncodingLegacyBase::ByteSwapOut(
	UAnimSequence& Seq,
	TArray<uint8>& SerializedData, 
	bool ForceByteSwapping)
{
	FMemoryWriter MemoryWriter( SerializedData, true );
	MemoryWriter.SetByteSwapping( ForceByteSwapping );

	uint8* StreamBase		= Seq.CompressedByteStream.GetData();
	const int32 NumTracks		= Seq.CompressedTrackOffsets.Num()/4;

	bool bHasValidScale = Seq.CompressedScaleOffsets.IsValid();

	for ( int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		const int32 OffsetTrans	= Seq.CompressedTrackOffsets[TrackIndex*4];
		const int32 NumKeysTrans	= Seq.CompressedTrackOffsets[TrackIndex*4+1];
		const int32 OffsetRot		= Seq.CompressedTrackOffsets[TrackIndex*4+2];
		const int32 NumKeysRot	= Seq.CompressedTrackOffsets[TrackIndex*4+3];

		// Translation data.
		checkSlow( (OffsetTrans % 4) == 0 && "CompressedByteStream not aligned to four bytes" );
		uint8* TransTrackData = StreamBase + OffsetTrans;
		if (Seq.TranslationCodec != NULL)
		{
			((AnimEncodingLegacyBase*)Seq.TranslationCodec)->ByteSwapTranslationOut(Seq, MemoryWriter, TransTrackData, NumKeysTrans);
		}
		else
		{
			UE_LOG(LogAnimationCompression, Fatal, TEXT("%i: unknown or unsupported animation format"), (int32)Seq.KeyEncodingFormat );
		};

		// Like the compressed byte stream, pad the serialization stream to four bytes.
		PadMemoryWriter(&MemoryWriter, TransTrackData, 4);

		// Rotation data.
		checkSlow( (OffsetRot % 4) == 0 && "CompressedByteStream not aligned to four bytes" );
		uint8* RotTrackData = StreamBase + OffsetRot;
		checkSlow(Seq.RotationCodec != NULL);
		((AnimEncodingLegacyBase*)Seq.RotationCodec)->ByteSwapRotationOut(Seq, MemoryWriter, RotTrackData, NumKeysRot);

		// Like the compressed byte stream, pad the serialization stream to four bytes.
		PadMemoryWriter(&MemoryWriter, RotTrackData, 4);

		if (bHasValidScale)
		{
			const int32 OffsetScale	= Seq.CompressedScaleOffsets.GetOffsetData(TrackIndex, 0);
			const int32 NumKeysScale	= Seq.CompressedScaleOffsets.GetOffsetData(TrackIndex, 1);

			// Scale data.
			checkSlow( (OffsetScale % 4) == 0 && "CompressedByteStream not aligned to four bytes" );
			uint8* ScaleTrackData = StreamBase + OffsetScale;
			if (Seq.ScaleCodec != NULL)
			{
				((AnimEncodingLegacyBase*)Seq.ScaleCodec)->ByteSwapScaleOut(Seq, MemoryWriter, ScaleTrackData, NumKeysScale);
			}
			else
			{
				UE_LOG(LogAnimationCompression, Fatal, TEXT("%i: unknown or unsupported animation format"), (int32)Seq.KeyEncodingFormat );
			};

			// Like the compressed byte stream, pad the serialization stream to four bytes.
			PadMemoryWriter(&MemoryWriter, ScaleTrackData, 4);
		}
	}
}

/**
 * Extracts statistics about a given Animation Sequence
 *
 * @param	Seq					An Animation Sequence.
 * @param	NumTransTracks		The total number of Translation Tracks found.
 * @param	NumRotTracks		The total number of Rotation Tracks found.
 * @param	TotalNumTransKeys	The total number of Translation Keys found.
 * @param	TotalNumRotKeys		The total number of Rotation Keys found.
 * @param	TranslationKeySize	The average size (in BYTES) of a single Translation Key.
 * @param	RotationKeySize		The average size (in BYTES) of a single Rotation Key.
 * @param	OverheadSize		The size (in BYTES) of overhead (offsets, scale tables, key->frame lookups, etc...)
 * @param	NumTransTracksWithOneKey	The total number of Translation Tracks found containing a single key.
 * @param	NumRotTracksWithOneKey		The total number of Rotation Tracks found containing a single key.
*/
void AnimationFormat_GetStats(	
	const UAnimSequence* Seq, 
	int32& NumTransTracks,
	int32& NumRotTracks,
	int32& NumScaleTracks,
	int32& TotalNumTransKeys,
	int32& TotalNumRotKeys,
	int32& TotalNumScaleKeys,
	float& TranslationKeySize,
	float& RotationKeySize,
	float& ScaleKeySize,
	int32& OverheadSize,
	int32& NumTransTracksWithOneKey,
	int32& NumRotTracksWithOneKey, 
	int32& NumScaleTracksWithOneKey)
{
	if (Seq)
	{
		OverheadSize = Seq->CompressedTrackOffsets.Num() * sizeof(int32);
		const size_t KeyFrameLookupSize = (Seq->NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);

		if (Seq->KeyEncodingFormat != AKF_PerTrackCompression)
		{
			const int32 TransStride	= GetCompressedTranslationStride(Seq);
			const int32 RotStride		= GetCompressedRotationStride(Seq);
			const int32 ScaleStride		= GetCompressedScaleStride(Seq);
			const int32 TransNum		= CompressedTranslationNum[Seq->TranslationCompressionFormat];
			const int32 RotNum		= CompressedRotationNum[Seq->RotationCompressionFormat];
			const int32 ScaleNum		= CompressedScaleNum[Seq->ScaleCompressionFormat];

			TranslationKeySize = TransStride * TransNum;
			RotationKeySize = RotStride * RotNum;
			ScaleKeySize = ScaleStride * ScaleNum;

			// Track number of tracks.
			NumTransTracks	= Seq->CompressedTrackOffsets.Num()/4;
			NumRotTracks	= Seq->CompressedTrackOffsets.Num()/4;
			NumScaleTracks	= Seq->CompressedScaleOffsets.GetNumTracks();

			// Track total number of keys.
			TotalNumTransKeys = 0;
			TotalNumRotKeys = 0;
			TotalNumScaleKeys = 0;

			// Track number of tracks with a single key.
			NumTransTracksWithOneKey = 0;
			NumRotTracksWithOneKey = 0;
			NumScaleTracksWithOneKey = 0;

			// Translation.
			for ( int32 TrackIndex = 0; TrackIndex < NumTransTracks; ++TrackIndex )
			{
				const int32 NumKeys = Seq->CompressedTrackOffsets[TrackIndex*4+1];
				TotalNumTransKeys += NumKeys;
				if ( NumKeys == 1 )
				{
					++NumTransTracksWithOneKey;
				}
				else
				{
					OverheadSize += (Seq->KeyEncodingFormat == AKF_VariableKeyLerp) ? NumKeys * KeyFrameLookupSize : 0;
				}
			}

			// Rotation.
			for ( int32 TrackIndex = 0; TrackIndex < NumRotTracks; ++TrackIndex )
			{
				const int32 NumKeys = Seq->CompressedTrackOffsets[TrackIndex*4+3];
				TotalNumRotKeys += NumKeys;
				if ( NumKeys == 1 )
				{
					++NumRotTracksWithOneKey;
				}
				else
				{
					OverheadSize += (Seq->KeyEncodingFormat == AKF_VariableKeyLerp) ? NumKeys * KeyFrameLookupSize : 0;
				}
			}

			// Scale.
			for ( int32 ScaleIndex = 0; ScaleIndex < NumScaleTracks; ++ScaleIndex )
			{
				const int32 NumKeys = Seq->CompressedScaleOffsets.GetOffsetData(ScaleIndex, 1);
				TotalNumScaleKeys += NumKeys;
				if ( NumKeys == 1 )
				{
					++NumScaleTracksWithOneKey;
				}
				else
				{
					OverheadSize += (Seq->KeyEncodingFormat == AKF_VariableKeyLerp) ? NumKeys * KeyFrameLookupSize : 0;
				}
			}

			// Add in scaling values (min+range for interval encoding)
			OverheadSize += (Seq->RotationCompressionFormat == ACF_IntervalFixed32NoW) ? (NumRotTracks - NumRotTracksWithOneKey) * sizeof(float) * 6 : 0;
			OverheadSize += (Seq->TranslationCompressionFormat == ACF_IntervalFixed32NoW) ? (NumTransTracks - NumTransTracksWithOneKey) * sizeof(float) * 6 : 0;
			OverheadSize += (Seq->ScaleCompressionFormat == ACF_IntervalFixed32NoW) ? (NumScaleTracks - NumScaleTracksWithOneKey) * sizeof(float) * 6 : 0;
		}
		else
		{
			int32 TotalTransKeysThatContributedSize = 0;
			int32 TotalRotKeysThatContributedSize = 0;
			int32 TotalScaleKeysThatContributedSize = 0;

			TranslationKeySize = 0;
			RotationKeySize = 0;
			ScaleKeySize = 0;

			// Track number of tracks.
			NumTransTracks = Seq->CompressedTrackOffsets.Num() / 2;
			NumRotTracks = Seq->CompressedTrackOffsets.Num() / 2;
			// should be without divided by 2
			NumScaleTracks = Seq->CompressedScaleOffsets.GetNumTracks();

			// Track total number of keys.
			TotalNumTransKeys = 0;
			TotalNumRotKeys = 0;
			TotalNumScaleKeys = 0;

			// Track number of tracks with a single key.
			NumTransTracksWithOneKey = 0;
			NumRotTracksWithOneKey = 0;
			NumScaleTracksWithOneKey = 0;

			// Translation.
			for (int32 TrackIndex = 0; TrackIndex < NumTransTracks; ++TrackIndex)
			{
				const int32 TransOffset = Seq->CompressedTrackOffsets[TrackIndex*2+0];
				if (TransOffset == INDEX_NONE)
				{
					++TotalNumTransKeys;
					++NumTransTracksWithOneKey;
				}
				else
				{
					const int32 Header = *((int32*)(Seq->CompressedByteStream.GetData() + TransOffset));

					int32 KeyFormat;
					int32 FormatFlags;
					int32 TrackBytesPerKey;
					int32 TrackFixedBytes;
					int32 NumKeys;

					FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/ TrackBytesPerKey, /*OUT*/ TrackFixedBytes);
					TranslationKeySize += TrackBytesPerKey * NumKeys;
					TotalTransKeysThatContributedSize += NumKeys;
					OverheadSize += TrackFixedBytes;
					OverheadSize += ((FormatFlags & 0x08) != 0) ? NumKeys * KeyFrameLookupSize : 0;
					
					TotalNumTransKeys += NumKeys;
					if (NumKeys <= 1)
					{
						++NumTransTracksWithOneKey;
					}
				}
			}

			// Rotation.
			for (int32 TrackIndex = 0; TrackIndex < NumRotTracks; ++TrackIndex)
			{
				const int32 RotOffset = Seq->CompressedTrackOffsets[TrackIndex*2+1];
				if (RotOffset == INDEX_NONE)
				{
					++TotalNumRotKeys;
					++NumRotTracksWithOneKey;
				}
				else
				{
					const int32 Header = *((int32*)(Seq->CompressedByteStream.GetData() + RotOffset));

					int32 KeyFormat;
					int32 FormatFlags;
					int32 TrackBytesPerKey;
					int32 TrackFixedBytes;
					int32 NumKeys;

					FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/ TrackBytesPerKey, /*OUT*/ TrackFixedBytes);
					RotationKeySize += TrackBytesPerKey * NumKeys;
					TotalRotKeysThatContributedSize += NumKeys;
					OverheadSize += TrackFixedBytes;
					OverheadSize += ((FormatFlags & 0x08) != 0) ? NumKeys * KeyFrameLookupSize : 0;

					TotalNumRotKeys += NumKeys;
					if (NumKeys <= 1)
					{
						++NumRotTracksWithOneKey;
					}
				}
			}

			// Scale.
			for (int32 TrackIndex = 0; TrackIndex < NumScaleTracks; ++TrackIndex)
			{
				const int32 ScaleOffset = Seq->CompressedScaleOffsets.GetOffsetData(TrackIndex, 0);
				if (ScaleOffset == INDEX_NONE)
				{
					++TotalNumScaleKeys;
					++NumScaleTracksWithOneKey;
				}
				else
				{
					const int32 Header = *((int32*)(Seq->CompressedByteStream.GetData() + ScaleOffset));

					int32 KeyFormat;
					int32 FormatFlags;
					int32 TrackBytesPerKey;
					int32 TrackFixedBytes;
					int32 NumKeys;

					FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/ TrackBytesPerKey, /*OUT*/ TrackFixedBytes);
					ScaleKeySize += TrackBytesPerKey * NumKeys;
					TotalScaleKeysThatContributedSize += NumKeys;
					OverheadSize += TrackFixedBytes;
					OverheadSize += ((FormatFlags & 0x08) != 0) ? NumKeys * KeyFrameLookupSize : 0;

					TotalNumScaleKeys += NumKeys;
					if (NumKeys <= 1)
					{
						++NumScaleTracksWithOneKey;
					}
				}
			}

			// Average key sizes
			if (TotalRotKeysThatContributedSize > 0)
			{
				RotationKeySize = RotationKeySize / TotalRotKeysThatContributedSize;
			}

			if (TotalTransKeysThatContributedSize > 0)
			{
				TranslationKeySize = TranslationKeySize / TotalTransKeysThatContributedSize;
			}

			if (TotalScaleKeysThatContributedSize > 0)
			{
				ScaleKeySize = ScaleKeySize / TotalScaleKeysThatContributedSize;
			}
		}
	}
}

/**
 * Sets the internal Animation Codec Interface Links within an Animation Sequence
 *
 * @param	Seq					An Animation Sequence to setup links within.
*/
void AnimationFormat_SetInterfaceLinks(UAnimSequence& Seq)
{
	Seq.TranslationCodec = NULL;
	Seq.RotationCodec = NULL;
	Seq.ScaleCodec = NULL;

	if (Seq.KeyEncodingFormat == AKF_ConstantKeyLerp)
	{
		static AEFConstantKeyLerp<ACF_None>					AEFConstantKeyLerp_None;
		static AEFConstantKeyLerp<ACF_Float96NoW>			AEFConstantKeyLerp_Float96NoW;
		static AEFConstantKeyLerp<ACF_Fixed48NoW>			AEFConstantKeyLerp_Fixed48NoW;
		static AEFConstantKeyLerp<ACF_IntervalFixed32NoW>	AEFConstantKeyLerp_IntervalFixed32NoW;
		static AEFConstantKeyLerp<ACF_Fixed32NoW>			AEFConstantKeyLerp_Fixed32NoW;
		static AEFConstantKeyLerp<ACF_Float32NoW>			AEFConstantKeyLerp_Float32NoW;
		static AEFConstantKeyLerp<ACF_Identity>				AEFConstantKeyLerp_Identity;

		// setup translation codec
		switch(Seq.TranslationCompressionFormat)
		{
			case ACF_None:
				Seq.TranslationCodec = &AEFConstantKeyLerp_None;
				break;
			case ACF_Float96NoW:
				Seq.TranslationCodec = &AEFConstantKeyLerp_Float96NoW;
				break;
			case ACF_IntervalFixed32NoW:
				Seq.TranslationCodec = &AEFConstantKeyLerp_IntervalFixed32NoW;
				break;
			case ACF_Identity:
				Seq.TranslationCodec = &AEFConstantKeyLerp_Identity;
				break;

			default:
				UE_LOG(LogAnimationCompression, Fatal, TEXT("%i: unknown or unsupported translation compression"), (int32)Seq.TranslationCompressionFormat );
		};

		// setup rotation codec
		switch(Seq.RotationCompressionFormat)
		{
			case ACF_None:
				Seq.RotationCodec = &AEFConstantKeyLerp_None;
				break;
			case ACF_Float96NoW:
				Seq.RotationCodec = &AEFConstantKeyLerp_Float96NoW;
				break;
			case ACF_Fixed48NoW:
				Seq.RotationCodec = &AEFConstantKeyLerp_Fixed48NoW;
				break;
			case ACF_IntervalFixed32NoW:
				Seq.RotationCodec = &AEFConstantKeyLerp_IntervalFixed32NoW;
				break;
			case ACF_Fixed32NoW:
				Seq.RotationCodec = &AEFConstantKeyLerp_Fixed32NoW;
				break;
			case ACF_Float32NoW:
				Seq.RotationCodec = &AEFConstantKeyLerp_Float32NoW;
				break;
			case ACF_Identity:
				Seq.RotationCodec = &AEFConstantKeyLerp_Identity;
				break;

			default:
				UE_LOG(LogAnimationCompression, Fatal, TEXT("%i: unknown or unsupported rotation compression"), (int32)Seq.RotationCompressionFormat );
		};

		// setup Scale codec
		switch(Seq.ScaleCompressionFormat)
		{
		case ACF_None:
			Seq.ScaleCodec = &AEFConstantKeyLerp_None;
			break;
		case ACF_Float96NoW:
			Seq.ScaleCodec = &AEFConstantKeyLerp_Float96NoW;
			break;
		case ACF_IntervalFixed32NoW:
			Seq.ScaleCodec = &AEFConstantKeyLerp_IntervalFixed32NoW;
			break;
		case ACF_Identity:
			Seq.ScaleCodec = &AEFConstantKeyLerp_Identity;
			break;

		default:
			UE_LOG(LogAnimationCompression, Fatal, TEXT("%i: unknown or unsupported Scale compression"), (int32)Seq.ScaleCompressionFormat );
		};
	}
	else if (Seq.KeyEncodingFormat == AKF_VariableKeyLerp)
	{
		static AEFVariableKeyLerp<ACF_None>					AEFVariableKeyLerp_None;
		static AEFVariableKeyLerp<ACF_Float96NoW>			AEFVariableKeyLerp_Float96NoW;
		static AEFVariableKeyLerp<ACF_Fixed48NoW>			AEFVariableKeyLerp_Fixed48NoW;
		static AEFVariableKeyLerp<ACF_IntervalFixed32NoW>	AEFVariableKeyLerp_IntervalFixed32NoW;
		static AEFVariableKeyLerp<ACF_Fixed32NoW>			AEFVariableKeyLerp_Fixed32NoW;
		static AEFVariableKeyLerp<ACF_Float32NoW>			AEFVariableKeyLerp_Float32NoW;
		static AEFVariableKeyLerp<ACF_Identity>				AEFVariableKeyLerp_Identity;

		// setup translation codec
		switch(Seq.TranslationCompressionFormat)
		{
			case ACF_None:
				Seq.TranslationCodec = &AEFVariableKeyLerp_None;
				break;
			case ACF_Float96NoW:
				Seq.TranslationCodec = &AEFVariableKeyLerp_Float96NoW;
				break;
			case ACF_IntervalFixed32NoW:
				Seq.TranslationCodec = &AEFVariableKeyLerp_IntervalFixed32NoW;
				break;
			case ACF_Identity:
				Seq.TranslationCodec = &AEFVariableKeyLerp_Identity;
				break;

			default:
				UE_LOG(LogAnimationCompression, Fatal, TEXT("%i: unknown or unsupported translation compression"), (int32)Seq.TranslationCompressionFormat );
		};

		// setup rotation codec
		switch(Seq.RotationCompressionFormat)
		{
			case ACF_None:
				Seq.RotationCodec = &AEFVariableKeyLerp_None;
				break;
			case ACF_Float96NoW:
				Seq.RotationCodec = &AEFVariableKeyLerp_Float96NoW;
				break;
			case ACF_Fixed48NoW:
				Seq.RotationCodec = &AEFVariableKeyLerp_Fixed48NoW;
				break;
			case ACF_IntervalFixed32NoW:
				Seq.RotationCodec = &AEFVariableKeyLerp_IntervalFixed32NoW;
				break;
			case ACF_Fixed32NoW:
				Seq.RotationCodec = &AEFVariableKeyLerp_Fixed32NoW;
				break;
			case ACF_Float32NoW:
				Seq.RotationCodec = &AEFVariableKeyLerp_Float32NoW;
				break;
			case ACF_Identity:
				Seq.RotationCodec = &AEFVariableKeyLerp_Identity;
				break;

			default:
				UE_LOG(LogAnimationCompression, Fatal, TEXT("%i: unknown or unsupported rotation compression"), (int32)Seq.RotationCompressionFormat );
		};

		// setup Scale codec
		switch(Seq.ScaleCompressionFormat)
		{
		case ACF_None:
			Seq.ScaleCodec = &AEFVariableKeyLerp_None;
			break;
		case ACF_Float96NoW:
			Seq.ScaleCodec = &AEFVariableKeyLerp_Float96NoW;
			break;
		case ACF_IntervalFixed32NoW:
			Seq.ScaleCodec = &AEFVariableKeyLerp_IntervalFixed32NoW;
			break;
		case ACF_Identity:
			Seq.ScaleCodec = &AEFVariableKeyLerp_Identity;
			break;

		default:
			UE_LOG(LogAnimationCompression, Fatal, TEXT("%i: unknown or unsupported Scale compression"), (int32)Seq.ScaleCompressionFormat );
		};
	}
	else if (Seq.KeyEncodingFormat == AKF_PerTrackCompression)
	{
		static AEFPerTrackCompressionCodec StaticCodec;

		Seq.RotationCodec = &StaticCodec;
		Seq.TranslationCodec = &StaticCodec;
		Seq.ScaleCodec = &StaticCodec;

		check(Seq.RotationCompressionFormat == ACF_Identity);
		check(Seq.TranslationCompressionFormat == ACF_Identity);
		// commenting out scale check because the older versions won't have this set correctly
		// and I can't get the version VER_UE4_ANIM_SUPPORT_NONUNIFORM_SCALE_ANIMATION here because this function
		// is called in Serialize where GetLinker is too early to call
		//checkf(Seq.ScaleCompressionFormat == ACF_Identity);
	}
	else
	{
		UE_LOG(LogAnimationCompression, Fatal, TEXT("%i: unknown or unsupported animation format"), (int32)Seq.KeyEncodingFormat );
	}
}
