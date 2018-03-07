// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class IPropertyHandle;
class IUnloadedBlueprintData;
class UBlueprint;

class FClassViewerNode
{
public:
	/**
	 * Creates a node for the widget's tree.
	 *
	 * @param	InClassName						The name of the class this node represents.
	 * @param	InClassDisplayName				The display name of the class this node represents
	 * @param	bInIsPlaceable					true if the class is a placeable class.
	 */
	FClassViewerNode( const FString& InClassName, const FString& InClassDisplayName );

	FClassViewerNode( const FClassViewerNode& InCopyObject);

	/**
	 * Adds the specified child to the node.
	 *
	 * @param	Child							The child to be added to this node for the tree.
	 */
	void AddChild( TSharedPtr<FClassViewerNode> Child );

	/**
	 * Adds the specified child to the node. If a child with the same class already exists the function add the child storing more info.
	 * The function does not persist child order.
	 *
	 * @param	NewChild	The child to be added to this node for the tree.
	 */
	void AddUniqueChild(TSharedPtr<FClassViewerNode> NewChild);

	/** 
	 * Retrieves the class name this node is associated with. 
	 * @param	bUseDisplayName	Whether to use the display name or class name
	 */
	TSharedPtr<FString> GetClassName(bool bUseDisplayName = false) const
	{
		return bUseDisplayName ? ClassDisplayName : ClassName;
	}

	/** Retrieves the children list. */
	TArray<TSharedPtr<FClassViewerNode>>& GetChildrenList()
	{
		return ChildrenList;
	}

	/** Checks if the class is placeable. */
	bool IsClassPlaceable() const;

	bool IsRestricted() const;

private:
	/** The class name for this tree node. */
	TSharedPtr<FString> ClassName;

	/** The class display name for this tree node. */
	TSharedPtr<FString> ClassDisplayName;

	/** List of children. */
	TArray<TSharedPtr<FClassViewerNode>> ChildrenList;

public:
	/** The class this node is associated with. */
	TWeakObjectPtr<UClass> Class;

	/** The blueprint this node is associated with. */
	TWeakObjectPtr<UBlueprint> Blueprint;

	/** Used to load up the package if it is unloaded, retrieved from meta data for the package. */
	FString GeneratedClassPackage;

	/** Used to examine the classname, retrieved from meta data for the package. */
	FName GeneratedClassname;

	/** Used to find the parent of this class, retrieved from meta data for the package. */
	FName ParentClassname;

	/** Used to load up the class if it is unloaded. */
	FString AssetName;

	/** true if the class passed the filter. */
	bool bPassesFilter;

	/** true if the class is a "normal type", this is used to identify unloaded blueprints as blueprint bases. */
	bool bIsBPNormalType;

	/** Pointer to the parent to this object. */
	TWeakPtr< FClassViewerNode > ParentNode;

	/** Data for unloaded blueprints, only valid if the class is unloaded. */
	TSharedPtr< class IUnloadedBlueprintData > UnloadedBlueprintData;

	/** The property this node will be working on. */
	TSharedPtr<class IPropertyHandle> PropertyHandle;
};
