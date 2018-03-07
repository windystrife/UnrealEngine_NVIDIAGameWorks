// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UnrealAudioTypes.h"

namespace UAudio
{
	/** IEmitter interface. */
	class UNREALAUDIO_API IEmitter
	{
	public:
		/** Destructor. */
		virtual ~IEmitter() {}

		/**
		 * Sets the emitter's position.
		 *
		 * @param	InPosition	The position to set the emitter to.
		 *
		 * @return	An ESystemError::Type.
		 */
		virtual ESystemError::Type SetPosition(const FVector& InPosition) = 0;

		/**
		 * Gets the emitter's position.
		 *
		 * @param [out]		OutPosition	The out position.
		 *
		 * @return	An ESystemError::Type.
		 */
		virtual ESystemError::Type GetPosition(FVector& OutPosition) = 0;

		/**
		 * Releases the emitter object.
		 *
		 * @return	An ESystemError::Type.
		 */
		virtual ESystemError::Type Release() = 0;

		/**
		 * Gets the emitter identifier.
		 *
		 * @return	The identifier.
		 */
		virtual uint32 GetId() const = 0;
	};
}

