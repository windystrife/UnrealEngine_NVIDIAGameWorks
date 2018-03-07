// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "SViewportToolBar.h"
#include "Framework/SlateDelegates.h"

class SMenuAnchor;
struct FSlateBrush;

namespace EMenuItemType
{
	enum Type
	{
		Default,
		Header,
		Separator,
	};
}


/**
 * Widget that opens a menu when clicked
 */
class UNREALED_API SEditorViewportToolbarMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SEditorViewportToolbarMenu ){}
		/** We need to know about the toolbar we are in */
		SLATE_ARGUMENT( TSharedPtr<class SViewportToolBar>, ParentToolBar );
		/** The label to show in the menu */
		SLATE_ATTRIBUTE( FText, Label )
		/** Optional icon to display next to the label */
		SLATE_ATTRIBUTE( const FSlateBrush*, LabelIcon )
		/** The image to show in the menu.  If both the label and image are valid, the button image is used.  Note that if this image is used, the label icon will not be displayed. */
		SLATE_ARGUMENT( FName, Image )
		/** Content to show in the menu */
		SLATE_EVENT( FOnGetContent, OnGetMenuContent )
	SLATE_END_ARGS()

	/**
	 * Constructs the menu
	 */
	void Construct( const FArguments& Declaration );
private:
	/**
	 * Called when the menu button is clicked.  Will toggle the visibility of the menu content                   
	 */
	FReply OnMenuClicked();

	/**
	 * Called when the mouse enters a menu button.  If there was a menu previously opened
	 * we open this menu automatically
	 */
	void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

	EVisibility GetLabelIconVisibility() const;

protected:
	/** Parent tool bar for querying other open menus */
	TWeakPtr<class SViewportToolBar> ParentToolBar;

private:
	/** Our menus anchor */
	TSharedPtr<SMenuAnchor> MenuAnchor;
	TAttribute<const FSlateBrush*> LabelIconBrush;
};
