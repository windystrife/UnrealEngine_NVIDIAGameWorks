// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SDockableTab;

/**
 * A node in the Docking/Tabbing hierarchy.
 * A DockTabStack shows a row of tabs and the content of one selected tab.
 * It also supports re-arranging tabs and dragging them out for the stack.
 */
class SLATE_API SDockTabStack 
{
public:
	SLATE_BEGIN_ARGS(SDockTabStack)
		: _IsDocumentArea(false)
		{}

		SLATE_ARGUMENT( bool, IsDocumentArea )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	void AddTab( const TSharedRef<SDockableTab>& InTab, int32 AtLocation = INDEX_NONE );

};
