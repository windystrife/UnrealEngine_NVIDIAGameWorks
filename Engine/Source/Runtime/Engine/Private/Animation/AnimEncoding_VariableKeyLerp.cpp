// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimEncoding_VariableKeyLerp.cpp: Skeletal mesh animation functions.
=============================================================================*/ 

#include "AnimEncoding_VariableKeyLerp.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

/**
 * Handles the ByteSwap of compressed rotation data on import
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryReader	The FMemoryReader to read from.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys present in the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapRotationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapRotationIn(Seq, MemoryReader, TrackData, NumKeys);

	// Load the track table if present
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, TrackData, 4); 

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, EntryStride);
		}
	}
}

/**
 * Handles the ByteSwap of compressed translation data on import
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryReader	The FMemoryReader to read from.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys present in the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapTranslationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapTranslationIn(Seq, MemoryReader, TrackData, NumKeys);

	// Load the track table if present
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, TrackData, 4); 

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, EntryStride);
		}
	}
}

/**
 * Handles the ByteSwap of compressed Scale data on import
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryReader	The FMemoryReader to read from.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys present in the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapScaleIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapScaleIn(Seq, MemoryReader, TrackData, NumKeys);

	// Load the track table if present
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, TrackData, 4); 

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, EntryStride);
		}
	}
}
/**
 * Handles the ByteSwap of compressed rotation data on export
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryWriter	The FMemoryWriter to write to.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys to write to the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapRotationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapRotationOut(Seq, MemoryWriter, TrackData, NumKeys);

	// Store the track table if needed
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryWriter(&MemoryWriter, TrackData, 4);

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, EntryStride);
		}
	}
}

/**
 * Handles the ByteSwap of compressed translation data on export
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryWriter	The FMemoryWriter to write to.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys to write to the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapTranslationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapTranslationOut(Seq, MemoryWriter, TrackData, NumKeys);

	// Store the track table if needed
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryWriter(&MemoryWriter, TrackData, 4);

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, EntryStride);
		}
	}
}


/**
 * Handles the ByteSwap of compressed Scale data on export
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryWriter	The FMemoryWriter to write to.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys to write to the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapScaleOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapScaleOut(Seq, MemoryWriter, TrackData, NumKeys);

	// Store the track table if needed
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryWriter(&MemoryWriter, TrackData, 4);

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(uint16) : sizeof(uint8);
		for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, EntryStride);
		}
	}
}

