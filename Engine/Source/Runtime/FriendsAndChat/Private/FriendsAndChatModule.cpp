// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IFriendsAndChatModule.h"
#include "FriendsAndChatStyle.h"


/**
 * Implements the FriendsAndChat module.
 */
class FFriendsAndChatModule
	: public IFriendsAndChatModule
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}

	virtual void ShutdownStyle() override
	{
		FFriendsAndChatModuleStyle::Shutdown();
	}
};


IMPLEMENT_MODULE( FFriendsAndChatModule, FriendsAndChat );
