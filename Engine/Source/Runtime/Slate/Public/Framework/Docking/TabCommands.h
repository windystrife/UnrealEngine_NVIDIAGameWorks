// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Framework/Commands/Commands.h"

class SLATE_API FTabCommands : public TCommands < FTabCommands >
{
public:

	FTabCommands()
		: TCommands<FTabCommands>(TEXT("TabCommands"), NSLOCTEXT("TabCommands", "DockingTabCommands", "Docking Tab Commands"), NAME_None, FCoreStyle::Get().GetStyleSetName())
	{
	}

	virtual ~FTabCommands()
	{
	}

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> CloseMajorTab;
	TSharedPtr<FUICommandInfo> CloseMinorTab;
};
