// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Docking/SDockingArea.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/SDockingTabStack.h"
#include "Framework/Docking/SDockingTarget.h"
#include "Framework/Docking/FDockingDragOperation.h"
#include "HAL/PlatformApplicationMisc.h"


void SDockingArea::Construct( const FArguments& InArgs, const TSharedRef<FTabManager>& InTabManager, const TSharedRef<FTabManager::FArea>& PersistentNode )
{
	MyTabManager = InTabManager;
	InTabManager->GetPrivateApi().OnDockAreaCreated( SharedThis(this) );

	bManageParentWindow = InArgs._ShouldManageParentWindow;
	bIsOverlayVisible = false;

	bIsCenterTargetVisible = false;

	const TAttribute<EVisibility> TargetCrossVisibility = TAttribute<EVisibility>(SharedThis(this), &SDockingArea::TargetCrossVisibility);
	const TAttribute<EVisibility> TargetCrossCenterVisibility = TAttribute<EVisibility>(SharedThis(this), &SDockingArea::TargetCrossCenterVisibility);

	// In DockSplitter mode we just act as a thin shell around a Splitter widget
	this->ChildSlot
	[
		SNew(SOverlay)
		.Visibility( EVisibility::SelfHitTestInvisible )
		+SOverlay::Slot()
		[
			SAssignNew(Splitter, SSplitter)
			. Orientation( PersistentNode->GetOrientation() )
		]
		
		+ SOverlay::Slot()
		// Houses the minimize, maximize, restore, and icon
		.Expose(WindowControlsArea)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		
		+SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		[
			SNew(SDockingTarget)
			.Visibility(TargetCrossVisibility)
			.OwnerNode( SharedThis(this) )
			.DockDirection( SDockingNode::LeftOf )
		]
		+SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		[
			SNew(SDockingTarget)
			.Visibility(TargetCrossVisibility)
			.OwnerNode( SharedThis(this) )
			.DockDirection( SDockingNode::RightOf )
		]
		+SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SNew(SDockingTarget)
			.Visibility(TargetCrossVisibility)
			.OwnerNode( SharedThis(this) )
			.DockDirection( SDockingNode::Above )
		]
		+SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		[
			SNew(SDockingTarget)
			.Visibility(TargetCrossVisibility)
			.OwnerNode( SharedThis(this) )
			.DockDirection( SDockingNode::Below )
		]
		+SOverlay::Slot()
		[
			SNew(SDockingTarget)
			.Visibility(TargetCrossCenterVisibility)
			.OwnerNode( SharedThis(this) )
			.DockDirection( SDockingNode::Center )
		]
	];



	bCleanUpUponTabRelocation = false;

	// If the owner window is set and bManageParentWindow is true, this docknode will close the window when its last tab is removed.
	if (InArgs._ParentWindow.IsValid())
	{
		SetParentWindow(InArgs._ParentWindow.ToSharedRef());
	}
	
		
	// Add initial content if it was provided
	if ( InArgs._InitialContent.IsValid() )
	{
		AddChildNode( InArgs._InitialContent.ToSharedRef() );
	}
}

void SDockingArea::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDockingDragOperation> DragDropOperation = DragDropEvent.GetOperationAs<FDockingDragOperation>();
	if ( DragDropOperation.IsValid() )
	{
		if ( DragDropOperation->CanDockInNode(SharedThis(this), FDockingDragOperation::DockingViaTarget) )
		{
			ShowCross();
		}		
	}

}

void SDockingArea::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FDockingDragOperation>().IsValid() )
	{
		HideCross();
	}
}

FReply SDockingArea::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if ( DragDropEvent.GetOperationAs<FDockingDragOperation>().IsValid() )
	{
		HideCross();
	}

	return FReply::Unhandled();
}

FReply SDockingArea::OnUserAttemptingDock( SDockingNode::RelativeDirection Direction, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDockingDragOperation> DragDropOperation = DragDropEvent.GetOperationAs<FDockingDragOperation>();
	if ( DragDropOperation.IsValid() )
	{
		if (Direction == Center)
		{
			//check(Children.Num() <= 1);

			TSharedRef<SDockingTabStack> NewStack = SNew(SDockingTabStack, FTabManager::NewStack());
			AddChildNode( NewStack );
			NewStack->OpenTab( DragDropOperation->GetTabBeingDragged().ToSharedRef() );
		}
		else
		{
			DockFromOutside( Direction, DragDropEvent );
		}		

		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

void SDockingArea::OnTabFoundNewHome( const TSharedRef<SDockTab>& RelocatedTab, const TSharedRef<SWindow>& NewOwnerWindow )
{
	HideCross();

	// The last tab has been successfully relocated elsewhere.
	// Destroy this window.
	TSharedPtr<SWindow> ParentWindow = ParentWindowPtr.Pin();
	if ( bManageParentWindow && bCleanUpUponTabRelocation && ParentWindow != NewOwnerWindow )
	{
		ParentWindow->SetRequestDestroyWindowOverride( FRequestDestroyWindowOverride() );
		ParentWindow->RequestDestroyWindow();
	}
}

TSharedPtr<SDockingArea> SDockingArea::GetDockArea()
{
	return SharedThis(this);
}

TSharedPtr<const SDockingArea> SDockingArea::GetDockArea() const
{
	return SharedThis(this);
}

TSharedPtr<SWindow> SDockingArea::GetParentWindow() const
{
	return ParentWindowPtr.IsValid() ? ParentWindowPtr.Pin() : TSharedPtr<SWindow>();
}

void SDockingArea::ShowCross()
{
	bIsOverlayVisible = true;
}

void SDockingArea::HideCross()
{
	bIsOverlayVisible = false;
}

void SDockingArea::CleanUp( ELayoutModification RemovalMethod )
{
	const ECleanupRetVal CleanupResult = CleanUpNodes();
	
	if ( CleanupResult != VisibleTabsUnderNode )
	{
		bIsCenterTargetVisible = true;
		// We may have a window to manage.
		TSharedPtr<SWindow> ParentWindow = ParentWindowPtr.Pin();
		if ( bManageParentWindow && ParentWindow.IsValid() )
		{
			if (RemovalMethod == TabRemoval_Closed)
			{
				MyTabManager.Pin()->GetPrivateApi().OnDockAreaClosing( SharedThis(this) );
				ParentWindow->RequestDestroyWindow();
			}
			else if ( RemovalMethod == TabRemoval_DraggedOut )
			{
				// We can't actually destroy this due to limitations of some platforms.
				// Just hide the window. We will destroy it when the drag and drop is done.
				bCleanUpUponTabRelocation = true;
				ParentWindow->HideWindow();
				MyTabManager.Pin()->GetPrivateApi().OnDockAreaClosing( SharedThis(this) );
			}
			else
			{
				ensure( RemovalMethod == TabRemoval_None );
			}
		}
	}
	else
	{
		bIsCenterTargetVisible = false;
	}

	// In some cases a dock area will control the window,
	// and we need to move some of the tabs out of the way
	// to make room for window chrome.
	MakeRoomForWindowChrome();
}

void SDockingArea::SetParentWindow( const TSharedRef<SWindow>& NewParentWindow )
{
	if( bManageParentWindow )
	{
		NewParentWindow->SetRequestDestroyWindowOverride( FRequestDestroyWindowOverride::CreateSP( this, &SDockingArea::OnOwningWindowBeingDestroyed ) );
	}

	// Even thought we don't manage the parent window's lifetime, we are still responsible for making its window chrome.
	{
		TSharedPtr<IWindowTitleBar> TitleBar;
		TSharedRef<SWidget> TitleBarWidget = FSlateApplication::Get().MakeWindowTitleBar(NewParentWindow, SNullWidget::NullWidget, HAlign_Fill, TitleBar);

		(*WindowControlsArea)[TitleBarWidget];
		NewParentWindow->SetTitleBar(TitleBar);
	}

	ParentWindowPtr = NewParentWindow;
	NewParentWindow->GetOnWindowActivatedEvent().AddSP(this, &SDockingArea::OnOwningWindowActivated);
}

TSharedPtr<FTabManager::FLayoutNode> SDockingArea::GatherPersistentLayout() const
{
	// Assume that all the nodes were dragged out, and there's no meaningful layout data to be gathered.
	bool bHaveLayoutData = false;

	TSharedPtr<FTabManager::FArea> PersistentNode;

	TSharedPtr<SWindow> ParentWindow = ParentWindowPtr.Pin();
	if ( ParentWindow.IsValid() && bManageParentWindow )
	{
		FSlateRect WindowRect = ParentWindow->GetNonMaximizedRectInScreen();

		// In order to restore SWindows to their correct size, we need to save areas as 
		// client area sizes, since the Constructor for SWindow uses a client size
		if (!ParentWindow->HasOSWindowBorder())
		{
			const FMargin WindowBorder = ParentWindow->GetWindowBorderSize();
			WindowRect.Right -= WindowBorder.Left + WindowBorder.Right;
			WindowRect.Bottom -= WindowBorder.Top + WindowBorder.Bottom;
		}

		// Remove DPI Scale when saving layout so that the saved size is DPI independent
		float DPIScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(WindowRect.Left, WindowRect.Top);

		PersistentNode = FTabManager::NewArea( WindowRect.GetSize() / DPIScale );
		PersistentNode->SetWindow( FVector2D( WindowRect.Left, WindowRect.Top ) / DPIScale, ParentWindow->IsWindowMaximized() );
	}
	else
	{
		// An area without a window persists because it must be a primary area.
		// Those must always be restored, even if they are empty.
		PersistentNode = FTabManager::NewPrimaryArea();
		bHaveLayoutData = true;
	}	

	PersistentNode->SetOrientation( this->GetOrientation() );

	for (int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		TSharedPtr<FTabManager::FLayoutNode> PersistentChild = Children[ChildIndex]->GatherPersistentLayout();
		if ( PersistentChild.IsValid() )
		{
			bHaveLayoutData = true;
			PersistentNode->Split( PersistentChild.ToSharedRef() );
		}
	}

	if( !bHaveLayoutData )
	{
		PersistentNode.Reset();
	}

	return PersistentNode;
}

TSharedRef<FTabManager> SDockingArea::GetTabManager() const
{
	return MyTabManager.Pin().ToSharedRef();
}

SDockingNode::ECleanupRetVal SDockingArea::CleanUpNodes()
{
	SDockingNode::ECleanupRetVal ReturnValue = SDockingSplitter::CleanUpNodes();
	return ReturnValue;
}

EVisibility SDockingArea::TargetCrossVisibility() const
{
	return (bIsOverlayVisible && !bIsCenterTargetVisible)
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SDockingArea::TargetCrossCenterVisibility() const
{
	return (bIsOverlayVisible && bIsCenterTargetVisible)
		? EVisibility::Visible
		: EVisibility::Collapsed;
}


void SDockingArea::DockFromOutside(SDockingNode::RelativeDirection Direction, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FDockingDragOperation> DragDropOperation = StaticCastSharedPtr<FDockingDragOperation>(DragDropEvent.GetOperation());
		
	//
	// Dock from outside.
	//
	const bool bDirectionMatches = DoesDirectionMatchOrientation( Direction, this->Splitter->GetOrientation() );

	if (!bDirectionMatches && Children.Num() > 1)
	{
		// We have multiple children, but the user wants to add a new node that's perpendicular to their orientation.
		// We need to nest our children into a child splitter so that we can re-orient ourselves.
		{
			// Create a new, re-oriented splitter and copy all the children into it.
			TSharedRef<SDockingSplitter> NewSplitter = SNew(SDockingSplitter, FTabManager::NewSplitter()->SetOrientation(Splitter->GetOrientation()) );
			for( int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex )
			{
				NewSplitter->AddChildNode( Children[ChildIndex], INDEX_NONE );
			}

			// Remove all our children.
			while( Children.Num() > 0 )
			{
				RemoveChildAt(Children.Num()-1);
			}
				
			AddChildNode( NewSplitter );
		}

		// Re-orient ourselves
		const EOrientation NewOrientation = (this->Splitter->GetOrientation() == Orient_Horizontal)
			? Orient_Vertical
			: Orient_Horizontal;

		this->SetOrientation( NewOrientation );
	}

	// Add the new node.
	{
		TSharedRef<SDockingTabStack> NewStack = SNew(SDockingTabStack, FTabManager::NewStack());

		if ( Direction == LeftOf || Direction == Above )
		{
			this->PlaceNode( NewStack, Direction, Children[0] );
		}
		else
		{
			this->PlaceNode( NewStack, Direction, Children.Last() );
		}

		NewStack->OpenTab( DragDropOperation->GetTabBeingDragged().ToSharedRef() );
	}

	HideCross();
}

void SDockingArea::OnOwningWindowBeingDestroyed(const TSharedRef<SWindow>& WindowBeingDestroyed)
{
	TArray< TSharedRef<SDockTab> > AllTabs = GetAllChildTabs();
	
	// Save the visual states of all the tabs.
	for (int32 TabIndex=0; TabIndex < AllTabs.Num(); ++TabIndex)
	{
		AllTabs[TabIndex]->PersistVisualState();
	}

	// First check if it's ok to close all the tabs that we have.
	bool CanDestroy = true;
	for (int32 TabIndex=0; CanDestroy && TabIndex < AllTabs.Num(); ++TabIndex)
	{
		CanDestroy = AllTabs[TabIndex]->CanCloseTab();
	}

	if ( CanDestroy )
	{
		// It's cool to close all tabs, so destroy them all and destroy the window.
		for (int32 TabIndex=0; TabIndex < AllTabs.Num(); ++TabIndex)
		{
			AllTabs[TabIndex]->RemoveTabFromParent();
		}

		// Destroy the window
		FSlateApplication::Get().RequestDestroyWindow(WindowBeingDestroyed);
	}
	else
	{
		// Some of the tabs cannot be closed, so we cannot close the window.
	}

}

void SDockingArea::OnOwningWindowActivated()
{
	// Update the global menu bar when the window activation changes.
	TArray< TSharedRef<SDockTab> > AllTabs = GetAllChildTabs();
	for (int32 TabIndex=0; TabIndex < AllTabs.Num(); ++TabIndex)
	{
		if (AllTabs[TabIndex]->IsForeground())
		{
			FGlobalTabmanager::Get()->UpdateMainMenu(AllTabs[TabIndex], true);
			break;
		}
	}
}

void SDockingArea::OnLiveTabAdded()
{
	bIsCenterTargetVisible = false;
	SDockingNode::OnLiveTabAdded();

	CleanUp(SDockingNode::TabRemoval_None);
}

void SDockingArea::MakeRoomForWindowChrome()
{
	TArray< TSharedRef<SDockingNode> > AllNodes = this->GetChildNodesRecursively();
	if (AllNodes.Num() > 0)
	{
		// Clear out all the reserved space.
		for (auto SomeNode : AllNodes)
		{
			if (SomeNode->GetNodeType() == DockTabStack)
			{
				auto SomeTabStack = StaticCastSharedRef<SDockingTabStack>(SomeNode);
				SomeTabStack->ClearReservedSpace();
			}
		}

		if (this->ParentWindowPtr.IsValid())
		{
			// Reserve some space for the minimize, restore, and close controls
			TSharedRef<SDockingTabStack> WindowControlHousing = this->FindTabStackToHouseWindowControls();
			WindowControlHousing->ReserveSpaceForWindowChrome(SDockingTabStack::EChromeElement::Controls);

			// Reserve some space for the app icons
			TSharedRef<SDockingTabStack> IconHousing = this->FindTabStackToHouseWindowIcon();
			IconHousing->ReserveSpaceForWindowChrome(SDockingTabStack::EChromeElement::Icon);
		}
	}

}

