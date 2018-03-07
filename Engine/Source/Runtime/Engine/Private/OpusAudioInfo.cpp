// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "OpusAudioInfo.h"
#include "ContentStreaming.h"
#include "Interfaces/IAudioFormat.h"

#define WITH_OPUS (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX || PLATFORM_XBOXONE)

#if WITH_OPUS
THIRD_PARTY_INCLUDES_START
#include "opus_multistream.h"
THIRD_PARTY_INCLUDES_END
#endif

#define USE_UE4_MEM_ALLOC 1
#define OPUS_MAX_FRAME_SIZE_MS 120

///////////////////////////////////////////////////////////////////////////////////////
// Followed pattern used in opus_multistream_encoder.c - this will allow us to setup //
// a multistream decoder without having to save extra information for every asset.   //
///////////////////////////////////////////////////////////////////////////////////////
struct UnrealChannelLayout{
	int32 NumStreams;
	int32 NumCoupledStreams;
	uint8 Mapping[8];
};

/* Index is NumChannels-1*/
static const UnrealChannelLayout UnrealMappings[8] = {
	{ 1, 0, { 0 } },                      /* 1: mono */
	{ 1, 1, { 0, 1 } },                   /* 2: stereo */
	{ 2, 1, { 0, 1, 2 } },                /* 3: 1-d surround */
	{ 2, 2, { 0, 1, 2, 3 } },             /* 4: quadraphonic surround */
	{ 3, 2, { 0, 1, 4, 2, 3 } },          /* 5: 5-channel surround */
	{ 4, 2, { 0, 1, 4, 5, 2, 3 } },       /* 6: 5.1 surround */
	{ 4, 3, { 0, 1, 4, 6, 2, 3, 5 } },    /* 7: 6.1 surround */
	{ 5, 3, { 0, 1, 6, 7, 2, 3, 4, 5 } }, /* 8: 7.1 surround */
};

/*------------------------------------------------------------------------------------
FOpusDecoderWrapper
------------------------------------------------------------------------------------*/
struct FOpusDecoderWrapper
{
	FOpusDecoderWrapper(uint16 SampleRate, uint8 NumChannels)
	{
#if WITH_OPUS
		check(NumChannels <= 8);
		const UnrealChannelLayout& Layout = UnrealMappings[NumChannels-1];
	#if USE_UE4_MEM_ALLOC
		int32 DecSize = opus_multistream_decoder_get_size(Layout.NumStreams, Layout.NumCoupledStreams);
		Decoder = (OpusMSDecoder*)FMemory::Malloc(DecSize);
		DecError = opus_multistream_decoder_init(Decoder, SampleRate, NumChannels, Layout.NumStreams, Layout.NumCoupledStreams, Layout.Mapping);
	#else
		Decoder = opus_multistream_decoder_create(SampleRate, NumChannels, Layout.NumStreams, Layout.NumCoupledStreams, Layout.Mapping, &DecError);
	#endif
#endif
	}

	~FOpusDecoderWrapper()
	{
#if WITH_OPUS
	#if USE_UE4_MEM_ALLOC
		FMemory::Free(Decoder);
	#else
		opus_multistream_encoder_destroy(Decoder);
	#endif
#endif
	}

	int32 Decode(const uint8* FrameData, uint16 FrameSize, int16* OutPCMData, int32 SampleSize)
	{
#if WITH_OPUS
		return opus_multistream_decode(Decoder, FrameData, FrameSize, OutPCMData, SampleSize, 0);
#else
		return -1;
#endif
	}

	bool WasInitialisedSuccessfully() const
	{
#if WITH_OPUS
		return DecError == OPUS_OK;
#else
		return false;
#endif
	}

private:
#if WITH_OPUS
	OpusMSDecoder* Decoder;
	int32 DecError;
#endif
};

/*------------------------------------------------------------------------------------
FOpusAudioInfo.
------------------------------------------------------------------------------------*/
FOpusAudioInfo::FOpusAudioInfo()
	: OpusDecoderWrapper(nullptr)
{
}

FOpusAudioInfo::~FOpusAudioInfo()
{
	if (OpusDecoderWrapper != nullptr)
	{
		delete OpusDecoderWrapper;
		OpusDecoderWrapper = nullptr;
	}
}

bool FOpusAudioInfo::ParseHeader(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, struct FSoundQualityInfo* QualityInfo)
{
	SrcBufferData = InSrcBufferData;
	SrcBufferDataSize = InSrcBufferDataSize;
	SrcBufferOffset = 0;
	CurrentSampleCount = 0;

	// Read Identifier, True Sample Count, Number of channels and Frames to Encode first
	if (!SrcBufferData || FCStringAnsi::Strcmp((char*)SrcBufferData, OPUS_ID_STRING) != 0)
	{
		return false;
	}
	else
	{
		SrcBufferOffset += FCStringAnsi::Strlen(OPUS_ID_STRING) + 1;
	}

	Read(&SampleRate, sizeof(uint16));
	Read(&TrueSampleCount, sizeof(uint32));
	Read(&NumChannels, sizeof(uint8));

	uint16 SerializedFrames = 0;
	Read(&SerializedFrames, sizeof(uint16));

	// Store the offset to where the audio data begins
	AudioDataOffset = SrcBufferOffset;

	// Write out the the header info
	if (QualityInfo)
	{
		QualityInfo->SampleRate = SampleRate;
		QualityInfo->NumChannels = NumChannels;
		QualityInfo->SampleDataSize = TrueSampleCount * QualityInfo->NumChannels * sizeof(int16);
		QualityInfo->Duration = (float)TrueSampleCount / QualityInfo->SampleRate;
	}

	return true;
}

bool FOpusAudioInfo::CreateDecoder()
{
	check(OpusDecoderWrapper == nullptr);
	OpusDecoderWrapper = new FOpusDecoderWrapper(SampleRate, NumChannels);
	if (!OpusDecoderWrapper->WasInitialisedSuccessfully())
	{
		delete OpusDecoderWrapper;
		OpusDecoderWrapper = nullptr;
		return false;
	}

	return true;
}

int32 FOpusAudioInfo::GetFrameSize()
{
	// Opus format has variable frame size at the head of each frame...
	// We have to assume that the SrcBufferOffset is at the correct location for the read
	uint16 FrameSize = 0;
	Read(&FrameSize, sizeof(uint16));
	return (int32)FrameSize;
}

uint32 FOpusAudioInfo::GetMaxFrameSizeSamples() const
{
	return SampleRate * OPUS_MAX_FRAME_SIZE_MS / 1000;
}

FDecodeResult FOpusAudioInfo::Decode(const uint8* CompressedData, const int32 CompressedDataSize, uint8* OutPCMData, const int32 OutputPCMDataSize)
{
	FDecodeResult Result;

	if (OpusDecoderWrapper)
	{
		const int32 SampleSize = OutputPCMDataSize / NumChannels * sizeof(int16);
		Result.NumAudioFramesProduced = OpusDecoderWrapper->Decode(CompressedData, CompressedDataSize, (int16*)OutPCMData, SampleSize);
	}
	
	return Result;
}

