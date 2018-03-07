// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateBrush.h"
#include "EdGraph/EdGraphSchema.h"
#include "BlueprintNodeBinder.h"
#include "BlueprintActionMenuItem.generated.h"

class UBlueprintNodeSpawner;
class UEdGraph;
struct FBlueprintActionContext;
struct FBlueprintActionUiSpec;

/**
 * Wrapper around a UBlueprintNodeSpawner, which takes care of specialized
 * node spawning. This class should not be sub-classed, any special handling
 * should be done inside a UBlueprintNodeSpawner subclass, which will be
 * invoked from this class (separated to divide ui and node-spawning).
 */
USTRUCT()
struct FBlueprintActionMenuItem : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();
	static FName StaticGetTypeId() { static FName const TypeId("FBlueprintActionMenuItem"); return TypeId; }

public:	
	/** Constructors */
	FBlueprintActionMenuItem(UBlueprintNodeSpawner const* NodeSpawner = nullptr) : Action(NodeSpawner), IconTint(FLinearColor::White), IconBrush(nullptr) {}
	FBlueprintActionMenuItem(UBlueprintNodeSpawner const* NodeSpawner, FBlueprintActionUiSpec const& UiDef, IBlueprintNodeBinder::FBindingSet const& Bindings = IBlueprintNodeBinder::FBindingSet(), FText InNodeCategory = FText(), int32 InGrouping = 0);
	
	// FEdGraphSchemaAction interface
	virtual FName         GetTypeId() const final { return StaticGetTypeId(); }
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, FVector2D const Location, bool bSelectNewNode = true) final;
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, FVector2D const Location, bool bSelectNewNode = true) final;
	virtual void          AddReferencedObjects(FReferenceCollector& Collector) final;
	// End FEdGraphSchemaAction interface

	/** @return */
	UBlueprintNodeSpawner const* GetRawAction() const;

	void AppendBindings(const FBlueprintActionContext& Context, IBlueprintNodeBinder::FBindingSet const& BindingSet);

	/**
	 * Retrieves the icon brush for this menu entry (to be displayed alongside
	 * in the menu).
	 *
	 * @param  ColorOut	An output parameter that's filled in with the color to tint the brush with.
	 * @return An slate brush to be used for this menu item in the action menu.
	 */
	FSlateBrush const* GetMenuIcon(FSlateColor& ColorOut);

	/** Utility struct for pairing documentation page names with excerpt names */
	struct FDocExcerptRef
	{
		bool IsValid() const;

		FString DocLink;
		FString DocExcerptName;		
	};
	/**  */
	const FDocExcerptRef& GetDocumentationExcerpt() const;

private:
	/** Specialized node-spawner, that comprises the action portion of this menu entry. */
	UBlueprintNodeSpawner const* Action;
	/** Tint to return along with the icon brush. */
	FSlateColor IconTint;
	/** Brush that should be used for the icon on this menu item. */
	FSlateBrush const* IconBrush;
	/** */
	IBlueprintNodeBinder::FBindingSet Bindings;
	/** References the documentation page/excerpt pertaining to the node this will spawn */
	FDocExcerptRef DocExcerptRef;
};
