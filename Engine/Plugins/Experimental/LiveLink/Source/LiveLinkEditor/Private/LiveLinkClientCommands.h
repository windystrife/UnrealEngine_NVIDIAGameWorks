// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FLiveLinkClientCommands : public TCommands<FLiveLinkClientCommands>
{
public:
	FLiveLinkClientCommands();

	TSharedPtr<FUICommandInfo> AddSource;
	TSharedPtr<FUICommandInfo> RemoveSource;
	TSharedPtr<FUICommandInfo> RemoveAllSources;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
