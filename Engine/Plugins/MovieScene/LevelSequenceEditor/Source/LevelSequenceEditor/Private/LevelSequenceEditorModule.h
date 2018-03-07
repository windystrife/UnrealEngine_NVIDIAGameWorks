// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"


class UObject;


/**
 * Interface for the Level Sequence Editor module.
 */
class ILevelSequenceEditorModule
	: public IModuleInterface
{
public:
	DECLARE_EVENT_OneParam(ILevelSequenceEditorModule, FOnMasterSequenceCreated, UObject*);
	virtual FOnMasterSequenceCreated& OnMasterSequenceCreated() = 0;
};
