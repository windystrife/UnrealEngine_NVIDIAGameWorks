// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioDeviceModule.h"
#include "UnrealAudioModule.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	// Forward declare
	class UNREALAUDIO_API IIntermediateBuffer
	{
	public:
		IIntermediateBuffer()
			: NumSamples(0)
			, WriteIndex(0)
			, ReadIndex(0)
		{}

		virtual ~IIntermediateBuffer() {}

		virtual void Initialize(uint32 InNumSamples) = 0;
		virtual bool Write(uint8* Buffer, uint32 Size) = 0;
		virtual bool Read(uint8* Buffer, uint32 Size) = 0;

		static IIntermediateBuffer* CreateIntermediateBuffer(EStreamFormat::Type Format);

	protected:
		uint32 NumSamples;
		uint32 WriteIndex;
		uint32 ReadIndex;
	};

}

#endif // #if ENABLE_UNREAL_AUDIO


