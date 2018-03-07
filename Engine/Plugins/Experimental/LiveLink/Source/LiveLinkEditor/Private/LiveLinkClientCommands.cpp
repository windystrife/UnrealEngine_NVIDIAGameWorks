// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LiveLinkClientCommands.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "LiveLinkClientCommands"

FLiveLinkClientCommands::FLiveLinkClientCommands()
	: TCommands<FLiveLinkClientCommands>("LiveLinkClient.Common", LOCTEXT("Common", "Common"), NAME_None, FName(TEXT("LiveLinkStyle")))
{
}

void FLiveLinkClientCommands::RegisterCommands()
{
	UI_COMMAND(AddSource, "Add", "Add a new live link source", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RemoveSource, "Remove", "Remove selected live link source", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RemoveAllSources, "Remove All", "Remove all live link sources", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
