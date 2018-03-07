// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FBlastMeshEditorCommands : public TCommands<FBlastMeshEditorCommands>
{

public:
	FBlastMeshEditorCommands();

	/**
	 * BlastMesh Editor Commands
	 */
	
	/**  */
	TSharedPtr< FUICommandInfo > Fracture;
	TSharedPtr< FUICommandInfo > Reset;
	TSharedPtr< FUICommandInfo > FixChunkHierarchy;
	//TSharedPtr< FUICommandInfo > Refresh;
	TSharedPtr< FUICommandInfo > ImportRootFromStaticMesh;
	TSharedPtr< FUICommandInfo > FitUvCoordinates;
	TSharedPtr< FUICommandInfo > RebuildCollisionMesh;
	
	TSharedPtr< FUICommandInfo > ToggleFractureVisualization;
	TSharedPtr< FUICommandInfo > ToggleAABBView;
	TSharedPtr< FUICommandInfo > ToggleCollisionMeshView;
	TSharedPtr< FUICommandInfo > ToggleVoronoiSitesView;

	TSharedPtr< FUICommandInfo > BlastVectorExit;
	TSharedPtr< FUICommandInfo > BlastVectorNormal;
	TSharedPtr< FUICommandInfo > BlastVectorPoint;
	TSharedPtr< FUICommandInfo > BlastVectorTwoPoint;
	TSharedPtr< FUICommandInfo > BlastVectorThreePoint;
	TSharedPtr< FUICommandInfo > ExportAssetToFile;


	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

public:
};
