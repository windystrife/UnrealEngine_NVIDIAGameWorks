// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioEmitterInternal.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	/**
	* FEmitterManager
	* Manages emitter handles, messaging, and updating.
	*/
	class FEmitterManager
	{
	public:
		FEmitterManager(FUnrealAudioModule* InAudioModule);
		~FEmitterManager();

		/************************************************************************/
		/* Main Thread Functions												*/
		/************************************************************************/

		FEmitterHandle CreateEmitter();
		ESystemError::Type ReleaseEmitter(const FEmitterHandle& EmitterHandle);
		ESystemError::Type SetEmitterPosition(const FEmitterHandle& EmitterHandle, const FVector& Position);
		bool IsValid(const FEmitterHandle& EmitterHandle) const;

		/************************************************************************/
		/* Audio System Thread Functions										*/
		/************************************************************************/

		void CreateEmitter(const FCommand& Command);
		void ReleaseEmitter(const FCommand& Command);
		void SetEmitterPosition(const FCommand& Command);
		const FVector* GetEmitterPosition(const FEmitterHandle& EmitterHandle) const;

	private:

		void ValidateEmitterEntry(const FEmitterHandle& EmitterHandle, uint32 EmitterIndex) const;

		/** Struct to hold data about emitters in audio system thread. */
		struct FEmitterData
		{
			FEmitterHandle EmitterHandle;
			FVector Position;

			FEmitterData()
				: Position(0)
			{}

			FEmitterData(FEmitterHandle InEmitterHandle)
				: EmitterHandle(InEmitterHandle)
				, Position(0)
			{}
		};

		/** Array of emitter data in audio system thread. */
		TArray<FEmitterData> EmitterData;

		/** Handles emitter entity handles. */
		FEntityManager EntityManager;

		/** Parent audio module for the emitter manager. */
		FUnrealAudioModule* AudioModule;

	};

}

#endif // #if ENABLE_UNREAL_AUDIO
