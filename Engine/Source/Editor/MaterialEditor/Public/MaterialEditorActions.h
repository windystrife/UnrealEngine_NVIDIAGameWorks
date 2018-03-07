// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.



#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputChord.h"
#include "EdGraph/EdGraphSchema.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

class UEdGraph;

/**
 * Unreal material editor actions
 */
class MATERIALEDITOR_API FMaterialEditorCommands : public TCommands<FMaterialEditorCommands>
{

public:
	FMaterialEditorCommands() : TCommands<FMaterialEditorCommands>
	(
		"MaterialEditor", // Context name for fast lookup
		NSLOCTEXT("Contexts", "MaterialEditor", "Material Editor"), // Localized context name for displaying
		NAME_None, FEditorStyle::GetStyleSetName()
	)
	{
	}
	
	/**
	 * Material Editor Commands
	 */
	
	/** Applys the following material to the world */
	TSharedPtr< FUICommandInfo > Apply;
	
	/** Flattens the material to a texture for mobile devices */
	TSharedPtr< FUICommandInfo > Flatten;

	/**
	 * Material Instance Editor Commands
	 */

	/** Toggles between showing all the material parameters or not */
	TSharedPtr< FUICommandInfo > ShowAllMaterialParameters;

	/**
	 * Preview Pane Commands
	 */

	/** Sets the preview mesh to a cylinder */
	TSharedPtr< FUICommandInfo > SetCylinderPreview;
	
	/** Sets the preview mesh to a sphere */
	TSharedPtr< FUICommandInfo > SetSpherePreview;
	
	/** Sets the preview mesh to a plane */
	TSharedPtr< FUICommandInfo > SetPlanePreview;
	
	/** Sets the preview mesh to a cube */
	TSharedPtr< FUICommandInfo > SetCubePreview;
	
	/** Sets the preview mesh to the current selection in the level editor */
	TSharedPtr< FUICommandInfo > SetPreviewMeshFromSelection;
	
	/** Toggles the preview pane's grid */
	TSharedPtr< FUICommandInfo > TogglePreviewGrid;
	
	/** Toggles the preview pane's background */
	TSharedPtr< FUICommandInfo > TogglePreviewBackground;


	/**
	 * Canvas Commands
	 */
	
	/** Moves the canvas camera to the home position */
	TSharedPtr< FUICommandInfo > CameraHome;
	
	/** Removes any unused nodes */
	TSharedPtr< FUICommandInfo > CleanUnusedExpressions;
	
	/** Shows or hides unused connectors */
	TSharedPtr< FUICommandInfo > ShowHideConnectors;
	
	/** Toggles live updating of the preview material. */
	TSharedPtr< FUICommandInfo > ToggleLivePreview;

	/** Toggles real time expression nodes */
	TSharedPtr< FUICommandInfo > ToggleRealtimeExpressions;
	
	/** Always refresh all previews when enabled */
	TSharedPtr< FUICommandInfo > AlwaysRefreshAllPreviews;

	/** Toggles the material stats on the canvas pane */
	TSharedPtr< FUICommandInfo > ToggleMaterialStats;

	/** Shows material stats and errors when compiled for mobile. */
	TSharedPtr< FUICommandInfo > ToggleMobileStats;
	
	/** Creates a new comment node */
	TSharedPtr< FUICommandInfo > NewComment;
	
	/** Uses the texture in the content browser for the selected node */
	TSharedPtr< FUICommandInfo > UseCurrentTexture;
	
	/** Pastes the copied items at the current location */
	TSharedPtr< FUICommandInfo > MatertialPasteHere;
	
	/** Converts selected objects to parameters */
	TSharedPtr< FUICommandInfo > ConvertObjects;

	/** Converts selected texture type into another */
	TSharedPtr< FUICommandInfo > ConvertToTextureObjects;
	TSharedPtr< FUICommandInfo > ConvertToTextureSamples;
	
	/** Converts selected objects to constants */
	TSharedPtr< FUICommandInfo > ConvertToConstant;

	/** Stops a node from being previewed in the viewport */
	TSharedPtr< FUICommandInfo > StopPreviewNode;
	
	/** Makes a new be previewed in the viewport */
	TSharedPtr< FUICommandInfo > StartPreviewNode;
	
	/** Enables realtime previewing of this node */
	TSharedPtr< FUICommandInfo > EnableRealtimePreviewNode;
	
	/** Disables realtime previewing of this node */
	TSharedPtr< FUICommandInfo > DisableRealtimePreviewNode;
	
	/** Breaks all outgoing links on the selected node */
	TSharedPtr< FUICommandInfo > BreakAllLinks;
	
	/** Duplicates all selected objects */
	TSharedPtr< FUICommandInfo > DuplicateObjects;
	
	/** Deletes all selected objects */
	TSharedPtr< FUICommandInfo > DeleteObjects;
	
	/** Selects all nodes that use the selected node's outgoing links */
	TSharedPtr< FUICommandInfo > SelectDownstreamNodes;
	
	/** Selects all nodes that use the selected node's incoming links */
	TSharedPtr< FUICommandInfo > SelectUpstreamNodes;
	
	/** Removes the selected expression from your favorites */
	TSharedPtr< FUICommandInfo > RemoveFromFavorites;
	
	/** Adds the selected expression to your favorites */
	TSharedPtr< FUICommandInfo > AddToFavorites;
	
	/** Deletes the selected link */
	TSharedPtr< FUICommandInfo > BreakLink;

	/** Forces a refresh of all previews */
	TSharedPtr< FUICommandInfo > ForceRefreshPreviews;

	/** Finds expressions in current material */
	TSharedPtr< FUICommandInfo > FindInMaterial;

	/** Create component mask node */
	TSharedPtr< FUICommandInfo > CreateComponentMaskNode;

	/** Create component mask node */
	TSharedPtr< FUICommandInfo > PromoteToParameter;


	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

public:
};

//////////////////////////////////////////////////////////////////////////
// FExpressionSpawnInfo

class FExpressionSpawnInfo
{
public:
	/** Constructor */
	FExpressionSpawnInfo(UClass* InMaterialExpressionClass) : MaterialExpressionClass(InMaterialExpressionClass) {}

	/** Holds the UI Command to verify chords for this action are held */
	TSharedPtr< FUICommandInfo > CommandInfo;

	/**
	 * Creates an action to be used for placing a node into the graph
	 *
	 * @param	InDestGraph		The graph the action should be created for
	 *
	 * @return					A fully prepared action containing the information to spawn the node
	 */
	TSharedPtr< FEdGraphSchemaAction > GetAction(UEdGraph* InDestGraph);

	UClass* GetClass() {return MaterialExpressionClass;}

private:
	/** Type of expression to spawn */
	UClass* MaterialExpressionClass;
};

//////////////////////////////////////////////////////////////////////////
// FMaterialEditorSpawnNodeCommands

/** Handles spawn node commands for the Material Editor */
class FMaterialEditorSpawnNodeCommands : public TCommands<FMaterialEditorSpawnNodeCommands>
{
public:
	/** Constructor */
	FMaterialEditorSpawnNodeCommands()
		: TCommands<FMaterialEditorSpawnNodeCommands>( TEXT("MaterialEditorSpawnNodes"), NSLOCTEXT("Contexts", "MaterialEditor_SpawnNodes", "Material Editor - Spawn Nodes"), NAME_None, FEditorStyle::GetStyleSetName() )
	{
	}	

	/** TCommands interface */
	virtual void RegisterCommands() override;

	/**
	 * Returns a graph action assigned to the passed in chord
	 *
	 * @param InChord		The chord to use for lookup
	 * @param InDestGraph	The graph to create the graph action for, used for validation purposes and to link any important node data to the graph
	 */
	TSharedPtr< FEdGraphSchemaAction > GetGraphActionByChord(FInputChord& InChord, UEdGraph* InDestGraph) const;

	const TSharedPtr<const FInputChord> GetChordByClass(UClass* MaterialExpressionClass) const;

private:
	/** An array of all the possible commands for spawning nodes */
	TArray< TSharedPtr< class FExpressionSpawnInfo > > NodeCommands;
};
