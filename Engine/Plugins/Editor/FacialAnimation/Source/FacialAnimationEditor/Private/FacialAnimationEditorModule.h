// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FFacialAnimationEditorModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Handle modules being loaded/unloaded */
	void HandleModulesChanged(FName InModuleName, EModuleChangeReason InModuleChangeReason);

private:
	/** Delegate for hooking into module loading */
	FDelegateHandle OnModulesChangedDelegate;

	/** Delegate for hooking into preview scene creation */
	FDelegateHandle OnPreviewSceneCreatedDelegate;
};
