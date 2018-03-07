// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BlueprintEditorSettings.generated.h"

UENUM()
enum ESaveOnCompile
{
	SoC_Never UMETA(DisplayName="Never"),
	SoC_SuccessOnly UMETA(DisplayName="On Success Only"),
	SoC_Always UMETA(DisplayName = "Always"),
};
 
UCLASS(config=EditorPerProjectUserSettings)
class BLUEPRINTGRAPH_API UBlueprintEditorSettings
	:	public UObject
{
	GENERATED_UCLASS_BODY()

// Style Settings
public:
	/** Should arrows indicating data/execution flow be drawn halfway along wires? */
	UPROPERTY(EditAnywhere, config, Category=VisualStyle, meta=(DisplayName="Draw midpoint arrows in Blueprints"))
	bool bDrawMidpointArrowsInBlueprints;

	/** Determines if lightweight tutorial text shows up at the top of empty blueprint graphs */
	UPROPERTY(EditAnywhere, config, Category = VisualStyle)
	bool bShowGraphInstructionText;

// Workflow Settings
public:
	/** If enabled, we'll save off your chosen target setting based off of the context (allowing you to have different preferences based off what you're operating on). */
	UPROPERTY(EditAnywhere, config, Category=Workflow, meta=(DisplayName="Context Menu: Divide Context Target Preferences"))
	bool bSplitContextTargetSettings;

	/** If enabled, then ALL component functions are exposed to the context menu (when the contextual target is a component owner). Ignores "ExposeFunctionCategories" metadata for components. */
	UPROPERTY(EditAnywhere, config, Category=Workflow, meta=(DisplayName="Context Menu: Expose All Sub-Component Functions"))
	bool bExposeAllMemberComponentFunctions;

	/** If enabled, then a separate section with your Blueprint favorites will be pined to the top of the context menu. */
	UPROPERTY(EditAnywhere, config, Category=Workflow, meta=(DisplayName="Context Menu: Show Favorites Section"))
	bool bShowContextualFavorites;

	/** If enabled, then call-on-member actions will be spawned as a single node (instead of a GetMember + FunctionCall node). */
	UPROPERTY(EditAnywhere, config, Category=Workflow)
	bool bCompactCallOnMemberNodes;

	/** If enabled, then your Blueprint favorites will be uncategorized, leaving you with less nested categories to sort through. */
	UPROPERTY(EditAnywhere, config, Category=Workflow)
	bool bFlattenFavoritesMenus;

	/** If enabled, then placed cast nodes will default to their "pure" form (meaning: without execution pins). */
	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category=Experimental, meta=(DisplayName="Default to Using Pure Cast Nodes"))
	bool bFavorPureCastNodes;

	/** If enabled, then you'll be able to directly connect arbitrary object pins together (a pure cast node will be injected automatically). */
	UPROPERTY(EditAnywhere, config, Category=Workflow)
	bool bAutoCastObjectConnections;

	/** If true will show the viewport tab when simulate is clicked. */
	UPROPERTY(EditAnywhere, config, Category=Workflow)
	bool bShowViewportOnSimulate;

	/** If set we'll show the inherited variables in the My Blueprint view. */
	UPROPERTY(config)
	bool bShowInheritedVariables;

	/** If set we'll show empty sections in the My Blueprint view. */
	UPROPERTY(config)
	bool bShowEmptySections;

	/** If set will spawn default nodes in new Blueprints */
	UPROPERTY(EditAnywhere, config, Category=Workflow)
	bool bSpawnDefaultBlueprintNodes;

	/** If set will exclude components added in a Blueprint class Construction Script from the component details view */
	UPROPERTY(EditAnywhere, config, Category=Workflow)
	bool bHideConstructionScriptComponentsInDetailsView;

	/** If enabled, the global Find in Blueprints command (CTRL-SHIFT-F) will be hosted in a standalone tab. This tab can remain open after the Blueprint Editor context is closed. */
	UPROPERTY(EditAnywhere, config, Category=Workflow)
	bool bHostFindInBlueprintsInGlobalTab;

// Compiler Settings
public:
	/** Determines when to save Blueprints post-compile */
	UPROPERTY(EditAnywhere, config, Category=Compiler)
	TEnumAsByte<ESaveOnCompile> SaveOnCompile;

	/** When enabled, if a blueprint has compiler errors, then the graph will jump and focus on the first node generating an error */
	UPROPERTY(EditAnywhere, config, Category=Compiler)
	bool bJumpToNodeErrors;

	/** If enabled, nodes can be explicitly disabled via context menu when right-clicking on impure nodes in the Blueprint editor. Disabled nodes will not be compiled, but also will not break existing connections. */
	UPROPERTY(EditAnywhere, config, Category=Experimental, AdvancedDisplay)
	bool bAllowExplicitImpureNodeDisabling;

// Developer Settings
public:
	/** If enabled, tooltips on action menu items will show the associated action's signature id (can be used to setup custom favorites menus).*/
	UPROPERTY(EditAnywhere, config, Category=DeveloperTools)
	bool bShowActionMenuItemSignatures;

	/** If enabled, blueprint nodes in the event graph will display with unique names rather than their display name. */
	UPROPERTY(EditAnywhere, config, Category = DeveloperTools, meta = (DisplayName = "Display Unique Names for Blueprint Nodes"))
	bool bBlueprintNodeUniqueNames;

// Perf Settings
public:
	/** If enabled, additional details will be displayed in the Compiler Results tab after compiling a blueprint. */
	UPROPERTY(EditAnywhere, config, Category=Performance)
	bool bShowDetailedCompileResults;

	/** Minimum event time threshold used as a filter when additional details are enabled for display in the Compiler Results tab. A value of zero means that all events will be included in the final summary. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category=Performance, DisplayName="Compile Event Results Threshold (ms)", meta=(ClampMin="0", UIMin="0"))
	int32 CompileEventDisplayThresholdMs;

	/** The node template cache is used to speed up blueprint menuing. This determines the peak data size for that cache. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, config, Category=Performance, DisplayName="Node-Template Cache Cap (MB)", meta=(ClampMin="0", UIMin="0"))
	float NodeTemplateCacheCapMB;

protected:
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface
};
