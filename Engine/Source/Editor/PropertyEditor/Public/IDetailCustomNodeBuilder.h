// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FDetailWidgetRow;
class IDetailChildrenBuilder;

/**
 * A custom node that can be given to a details panel category to customize widgets
 */
class IDetailCustomNodeBuilder
{
public:
	virtual ~IDetailCustomNodeBuilder(){};

	/**
	 * Sets a delegate that should be used when the custom node needs to rebuild children
	 *
	 * @param A delegate to invoke when the tree should rebuild this nodes children
	 */
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRegenerateChildren ) = 0;

	/**
	 * Called to generate content in the header of this node ( the actual node content ). 
	 * Only called if HasHeaderRow is true
	 *
	 * @param NodeRow The row to put content in
	 */
	virtual void GenerateHeaderRowContent( FDetailWidgetRow& NodeRow ) = 0;

	/**
	 * Called to generate child content of this node
	 *
	 * @param OutChildRows An array of rows to add children to
	 */
	virtual void GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder ) = 0;

	/**
	 * Called each tick if ReqiresTick is true
	 */
	virtual void Tick( float DeltaTime ) = 0;

	/**
	 * @return true if this node requires tick, false otherwise
	 */
	virtual bool RequiresTick() const = 0;

	/**
	 * @return true if this node should be collapsed in the tree
	 */
	virtual bool InitiallyCollapsed() const = 0;

	/**
	 * @return The name of this custom builder.  This is used as an identifier to save expansion state if needed
	 */
	virtual FName GetName() const = 0;
};
