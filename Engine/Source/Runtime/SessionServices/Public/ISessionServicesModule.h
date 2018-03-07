// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "ISessionManager.h"
#include "ISessionService.h"

/**
 * Interface for session core modules.
 */
class ISessionServicesModule
	: public IModuleInterface
{
public:

	/**
	 * Gets the session manager.
	 *
	 * @return The session manager.
	 * @see GetSessionService
	 */
	virtual TSharedRef<ISessionManager> GetSessionManager() = 0;

	/** 
	 * Gets the session service.
	 *
	 * @return The session service.
	 * @see GetSessionManager
	 */
	virtual TSharedRef<ISessionService> GetSessionService() = 0;

public:

	/** Virtual destructor. */
	virtual ~ISessionServicesModule() { }
};
