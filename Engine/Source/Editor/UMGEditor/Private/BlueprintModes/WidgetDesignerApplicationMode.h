// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintModes/WidgetBlueprintApplicationMode.h"

/////////////////////////////////////////////////////
// FWidgetDesignerApplicationMode

class FWidgetDesignerApplicationMode : public FWidgetBlueprintApplicationMode
{
public:
	FWidgetDesignerApplicationMode(TSharedPtr<FWidgetBlueprintEditor> InWidgetEditor);

	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	virtual void PreDeactivateMode() override;
	virtual void PostActivateMode() override;
	// End of FApplicationMode interface
};
