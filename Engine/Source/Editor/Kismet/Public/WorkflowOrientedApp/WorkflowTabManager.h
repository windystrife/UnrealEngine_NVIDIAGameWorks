// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Framework/Docking/TabManager.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"

class SMenuAnchor;
struct FGenericTabHistory;

/////////////////////////////////////////////////////

namespace EDocumentOpenBehavior
{
	enum Type
	{
		OnlyReturnExisting,
		AutomaticallyOpen
	};
}

/////////////////////////////////////////////////////
// FWorkflowAllowedTabSet

class KISMET_API FWorkflowAllowedTabSet
{
public:
	// Searches this set for a factory with the specified ID, or returns NULL.
	TSharedPtr<class FWorkflowTabFactory> GetFactory(FName FactoryID) const;

	// Registers a factory with this set - must be unique.
	void RegisterFactory(TSharedPtr<class FWorkflowTabFactory> Factory);

	// Unregisters a factory with this specified ID.
	void UnregisterFactory(FName FactoryID);

	// Merges in a set of factories into this set - duplicates are OK.
	void MergeInSet(const FWorkflowAllowedTabSet& OtherSet);

	// Clears the set
	void Clear();

	// Allows iteration over Factories
	TMap< FName, TSharedPtr<class FWorkflowTabFactory> >::TIterator CreateIterator();

private:
	// Map of available factories in this mode
	TMap< FName, TSharedPtr<class FWorkflowTabFactory> > Factories;

	friend class FDocumentTracker;
};

/////////////////////////////////////////////////////
// FTabInfo

class FTabInfo : public TSharedFromThis<FTabInfo>
{
public:
	FTabInfo(const TSharedRef<SDockTab>& InTab, const TSharedPtr<FDocumentTabFactory>& InSpawner);

	/** Returns TRUE if the payload used by this TabInfo is the same as passed in */
	bool PayloadMatches(const TSharedPtr<FTabPayload> TestPayload) const;

	/**
	 * Adds history data to immediately after the current location in the history list, wiping out any history after in the process
	 *
	 * @param InHistoryNode		The history node to add
	 * @param bInSaveHistory	TRUE if history should be saved
	 */
	void AddTabHistory(TSharedPtr< struct FGenericTabHistory > InHistoryNode, bool bInSaveHistory = true);

	/** Single step forward in history */
	FReply OnGoForwardInHistory();

	/** Single step back in history */
	FReply OnGoBackInHistory();

	/** Jumps backwards to the nearest valid history. If no history backwards is valid, jumps forward. In the case that there is no valid history, the tab is closed */
	void JumpToNearestValidHistoryData();

	/** Retrieves the factory from the current history data, which is being displayed by the tab */
	TWeakPtr<FDocumentTabFactory> GetFactory() const;

	/** Retrieves the payload from the current history data, which is being displayed by the tab */
	TSharedPtr<FTabPayload> GetPayload() const;

	/** Retrieves the tab used by this TabInfo */
	TWeakPtr<class SDockTab> GetTab() const
	{
		return Tab;
	}

	/** Jumps immediately to an index in the history if valid */
	void GoToHistoryIndex(int32 InHistoryIdx);

	/** Returns TRUE if stepping backward in history is allowed */
	bool CanStepBackwardInHistory() const;

	/** Returns TRUE if stepping forward in history is allowed */
	bool CanStepForwardInHistory() const;

	/** Creates the history navigation widget used for navigating back and forward in history */
	TSharedRef< SWidget > CreateHistoryNavigationWidget();

private:
	/**
	 * Mouse down callback to display a history menu
	 *
	 * @param InMenuAnchor		This is the anchor the menu will use for positioning
	 */
	FReply OnMouseDownHisory( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TWeakPtr< SMenuAnchor > InMenuAnchor );

	/** 
	 * Callback to create the history menu.
	 *
	 * @param bInBackHistory		TRUE if the back history is requested, FALSE if the forward history is
	 *
	 * @return						The menu widget displaying all available history
	 */
	TSharedRef<SWidget> CreateHistoryMenu(bool bInBackHistory) const;

private:	
	/** Cached history navigation widget */
	TSharedPtr<SWidget> HistoryNavigationWidget;
	/** Tab this info represents */
	TWeakPtr<class SDockTab> Tab;
	/** List of history data nodes */
	TArray< TSharedPtr< struct FGenericTabHistory > > History;
	/** Current history index */
	int32 CurrentHistoryIndex;
};

/////////////////////////////////////////////////////
// FGenericTabHistory

struct FGenericTabHistory
{
public:
	/**
	 * @param InFactory		The factory used to regenerate the content
	 * @param InPayload		The payload object used to regenerate the content
	 */
	FGenericTabHistory(TSharedPtr<class FDocumentTabFactory> InFactory, TSharedPtr<FTabPayload> InPayload)
		: FactoryPtr(InFactory)
		, Payload(InPayload)
	{

	}

	virtual ~FGenericTabHistory() {}

	/**
	 * Evokes the history data this struct represents, reinstating the payload and other data it had at the time the history was saved
	 *
	 * @param InTab				The tab to update with the history data
	 */
	virtual void EvokeHistory(TSharedPtr<FTabInfo> InTabInfo)
	{
		FWorkflowTabSpawnInfo SpawnInfo;
		SpawnInfo.Payload = Payload;
		SpawnInfo.TabInfo = InTabInfo;

		FactoryPtr.Pin()->UpdateTab(InTabInfo->GetTab().Pin(), SpawnInfo, FactoryPtr.Pin()->CreateTabBody(SpawnInfo));
	}

	/** Saves any important information from the payload into the history node */
	virtual void SaveHistory() {}

	/** Retrieves any important information from the history node and sets it in the payload */
	virtual void RestoreHistory() {}

	/** Returns the Payload this history node represents */
	TSharedPtr<FTabPayload> GetPayload() const
	{
		return Payload;
	}

	/** Returns the Factory this history node represents */
	TWeakPtr<FDocumentTabFactory> GetFactory() const
	{
		return FactoryPtr;
	}

	/**
	 * Checks if the payload for this history node is currently valid
	 *
	 * @return		TRUE if the history is valid
	 */
	bool IsHistoryValid() const
	{
		return FactoryPtr.Pin()->IsPayloadValid(Payload.ToSharedRef());
	}

	/** Returns the title used to display this history node */
	TAttribute<FText> GetHistoryTitle() const
	{
		FWorkflowTabSpawnInfo SpawnInfo;
		SpawnInfo.Payload = Payload;

		return FactoryPtr.Pin()->ConstructTabName(SpawnInfo);
	}

protected:
	/** The factory used to generate widgets for the payload */
	TWeakPtr<class FDocumentTabFactory> FactoryPtr;

	/** The payload this history node represents */
	TSharedPtr<FTabPayload> Payload;
};

/////////////////////////////////////////////////////
// FDocumentTracker

DECLARE_DELEGATE_OneParam(FOnTabActivated, TSharedPtr<SDockTab>);

class KISMET_API FDocumentTracker : public TSharedFromThis<FDocumentTracker>
{
public:
	FDocumentTracker();
	~FDocumentTracker();

	void ClearDocumentFactories();
	void RegisterDocumentFactory(TSharedPtr<class FDocumentTabFactory> Factory);

	// Creates a new tab manager
	void Initialize(TSharedPtr<FAssetEditorToolkit> InHostingApp);

	void SetTabManager(const TSharedRef<FTabManager>& InTabManager);

	/** Are we opening a new document, or restoring a previously opened document */
	enum EOpenDocumentCause
	{
		OpenNewDocument,
		ForceOpenNewDocument,
		RestorePreviousDocument,
		NavigatingCurrentDocument,
		QuickNavigateCurrentDocument,
		NavigateBackwards,
		NavigateForwards,
		CreateHistoryEvent
	};

	/**
	 * Handles opening a document and handling tab navigation or forcing a new tab to be opened
	 *
	 * @param InPayload					Payload to use for the tab
	 * @param InOpenCause				The cause of the document being opened
	 *
	 * @return							The tab modified
	 */
	TSharedPtr<SDockTab> OpenDocument(TSharedPtr<FTabPayload> InPayload, EOpenDocumentCause InOpenCause);

	// Closes any tabs of the specified type that match the payload (or all of them if the payload is NULL)
	void CloseTab(TSharedPtr<FTabPayload> Payload);

	/** Cleans any invalid tabs, either moving them to a valid place in history or closing the tab altogether */
	void CleanInvalidTabs();

	// Calls OnTabRefreshed for each open tab (on the factories that created them)
	void RefreshAllTabs() const;

	// Finds a factory that can deal with the supplied payload
	TSharedPtr<FDocumentTabFactory> FindSupportingFactory(TSharedRef<FTabPayload> Payload) const;

	// Finds all tabs that match the specified identifier and optional payload, placing them in the specified array
	void FindMatchingTabs(TSharedPtr<FTabPayload> Payload, /*inout*/ TArray< TSharedPtr<SDockTab> >& Results);

	// Finds all tabs that match the specified identifier, placing them in the specified array
	void FindAllTabsForFactory(const TWeakPtr<FDocumentTabFactory>& Factory, /*inout*/ TArray< TSharedPtr<SDockTab> >& Results);

	// Saves the state of all tabs
	void SaveAllState();

	// Replaces the open payload in the specified tab with a new one; recreating the contents
	void ReplacePayloadInTab(TSharedPtr<SDockTab> TargetTab, TSharedPtr<FTabPayload> NewPayload);

	/** Get all the live documents tracked by this tracker */
	TArray< TSharedPtr<SDockTab> > GetAllDocumentTabs() const;

	/** Returns the current active tab */
	TSharedPtr<SDockTab> GetActiveTab() const;
private:

	// List of open tabs; used to bring one to front and focus it
	typedef TArray< TSharedPtr< FTabInfo > > FTabList;
private:
	// List of currently spawned tool panels
	FTabList SpawnedTabs;
	
	// List of allowable tab spawning mechanisms
	TMap< FName, TSharedPtr<class FDocumentTabFactory> > PotentialTabFactories;

	// The hosting application we're associated with
	TWeakPtr<FAssetEditorToolkit> HostingAppPtr;

	// The Slate tab manager for the application we are managing documents for
	TSharedPtr<FTabManager> TabManager;

	// The last active tab info to be selected, used for opening new documents in the most recently used tab
	TWeakPtr<FTabInfo> LastEditedTabInfo;

	/** Handle to the registered OnActiveTabChanged delegate */
	FDelegateHandle OnActiveTabChangedDelegateHandle;

	/** Handle to the registered OnTabForegrounded delegate */
	FDelegateHandle TabForegroundedDelegateHandle;

private:
	// Clean the spawned list 
	FTabList& GetSpawnedList();

	// Called by the global active tab changed callback; dispatches to individually registered callbacks
	void OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated);

	// Called by the global tab manager foregrounds a tab; dispatches to individually registered callbacks
	void OnTabForegrounded(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated);

	/** Callback for navigating tabs with buttons */
	FReply OnNavigateTab(FDocumentTracker::EOpenDocumentCause InCause);

	/** Helper function to find the tab in the foreground */
	TWeakPtr< FTabInfo > FindTabInForeground();

	/**
	 * Navigates current tab
	 *
	 * @param InPayload					Payload to use for the tab
	 * @param InNavigateCause			The cause of the navigation
	 *
	 * @return							The tab navigated and modified
	 */
	TSharedPtr<SDockTab> NavigateCurrentTab(TSharedPtr<FTabPayload> InPayload, EOpenDocumentCause InNavigateCause);

	/**
	 * Opens a new tab
	 *
	 * @param InPayload					Payload to use for the tab
	 * @param InOpenCause				The cause of the tab being opened
	 *
	 * @return							The tab opened and modified
	 */
	TSharedPtr<SDockTab> OpenNewTab(TSharedPtr<FTabPayload> InPayload, EOpenDocumentCause InOpenCause);
};

