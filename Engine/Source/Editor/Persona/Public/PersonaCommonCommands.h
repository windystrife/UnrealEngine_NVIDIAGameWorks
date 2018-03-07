// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

class PERSONA_API FPersonaCommonCommands : public TCommands<FPersonaCommonCommands>
{
public:
	FPersonaCommonCommands()
		: TCommands<FPersonaCommonCommands>(TEXT("PersonaCommon"), NSLOCTEXT("Contexts", "PersonaCommon", "Persona Common"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;
	FORCENOINLINE static const FPersonaCommonCommands& Get();

public:
	// Toggle playback
	TSharedPtr<FUICommandInfo> TogglePlay;
};
