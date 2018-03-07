// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBorder.h"
#include "EdGraph/EdGraphSchema.h"
#include "GraphEditor.h"

class SEditableTextBox;
class SGraphActionMenu;
class UAIGraphNode;
class UEdGraph;

/////////////////////////////////////////////////////////////////////////////////////////////////

class AIGRAPH_API SGraphEditorActionMenuAI : public SBorder
{
public:
	SLATE_BEGIN_ARGS(SGraphEditorActionMenuAI)
		: _GraphObj( static_cast<UEdGraph*>(NULL) )
		,_GraphNode(NULL)
		, _NewNodePosition( FVector2D::ZeroVector )
		, _AutoExpandActionMenu( false )
		, _SubNodeFlags(0)
		{}

		SLATE_ARGUMENT( UEdGraph*, GraphObj )
		SLATE_ARGUMENT( UAIGraphNode*, GraphNode)
		SLATE_ARGUMENT( FVector2D, NewNodePosition )
		SLATE_ARGUMENT( TArray<UEdGraphPin*>, DraggedFromPins )
		SLATE_ARGUMENT( SGraphEditor::FActionMenuClosed, OnClosedCallback )
		SLATE_ARGUMENT( bool, AutoExpandActionMenu )
		SLATE_ARGUMENT( int32, SubNodeFlags)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	~SGraphEditorActionMenuAI();

	TSharedRef<SEditableTextBox> GetFilterTextBox();

protected:
	UEdGraph* GraphObj;
	UAIGraphNode* GraphNode;
	TArray<UEdGraphPin*> DraggedFromPins;
	FVector2D NewNodePosition;
	bool AutoExpandActionMenu;
	int32 SubNodeFlags;

	SGraphEditor::FActionMenuClosed OnClosedCallback;
	TSharedPtr<SGraphActionMenu> GraphActionMenu;

	void OnActionSelected( const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedAction, ESelectInfo::Type InSelectionType );

	/** Callback used to populate all actions list in SGraphActionMenu */
	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions);
};
