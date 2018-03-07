// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"
#include "Widgets/Docking/SDockTab.h"

/////////////////////////////////////////////////////
// FTabInfo

namespace WorkflowTabManagerHelpers
{
	/** Max number of history items that can be stored.  Once the max is reached, the oldest history item is removed */
	const int32 MaxHistoryEntries = 300;
}

FTabInfo::FTabInfo(const TSharedRef<SDockTab>& InTab, const TSharedPtr<FDocumentTabFactory>& InSpawner)
	: Tab(InTab)
	, CurrentHistoryIndex(INDEX_NONE)
{
}

bool FTabInfo::PayloadMatches(const TSharedPtr<FTabPayload> TestPayload) const
{
	TSharedPtr<FTabPayload> CurrentPayload = History[CurrentHistoryIndex]->GetPayload();

	if (CurrentPayload.IsValid() && TestPayload.IsValid())
	{
		return CurrentPayload->IsEqual(TestPayload.ToSharedRef());
	}
	else if (!TestPayload.IsValid() && !CurrentPayload.IsValid())
	{
		return true;
	}
	else
	{
		return false;
	}
}

void FTabInfo::AddTabHistory(TSharedPtr< struct FGenericTabHistory > InHistoryNode, bool bInSaveHistory/* = true*/)
{
	// If the tab is not new, save the current history.
	if(CurrentHistoryIndex >= 0 && bInSaveHistory)
	{
		History[CurrentHistoryIndex]->SaveHistory();
	}

	if (CurrentHistoryIndex == History.Num() - 1)
	{
		// History added to the end
		if (History.Num() == WorkflowTabManagerHelpers::MaxHistoryEntries)
		{
			// If max history entries has been reached
			// remove the oldest history
			History.RemoveAt(0);
		}
	}
	else
	{
		// Clear out any history that is in front of the current location in the history list
		History.RemoveAt(CurrentHistoryIndex + 1, History.Num() - (CurrentHistoryIndex + 1), true);
	}

	History.Add(InHistoryNode);
	CurrentHistoryIndex = History.Num() - 1;

	// Evoke the history
	InHistoryNode->EvokeHistory(AsShared());
	InHistoryNode->GetFactory().Pin()->OnTabActivated(Tab.Pin());
}

FReply FTabInfo::OnGoForwardInHistory()
{
	if( CurrentHistoryIndex < History.Num() - 1 )
	{
		History[CurrentHistoryIndex]->SaveHistory();

		while( CurrentHistoryIndex < History.Num() - 1)
		{
			++CurrentHistoryIndex;

			if( History[CurrentHistoryIndex]->IsHistoryValid() )
			{
				History[CurrentHistoryIndex]->EvokeHistory(AsShared());
				History[CurrentHistoryIndex]->RestoreHistory();
				History[CurrentHistoryIndex]->GetFactory().Pin()->OnTabActivated(Tab.Pin());
				break;
			}
		}
	}
	return FReply::Handled();
}

FReply FTabInfo::OnGoBackInHistory()
{
	if( CurrentHistoryIndex > 0 )
	{
		History[CurrentHistoryIndex]->SaveHistory();

		while( CurrentHistoryIndex > 0 )
		{
			--CurrentHistoryIndex;
				
			if( History[CurrentHistoryIndex]->IsHistoryValid() )
			{
				History[CurrentHistoryIndex]->EvokeHistory(AsShared());
				History[CurrentHistoryIndex]->RestoreHistory();
				History[CurrentHistoryIndex]->GetFactory().Pin()->OnTabActivated(Tab.Pin());

				break;
			}
		}
	}
	return FReply::Handled();
}

void FTabInfo::JumpToNearestValidHistoryData()
{
	if(!History[CurrentHistoryIndex]->IsHistoryValid())
	{
		if(History.Num() == 1)
		{
			Tab.Pin()->RequestCloseTab();
		}
		else
		{
			OnGoBackInHistory();

			if(!History[CurrentHistoryIndex]->IsHistoryValid())
			{
				OnGoForwardInHistory();

				if(!History[CurrentHistoryIndex]->IsHistoryValid())
				{
					// There are no valid history nodes to switch to, delete the tab
					Tab.Pin()->RequestCloseTab();
					return;
				}
			}

			History[CurrentHistoryIndex]->EvokeHistory(AsShared());
			History[CurrentHistoryIndex]->RestoreHistory();
			History[CurrentHistoryIndex]->GetFactory().Pin()->OnTabActivated(Tab.Pin());
			FGlobalTabmanager::Get()->SetActiveTab(nullptr);
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
		}
	}
}

TWeakPtr<FDocumentTabFactory> FTabInfo::GetFactory() const
{
	return History[CurrentHistoryIndex]->GetFactory();
}

TSharedPtr<FTabPayload> FTabInfo::GetPayload() const
{
	return History[CurrentHistoryIndex]->GetPayload();
}

void FTabInfo::GoToHistoryIndex(int32 InHistoryIdx)
{
	if(History[InHistoryIdx]->IsHistoryValid())
	{
		History[CurrentHistoryIndex]->SaveHistory();

		CurrentHistoryIndex = InHistoryIdx;
		History[CurrentHistoryIndex]->EvokeHistory(AsShared());
		History[CurrentHistoryIndex]->RestoreHistory();
		History[CurrentHistoryIndex]->GetFactory().Pin()->OnTabActivated(Tab.Pin());
	}
}

TSharedRef<SWidget> FTabInfo::CreateHistoryMenu(bool bInBackHistory) const
{
	FMenuBuilder MenuBuilder(true, NULL);
	if(bInBackHistory)
	{
		int32 HistoryIdx = CurrentHistoryIndex - 1;
		while( HistoryIdx >= 0 )
		{
			if(History[HistoryIdx]->IsHistoryValid())
			{
				MenuBuilder.AddMenuEntry(History[HistoryIdx]->GetHistoryTitle().Get(), FText(), FSlateIcon(), 
					FUIAction(
					FExecuteAction::CreateRaw(this, &FTabInfo::GoToHistoryIndex, HistoryIdx)
					), 
					NAME_None, EUserInterfaceActionType::Button);
			}

			--HistoryIdx;
		}
	}
	else
	{
		int32 HistoryIdx = CurrentHistoryIndex + 1;
		while( HistoryIdx < History.Num() )
		{
			if(History[HistoryIdx]->IsHistoryValid())
			{
				MenuBuilder.AddMenuEntry(History[HistoryIdx]->GetHistoryTitle().Get(), FText(), FSlateIcon(), 
					FUIAction(
					FExecuteAction::CreateRaw(this, &FTabInfo::GoToHistoryIndex, HistoryIdx)
					), 
					NAME_None, EUserInterfaceActionType::Button);
			}

			++HistoryIdx;
		}
	}

	return MenuBuilder.MakeWidget();
}

bool FTabInfo::CanStepBackwardInHistory() const
{
	int32 HistoryIdx = CurrentHistoryIndex - 1;
	while( HistoryIdx >= 0 )
	{
		if(History[HistoryIdx]->IsHistoryValid())
		{
			return true;
		}

		--HistoryIdx;
	}
	return false;
}

bool FTabInfo::CanStepForwardInHistory() const
{
	int32 HistoryIdx = CurrentHistoryIndex + 1;
	while( HistoryIdx < History.Num() )
	{
		if(History[HistoryIdx]->IsHistoryValid())
		{
			return true;
		}

		++HistoryIdx;
	}
	return false;
}

FReply FTabInfo::OnMouseDownHisory( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TWeakPtr< SMenuAnchor > InMenuAnchor )
{
	if(MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && InMenuAnchor.IsValid())
	{
		InMenuAnchor.Pin()->SetIsOpen(true);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

TSharedRef< SWidget > FTabInfo::CreateHistoryNavigationWidget()
{
	if(!HistoryNavigationWidget.IsValid())
	{
		TWeakPtr< SMenuAnchor > BackMenuAnchorPtr;
		TWeakPtr< SMenuAnchor > FwdMenuAnchorPtr;
		HistoryNavigationWidget = 
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.OnMouseButtonDown(this, &FTabInfo::OnMouseDownHisory, BackMenuAnchorPtr)
				.BorderImage( FEditorStyle::GetBrush("NoBorder") )
				[
					SAssignNew(BackMenuAnchorPtr, SMenuAnchor)
					.Placement( MenuPlacement_BelowAnchor )
					.OnGetMenuContent( this, &FTabInfo::CreateHistoryMenu, true )
					[
						SNew(SButton)
						.OnClicked( this, &FTabInfo::OnGoBackInHistory )
						.ButtonStyle( FEditorStyle::Get(), "GraphBreadcrumbButton" )
						.IsEnabled(this, &FTabInfo::CanStepBackwardInHistory)
						.ToolTipText(NSLOCTEXT("WorkflowNavigationBrowser", "Backward_Tooltip", "Step backward in the tab history. Right click to see full history."))
						[
							SNew(SImage)
							.Image( FEditorStyle::GetBrush("GraphBreadcrumb.BrowseBack") )
						]
					]
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.OnMouseButtonDown(this, &FTabInfo::OnMouseDownHisory, FwdMenuAnchorPtr)
				.BorderImage( FEditorStyle::GetBrush("NoBorder") )
				[
					SAssignNew(FwdMenuAnchorPtr, SMenuAnchor)
					.Placement( MenuPlacement_BelowAnchor )
					.OnGetMenuContent( this, &FTabInfo::CreateHistoryMenu, false )
					[
						SNew(SButton)
						.OnClicked( this, &FTabInfo::OnGoForwardInHistory )
						.ButtonStyle( FEditorStyle::Get(), "GraphBreadcrumbButton" )
						.IsEnabled(this, &FTabInfo::CanStepForwardInHistory)
						.ToolTipText(NSLOCTEXT("WorkflowNavigationBrowser", "Forward_Tooltip", "Step forward in the tab history. Right click to see full history."))
						[
							SNew(SImage)
							.Image( FEditorStyle::GetBrush("GraphBreadcrumb.BrowseForward") )
						]
					]
				]
			];
	}

	return HistoryNavigationWidget.ToSharedRef();
}

/////////////////////////////////////////////////////
// FWorkflowAllowedTabSet

// Searches this set for a factory with the specified ID, or returns NULL.
TSharedPtr<class FWorkflowTabFactory> FWorkflowAllowedTabSet::GetFactory(FName FactoryID) const
{
	return Factories.FindRef(FactoryID);
}

// Registers a factory with this set
void FWorkflowAllowedTabSet::RegisterFactory(TSharedPtr<class FWorkflowTabFactory> Factory)
{
	FName NewIdentifier = Factory->GetIdentifier();
	check(!Factories.Contains(NewIdentifier));
	Factories.Add(NewIdentifier, Factory);
}

void FWorkflowAllowedTabSet::UnregisterFactory(FName FactoryID)
{
	int32 Removed = Factories.Remove(FactoryID);
	check(Removed != 0);
}

// Merges in a set of factories into this set
void FWorkflowAllowedTabSet::MergeInSet(const FWorkflowAllowedTabSet& OtherSet)
{
	Factories.Append(OtherSet.Factories);
}

// Clears the set
void FWorkflowAllowedTabSet::Clear()
{
	Factories.Empty();
}

TMap< FName, TSharedPtr<class FWorkflowTabFactory> >::TIterator FWorkflowAllowedTabSet::CreateIterator()
{
	return Factories.CreateIterator();
}

/////////////////////////////////////////////////////
// FDocumentTracker

void FDocumentTracker::ClearDocumentFactories()
{
	PotentialTabFactories.Empty();
}

void FDocumentTracker::RegisterDocumentFactory(TSharedPtr<class FDocumentTabFactory> Factory)
{
	FName NewIdentifier = Factory->GetIdentifier();
	check(!PotentialTabFactories.Contains(NewIdentifier));
	PotentialTabFactories.Add(NewIdentifier, Factory);
}

FDocumentTracker::FDocumentTracker()
{
	// Make sure we know when tabs become active
	OnActiveTabChangedDelegateHandle = FGlobalTabmanager::Get()->OnActiveTabChanged_Subscribe( FOnActiveTabChanged::FDelegate::CreateRaw( this, &FDocumentTracker::OnActiveTabChanged ) );
	TabForegroundedDelegateHandle = FGlobalTabmanager::Get()->OnTabForegrounded_Subscribe(FOnActiveTabChanged::FDelegate::CreateRaw(this, &FDocumentTracker::OnTabForegrounded));
}

FDocumentTracker::~FDocumentTracker()
{
	FGlobalTabmanager::Get()->OnActiveTabChanged_Unsubscribe(OnActiveTabChangedDelegateHandle);
	FGlobalTabmanager::Get()->OnTabForegrounded_Unsubscribe(TabForegroundedDelegateHandle);
}

// Called by the global active tab changed callback; dispatches to individually registered callbacks
void FDocumentTracker::OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated)
{
	FTabList& List = GetSpawnedList();
	for (auto ListIt = List.CreateIterator(); ListIt; ++ListIt)
	{
		// Get the factory (can't fail; the tabs had to come from somewhere; failing means a tab survived a mode transition to a mode where it is not allowed!)
		TSharedPtr<FDocumentTabFactory> Factory = (*ListIt)->GetFactory().Pin();
		if (ensure(Factory.IsValid()))
		{
			TSharedPtr<SDockTab> Tab = (*ListIt)->GetTab().Pin();
			if (Tab == NewlyActivated)
			{
				LastEditedTabInfo = *ListIt;
				Factory->OnTabActivated(Tab);
			}
		}
	}
}

void FDocumentTracker::OnTabForegrounded(TSharedPtr<SDockTab> ForegroundedTab, TSharedPtr<SDockTab> BackgroundedTab)
{
	TSharedPtr<SDockTab> OwnedForeground, OwnedBackground;
	TSharedPtr<FDocumentTabFactory> ForegroundFactory, BackgroundFactory;

	FTabList& List = GetSpawnedList();
	for ( auto ListIt = List.CreateIterator(); ListIt; ++ListIt )
	{
		// Get the factory (can't fail; the tabs had to come from somewhere; failing means a tab survived a mode transition to a mode where it is not allowed!)
		TSharedPtr<FDocumentTabFactory> Factory = ( *ListIt )->GetFactory().Pin();
		if ( ensure(Factory.IsValid()) )
		{
			TSharedPtr<SDockTab> Tab = ( *ListIt )->GetTab().Pin();
			if ( Tab == ForegroundedTab )
			{
				OwnedForeground = Tab;
				ForegroundFactory = Factory;
			}
			else if ( Tab == BackgroundedTab )
			{
				OwnedBackground = Tab;
				BackgroundFactory = Factory;
			}
		}
	}

	if ( OwnedBackground.IsValid() )
	{
		BackgroundFactory->OnTabBackgrounded(OwnedBackground);
	}

	if ( OwnedForeground.IsValid() )
	{
		ForegroundFactory->OnTabForegrounded(OwnedForeground);
	}
}

FDocumentTracker::FTabList& FDocumentTracker::GetSpawnedList()
{
	// Remove any closed tabs
	{
		struct
		{ 
			bool operator()(const TSharedPtr<FTabInfo>& RemovalCandidate) const
			{
				return !RemovalCandidate->GetTab().IsValid();
			}
		} CullClosedTabs;
	
		SpawnedTabs.RemoveAll( CullClosedTabs );
	}

	return SpawnedTabs;
}

void FDocumentTracker::Initialize(TSharedPtr<FAssetEditorToolkit> InHostingApp )
{
	check(!HostingAppPtr.IsValid());
	HostingAppPtr = InHostingApp;
}

void FDocumentTracker::SetTabManager( const TSharedRef<FTabManager>& InTabManager )
{
	TabManager = InTabManager;
}

TSharedPtr<SDockTab> FDocumentTracker::OpenDocument(TSharedPtr<FTabPayload> InPayload, EOpenDocumentCause InOpenCause)
{
	if(FSlateApplication::Get().GetModifierKeys().IsControlDown())
	{
		InOpenCause = FDocumentTracker::ForceOpenNewDocument;
	}

	if(InOpenCause == NavigatingCurrentDocument || InOpenCause == QuickNavigateCurrentDocument)
	{
		return NavigateCurrentTab(InPayload, InOpenCause);
	}
	else if(InOpenCause == OpenNewDocument || InOpenCause == CreateHistoryEvent)
	{
		TSharedPtr<SDockTab> Tab;

		// If the current tab matches we'll re-use it.
		if(LastEditedTabInfo.IsValid() && LastEditedTabInfo.Pin()->PayloadMatches(InPayload))
		{
			Tab = LastEditedTabInfo.Pin()->GetTab().Pin();
		}
		else
		{
			// Check if the payload is currently open in any tab
			FTabList& List = GetSpawnedList();
			for (auto ListIt = List.CreateIterator(); ListIt; ++ListIt)
			{
				if ((*ListIt)->PayloadMatches(InPayload))
				{
					Tab = (*ListIt)->GetTab().Pin();
					TabManager->DrawAttention( Tab.ToSharedRef() );
				}
			}
		}
		
		if(!Tab.IsValid())
		{
			// If no tab was found with the payload
			Tab = OpenNewTab(InPayload, OpenNewDocument);
		}
		else if(InOpenCause == CreateHistoryEvent)
		{
			// We are forcing a history event, navigate the current tab.
			Tab = NavigateCurrentTab(InPayload, NavigatingCurrentDocument);
		}
		
		return Tab;
	}
	else if(InOpenCause == NavigateBackwards || InOpenCause == NavigateForwards)
	{
		return NavigateCurrentTab(InPayload, InOpenCause);
	}

	// Occurs only when forcing open a new tab.
	return OpenNewTab(InPayload, InOpenCause);
}

TWeakPtr< FTabInfo > FDocumentTracker::FindTabInForeground()
{
	// Find a tab that is in the foreground
	FTabList& List = GetSpawnedList();
	for (auto ListIt = List.CreateIterator(); ListIt; ++ListIt)
	{
		if((*ListIt)->GetTab().Pin()->IsForeground())
		{
			return *ListIt;
		}
	}

	return NULL;
}

TSharedPtr<SDockTab> FDocumentTracker::NavigateCurrentTab(TSharedPtr<FTabPayload> InPayload, EOpenDocumentCause InNavigateCause)
{
	ensure(InNavigateCause == NavigatingCurrentDocument || InNavigateCause == QuickNavigateCurrentDocument || InNavigateCause == NavigateBackwards || InNavigateCause == NavigateForwards);

	FTabList& List = GetSpawnedList();
	if(List.Num())
	{
		// Make sure we find an available tab to navigate, there are ones available.
		if(!LastEditedTabInfo.IsValid())
		{
			LastEditedTabInfo = FindTabInForeground();

			// Check if we still do not have a valid tab, if we do not, activate the first tab
			if(!LastEditedTabInfo.IsValid())
			{
				LastEditedTabInfo = List[0];
				LastEditedTabInfo.Pin()->GetTab().Pin()->ActivateInParent(ETabActivationCause::SetDirectly);
			}
		}

		check(LastEditedTabInfo.IsValid());
		TSharedPtr<FAssetEditorToolkit> HostingApp = HostingAppPtr.Pin();
		FWorkflowTabSpawnInfo SpawnInfo;
		SpawnInfo.Payload = InPayload;
		SpawnInfo.TabInfo = LastEditedTabInfo.Pin();

		if(InNavigateCause == NavigatingCurrentDocument || InNavigateCause == QuickNavigateCurrentDocument)
		{
			TSharedPtr<FDocumentTabFactory> Factory = FindSupportingFactory(InPayload.ToSharedRef());
			// If doing a Quick navigate of the document, do not save history data as it's likely still at the default values. The object is always saved
			LastEditedTabInfo.Pin()->AddTabHistory(Factory->CreateTabHistoryNode(InPayload), InNavigateCause != QuickNavigateCurrentDocument);
			// Ensure that the tab appears if the tab isn't currently in the foreground.
			LastEditedTabInfo.Pin()->GetTab().Pin()->ActivateInParent(ETabActivationCause::SetDirectly);
		}
		else if(InNavigateCause == NavigateBackwards)
		{
			LastEditedTabInfo.Pin()->OnGoBackInHistory();
		}
		else if(InNavigateCause == NavigateForwards)
		{
			LastEditedTabInfo.Pin()->OnGoForwardInHistory();
		}

		return LastEditedTabInfo.Pin()->GetTab().Pin();
	}
	
	// Open in new tab
	return OpenNewTab(InPayload, OpenNewDocument);
}

TSharedPtr<SDockTab> FDocumentTracker::OpenNewTab(TSharedPtr<FTabPayload> InPayload, EOpenDocumentCause InOpenCause)
{
	ensure(InOpenCause == ForceOpenNewDocument ||InOpenCause == OpenNewDocument || InOpenCause == RestorePreviousDocument);

	TSharedPtr<FDocumentTabFactory> Factory = FindSupportingFactory(InPayload.ToSharedRef());

	TSharedPtr<SDockTab> NewTab;

	if(Factory.IsValid())
	{
		TSharedPtr<FAssetEditorToolkit> HostingApp = HostingAppPtr.Pin();
		FWorkflowTabSpawnInfo SpawnInfo;
		SpawnInfo.Payload = InPayload;

		NewTab = Factory->SpawnBlankTab();

		TSharedPtr<FTabInfo> NewTabInfo = MakeShareable( new FTabInfo(NewTab.ToSharedRef(), Factory) );
		SpawnedTabs.Add( NewTabInfo );
		NewTabInfo->AddTabHistory(Factory->CreateTabHistoryNode(InPayload));

		if (InOpenCause == ForceOpenNewDocument  || InOpenCause == OpenNewDocument)
		{
			TabManager->InsertNewDocumentTab( "Document", FTabManager::ESearchPreference::RequireClosedTab, NewTab.ToSharedRef() );
		}
		else if (InOpenCause == RestorePreviousDocument)
		{
			TabManager->RestoreDocumentTab( "Document", FTabManager::ESearchPreference::RequireClosedTab, NewTab.ToSharedRef() );
		}
	}

	return NewTab;
}

void FDocumentTracker::CloseTab(TSharedPtr<FTabPayload> Payload)
{
	FTabList& List = GetSpawnedList();
	for (auto ListIt = List.CreateIterator(); ListIt; ++ListIt)
	{
		if ((*ListIt)->PayloadMatches(Payload))
		{
			TSharedPtr<SDockTab> Tab = (*ListIt)->GetTab().Pin();
			Tab->RequestCloseTab();
		}
	}
}

void FDocumentTracker::CleanInvalidTabs()
{
	FTabList List = GetSpawnedList();
	for (auto ListIt = List.CreateIterator(); ListIt; ++ListIt)
	{
		(*ListIt)->JumpToNearestValidHistoryData();
	}
}

// Finds a factory that can deal with the supplied payload
TSharedPtr<FDocumentTabFactory> FDocumentTracker::FindSupportingFactory(TSharedRef<FTabPayload> Payload) const
{
	for (auto FactoryIt = PotentialTabFactories.CreateConstIterator(); FactoryIt; ++FactoryIt)
	{
		TSharedPtr<FDocumentTabFactory> Factory = FactoryIt.Value();

		if (Factory->IsPayloadSupported(Payload))
		{
			return Factory;
		}
	}

	return TSharedPtr<FDocumentTabFactory>();
}

// Finds all tabs that match the specified identifier and payload, placing them in the specified array
void FDocumentTracker::FindMatchingTabs( TSharedPtr<FTabPayload> Payload, /*inout*/ TArray< TSharedPtr<SDockTab> >& Results)
{
	FTabList& List = GetSpawnedList();
	for (auto ListIt = List.CreateIterator(); ListIt; ++ListIt)
	{
		if ((*ListIt)->PayloadMatches(Payload))
		{
			Results.Add((*ListIt)->GetTab().Pin());
		}
	}
}

// Finds all tabs that match the specified identifier, placing them in the specified array
void FDocumentTracker::FindAllTabsForFactory(const TWeakPtr<FDocumentTabFactory>& Factory, /*inout*/ TArray< TSharedPtr<SDockTab> >& Results)
{
	FTabList& List = GetSpawnedList();
	for (auto ListIt = List.CreateIterator(); ListIt; ++ListIt)
	{
		if ((*ListIt)->GetFactory() == Factory)
		{
			Results.Add((*ListIt)->GetTab().Pin());
		}
		
	}
}

// Saves the state of all tabs
//@TODO: DOCMANAGEMENT: Basically a total copy of RefreshAllTabs; need to make this sort of pattern be a delegate for-all-tabs
void FDocumentTracker::SaveAllState()
{
	for (auto ToolPanelIt = SpawnedTabs.CreateIterator(); ToolPanelIt; ++ToolPanelIt)
	{
		// Get the factory (can't fail; the tabs had to come from somewhere; failing means a tab survived a mode transition to a mode where it is not allowed!)
		TSharedPtr<FDocumentTabFactory> Factory = (*ToolPanelIt)->GetFactory().Pin();
		if (ensure(Factory.IsValid()) && (*ToolPanelIt)->GetTab().IsValid() && !(*ToolPanelIt)->GetTab().Pin()->IsForeground())
		{
			Factory->SaveState((*ToolPanelIt)->GetTab().Pin(), (*ToolPanelIt)->GetPayload());
		}
	}
	// Save the foreground tabs after saving the background tabs
	// This ensures foreground tabs always are restored after the background tabs.
	for (auto ToolPanelIt = SpawnedTabs.CreateIterator(); ToolPanelIt; ++ToolPanelIt)
	{
		TSharedPtr<FDocumentTabFactory> Factory = (*ToolPanelIt)->GetFactory().Pin();
		if (ensure(Factory.IsValid()) && (*ToolPanelIt)->GetTab().IsValid() && (*ToolPanelIt)->GetTab().Pin()->IsForeground())
		{
			Factory->SaveState((*ToolPanelIt)->GetTab().Pin(), (*ToolPanelIt)->GetPayload());
		}
	}
}

// Calls OnTabRefreshed for each open tab (on the factories that created them)
void FDocumentTracker::RefreshAllTabs() const
{
	for (auto ToolPanelIt = SpawnedTabs.CreateConstIterator(); ToolPanelIt; ++ToolPanelIt)
	{
		// Get the factory (can't fail; the tabs had to come from somewhere; failing means a tab survived a mode transition to a mode where it is not allowed!)
		TSharedPtr<FDocumentTabFactory> Factory = (*ToolPanelIt)->GetFactory().Pin();
		// Run thru the open tabs for this one
		if (ensure(Factory.IsValid()))
		{
			TSharedPtr<SDockTab> Tab = (*ToolPanelIt)->GetTab().Pin();
			if (Tab.IsValid())
			{
				Factory->OnTabRefreshed(Tab);
			}			
		}
	}
}

// Replaces the open payload in the specified tab with a new one; recreating the contents
void FDocumentTracker::ReplacePayloadInTab(TSharedPtr<SDockTab> TargetTab, TSharedPtr<FTabPayload> NewPayload)
{
	// Find the existing tab
	for (auto ToolPanelIt = SpawnedTabs.CreateIterator(); ToolPanelIt; ++ToolPanelIt)
	{
		TSharedPtr<SDockTab> Tab = (*ToolPanelIt)->GetTab().Pin();

		if (Tab == TargetTab)
		{
			// Get the factory (can't fail; the tabs had to come from somewhere; failing means a tab survived a mode transition to a mode where it is not allowed!)
			TSharedPtr<FDocumentTabFactory> Factory = (*ToolPanelIt)->GetFactory().Pin();
			if (ensure(Factory.IsValid()) && ensure(Factory->IsPayloadSupported(NewPayload.ToSharedRef())))
			{
				FWorkflowTabSpawnInfo Info;
				Info.Payload = NewPayload;
				Tab->SetContent(Factory->CreateTabBody(Info));

				return;
			}
		}
	}

}

TArray< TSharedPtr<SDockTab> > FDocumentTracker::GetAllDocumentTabs() const
{
	TArray< TSharedPtr<SDockTab> > AllSpawnedDocuments;
	for (int32 DocIndex=0; DocIndex < SpawnedTabs.Num(); ++DocIndex)
	{
		if (SpawnedTabs[DocIndex]->GetTab().IsValid())
		{
			AllSpawnedDocuments.Add(SpawnedTabs[DocIndex]->GetTab().Pin());
		}
	}

	return AllSpawnedDocuments;
}

TSharedPtr<SDockTab> FDocumentTracker::GetActiveTab() const
{
	if(LastEditedTabInfo.IsValid())
	{
		return LastEditedTabInfo.Pin()->GetTab().Pin();
	}

	return NULL;
}

FReply FDocumentTracker::OnNavigateTab(FDocumentTracker::EOpenDocumentCause InCause)
{
	NavigateCurrentTab(FTabPayload_UObject::Make(NULL), InCause);
	return FReply::Handled();
}
