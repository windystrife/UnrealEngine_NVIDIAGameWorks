// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 
 * A non-UObject based structure used to pass data about a sound
 * node wave around the engine and tools.
 */
struct FSoundQualityInfo
{
	/** Holds the quality value ranging from 1 [poor] to 100 [very good]. */
	int32 Quality;

	/** Holds the number of distinct audio channels. */
	uint32 NumChannels;

	/** Holds the number of PCM samples per second. */
	uint32 SampleRate;

	/** Holds the size of sample data in bytes. */
	uint32 SampleDataSize;

	/** Holds the length of the sound in seconds. */
	float Duration;

	/** Holds a string for debugging purposes. */
	FString DebugName;
};


/**
 * Interface for audio formats.
 */
class IAudioFormat
{
public:

	/**
	 * Checks whether parallel audio cooking is allowed.
	 *
	 * Note: This method is not currently used yet.
	 *
	 * @return true if this audio format can cook in parallel, false otherwise.
	 */
	virtual bool AllowParallelBuild() const
	{
		return false;
	}

	/**
	 * Cooks the source data for the platform and stores the cooked data internally.
	 *
	 * @param Format The desired format.
	 * @param SrcBuffer The source buffer.
	 * @param QualityInfo All the information the compressor needs to compress the audio.
	 * @param OutBuffer Will hold the resulting compressed audio.
	 * @return true on success, false otherwise.
	 */
	virtual bool Cook( FName Format, const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo, TArray<uint8>& OutBuffer ) const = 0;

	/**
	 * Cooks up to 8 mono files into a multi-stream file (e.g. 5.1). The front left channel is required, the rest are optional.
	 *
	 * @param Format The desired format.
	 * @param SrcBuffers The source buffers.
	 * @param QualityInfo All the information the compressor needs to compress the audio
	 * @param OutBuffer Will contain the resulting compressed audio.
	 * @return true if succeeded, false otherwise
	 */
	virtual bool CookSurround( FName Format, const TArray<TArray<uint8> >& SrcBuffers, FSoundQualityInfo& QualityInfo, TArray<uint8>& OutBuffer ) const = 0;

	/**
	 * Gets the list of supported formats.
	 *
	 * @param OutFormats Will contain the list of formats.
	 */
	virtual void GetSupportedFormats( TArray<FName>& OutFormats ) const = 0;

	/**
	 * Gets the current version of the specified audio format.
	 *
	 * @param Format The format to get the version for.
	 * @return Version number.
	 */
	virtual uint16 GetVersion( FName Format ) const = 0;

	/** 
	 * Re-compresses raw PCM to the the platform dependent format, and then back to PCM. Used for quality previewing.
	 *
	 * @param Format The desired format.
	 * @param SrcBuffer Uncompressed PCM data.
	 * @param QualityInfo All the information the compressor needs to compress the audio.
	 * @param OutBuffer Uncompressed PCM data after being compressed.
	 * @return The size of the compressed audio, or 0 on failure.
	 */
	virtual int32 Recompress( FName Format, const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo, TArray<uint8>& OutBuffer ) const = 0;

	/**
	 * Splits compressed data into chunks suitable for streaming audio.
	 *
	 * @param SrcBuffer Pre-compressed data as an array of bytes.
	 * @param OutBuffers Array of buffers that contain the chunks the original data was split into.
	 * @return Whether bulk data could be split for streaming.
	 */
	virtual bool SplitDataForStreaming(const TArray<uint8>& SrcBuffer, TArray<TArray<uint8>>& OutBuffers) const {return false;}

public:

	/** Virtual destructor. */
	virtual ~IAudioFormat() { }
};
