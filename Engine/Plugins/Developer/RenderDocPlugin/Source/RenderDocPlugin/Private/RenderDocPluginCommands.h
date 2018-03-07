// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "RenderDocPluginStyle.h"
#include "Commands.h"

class FRenderDocPluginCommands : public TCommands<FRenderDocPluginCommands>
{
public:
	FRenderDocPluginCommands()
		: TCommands<FRenderDocPluginCommands>(TEXT("RenderDocPlugin"), NSLOCTEXT("Contexts", "RenderDocPlugin", "RenderDoc Plugin"), NAME_None, FRenderDocPluginStyle::Get()->GetStyleSetName())
	{			
	}

	virtual void RegisterCommands() override;

	TSharedPtr<class FUICommandInfo> CaptureFrameCommand;
};

#endif
