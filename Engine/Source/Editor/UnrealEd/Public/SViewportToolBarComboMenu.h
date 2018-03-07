// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Textures/SlateIcon.h"
#include "Widgets/SCompoundWidget.h"
#include "SViewportToolBar.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Widgets/Input/SCheckBox.h"

class SMenuAnchor;

/**
 * Custom widget to display a toggle/drop down menu. 
 *	Displayed as shown:
 * +--------+-------------+
 * | Toggle | Menu button |
 * +--------+-------------+
 */
class UNREALED_API SViewportToolBarComboMenu : public SCompoundWidget 
{
public:
	SLATE_BEGIN_ARGS(SViewportToolBarComboMenu) : _BlockLocation(EMultiBlockLocation::Start), _MinDesiredButtonWidth(-1.0f) {}
	
		/** We need to know about the toolbar we are in */
		SLATE_ARGUMENT( TSharedPtr<class SViewportToolBar>, ParentToolBar );

		/** Content for the drop down menu  */
		SLATE_EVENT( FOnGetContent, OnGetMenuContent )

		/** Called upon state change with the value of the next state */
		SLATE_EVENT( FOnCheckStateChanged, OnCheckStateChanged )
			
		/** Sets the current checked state of the checkbox */
		SLATE_ATTRIBUTE( ECheckBoxState, IsChecked )

		/** Icon shown on the toggle button */
		SLATE_ATTRIBUTE( FSlateIcon, Icon )

		/** Label shown on the menu button */
		SLATE_ATTRIBUTE( FText, Label )

		/** Overall style */
		SLATE_ATTRIBUTE( FName, Style )

		/** ToolTip shown on the menu button */
		SLATE_ATTRIBUTE( FText, MenuButtonToolTip )

		/** ToolTip shown on the toggle button */
		SLATE_ATTRIBUTE( FText, ToggleButtonToolTip )

		/** The button location */
		SLATE_ARGUMENT( EMultiBlockLocation::Type, BlockLocation )

		/** The minimum desired width of the menu button contents */
		SLATE_ARGUMENT( float, MinDesiredButtonWidth )

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
	void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

private:
	/** Our menus anchor */
	TSharedPtr<SMenuAnchor> MenuAnchor;

	/** Parent tool bar for querying other open menus */
	TWeakPtr<class SViewportToolBar> ParentToolBar;
};
