// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "ModuleManager.h"


class ILiveLinkInterfaceModule
	: public IModuleInterface
{
public:

	/**
	 * Gets a reference to the live link interface module instance.
	 *
	 * @return A reference to the live link interface module.
	 */
	static ILiveLinkInterfaceModule& Get()
	{
#if PLATFORM_IOS
        static ILiveLinkInterfaceModule& LiveLinkInterfaceModule = FModuleManager::LoadModuleChecked<ILiveLinkInterfaceModule>("LiveLinkInterface");
        return LiveLinkInterfaceModule;
#else
        return FModuleManager::LoadModuleChecked<ILiveLinkInterfaceModule>("LiveLinkInterface");
#endif
	}

public:

	/** Virtual destructor. */
	virtual ~ILiveLinkInterfaceModule() { }
};

