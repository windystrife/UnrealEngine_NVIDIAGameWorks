// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

struct FSlateBrush;

class IBspModeModule : public IModuleInterface
{
public:
	virtual void RegisterBspBuilderType( class UClass* InBuilderClass, const FText& InBuilderName, const FText& InBuilderTooltip, const FSlateBrush* InBuilderIcon ) = 0;
	virtual void UnregisterBspBuilderType( class UClass* InBuilderClass ) = 0;
};

