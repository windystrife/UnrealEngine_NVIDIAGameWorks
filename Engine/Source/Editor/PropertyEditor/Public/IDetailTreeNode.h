// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IPropertyHandle;

enum class EDetailNodeType
{
	/** Node represents a category */
	Category,
	/** Node represents an item such as a property or widget */
	Item,
	/** Node represents an advanced dropdown */
	Advanced,
	/** Represents a top level object node if a view supports multiple root objects */
	Object,
};

/** The widget contents of the node.  Any of these can be null depending on how the row was generated */
struct FNodeWidgets
{
	/** Widget for the name column */
	TSharedPtr<SWidget> NameWidget;
	/** Widget for the value column*/
	TSharedPtr<SWidget> ValueWidget;


	/** Widget that spans the entire row.  Mutually exclusive with name/value widget */
	TSharedPtr<SWidget> WholeRowWidget;
};

class IDetailTreeNode
{
public:
	virtual ~IDetailTreeNode() {}


	virtual EDetailNodeType GetNodeType() const = 0;

	/** 
	 * Creates a handle to the property on this row if the row represents a property. Only compatible with item node types that are properties
	 * 
	 * @return The property handle for the row or null if the node doesn't have a property 
	 */
	virtual TSharedPtr<IPropertyHandle> CreatePropertyHandle() const = 0;

	virtual FNodeWidgets CreateNodeWidgets() const = 0;

	virtual void GetChildren(TArray<TSharedRef<IDetailTreeNode>>& OutChildren) = 0;
};