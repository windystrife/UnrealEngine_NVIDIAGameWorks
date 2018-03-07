// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class IEditorStyleModule : public IModuleInterface
{
public:

	virtual TSharedRef< class FSlateStyleSet > CreateEditorStyleInstance() const = 0;
};
