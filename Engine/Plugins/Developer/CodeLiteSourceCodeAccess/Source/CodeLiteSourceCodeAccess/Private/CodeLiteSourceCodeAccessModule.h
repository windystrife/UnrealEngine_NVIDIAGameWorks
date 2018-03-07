// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "CodeLiteSourceCodeAccessor.h"

class FCodeLiteSourceCodeAccessModule : public IModuleInterface
{
public:

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule() override;

	/**
	 * Called before the module is unloaded, right before the module object is destroyed.
	 */
	virtual void ShutdownModule() override;

	FCodeLiteSourceCodeAccessor& GetAccessor();

private:

	FCodeLiteSourceCodeAccessor CodeLiteSourceCodeAccessor;
};
