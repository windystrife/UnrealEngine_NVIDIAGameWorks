// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioEmitter.h"
#include "UnrealAudioPrivate.h"

#define UNREAL_AUDIO_EMITTER_COMMAND_QUEUE_SIZE (50)

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	/************************************************************************/
	/* FEmitter implementation                                              */
	/************************************************************************/

	FEmitter::FEmitter(FUnrealAudioModule* InParentModule)
		: Position(0)
		, AudioModule(InParentModule)
	{
		check(AudioModule != nullptr);
		FEmitterManager& EmitterManager = AudioModule->GetEmitterManager();
		EmitterHandle = EmitterManager.CreateEmitter();
	}

	FEmitter::~FEmitter()
	{
		// Release this emitter if it hasn't already been released
		Release();
	}

	ESystemError::Type FEmitter::SetPosition(const FVector& InPosition)
	{
		if (InPosition != Position)
		{
			Position = InPosition;
			FEmitterManager& EmitterManager = AudioModule->GetEmitterManager();
			return EmitterManager.SetEmitterPosition(EmitterHandle, InPosition);
		}

		return ESystemError::NONE;
	}

	ESystemError::Type FEmitter::GetPosition(FVector& OutPosition)
	{
		OutPosition = Position;
		return ESystemError::NONE;
	}

	ESystemError::Type FEmitter::Release()
	{
		FEmitterManager& EmitterManager = AudioModule->GetEmitterManager();
		if (EmitterHandle.IsInitialized())
		{
			ESystemError::Type Result = EmitterManager.ReleaseEmitter(EmitterHandle);
			EmitterHandle.Id = INDEX_NONE;
			return Result;
		}
		return ESystemError::INVALID_HANDLE;
	}

	uint32 FEmitter::GetId() const
	{
		return EmitterHandle.Id;
	}

	FEmitterHandle FEmitter::GetHandle() const
	{
		return EmitterHandle;
	}


}

#endif // #if ENABLE_UNREAL_AUDIO

