// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Textures/SlateIcon.h"
#include "Widgets/SCompoundWidget.h"
#include "SViewportToolBar.h"
#include "Framework/SlateDelegates.h"

class SMenuAnchor;

/**
 * Custom widget to display an icon/drop down menu. 
 *	Displayed as shown:
 * +--------+-------------+
 * | Icon     Menu button |
 * +--------+-------------+
 */
class UNREALED_API SViewportToolBarIconMenu : public SCompoundWidget 
{
public:
	SLATE_BEGIN_ARGS( SViewportToolBarIconMenu ){}
	
		/** We need to know about the toolbar we are in */
		SLATE_ARGUMENT( TSharedPtr<class SViewportToolBar>, ParentToolBar );

		/** Content for the drop down menu  */
		SLATE_EVENT( FOnGetContent, OnGetMenuContent )

		/** Icon shown */
		SLATE_ATTRIBUTE( FSlateIcon, Icon )

		/** Label shown on the menu button */
		SLATE_ATTRIBUTE( FText, Label )

		/** Overall style */
		SLATE_ATTRIBUTE( FName, Style )

	SLATE_END_ARGS( )

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );
	
private:
	/**
	 * Called when the menu button is clicked.  Will toggle the visibility of the menu content                   
	 */
	FReply OnMenuClicked();

	/**
	 * Called when the mouse enters a menu button.  If there was a menu previously opened
	 * we open this menu automatically
	 */
	void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

private:
	/** Our menus anchor */
	TSharedPtr<SMenuAnchor> MenuAnchor;

	/** Parent tool bar for querying other open menus */
	TWeakPtr<class SViewportToolBar> ParentToolBar;
};
