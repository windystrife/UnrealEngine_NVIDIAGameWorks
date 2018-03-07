// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


/**
 * Implements the Slate module.
 */
class FSlateModule
	: public IModuleInterface
{ };


IMPLEMENT_MODULE(FSlateModule, Slate);
