// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Framework/Commands/Commands.h"

class SLATE_API FGenericCommands : public TCommands<FGenericCommands>
{
public:
	
	FGenericCommands()
		: TCommands<FGenericCommands>( TEXT("GenericCommands"), NSLOCTEXT("GenericCommands", "Generic Commands", "Common Commands"), NAME_None, FCoreStyle::Get().GetStyleSetName() )
	{
	}

	virtual ~FGenericCommands()
	{
	}

	virtual void RegisterCommands() override;	

	TSharedPtr<FUICommandInfo> Cut;
	TSharedPtr<FUICommandInfo> Copy;
	TSharedPtr<FUICommandInfo> Paste;
	TSharedPtr<FUICommandInfo> Duplicate;
	TSharedPtr<FUICommandInfo> Undo;
	TSharedPtr<FUICommandInfo> Redo;
	TSharedPtr<FUICommandInfo> Delete;
	TSharedPtr<FUICommandInfo> Rename;
	TSharedPtr<FUICommandInfo> SelectAll;	
};
