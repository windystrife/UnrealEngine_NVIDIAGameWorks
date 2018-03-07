// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/PopupMethodReply.h"
#include "Widgets/SWidget.h"
#include "Layout/Margin.h"
#include "Layout/Children.h"
#include "Widgets/SPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/IMenu.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;
class SWindow;

/** Notification when popup is opened/closed. */
DECLARE_DELEGATE_OneParam(FOnIsOpenChanged, bool)




/**
 * A PopupAnchor summons a Popup relative to its content.
 * Summoning a popup relative to the cursor is accomplished via the application.
 */
class SLATE_API SMenuAnchor : public SPanel, public IMenuHost
{
public:
	SLATE_BEGIN_ARGS( SMenuAnchor )
		: _Content()
		, _Padding(FMargin(0.f))
		, _MenuContent( SNew(STextBlock) .Text( NSLOCTEXT("SMenuAnchor", "NoMenuContent", "No Menu Content Assigned; use .MenuContent") ) )
		, _OnGetMenuContent()
		, _Placement( MenuPlacement_BelowAnchor )
		, _Method()
		, _ShouldDeferPaintingAfterWindowContent(true)
		, _UseApplicationMenuStack(true)
		, _IsCollapsedByParent(false)
		, _ApplyWidgetStyleToMenu(true)
		{}
		
		SLATE_DEFAULT_SLOT( FArguments, Content )

		SLATE_ARGUMENT( FMargin, Padding )
		
		SLATE_ARGUMENT( TSharedPtr<SWidget>, MenuContent )

		SLATE_EVENT( FOnGetContent, OnGetMenuContent )
		
		SLATE_EVENT( FOnIsOpenChanged, OnMenuOpenChanged )

		SLATE_ATTRIBUTE( EMenuPlacement, Placement )

		SLATE_ARGUMENT(TOptional<EPopupMethod>, Method)

		SLATE_ARGUMENT(bool, ShouldDeferPaintingAfterWindowContent)

		SLATE_ARGUMENT(bool, UseApplicationMenuStack)
		
		/** True if this menu anchor should be collapsed when its parent receives focus, false (default) otherwise */
		SLATE_ARGUMENT(bool, IsCollapsedByParent)

		/** True to apply the InWidgetStyle of the menu anchor when painting the popup, false to always paint the popup with full opacity and no tint. */
		SLATE_ARGUMENT(bool, ApplyWidgetStyleToMenu)

	SLATE_END_ARGS()

	virtual ~SMenuAnchor();
	
	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );
	
	SMenuAnchor();

	/** See Content Slot attribute */
	void SetContent(TSharedRef<SWidget> InContent);

	/** See MenuContent attribute */
	virtual void SetMenuContent(TSharedRef<SWidget> InMenuContent);
	
	/**
	 * Open or close the popup
	 *
	 * @param InIsOpen    If true, open the popup. Otherwise close it.
	 * @param bFocusMenu  Should we focus the popup as soon as it opens?
	 */
	virtual void SetIsOpen( bool InIsOpen, const bool bFocusMenu = true, const int32 FocusUserIndex = 0 );
	
	/** @return true if the popup is open; false otherwise. */
	bool IsOpen() const;

	/** @return true if we should open the menu due to a click. Sometimes we should not, if
	   the same MouseDownEvent that just closed the menu is about to re-open it because it happens to land on the button.*/
	bool ShouldOpenDueToClick() const;

	/** @return The current menu position */
	FVector2D GetMenuPosition() const;

	/** @return Whether this menu has open submenus */
	bool HasOpenSubMenus() const;

	// IMenuHost interface
	virtual TSharedPtr<SWindow> GetMenuWindow() const override;
	virtual void OnMenuDismissed() override;
	virtual bool UsingApplicationMenuStack() const override;
	// End of IMenuHost interface

	static void DismissAllApplicationMenus();

protected:
	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual bool ComputeVolatility() const override;
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	// End of SWidget interface

	/** @return true if the popup is currently open and reusing an existing window */
	bool IsOpenAndReusingWindow() const;

	/** @return true if the popup is currently open and we created a dedicated window for it */
	bool IsOpenViaCreatedWindow() const;

	/** Handler/callback called by menus created by this anchor, when they are dismissed */
	void OnMenuClosed(TSharedRef<IMenu> InMenu);

	/** Computes the placement geometry for menus displayed in a separately created window */
	FGeometry ComputeNewWindowMenuPlacement(const FGeometry& AllottedGeometry, const FVector2D& PopupDesiredSize, EMenuPlacement PlacementMode) const;

	static TArray<TWeakPtr<IMenu>> OpenApplicationMenus;

protected:
	struct SLATE_API FPopupPlacement
	{
		FPopupPlacement(const FGeometry& PlacementGeometry, const FVector2D& PopupDesiredSize, EMenuPlacement PlacementMode);
		
		FVector2D LocalPopupSize;
		FVector2D LocalPopupOffset;
		FSlateRect AnchorLocalSpace;
		EOrientation Orientation;
	};

	/**
	 * A pointer to the window presenting this popup.
	 * Can be the window created to hold a menu or the window containing this anchor if the menu is drawn as a child of the this anchor.
	 * Pointer is null when a popup is not visible.
	 */
	TWeakPtr<SWindow> PopupWindowPtr;

	/**
	 * An interface pointer to the menu object presenting this popup.
	 * Pointer is null when a popup is not visible.
	 */
	TWeakPtr<IMenu> PopupMenuPtr;

	/**
	 * An interface pointer to the menu object presenting this popup.
	 * This one is for menus owned by this widget and not by the application's menu stack
	 */
	TSharedPtr<IMenu> OwnedMenuPtr;

	/** Static menu content to use when the delegate used when OnGetMenuContent is not defined. */
	TSharedPtr<SWidget> MenuContent;

	/** MenuContent plus any extra wrapping widgets needed by the menu infrastructure. */
	TSharedPtr<SWidget> WrappedContent;
	
	/** Callback invoked when the popup is being summoned */
	FOnGetContent OnGetMenuContent;

	/** Callback invoked when the popup is being opened/closed */
	FOnIsOpenChanged OnMenuOpenChanged;

	/** How should the popup be placed relative to the anchor. */
	TAttribute<EMenuPlacement> Placement;

	/** Was the menu just dismissed this tick? */
	bool bDismissedThisTick;

	/** Whether this menu should be collapsed when its parent gets focus */
	bool bIsCollapsedByParent;

	/** Should we summon a new window for this popup or  */
	TOptional<EPopupMethod> Method;

	/** Method currently being used to show the popup. No value if popup is closed. */
	FPopupMethodReply MethodInUse;

	/** Should the menu content painting be deferred? If not, the menu content will layer over the menu anchor, rather than above all window contents. */
	bool bShouldDeferPaintingAfterWindowContent;

	/** Should the menu by created by the application stack code making it behave like and have the lifetime of a normal menu? */
	bool bUseApplicationMenuStack;

	/** Should we paint the popup using the received InWidgetStyle? */
	bool bApplyWidgetStyleToMenu;

	/**
	 * @todo Slate : Unify geometry so that this is not necessary.
	 * Stores the local offset of the popup as we have to compute it in OnTick;
	 * Cannot compute in OnArrangeChildren() because it can be
	 * called in different spaces (window or desktop.)
	 */
	FVector2D LocalPopupPosition;

	/**
	 * Screen-space version of LocalPopupPosition, also cached in Tick.
	 * Used to satisfy calls to GetMenuPosition() for menus that are reusing the current window.
	 */
	FVector2D ScreenPopupPosition;

	/** The currently arranged children in the menu anchor.  Changes as the opened/closed state of the widget changes. */
	TPanelChildren<FSimpleSlot> Children;
};
