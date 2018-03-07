// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** 
 * Abstraction of a localization revision from a localization service provider.
 */
class ILocalizationServiceRevision : public TSharedFromThis<ILocalizationServiceRevision, ESPMode::ThreadSafe>
{
public:
	/**
	 * Virtual destructor
	 */
	virtual ~ILocalizationServiceRevision() {}

	/** Number of the revision */
	virtual int32 GetRevisionNumber() const = 0;

	/** String representation of the revision */
	virtual const FString& GetRevision() const = 0;

	/** User name of the submitter */
	virtual const FString& GetUserName() const = 0;

	/** Date of the revision */
	virtual const FDateTime& GetDate() const = 0;
};
