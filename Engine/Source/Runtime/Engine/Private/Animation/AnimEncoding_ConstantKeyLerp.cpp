// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimEncoding_ConstantKeyLerp.cpp: Skeletal mesh animation functions.
=============================================================================*/ 

#include "AnimEncoding_ConstantKeyLerp.h"
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
void AEFConstantKeyLerpShared::ByteSwapRotationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_Float96NoW : (int32) Seq.RotationCompressionFormat;
	const int32 KeyComponentSize = CompressedRotationStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedRotationNum[EffectiveFormat];

	// Load the bounds if present
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, sizeof(float));
		}
	}

	// Load the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapTranslationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_None : (int32) Seq.TranslationCompressionFormat;
	const int32 KeyComponentSize = CompressedTranslationStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedTranslationNum[EffectiveFormat];

	// Load the bounds if present
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, sizeof(float));
		}
	}

	// Load the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapScaleIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_None : (int32) Seq.ScaleCompressionFormat;
	const int32 KeyComponentSize = CompressedScaleStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedScaleNum[EffectiveFormat];

	// Load the bounds if present
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, sizeof(float));
		}
	}

	// Load the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapRotationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_Float96NoW : (int32) Seq.RotationCompressionFormat;
	const int32 KeyComponentSize = CompressedRotationStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedRotationNum[EffectiveFormat];

	// Store the bounds if needed
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, sizeof(float));
		}
	}

	// Store the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapTranslationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_None : (int32) Seq.TranslationCompressionFormat;
	const int32 KeyComponentSize = CompressedTranslationStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedTranslationNum[EffectiveFormat];

	// Store the bounds if needed
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, sizeof(float));
		}
	}

	// Store the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapScaleOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	uint8*& TrackData,
	int32 NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const int32 EffectiveFormat = (NumKeys == 1) ? ACF_None : (int32) Seq.ScaleCompressionFormat;
	const int32 KeyComponentSize = CompressedScaleStrides[EffectiveFormat];
	const int32 KeyNumComponents = CompressedScaleNum[EffectiveFormat];

	// Store the bounds if needed
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (int32 i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, sizeof(float));
		}
	}

	// Store the keys
	for (int32 KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (int32 i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, KeyComponentSize);
		}
	}
}
