// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Layout/Geometry.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Animation/CurveSequence.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Layout/SSplitter.h"
#include "SDockingNode.h"
#include "Widgets/Docking/SDockTab.h"

#define DEBUG_TAB_MANAGEMENT 0

class FUICommandList;
class FWeakWidgetPath;
class FWidgetPath;
class SDockingTabWell;

template<typename ChildType> class TSlotlessChildren;

/**
 * A node in the Docking/Tabbing hierarchy.
 * A DockTabStack shows a row of tabs and the content of one selected tab.
 * It also supports re-arranging tabs and dragging them out for the stack.
 */
class SLATE_API SDockingTabStack : public SDockingNode
{
public:
	SLATE_BEGIN_ARGS(SDockingTabStack)
			: _IsDocumentArea(false) {}
		SLATE_ARGUMENT( bool, IsDocumentArea )
	SLATE_END_ARGS()

	virtual Type GetNodeType() const override
	{
		return SDockingNode::DockTabStack;
	}

	void OnLastTabRemoved();

	void OnTabClosed( const TSharedRef<SDockTab>& ClosedTab );
	void OnTabRemoved( const FTabId& TabId );

	void Construct( const FArguments& InArgs, const TSharedRef<FTabManager::FStack>& PersistentNode );

	// TabStack methods

	void OpenTab( const TSharedRef<SDockTab>& InTab, int32 InsertAtLocation = INDEX_NONE );

	void AddTabWidget( const TSharedRef<SDockTab>& InTab, int32 AtLocation = INDEX_NONE);

	/** @return All child tabs in this node */
	const TSlotlessChildren<SDockTab>& GetTabs() const;
	
	/** @return How many tabs are in this node */
	int32 GetNumTabs() const;

	bool HasTab( const struct FTabMatcher& TabMatcher ) const;

	/** @return the last known geometry of this TabStack */
	FGeometry GetTabStackGeometry() const;

	void RemoveClosedTabsWithName( FName InName );

	bool IsShowingLiveTabs() const;

	void BringToFront( const TSharedRef<SDockTab>& TabToBringToFront );

	/** Set the content that the DockNode is presenting. */
	void SetNodeContent( const TSharedRef<SWidget>& InContent, const TSharedRef<SWidget>& InContentLeft, const TSharedRef<SWidget>& InContentRight, const TSharedRef<SWidget>& InContentBackground );

	virtual FReply OnUserAttemptingDock( SDockingNode::RelativeDirection Direction, const FDragDropEvent& DragDropEvent ) override;
	
	/** Recursively searches through all children looking for child tabs */
	virtual TArray< TSharedRef<SDockTab> > GetAllChildTabs() const override;

	virtual SSplitter::ESizeRule GetSizeRule() const override;

	void SetTabWellHidden( bool bShouldHideTabWell );
	bool IsTabWellHidden() const;

	virtual TSharedPtr<FTabManager::FLayoutNode> GatherPersistentLayout() const override;

	/** Elements for which we might want to reserve space. */
	enum class EChromeElement
	{
		Icon,
		Controls
	};

	/**
	 * Remove all the space that might have been reserved for
	 * various window chrome elements (app icons, minimize, close, etc.)
	 */
	void ClearReservedSpace();
	/**
	 * Add some extra padding so that the corresponding window chrome element
	 * does not overlap our tabs.
	 */
	void ReserveSpaceForWindowChrome(EChromeElement);

public:

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

	virtual void OnFocusChanging( const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent ) override;

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	virtual bool SupportsKeyboardFocus() const override { return true; }

protected:

	virtual EWindowZone::Type GetWindowZoneOverride() const override
	{
		// Pretend we are a title bar so the user can grab the area to move the window around
		return EWindowZone::TitleBar;
	}

	void CloseForegroundTab();

	enum ETabsToClose
	{
		CloseDocumentTabs,
		CloseDocumentAndMajorTabs,
		CloseAllTabs
	};
	/**
	 * Close all the background tabs.
	 *
	 * @param  TabsToClose   Close just document tabs or both document and tool tabs.
	 */
	void CloseAllButForegroundTab(ETabsToClose TabsToClose);

	FReply TabWellRightClicked( const FGeometry& TabWellGeometry, const FPointerEvent& MouseEvent );

	virtual SDockingNode::ECleanupRetVal CleanUpNodes() override;

	int32 OpenPersistentTab( const FTabId& TabId, int32 OpenLocationAmongActiveTabs = INDEX_NONE );

	int32 ClosePersistentTab( const FTabId& TabId );

	void RemovePersistentTab( const FTabId& TabId );

	/** Overridden from SDockingNode */
	virtual void SetParentNode( TSharedRef<class SDockingSplitter> InParent ) override;

private:
	
	/** Data that persists across sessions and when the widget associated with this node is removed. */	
	TArray<FTabManager::FTab> Tabs;


	/**
	 * Creates a SDockingTabStack by adding a new split to this stack's parent splitter and attaching the new SDockingTabStack.
	 * 
	 * @param	Direction	The relative direction to split the parent splitter
	 *
	 * @return	The newly created empty SDockingTabStack, ready for a tab to be added to it
	 */
	TSharedRef< SDockingTabStack > CreateNewTabStackBySplitting( const SDockingNode::RelativeDirection Direction );

	/** What should the content area look like for the current tab? */
	const FSlateBrush* GetContentAreaBrush() const;

	/** How much padding to show around the content currently being presented */
	FMargin GetContentPadding() const;

	/** Depending on the tabs we put into the tab well, we want a different background brush. */
	const FSlateBrush* GetTabWellBrush() const;

	/** Show the tab well? */
	EVisibility GetTabWellVisibility() const;

	/** Show the stuff needed to unhide the tab well? */
	EVisibility GetUnhideButtonVisibility() const;
	
	/** Show/Hide the tab well; do it smoothly with an animation */
	void ToggleTabWellVisibility();

	FReply UnhideTabWell();

	/** Only allow hiding the tab well when there is a single tab in it. */
	bool CanHideTabWell() const;

	/** Only allow closing the tab well when the tab allows it. */
	bool CanCloseForegroundTab() const;

	/** Only allow closing all other tabs when there are more then one tab open and the tab is of type Document or Major. */
	bool CanCloseAllButForegroundTab() const;

	/** Keep around our geometry from the last frame so that we can resize the preview windows correctly */
	FGeometry TabStackGeometry;

	/** The tab well widget shows all tabs, keeps track of the selected tab, allows tab rearranging, etc. */
	TSharedPtr<class SDockingTabWell> TabWell;

	/** The borders that hold any potential inline content areas. */
	SHorizontalBox::FSlot* InlineContentAreaLeft;
	SHorizontalBox::FSlot* InlineContentAreaRight;
	SOverlay::FOverlaySlot* BackgroundContentArea;
	SVerticalBox::FSlot* TitleBarSlot;
	TSharedPtr<SWidget> TitleBarContent;

	TSharedPtr<SBorder> ContentSlot;

	FOverlayManagement OverlayManagement;

	TSharedRef<SWidget> MakeContextMenu();

	/** Show the docking cross */
	void ShowCross();

	/** Hide the docking cross */
	void HideCross();

	/** Document Areas do not disappear when out of tabs, and instead say 'Document Area' */
	bool bIsDocumentArea;

	/** Animation that shows/hides the tab well; also used as a state machine to determine whether tab well is shown/hidden */
	FCurveSequence ShowHideTabWell;

	/** Grabs the scaling factor for the tab well size from the tab well animation. */
	FVector2D GetTabWellScale() const;

	/** Get the scale for the button that unhides the tab well */
	FVector2D GetUnhideTabWellButtonScale() const;
	/** Get the opacity for the button that unhides the tab well */
	FSlateColor GetUnhideTabWellButtonOpacity() const;

	/** @return Gets the visibility state for spacers that pad out the tab well to make room for title bar widgets */
	EVisibility GetTitleAreaSpacerVisibility();

	/** Visibility of TitleBar spacer based on maximize/restore status of the window.
	 ** This gives us a little more space to grab the title bar when the window is not maximized
	*/
	EVisibility GetMaximizeSpacerVisibility() const;

	/** Bind tab commands into the ActionList */
	void BindTabCommands();

	/** Attempts to close the foreground tab when the CloseMajorTab command is executed */
	void ExecuteCloseMajorTabCommand();

	/** Attempts to find the foreground tab that can be closed by the CloseMajorTab command */
	bool CanExecuteCloseMajorTabCommand();

	/** Attempts to close the active tab when the CloseMinorTab command is executed */
	void ExecuteCloseMinorTabCommand();

	/** Attempts to find the active tab that can be closed by the CloseMinorTab command */
	bool CanExecuteCloseMinorTabCommand();

#if DEBUG_TAB_MANAGEMENT
	FString ShowPersistentTabs() const;
#endif

	/** Tab command list */
	TSharedPtr<FUICommandList> ActionList;
};


struct FTabMatcher
{
	FTabMatcher( const FTabId& InTabId, ETabState::Type InTabState = static_cast<ETabState::Type>(ETabState::ClosedTab | ETabState::OpenedTab), const bool InTreatIndexNoneAsWildcard = true )
		: TabIdToMatch( InTabId )
		, RequiredTabState( InTabState )
		, TreatIndexNoneAsWildcard( InTreatIndexNoneAsWildcard )
	{
	}

	bool operator()(const FTabManager::FTab& Candidate) const
	{
		return
			((Candidate.TabState & RequiredTabState) != 0) &&
			(Candidate.TabId.TabType == TabIdToMatch.TabType) &&
			// INDEX_NONE is treated as a wildcard
			((TreatIndexNoneAsWildcard && TabIdToMatch.InstanceId == INDEX_NONE) || TabIdToMatch.InstanceId == Candidate.TabId.InstanceId);;
	}

	FTabId TabIdToMatch;
	ETabState::Type RequiredTabState;
	bool TreatIndexNoneAsWildcard;
};
