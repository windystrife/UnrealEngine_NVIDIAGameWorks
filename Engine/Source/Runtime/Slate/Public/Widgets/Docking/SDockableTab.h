// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/Docking/SDockTab.h"

class FMenuBuilder;
class SDockTabStack;

DECLARE_DELEGATE_TwoParams(
	FOnTabStackMenuOpening,
	/** Builder for the menu that will be opened */
	FMenuBuilder& ,
	/** Tab stack that this menu is being opened in */
	const TSharedRef<SDockTabStack>& );


/**
 * A tab widget that also holds on to some content that should be shown when this tab is selected.
 * Intended to be used in conjunction with SDockTabStack.
 */
class SLATE_API SDockableTab
{
public:
	DECLARE_DELEGATE_OneParam(FOnTabClosedCallback, TSharedRef<SDockableTab>);

	SLATE_BEGIN_ARGS(SDockableTab)
	{}

	SLATE_END_ARGS()

	/** Construct the widget from the declaration. */
	void Construct( const FArguments& InArgs );

	void SetContent( TSharedRef<SWidget> InContent );

	/** @return True if this tab is currently focused */
	bool IsActive() const;

	/** @return True if this tab appears active; False otherwise */
	bool IsForeground() const;


	/** @return the content associated with this tab */
	TSharedRef<SWidget> GetContent();

	/** Gets the dock tab stack this dockable tab resides within, if any */
	TSharedPtr<SDockTabStack> GetParentDockTabStack() const;

	/** Brings this tab to the front of its parent's tab well, if applicable. */
	void BringToFrontInParent();

	/** Should this tab be sized based on its content. */
	bool ShouldAutosize() const;

	/** Set delegate to invoke when tab stack menu is opening */
	void SetOnTabStackMenuOpening( const FOnTabStackMenuOpening& InHandler );

	/**
	 * Requests that the tab be closed.  Tabs may prevent closing depending on their state
	 */	
	void RequestCloseTab();

	/** 
	 * Pulls this tab out of it's parent tab stack and destroys it
	 * Note: This does not check if its safe to remove the tab.  Use RequestCloseTab to do this safely.
	 */
	void RemoveTabFromParent();

	/** 
	 * Make this tab active in its tabwell 
	 * @param	InActivationMethod	How this tab was activated.
	 */
	void ActivateInParent(ETabActivationCause InActivationCause);

protected:

	/** The tab's layout identifier */
	FName LayoutIdentifier;

	/** Is this an MajorTab? A tool panel tab? */
	ETabRole TabRole;

	/** The width that this tab will overlap with side-by-side tabs. */
	float OverlapWidth;

	/** The label on the tab */
	TAttribute<FString> TabLabel;
	
	/** Callback to call when this tab is destroyed */
	FOnTabClosedCallback OnTabClosed;

	/** Delegate to execute to determine if we can close this tab */
	SDockTab::FCanCloseTab OnCanCloseTab;

	/**
	 * The brush that the SDockTabStack should use to draw the content associated with this tab
	 * Documents, Apps, and Tool Panels have different backgrounds
	 */
	const FSlateBrush* ContentAreaBrush;

	TAttribute<FMargin> ContentAreaPadding;

	const FSlateBrush* TabWellBrush;

	FMargin TabWellPadding;

	/** Called when the the tab stack's context menu is open; gives the currently focused tab a change to add custom  */
	FOnTabStackMenuOpening OnTabStackMenuOpeningHandler;

	/** Should this tab be auto-sized based on its content? */
	bool bShouldAutosize;

	/** Color of this tab */
	FLinearColor TabColorScale;
};
