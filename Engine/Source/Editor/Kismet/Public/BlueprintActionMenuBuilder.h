// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"

class FBlueprintActionFilter;
class FBlueprintEditor;

namespace FBlueprintActionMenuBuilderImpl
{
	// internal type, forward declared to hide implementation details
	struct FMenuSectionDefinition; 
};
class  FBlueprintActionFilter;
struct FBlueprintActionContext;
class  FBlueprintEditor;

/**
 * Responsible for constructing a list of viable blueprint actions. Runs the 
 * blueprint actions database through a filter and spawns a series of 
 * FBlueprintActionMenuItems for actions that pass. Takes care of generating the 
 * each menu item's category/name/etc.
 */
struct KISMET_API FBlueprintActionMenuBuilder : public FGraphActionListBuilderBase
{
public:
	/** Flags used to customize specific sections of the menu. */
	enum ESectionFlag
	{
		// Rolls properties into a single menu item that will spawn a drag-drop
		// menu for users to pick a node type from.
		ConsolidatePropertyActions = (1<<0),

		// Rolls bound node spawners into a single menu entry that will spawn 
		// multiple nodes, each bound to a single binding.
		ConsolidateBoundActions    = (1<<1),

		// Will clear all action categories (except the section's root) 
		FlattenCategoryHierarcy    = (1<<2),
	};
	
public:
	FBlueprintActionMenuBuilder(TWeakPtr<FBlueprintEditor> BlueprintEditorPtr);
	
	// FGraphActionListBuilderBase interface
	virtual void Empty() override;
	// End FGraphActionListBuilderBase interface
	
	/**
	 * Some action menus require multiple sections. One option is to create 
	 * multiple FBlueprintActionMenuBuilders and append them together, but that
	 * can be unperformant (each builder will run through the entire database
	 * separately)... This method provides an alternative, where you can specify
	 * a separate filter/heading/ordering for a sub-section of the menu.
	 *
	 * @param  Filter	 The filter you want applied to this section of the menu.
	 * @param  Heading	 The root category for this section of the menu (can be left blank).
	 * @param  MenuOrder The sort order to assign this section of the menu (higher numbers come first).
	 * @param  Flags	 Set of ESectionFlags to customize this menu section.
	 */
	void AddMenuSection(FBlueprintActionFilter const& Filter, FText const& Heading = FText::GetEmpty(), int32 MenuOrder = 0, uint32 const Flags = 0);
	
	/**
	 * Regenerates the entire menu list from the cached menu sections. Filters 
	 * and adds action items from the blueprint action database (as defined by 
	 * the MenuSections list).
	 */
	void RebuildActionList();

private:
	/** 
	 * Defines all the separate sections of the menu (filter, sort order, etc.).
	 * Defined as a TSharedRef<> so as to hide the implementation details (keep 
	 * this API clean).
	 */
	TArray< TSharedRef<FBlueprintActionMenuBuilderImpl::FMenuSectionDefinition> > MenuSections;

	/** */
	TWeakPtr<FBlueprintEditor> BlueprintEditorPtr;
};
