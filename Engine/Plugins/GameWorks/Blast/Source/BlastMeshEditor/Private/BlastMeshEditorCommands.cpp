// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlastMeshEditorCommands.h"
#include "BlastMeshEditorStyle.h"

#define LOCTEXT_NAMESPACE "BlastMeshEditorCommands"

FBlastMeshEditorCommands::FBlastMeshEditorCommands()
	: TCommands<FBlastMeshEditorCommands>
	(
		"BlastMeshEditor", // Context name for fast lookup
		NSLOCTEXT("Contexts", "BlastMeshEditor", "BlastMesh Editor"), // Localized context name for displaying
		NAME_None, // Parent
		FBlastMeshEditorStyle::Get()->GetStyleSetName() // Icon Style Set
	)
{
}

void FBlastMeshEditorCommands::RegisterCommands()
{
	UI_COMMAND( Fracture, "Fracture", "Fractures the mesh's root chunk(s) based upon the Fracture Settings.", EUserInterfaceActionType::Button, FInputChord(EKeys::F) );
	UI_COMMAND( Reset, "Reset", "Removes children of selected chunks.", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt, EKeys::R));
	UI_COMMAND( FixChunkHierarchy, "Optimize", "Tries to unite chunks and build optimized hierarchy for better Blast runtime performance.", EUserInterfaceActionType::Button, FInputChord() );
	//UI_COMMAND( Refresh, "Refresh", "Refreshes the BlastMesh from the StaticMesh it was created from.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND( ImportRootFromStaticMesh, "Import", "Imports root chunk from static mesh.", EUserInterfaceActionType::Button, FInputChord() );
	UI_COMMAND(FitUvCoordinates, "FitUVs", "Fit uv coordinates to given square.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RebuildCollisionMesh, "Collision", "Rebuild collision mesh.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ExportAssetToFile, "Export asset", "Export asset to a file.", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(ToggleFractureVisualization, "Fracture visualization", "Show fracture method specific visualization.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Control, EKeys::V));
	UI_COMMAND(ToggleAABBView, "Bounding box", "Show axis aligned bounding box rendering for selected chunks.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Control, EKeys::B));
	UI_COMMAND(ToggleCollisionMeshView, "Collision mesh", "Show collision mesh rendering.", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Control, EKeys::C));
	UI_COMMAND(ToggleVoronoiSitesView, "Voronoi sites", "Show Voronoi sites rendering.", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BlastVectorExit, "Exit", "Exit from vector picking mode.", EUserInterfaceActionType::Button, FInputChord(EKeys::Escape));
	UI_COMMAND(BlastVectorNormal, "Normal", "Click on any chunk for picking the same normal as in clicked position.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::N));
	UI_COMMAND(BlastVectorPoint, "Point", "Click on any chunk for picking position.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::One));
	UI_COMMAND(BlastVectorTwoPoint, "Two points", "Click twice on any chunks for picking vector between this two positions.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::Two));
	UI_COMMAND(BlastVectorThreePoint, "Three points", "Click twice on any chunks for picking line, then pick position on this line.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::Three));
}

#undef LOCTEXT_NAMESPACE
