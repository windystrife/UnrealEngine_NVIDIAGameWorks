// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "PLUGIN_NAMEStyle.h"

class FPLUGIN_NAMECommands : public TCommands<FPLUGIN_NAMECommands>
{
public:

	FPLUGIN_NAMECommands()
		: TCommands<FPLUGIN_NAMECommands>(TEXT("PLUGIN_NAME"), NSLOCTEXT("Contexts", "PLUGIN_NAME", "PLUGIN_NAME Plugin"), NAME_None, FPLUGIN_NAMEStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
