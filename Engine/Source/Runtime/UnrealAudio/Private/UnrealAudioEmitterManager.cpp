// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealAudioEmitterManager.h"
#include "UnrealAudioPrivate.h"

#define UNREAL_AUDIO_EMITTER_COMMAND_QUEUE_SIZE (50)

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	/************************************************************************/
	/* Main Thread Functions												*/
	/************************************************************************/

	FEmitterManager::FEmitterManager(FUnrealAudioModule* InAudioModule)
		: EntityManager(500)
		, AudioModule(InAudioModule)
	{
		check(AudioModule);
	}

	FEmitterManager::~FEmitterManager()
	{
	}

	FEmitterHandle FEmitterManager::CreateEmitter()
	{
		FEmitterHandle EmitterHandle(EntityManager.CreateEntity());

		AudioModule->SendAudioThreadCommand(FCommand(EAudioThreadCommand::EMITTER_CREATE, EmitterHandle));

		return EmitterHandle;
	}

	ESystemError::Type FEmitterManager::ReleaseEmitter(const FEmitterHandle& EmitterHandle)
	{
		EntityManager.ReleaseEntity(EmitterHandle);

		AudioModule->SendAudioThreadCommand(FCommand(EAudioThreadCommand::EMITTER_RELEASE, EmitterHandle.Id));

		return ESystemError::NONE;
	}

	ESystemError::Type FEmitterManager::SetEmitterPosition(const FEmitterHandle& EmitterHandle, const FVector& Position)
	{
		if (EntityManager.IsValidEntity(EmitterHandle))
		{
			return ESystemError::INVALID_HANDLE;
		}

		AudioModule->SendAudioThreadCommand(FCommand(EAudioThreadCommand::EMITTER_SET_POSITION, EmitterHandle, Position.X, Position.Y, Position.Z));

		return ESystemError::NONE;
	}

	bool FEmitterManager::IsValid(const FEmitterHandle& EmitterHandle) const
	{
		return EntityManager.IsValidEntity(EmitterHandle);
	}

	/************************************************************************/
	/* Audio Thread Functions												*/
	/************************************************************************/

	void FEmitterManager::CreateEmitter(const FCommand& Command)
	{
		check(Command.NumArguments == 1);
		check(Command.Arguments[0].DataType == ECommandData::UINT32);

		FEmitterHandle EmitterHandle = FEmitterHandle::Create(Command.Arguments[0].Data.UnsignedInt32);
		uint32 EmitterIndex = EmitterHandle.GetIndex();

		if (EmitterIndex < (uint32)EmitterData.Num())
		{
			FEmitterData& Data = EmitterData[EmitterIndex];
			check(!Data.EmitterHandle.IsInitialized());
			Data.EmitterHandle = EmitterHandle;
		}
		else
		{
			check(EmitterIndex == (uint32)EmitterData.Num());
			EmitterData.Add(FEmitterData(EmitterHandle));
		}
	}

	void FEmitterManager::ReleaseEmitter(const FCommand& Command)
	{
		check(Command.NumArguments == 1);
		check(Command.Arguments[0].DataType == ECommandData::UINT32);

		FEmitterHandle EmitterHandle = FEmitterHandle::Create(Command.Arguments[0].Data.UnsignedInt32);
		uint32 EmitterIndex = EmitterHandle.GetIndex();
		ValidateEmitterEntry(EmitterHandle, EmitterIndex);

		EmitterData[EmitterIndex].EmitterHandle.Id = INDEX_NONE;
	}

	void FEmitterManager::SetEmitterPosition(const FCommand& Command)
	{
		check(Command.NumArguments == 4);
		check(Command.Arguments[0].DataType == ECommandData::UINT32);
		check(Command.Arguments[1].DataType == ECommandData::FLOAT_32);
		check(Command.Arguments[2].DataType == ECommandData::FLOAT_32);
		check(Command.Arguments[3].DataType == ECommandData::FLOAT_32);

		FEmitterHandle EmitterHandle = FEmitterHandle::Create(Command.Arguments[0].Data.UnsignedInt32);

		FVector Position;
		Position.X = Command.Arguments[1].Data.Float32Val;
		Position.Y = Command.Arguments[2].Data.Float32Val;
		Position.Z = Command.Arguments[3].Data.Float32Val;

		uint32 EmitterIndex = EmitterHandle.GetIndex();
		ValidateEmitterEntry(EmitterHandle, EmitterIndex);

		// Make sure we are getting a new position -- this should be checked before sending a command
		check(EmitterData[EmitterIndex].Position != Position);

		// Update the position at the index
		EmitterData[EmitterIndex].Position = Position;
	}

	const FVector* FEmitterManager::GetEmitterPosition(const FEmitterHandle& EmitterHandle) const
	{
		uint32 EmitterIndex = EmitterHandle.GetIndex();
		ValidateEmitterEntry(EmitterHandle, EmitterIndex);
		return &EmitterData[EmitterIndex].Position;
	}

	void FEmitterManager::ValidateEmitterEntry(const FEmitterHandle& EmitterHandle, uint32 EmitterIndex) const
	{
		check(EmitterIndex < (uint32)EmitterData.Num());
		check(EmitterData[EmitterIndex].EmitterHandle.Id == EmitterHandle.Id);
	}

}

#endif // #if ENABLE_UNREAL_AUDIO

