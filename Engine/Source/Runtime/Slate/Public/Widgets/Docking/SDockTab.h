// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Styling/StyleDefaults.h"
#include "Animation/CurveSequence.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/Docking/TabManager.h"

class FActiveTimerHandle;
class SDockingArea;
class SDockingTabStack;
class SDockingTabWell;
class SImage;
class STextBlock;
class SToolTip;

/** How will this tab be used. */
enum ETabRole : uint8
{
	MajorTab,
	PanelTab,
	NomadTab,
	DocumentTab,
	NumRoles
};

	/** The cause of a tab activation */
enum ETabActivationCause : uint8
{
	UserClickedOnTab,
	SetDirectly
};

/**
 * A tab widget that also holds on to some content that should be shown when this tab is selected.
 * Intended to be used in conjunction with SDockingTabStack.
 */
class SLATE_API SDockTab : public SBorder
{
	friend class FTabManager;
public:

	/** Invoked when a tab is closing */
	DECLARE_DELEGATE_OneParam(FOnTabClosedCallback, TSharedRef<SDockTab>);
	
	/** Invoked when a tab is activated */
	DECLARE_DELEGATE_TwoParams(FOnTabActivatedCallback, TSharedRef<SDockTab>, ETabActivationCause);

	/** Invoked when this tab should save some information about its content. */
	DECLARE_DELEGATE(FOnPersistVisualState);

	/** Delegate called before a tab is closed.  Returning false will prevent the tab from closing */
	DECLARE_DELEGATE_RetVal( bool, FCanCloseTab );

	SLATE_BEGIN_ARGS(SDockTab)
		: _Content()
		, _TabWellContentLeft()
		, _TabWellContentRight()
		, _TabWellContentBackground()
		, _ContentPadding( FMargin( 2 ) )
		, _TabRole(ETabRole::PanelTab)
		, _Label()
		, _Icon( FStyleDefaults::GetNoBrush() )
		, _OnTabClosed()
		, _OnTabActivated()
		, _ShouldAutosize(false)
		, _OnCanCloseTab()
		, _OnPersistVisualState()
		, _TabColorScale(FLinearColor::Transparent)
		{}

		SLATE_DEFAULT_SLOT( FArguments, Content )
		SLATE_NAMED_SLOT( FArguments, TabWellContentLeft )
		SLATE_NAMED_SLOT( FArguments, TabWellContentRight )
		SLATE_NAMED_SLOT(FArguments, TabWellContentBackground)
		SLATE_ATTRIBUTE( FMargin, ContentPadding )
		SLATE_ARGUMENT( ETabRole, TabRole )
		SLATE_ATTRIBUTE( FText, Label )
		SLATE_ATTRIBUTE( const FSlateBrush*, Icon )
		SLATE_EVENT( FOnTabClosedCallback, OnTabClosed )
		SLATE_EVENT( FOnTabActivatedCallback, OnTabActivated )
		SLATE_ARGUMENT( bool, ShouldAutosize )
		SLATE_EVENT( FCanCloseTab, OnCanCloseTab )
		SLATE_EVENT( FOnPersistVisualState, OnPersistVisualState )
		SLATE_ATTRIBUTE( FLinearColor, TabColorScale )
	SLATE_END_ARGS()

	/** Construct the widget from the declaration. */
	void Construct( const FArguments& InArgs );

	// SWidget interface
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent  ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual FReply OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	virtual FReply OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent ) override;
	// End of SWidget interface

	// SBorder interface
	virtual void SetContent(TSharedRef<SWidget> InContent) override;
	// End of SBorder interface

	/** Content that appears in the TabWell to the left of the tabs */
	void SetLeftContent( TSharedRef<SWidget> InContent );
	/** Content that appears in the TabWell to the right of the tabs */
	void SetRightContent( TSharedRef<SWidget> InContent );
	/** Content that appears in the TabWell behind the tabs */
	void SetBackgroundContent( TSharedRef<SWidget> InContent );

	/** @return True if this tab is currently focused */
	bool IsActive() const;

	/** @return True if this tab appears active; False otherwise */
	bool IsForeground() const;

	/** Is this an MajorTab? A tool panel tab? */
	ETabRole GetTabRole() const;

	/** Similar to GetTabRole() but returns the correct role for UI style and user input purposes */
	ETabRole GetVisualTabRole() const;

	/**
	 * What should the content area look like for this type of tab?
	 * Documents, Apps, and Tool Panels have different backgrounds.
	 */
	const FSlateBrush* GetContentAreaBrush() const;

	/** Depending on the tabs we put into the tab well, we want a different background brush. */
	const FSlateBrush* GetTabWellBrush() const;

	/** @return the content associated with this tab */
	TSharedRef<SWidget> GetContent();
	TSharedRef<SWidget> GetLeftContent();
	TSharedRef<SWidget> GetRightContent();
	TSharedRef<SWidget> GetBackgrounfContent();

	/** Padding around the content when it is presented by the SDockingTabStack */
	FMargin GetContentPadding() const;

	/** Gets this tab's layout identifier */
	const FTabId& GetLayoutIdentifier() const;

	/** Sets the tab's tab well parent, or resets it if nothing is passed in */
	void SetParent(TSharedPtr<SDockingTabWell> Parent = TSharedPtr<SDockingTabWell>());

	/** Gets the tab's tab well parent, or nothing, if it has none */
	TSharedPtr<SDockingTabWell> GetParent() const;

	/** Gets the dock area that this resides in */
	TSharedPtr<class SDockingArea> GetDockArea() const;

	/** Get the window in which this tab's tabmanager has placed it */
	TSharedPtr<SWindow> GetParentWindow() const;

	/** The width that this tab will overlap with side-by-side tabs. */
	float GetOverlapWidth() const;

	/** The label on the tab */
	FText GetTabLabel() const;

	/** The label that appears on the tab. */
	void SetLabel( const TAttribute<FText>& InTabLabel );

	/** The tooltip text that appears on the tab. */
	void SetTabToolTipWidget(TSharedPtr<SToolTip> InTabToolTipWidget);

	/** Gets the tab icon */
	const FSlateBrush* GetTabIcon() const;

	/** Sets the tab icon */
	void SetTabIcon( const TAttribute<const FSlateBrush*> InTabIcon );

	/** Should this tab be sized based on its content. */
	bool ShouldAutosize() const;

	/** @return true if the tab can be closed */
	bool CanCloseTab() const;

	/** Requests that the tab be closed.  Tabs may prevent closing depending on their state */	
	bool RequestCloseTab();

	/** A chance for the tab's content to save any internal layout info */
	void PersistVisualState();

	/** 
	 * Pulls this tab out of it's parent tab stack and destroys it
	 * Note: This does not check if its safe to remove the tab.  Use RequestCloseTab to do this safely.
	 */
	void RemoveTabFromParent();

	/** Protected constructor; Widgets may only be constructed via a FArguments (i.e.: SNew(SDockTab) ) */
	SDockTab();

	/** 
	 * Make this tab active in its tabwell 
	 * @param	InActivationMethod	How this tab was activated.
	 */
	void ActivateInParent(ETabActivationCause InActivationCause);

	/** Set the tab manager that is controlling this tab */
	void SetTabManager( const TSharedPtr<FTabManager>& InTabManager );

	/**
	 * Set the custom code to execute for saving visual state in this tab.
	 * e.g. ContentBrowser saves the visible filters.
	 */
	void SetOnPersistVisualState( const FOnPersistVisualState& Handler );

	/** Set the handler to be invoked when the user requests that this tab be closed. */
	void SetCanCloseTab( const FCanCloseTab& InOnTabClosing );

	/** Set the handler that will be invoked when the tab is closed */
	void SetOnTabClosed( const FOnTabClosedCallback& InDelegate );

	/** Set the handler that will be invoked when the tab is activated */
	void SetOnTabActivated( const FOnTabActivatedCallback& InDelegate );

	/** Get the tab manager currently managing this tab. Note that a user move the tab between Tab Managers, so this return value may change. */
	TSharedRef<FTabManager> GetTabManager() const;

	/** Draws attention to the tab. */
	void DrawAttention();

	/** Provide a default tab label in case the spawner did not set one. */
	void ProvideDefaultLabel( const FText& InDefaultLabel );

	/** Provide a default tab icon in case the spawner did not set one. */
	void ProvideDefaultIcon( const FSlateBrush* InDefaultIcon );

	/** Play an animation showing this tab as opening */
	void PlaySpawnAnim();

	/** Flash the tab, used for drawing attention to it */
	void FlashTab();

	/** Used by the drag/drop operation to signal to this tab what it is dragging over. */
	void SetDraggedOverDockArea( const TSharedPtr<SDockingArea>& Area );

	/** 
	 * Check to see whether this tab has a sibling tab with the given tab ID
	 * 
	 * @param	SiblingTabId				The ID of the tab we want to find
	 * @param	TreatIndexNoneAsWildcard	Note that this variable only takes effect if SiblingTabId has an InstanceId of INDEX_NONE.
	 *										If true, we will consider this a "wildcard" search (matching any tab with the correct TabType, regardless 
	 *										of its InstanceId). If false, we will explicitly look for a tab with an InstanceId of INDEX_NONE
	 */
	bool HasSiblingTab(const FTabId& SiblingTabId, const bool TreatIndexNoneAsWildcard = true) const;

	/** Updates the 'last activated' time to the current time */
	void UpdateActivationTime();

	/** Returns the time this tab was last activated */
	double GetLastActivationTime()
	{
		return LastActivationTime;
	}
protected:
	
	/** Gets the dock tab stack this dockable tab resides within, if any */
	TSharedPtr<SDockingTabStack> GetParentDockTabStack() const;

	/** @return the style currently applied to the dock tab */
	const FDockTabStyle& GetCurrentStyle() const;

	/** @return the image brush that best represents this tab's in its current state */
	const FSlateBrush* GetImageBrush() const;

	/** @return the padding for the tab widget */
	FMargin GetTabPadding() const;

	/** @return the image brush for the tab's color overlay */
	const FSlateBrush* GetColorOverlayImageBrush() const;

	/** @return the image brush for the tab's active state overlay */
	const FSlateBrush* GetActiveTabOverlayImageBrush() const;

	/** @return Returns a color to scale the background of this tab by */
	FSlateColor GetTabColor() const;

	/** @return the image brush for the tab's flasher overlay */
	const FSlateBrush* GetFlashOverlayImageBrush() const;

	/** @return Returns a color to flash the background of this tab with */
	FSlateColor GetFlashColor() const;

	/** Called when the close button is clicked on the tab. */
	FReply OnCloseButtonClicked();

	/** The close button tooltip showing the appropriate close command shortcut */
	FText GetCloseButtonToolTipText() const;

	/** Specify the TabId that was used to spawn this tab. */
	void SetLayoutIdentifier( const FTabId& TabId );

	/** @return if the close button should be visible. */
	EVisibility HandleIsCloseButtonVisible() const;

private:
	/** Activates the tab in its tab well */
	EActiveTimerReturnType TriggerActivateTab( double InCurrentTime, float InDeltaTime );

	/** The handle to the active tab activation tick */
	TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;

protected:

	/** The tab manager that created this tab. */
	TWeakPtr<FTabManager> MyTabManager;

	/** The stuff to show when this tab is selected */
	TSharedRef<SWidget> Content;
	TSharedRef<SWidget> TabWellContentLeft;
	TSharedRef<SWidget> TabWellContentRight;
	TSharedRef<SWidget> TabWellContentBackground;

	
	/** The tab's layout identifier */
	FTabId LayoutIdentifier;

	/** Is this an MajorTab? A tool panel tab? */
	ETabRole TabRole;

	/** The tab's parent tab well. Null if it is a floating tab. */
	TWeakPtr<SDockingTabWell> ParentPtr;

	/** The label on the tab */
	TAttribute<FText> TabLabel;

	/** The icon on the tab */
	TAttribute<const FSlateBrush*> TabIcon;
	
	/** Callback to call when this tab is destroyed */
	FOnTabClosedCallback OnTabClosed;

	/** Callback to call when this tab is activated */
	FOnTabActivatedCallback OnTabActivated;

	/** Delegate to execute to determine if we can close this tab */
	FCanCloseTab OnCanCloseTab;

	/**
	 * Invoked during the Save Visual State pass; gives this tab a chance to save misc info about visual state.
	 * e.g. Content Browser might save the current filters, current folder, whether some panel is collapsed, etc.
	 */
	FOnPersistVisualState OnPersistVisualState;

	/** The styles used to draw the tab in its various states */
	const FDockTabStyle* MajorTabStyle;
	const FDockTabStyle* GenericTabStyle;

	TAttribute<FMargin> ContentAreaPadding;

	/** Should this tab be auto-sized based on its content? */
	bool bShouldAutosize;

	/** Color of this tab */
	TAttribute<FLinearColor> TabColorScale;

	/** @return the scaling of the tab based on the opening/closing animation */
	FVector2D GetAnimatedScale() const;

	/** Animation that shows the tab opening up */
	FCurveSequence SpawnAnimCurve;

	/** Animation that causes the tab to flash */
	FCurveSequence FlashTabCurve;

	/** Get the desired color of tab. These change during flashing. */
	float GetFlashValue() const;

	/** The dock area this tab is currently being dragged over. Used in nomad tabs to change style */
	TSharedPtr<SDockingArea> DraggedOverDockingArea;

	/** Widget used to show the label on the tab */
	TSharedPtr<STextBlock> LabelWidget;
	
	/** Widget used to show the icon on the tab */
	TSharedPtr<SImage> IconWidget;
	
	/** Time this tab was last activated */
	double LastActivationTime;
};
