// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "GraphEditor.h"
#include "EdGraph/EdGraphSchema.h"
#include "Widgets/Layout/SBorder.h"

class SGraphActionMenu;
class SSoundClassActionMenu;
class UEdGraph;

/** Widget for displaying a single item  */
class SSoundClassActionMenuItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SSoundClassActionMenuItem )
		{}

		SLATE_ATTRIBUTE( FText, HighlightText )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FEdGraphSchemaAction> InAction, TWeakPtr<class SSoundClassActionMenu> InOwner);

private:
	/* Create the new sound class widget */
	TSharedRef<SWidget> CreateNewSoundClassWidget( const FText& DisplayText, const FText& ToolTip, const FSlateFontInfo& NameFont, TSharedPtr<FEdGraphSchemaAction>& InAction );

	/** Called when confirming name for new sound class */
	void OnNewSoundClassNameEntered(const FText& NewText, ETextCommit::Type CommitInfo, TSharedPtr<FEdGraphSchemaAction> InAction );

	/** Called when text is changed for a new sound class name */
	void OnNewSoundClassNameChanged(const FText& NewText,  TSharedPtr<FEdGraphSchemaAction> InAction );

	TWeakPtr<class SSoundClassActionMenu> Owner;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

class SSoundClassActionMenu : public SBorder
{
public:
	SLATE_BEGIN_ARGS( SSoundClassActionMenu )
		: _GraphObj( static_cast<UEdGraph*>(NULL) )
		, _NewNodePosition( FVector2D::ZeroVector )
		, _AutoExpandActionMenu( true )
		{}

		SLATE_ARGUMENT( UEdGraph*, GraphObj )
		SLATE_ARGUMENT( FVector2D, NewNodePosition )
		SLATE_ARGUMENT( TArray<UEdGraphPin*>, DraggedFromPins )
		SLATE_ARGUMENT( SGraphEditor::FActionMenuClosed, OnClosedCallback )
		SLATE_ARGUMENT( bool, AutoExpandActionMenu )
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	~SSoundClassActionMenu();

protected:
	UEdGraph* GraphObj;
	TArray<UEdGraphPin*> DraggedFromPins;
	FVector2D NewNodePosition;
	bool bAutoExpandActionMenu;

	SGraphEditor::FActionMenuClosed OnClosedCallback;

	TSharedPtr<SGraphActionMenu> GraphActionMenu;

	void OnActionSelected( const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedActions, ESelectInfo::Type InSelectionType );

	TSharedRef<SWidget> OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData);

	/** Callback used to populate all actions list in SGraphActionMenu */
	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions);

	friend class SSoundClassActionMenuItem;
};

