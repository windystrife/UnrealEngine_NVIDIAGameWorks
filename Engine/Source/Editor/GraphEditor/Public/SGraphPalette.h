// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "SGraphActionMenu.h"

//////////////////////////////////////////////////////////////////////////

/** Widget for displaying a single item  */
class GRAPHEDITOR_API SGraphPaletteItem : public SCompoundWidget
{
public:
	/** Delegate executed when mouse button goes down */
	FCreateWidgetMouseButtonDown MouseButtonDownDelegate;
	/** The item that we want to display with this widget */
	TWeakPtr<FEdGraphSchemaAction>	ActionPtr;
	/** Holds the inline renaming widget if one was created */
	TSharedPtr< SInlineEditableTextBlock > InlineRenameWidget;

public:
	SLATE_BEGIN_ARGS( SGraphPaletteItem ) {};
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FCreateWidgetForActionData* const InCreateData);

	// SWidget interface
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	// End of SWidget interface

protected:
	/** Creates an icon type widget, including big tooltip link */
	TSharedRef<SWidget> CreateIconWidget( const FText& IconToolTip, const FSlateBrush* IconBrush, const FSlateColor& IconColor, const FString& DocLink, const FString& DocExcerpt, const FSlateBrush* SecondaryIconBrush, const FSlateColor& SecondaryColor );
	/** Create an icon type widget */
	TSharedRef<SWidget> CreateIconWidget(const FText& IconToolTip, const FSlateBrush* IconBrush, const FSlateColor& IconColor);

	/* Create the text widget */
	virtual TSharedRef<SWidget> CreateTextSlotWidget( const FSlateFontInfo& NameFont, FCreateWidgetForActionData* const InCreateData, TAttribute<bool> bIsReadOnly );

	/** Callback when rename text is being verified on text changed */
	virtual bool OnNameTextVerifyChanged(const FText& InNewText, FText& OutErrorMessage);

	/** Callback when rename text is committed */
	virtual void OnNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit);

	/** Callback to get the display text */ 
	virtual FText GetDisplayText() const;

	/** Callback to get the tooltip */
	virtual FText GetItemTooltip() const;
};

//////////////////////////////////////////////////////////////////////////

class GRAPHEDITOR_API SGraphPalette : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SGraphPalette )
		{
			_AutoExpandActionMenu = false;
		}
		SLATE_ARGUMENT( bool, AutoExpandActionMenu )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGraphPalette() {};

protected:
	virtual TSharedRef<SWidget> OnCreateWidgetForAction(FCreateWidgetForActionData* const InCreateData);

	virtual FReply OnActionDragged(const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions, const FPointerEvent& MouseEvent);

	virtual void RefreshActionsList(bool bPreserveExpansion);

	/** Callback used to populate all actions list in SGraphActionMenu */
	virtual void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) = 0;

protected:
	TSharedPtr<SGraphActionMenu> GraphActionMenu;
};

