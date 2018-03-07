// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaintModeCommands.h"

#define LOCTEXT_NAMESPACE "LevelMeshPainterCommands"

void FPaintModeCommands::RegisterCommands()
{
	UI_COMMAND(NextTexture, "NextTexture", "Cycle To Next Texture", EUserInterfaceActionType::Button, FInputChord(EKeys::Period));
	Commands.Add(NextTexture);
	UI_COMMAND(PreviousTexture, "PreviousTexture", "Cycle To Previous Texture", EUserInterfaceActionType::Button, FInputChord(EKeys::Comma));
	Commands.Add(PreviousTexture);

	UI_COMMAND(CommitTexturePainting, "CommitTexturePainting", "Commits Texture Painting Changes", EUserInterfaceActionType::Button, FInputChord(EKeys::C, EModifierKey::Control | EModifierKey::Shift));
	Commands.Add(CommitTexturePainting);

	UI_COMMAND(Copy, "CopyVertexColors", "Copies Vertex Colors from the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
	Commands.Add(Copy);

	UI_COMMAND(Paste, "PasteVertexColors", "Tried to Paste Vertex Colors on the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
	Commands.Add(Paste);

	UI_COMMAND(Remove, "RemoveVertexColors", "Removes Vertex Colors from the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
	Commands.Add(Remove);

	UI_COMMAND(Fix, "FixVertexColors", "If necessary fixes Vertex Colors applied to the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
	Commands.Add(Fix);
	
	UI_COMMAND(Fill, "FillVertexColors", "Fills the selected Meshes with the Paint Color", EUserInterfaceActionType::Button, FInputChord());
	Commands.Add(Fill);

	UI_COMMAND(Propagate, "PropagateVertexColors", "Propagates Instance Vertex Colors to the Source Meshes", EUserInterfaceActionType::Button, FInputChord());
	Commands.Add(Propagate);

	UI_COMMAND(Import, "ImportVertexColors", "Imports Vertex Colors from a TGA Texture File to the Selected Meshes", EUserInterfaceActionType::Button, FInputChord());
	Commands.Add(Import);

	UI_COMMAND(Save, "SaveVertexColors", "Saves the Source Meshes for the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
	Commands.Add(Save);
	
	UI_COMMAND(PropagateTexturePaint, "SaveVertexColors", "Propagates Modifications to the Textures", EUserInterfaceActionType::Button, FInputChord());
	Commands.Add(PropagateTexturePaint);

	UI_COMMAND(SaveTexturePaint, "SaveVertexColors", "Saves the Modified Textures for the selected Mesh Components", EUserInterfaceActionType::Button, FInputChord());
	Commands.Add(SaveTexturePaint);
}

#undef LOCTEXT_NAMESPACE

