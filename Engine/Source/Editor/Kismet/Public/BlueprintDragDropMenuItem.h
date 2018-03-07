// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateBrush.h"
#include "EdGraph/EdGraphSchema.h"
#include "BlueprintEditor.h"
#include "BlueprintActionFilter.h"
#include "BlueprintDragDropMenuItem.generated.h"

class UBlueprintNodeSpawner;
class UEdGraph;

/**
 * At certain times we want a single menu entry that represents a set of 
 * UBlueprintNodeSpawners (generally when all those UBlueprintNodeSpawners wrap 
 * the same UField). We do this to keep the menu less jumbled, and instead
 * use the drag/drop action to present a sub-menu to the user (so they can pick 
 * the the node type that they want). We do this with both delegates and 
 * variable nodes (where the user can pick a getter vs. a setter, etc.).
 *
 * This class represents those "consolidated" actions, and essentially serves 
 * as a FDragDropOperation spawner. It wraps a single UBlueprintNodeSpawner (any
 * one of the set that it is supposed to represent), that it uses to determine
 * the proper FDragDropOperation.
 */
USTRUCT()
struct FBlueprintDragDropMenuItem : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();
	
public:
	static FName StaticGetTypeId() { static FName const TypeId("FBlueprintDragDropMenuItem"); return TypeId; }
	/** Default constructor for USTRUCT() purposes. */
	FBlueprintDragDropMenuItem() {}

	/**
	 * Sole constructor, which ensures that the RepresentativeAction member is
	 * set (and valid).
	 * 
	 * @param  SampleAction	One of the node-spawners that this "consolidated" menu item represents (shouldn't matter which one).
	 * @param  MenuGrouping	Defines the order in which this entry is listed in the menu.
	 */
	FBlueprintDragDropMenuItem(FBlueprintActionContext const& Context, UBlueprintNodeSpawner const* SampleAction, int32 MenuGrouping, FText InNodeCategory, FText InMenuDesc, FText InToolTip );
	
	// FEdGraphSchemaAction interface
	virtual FName GetTypeId() const final { return StaticGetTypeId(); }
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, FVector2D const Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, FVector2D const Location, bool bSelectNewNode = true) override;
	virtual void  AddReferencedObjects(FReferenceCollector& Collector) override;
	// End FEdGraphSchemaAction interface

	/**
	 * Retrieves the icon brush for this menu entry (to be displayed alongside
	 * in the menu).
	 *
	 * @param  ColorOut	An output parameter that's filled in with the color to tint the brush with.
	 * @return An slate brush to be used for this menu item in the action menu.
	 */
	FSlateBrush const* GetMenuIcon(FSlateColor& ColorOut);

	/** */
	void AppendAction(UBlueprintNodeSpawner const* Action);

	/** @return   */
	UBlueprintNodeSpawner const* GetSampleAction() const;

	/** @return   */
	TSet<UBlueprintNodeSpawner const*> const& GetActionSet() const;

	/**
	 * Attempts to create a certain drag/drop action with respect to the 
	 * set UBlueprintNodeSpawner.
	 * 
	 * @param  AnalyticsDelegate	The analytics callback to assign the drag/drop operation.
	 * @return An empty TSharedPtr if we failed to determine the FDragDropOperation type to use, else a newly instantiated drag/drop op.
	 */
	TSharedPtr<FDragDropOperation> OnDragged(FNodeCreationAnalytic AnalyticsDelegate) const;

private:
	/** An arbitrary member of the node-spawner subset that this menu entry represents. */
	TSet<UBlueprintNodeSpawner const*> ActionSet;
};
