// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioUtilities.h"
#include "UnrealAudioHandles.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	/**
	* FEmitter
	* Concrete implementation of IEmitter
	*/
	class FEmitter : public IEmitter
	{
	public:
		FEmitter(class FUnrealAudioModule* ParentModule);
		~FEmitter() override;

		// IEmitter interface
		ESystemError::Type SetPosition(const FVector& InPosition) override;
		ESystemError::Type GetPosition(FVector& OutPosition) override;
		ESystemError::Type Release() override;
		uint32 GetId() const override;

		FEmitterHandle GetHandle() const;

	private:
		/** The current position of the emitter. */
		FVector Position;

		/** Handle to the emitter on the audio thread. */
		FEmitterHandle EmitterHandle;

		/** The parent unreal module instance. */
		FUnrealAudioModule* AudioModule;
	};


	namespace EEmitterCommand
	{
		enum Type
		{
			NONE,			// No emitter command
			CREATE,			// create a new emitter
			RELEASE,		// release the emitter
			SET_POSITION,	// set the emitter position
		};
	}
}

#endif // #if ENABLE_UNREAL_AUDIO
