// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelEditorModesActions.h"
#include "EditorModeRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LevelEditorModesActions, Log, All);

#define LOCTEXT_NAMESPACE "LevelEditorModesActions"

/** UI_COMMAND takes long for the compile to optimize */
PRAGMA_DISABLE_OPTIMIZATION
void FLevelEditorModesCommands::RegisterCommands()
{
	EditorModeCommands.Empty();

	int editorMode = 0;
	FKey EdModeKeys[9] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };

	for ( const FEditorModeInfo& Mode : FEditorModeRegistry::Get().GetSortedModeInfo() )
	{
		// If the mode isn't visible don't create a menu option for it.
		if (!Mode.bVisible)
		{
			continue;
		}

		FName EditorModeCommandName = FName(*(FString("EditorMode.") + Mode.ID.ToString()));

		TSharedPtr<FUICommandInfo> EditorModeCommand = 
			FInputBindingManager::Get().FindCommandInContext(GetContextName(), EditorModeCommandName);

		// If a command isn't yet registered for this mode, we need to register one.
		if ( !EditorModeCommand.IsValid() )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("Mode"), Mode.Name );
			const FText Tooltip = FText::Format( NSLOCTEXT("LevelEditor", "ModeTooltipF", "Activate {Mode} Editing Mode"), Args );

			FUICommandInfo::MakeCommandInfo(
				this->AsShared(),
				EditorModeCommand,
				EditorModeCommandName,
				Mode.Name,
				Tooltip,
				Mode.IconBrush,
				EUserInterfaceActionType::ToggleButton,
				editorMode < 9 ? FInputChord( EModifierKey::Shift, EdModeKeys[editorMode] ) : FInputChord() );

			EditorModeCommands.Add(EditorModeCommand);
		}

		editorMode++;
	}
}

PRAGMA_ENABLE_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
