// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "GraphEditor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "EdGraph/EdGraphSchema.h"

class SGraphActionMenu;
class UEdGraph;

/////////////////////////////////////////////////////////////////////////////////////////////////

class SGraphEditorActionMenu : public SBorder
{
public:
	SLATE_BEGIN_ARGS( SGraphEditorActionMenu )
		: _GraphObj( static_cast<UEdGraph*>(NULL) )
		, _NewNodePosition( FVector2D::ZeroVector )
		, _AutoExpandActionMenu( false )
		{}

		SLATE_ARGUMENT( UEdGraph*, GraphObj )
		SLATE_ARGUMENT( FVector2D, NewNodePosition )
		SLATE_ARGUMENT( TArray<UEdGraphPin*>, DraggedFromPins )
		SLATE_ARGUMENT( SGraphEditor::FActionMenuClosed, OnClosedCallback )
		SLATE_ARGUMENT( bool, AutoExpandActionMenu )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	~SGraphEditorActionMenu();

	TSharedRef<SEditableTextBox> GetFilterTextBox();

protected:
	UEdGraph* GraphObj;
	TArray<UEdGraphPin*> DraggedFromPins;
	FVector2D NewNodePosition;
	bool AutoExpandActionMenu;

	SGraphEditor::FActionMenuClosed OnClosedCallback;

	TSharedPtr<SGraphActionMenu> GraphActionMenu;

	void OnActionSelected( const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedAction, ESelectInfo::Type InSelectionType );

	/** Callback used to populate all actions list in SGraphActionMenu */
	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions);
};
