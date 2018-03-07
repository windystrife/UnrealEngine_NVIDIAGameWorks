// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

// Actions that can be invoked in the Size Map
class FSizeMapActions : public TCommands<FSizeMapActions>
{
public:
	FSizeMapActions() : TCommands<FSizeMapActions>
	(
		"SizeMap",
		NSLOCTEXT("Contexts", "SizeMap", "Size Map"),
		"MainFrame", FEditorStyle::GetStyleSetName()
	)
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface
public:

	// @todo sizemap: Register any commands we have here
	// TSharedPtr<FUICommandInfo> DoSomething;
	
};
