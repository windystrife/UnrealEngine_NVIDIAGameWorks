// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MacNoEditorTargetPlatformModule.cpp: Implements the FMacNoEditorTargetPlatformModule class.
=============================================================================*/

#include "CoreMinimal.h"
#include "GenericMacTargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for the Mac target platform (without editor).
 */
class FMacNoEditorTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual ~FMacNoEditorTargetPlatformModule( )
	{
		Singleton = NULL;
	}

	virtual ITargetPlatform* GetTargetPlatform( ) override
	{
		if (Singleton == NULL)
		{
			Singleton = new TGenericMacTargetPlatform<false, false, false>();
		}

		return Singleton;
	}
};


IMPLEMENT_MODULE(FMacNoEditorTargetPlatformModule, MacNoEditorTargetPlatform);