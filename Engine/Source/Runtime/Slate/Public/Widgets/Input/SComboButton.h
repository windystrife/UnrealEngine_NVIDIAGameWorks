// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SMenuAnchor.h"

DECLARE_DELEGATE( FOnComboBoxOpened )

/**
 * A button that, when clicked, brings up a popup.
 */
class SLATE_API SComboButton : public SMenuAnchor
{
public:

	SLATE_BEGIN_ARGS( SComboButton )
		: _ComboButtonStyle( &FCoreStyle::Get().GetWidgetStyle< FComboButtonStyle >( "ComboButton" ) )
		, _ButtonStyle(nullptr)
		, _ButtonContent()
		, _MenuContent()
		, _IsFocusable(true)
		, _HasDownArrow(true)
		, _ForegroundColor(FCoreStyle::Get().GetSlateColor("InvertedForeground"))
		, _ButtonColorAndOpacity(FLinearColor::White)
		, _ContentPadding(FMargin(5))
		, _MenuPlacement(MenuPlacement_ComboBox)
		, _HAlign(HAlign_Fill)
		, _VAlign(VAlign_Center)
		, _Method()
		, _CollapseMenuOnParentFocus(false)
		{}

		SLATE_STYLE_ARGUMENT( FComboButtonStyle, ComboButtonStyle )

		/** The visual style of the button (overrides ComboButtonStyle) */
		SLATE_STYLE_ARGUMENT( FButtonStyle, ButtonStyle )
		
		SLATE_NAMED_SLOT( FArguments, ButtonContent )
		
		/** Optional static menu content.  If the menu content needs to be dynamically built, use the OnGetMenuContent event */
		SLATE_NAMED_SLOT( FArguments, MenuContent )
		
		/** Sets an event handler to generate a widget dynamically when the menu is needed. */
		SLATE_EVENT( FOnGetContent, OnGetMenuContent )
		SLATE_EVENT( FOnIsOpenChanged, OnMenuOpenChanged )
		
		SLATE_EVENT( FOnComboBoxOpened, OnComboBoxOpened )
		SLATE_ARGUMENT( bool, IsFocusable )
		SLATE_ARGUMENT( bool, HasDownArrow )

		SLATE_ATTRIBUTE( FSlateColor, ForegroundColor )
		SLATE_ATTRIBUTE( FSlateColor, ButtonColorAndOpacity )
		SLATE_ATTRIBUTE( FMargin, ContentPadding )
		SLATE_ATTRIBUTE( EMenuPlacement, MenuPlacement )
		SLATE_ARGUMENT( EHorizontalAlignment, HAlign )
		SLATE_ARGUMENT( EVerticalAlignment, VAlign )

		/** Spawn a new window or reuse current window for this combo*/
		SLATE_ARGUMENT( TOptional<EPopupMethod>, Method )

		/** True if this combo's menu should be collapsed when our parent receives focus, false (default) otherwise */
		SLATE_ARGUMENT(bool, CollapseMenuOnParentFocus)

	SLATE_END_ARGS()

	// SMenuAnchor interface
	virtual void SetMenuContent(TSharedRef<SWidget> InContent) override;
	// End of SMenuAnchor interface

	/** See the OnGetMenuContent event */
	void SetOnGetMenuContent( FOnGetContent InOnGetMenuContent );

	/**
	 * Construct the widget from a declaration
	 *
	 * @param InArgs  The declaration from which to construct
	 */
	void Construct(const FArguments& InArgs);

	void SetMenuContentWidgetToFocus( TWeakPtr<SWidget> InWidgetToFocusPtr )
	{
		WidgetToFocusPtr = InWidgetToFocusPtr;
	}

protected:
	/**
	 * Handle the button being clicked by summoning the ComboButton.
	 */
	virtual FReply OnButtonClicked();
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

protected:
	/** Area where the button's content resides */
	SHorizontalBox::FSlot* ButtonContentSlot;

	/** Delegate to execute when the combo list is opened */
	FOnComboBoxOpened OnComboBoxOpened;

	TWeakPtr<SWidget> WidgetToFocusPtr;

	/** Brush to use to add a "menu border" around the drop-down content */
	const FSlateBrush* MenuBorderBrush;

	/** Padding to use to add a "menu border" around the drop-down content */
	FMargin MenuBorderPadding;

	/** The content widget, if any, set by the user on creation */
	TWeakPtr<SWidget> ContentWidgetPtr;

	/** Can this button be focused? */
	bool bIsFocusable;
};
