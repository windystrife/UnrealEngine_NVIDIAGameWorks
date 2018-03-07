// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpusAudioInfo.h: Unreal audio opus decompression interface object.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "AudioDecompress.h"

#define OPUS_ID_STRING "UE4OPUS"

struct FOpusDecoderWrapper;
struct FSoundQualityInfo;

/**
* Helper class to parse opus data
*/
class FOpusAudioInfo : public IStreamedCompressedInfo
{
public:
	ENGINE_API FOpusAudioInfo();
	ENGINE_API virtual ~FOpusAudioInfo();

	//~ Begin IStreamedCompressedInfo Interface
	virtual bool ParseHeader(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, FSoundQualityInfo* QualityInfo) override;
	virtual int32 GetFrameSize() override;
	virtual uint32 GetMaxFrameSizeSamples() const override;
	virtual bool CreateDecoder() override;
	virtual FDecodeResult Decode(const uint8* CompressedData, const int32 CompressedDataSize, uint8* OutPCMData, const int32 OutputPCMDataSize) override;
	//~ End IStreamedCompressedInfo Interface

protected:
	/** Wrapper around Opus-specific decoding state and APIs */
	FOpusDecoderWrapper*	OpusDecoderWrapper;
};
