// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IOneSkyLocalizationServiceWorker
{
public:
	/**
	 * Name describing the work that this worker does. Used for factory method hookup.
	 */
	virtual FName GetName() const = 0;

	/**
	 * Function that actually does the work. Can be executed on another thread.
	 */
	virtual bool Execute( class FOneSkyLocalizationServiceCommand& InCommand ) = 0;

	/**
	 * Updates the state of any items after completion (if necessary). This is always executed on the main thread.
	 * @returns true if states were updated
	 */
	virtual bool UpdateStates() const = 0;

};

typedef TSharedRef<IOneSkyLocalizationServiceWorker, ESPMode::ThreadSafe> FOneSkyLocalizationServiceWorkerRef;
