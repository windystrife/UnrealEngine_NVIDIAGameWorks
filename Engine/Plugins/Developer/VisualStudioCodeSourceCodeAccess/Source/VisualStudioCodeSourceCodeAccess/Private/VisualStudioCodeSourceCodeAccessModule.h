// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "VisualStudioCodeSourceCodeAccessor.h"

class FVisualStudioCodeSourceCodeAccessModule : public IModuleInterface
{
public:
	FVisualStudioCodeSourceCodeAccessModule();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FVisualStudioCodeSourceCodeAccessor& GetAccessor();

private:

	TSharedRef<FVisualStudioCodeSourceCodeAccessor> VisualStudioCodeSourceCodeAccessor;
};