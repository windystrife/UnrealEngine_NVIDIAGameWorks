// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Docking/SDockingTabWell.h"
#include "Types/PaintArgs.h"
#include "Layout/ArrangedChildren.h"
#include "Framework/Docking/FDockingDragOperation.h"
#include "HAL/PlatformApplicationMisc.h"

const FVector2D FDockingConstants::MaxMinorTabSize(150.f, 50.0f);
const FVector2D FDockingConstants::MaxMajorTabSize(210.f, 50.f);

const FVector2D FDockingConstants::GetMaxTabSizeFor( ETabRole TabRole )
{
	return (TabRole == ETabRole::MajorTab)
		? MaxMajorTabSize
		: MaxMinorTabSize;
}

SDockingTabWell::SDockingTabWell()
: Tabs()
{
}

void SDockingTabWell::Construct( const FArguments& InArgs )
{
	ForegroundTabIndex = INDEX_NONE;
	TabBeingDraggedPtr = TSharedPtr<SDockTab>(nullptr);
	ChildBeingDraggedOffset = 0.0f;
	TabGrabOffsetFraction = FVector2D::ZeroVector;
		
	// We need a valid parent here. TabPanels must exist in a SDockingNode
	check( InArgs._ParentStackNode.Get().IsValid() );
	ParentTabStackPtr = InArgs._ParentStackNode.Get();
}

const TSlotlessChildren<SDockTab>& SDockingTabWell::GetTabs() const
{
	return Tabs;
}

int32 SDockingTabWell::GetNumTabs() const
{
	return Tabs.Num();
}

void SDockingTabWell::AddTab( const TSharedRef<SDockTab>& InTab, int32 AtIndex )
{
	// Add the tab and implicitly activate it.
	if (AtIndex == INDEX_NONE)
	{
		this->Tabs.Add( InTab );
		BringTabToFront( Tabs.Num()-1 );
	}
	else
	{
		AtIndex = FMath::Clamp( AtIndex, 0, Tabs.Num() );
		this->Tabs.Insert( InTab, AtIndex );
		BringTabToFront( AtIndex );
	}

	InTab->SetParent(SharedThis(this));

	const TSharedPtr<SDockingTabStack> ParentTabStack = ParentTabStackPtr.Pin();
	if (ParentTabStack.IsValid() && ParentTabStack->GetDockArea().IsValid())
	{
		ParentTabStack->GetDockArea()->GetTabManager()->GetPrivateApi().OnTabOpening( InTab );
	}
}


void SDockingTabWell::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	// The specialized TabWell is dedicated to arranging tabs.
	// Tabs have uniform sizing (all tabs the same size).
	// TabWell also ignores widget visibilit, as it is not really
	// relevant.


	// The tab that is being dragged by the user, if any.
	TSharedPtr<SDockTab> TabBeingDragged = TabBeingDraggedPtr;
		
	const int32 NumChildren = Tabs.Num();

	// Tabs have a uniform size.
	const FVector2D ChildSize = ComputeChildSize(AllottedGeometry);


	const float DraggedChildCenter = ChildBeingDraggedOffset + ChildSize.X / 2;

	// Arrange all the tabs left to right.
	float XOffset = 0;
	for( int32 TabIndex=0; TabIndex < NumChildren; ++TabIndex )
	{
		const TSharedRef<SDockTab> CurTab = Tabs[TabIndex];
		const float ChildWidthWithOverlap = ChildSize.X - CurTab->GetOverlapWidth();

		// Is this spot reserved from the tab that is being dragged?
		if ( TabBeingDragged.IsValid() && XOffset <= DraggedChildCenter && DraggedChildCenter < (XOffset + ChildWidthWithOverlap) )
		{
			// if so, leave some room to signify that this is where the dragged tab would end up
			XOffset += ChildWidthWithOverlap;
		}

		ArrangedChildren.AddWidget( AllottedGeometry.MakeChild(CurTab, FVector2D(XOffset, 0), ChildSize) );

		XOffset += ChildWidthWithOverlap;
	}
		
	// Arrange the tab currently being dragged by the user, if any
	if ( TabBeingDragged.IsValid() )
	{
		ArrangedChildren.AddWidget( AllottedGeometry.MakeChild( TabBeingDragged.ToSharedRef(), FVector2D(ChildBeingDraggedOffset,0), ChildSize) );
	}
}
	

int32 SDockingTabWell::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// When we are dragging a tab, it must be painted on top of the other tabs, so we cannot
	// just reuse the Panel's default OnPaint.


	// The TabWell has no visualization of its own; it just visualizes its child tabs.
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	this->ArrangeChildren(AllottedGeometry, ArrangedChildren);

	// Because we paint multiple children, we must track the maximum layer id that they produced in case one of our parents
	// wants to an overlay for all of its contents.
	int32 MaxLayerId = LayerId;

	TSharedPtr<SDockTab> ForegroundTab = GetForegroundTab();
	FArrangedWidget* ForegroundTabGeometry = nullptr;
	
	// Draw all inactive tabs first, from last, to first, so that the inactive tabs
	// that come later, are drawn behind tabs that come before it.
	for (int32 ChildIndex = ArrangedChildren.Num() - 1; ChildIndex >= 0; --ChildIndex)
	{
		FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];
		if (CurWidget.Widget == ForegroundTab)
		{
			ForegroundTabGeometry = &CurWidget;
		}
		else
		{
			FSlateRect ChildClipRect = MyCullingRect.IntersectionWith( CurWidget.Geometry.GetLayoutBoundingRect() );
			const int32 CurWidgetsMaxLayerId = CurWidget.Widget->Paint( Args.WithNewParent(this), CurWidget.Geometry, ChildClipRect, OutDrawElements, MaxLayerId, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
			MaxLayerId = FMath::Max( MaxLayerId, CurWidgetsMaxLayerId );
		}
	}

	// Draw active tab in front
	if (ForegroundTab != TSharedPtr<SDockTab>())
	{
		checkSlow(ForegroundTabGeometry);
		FSlateRect ChildClipRect = MyCullingRect.IntersectionWith( ForegroundTabGeometry->Geometry.GetLayoutBoundingRect() );
		const int32 CurWidgetsMaxLayerId = ForegroundTabGeometry->Widget->Paint( Args.WithNewParent(this), ForegroundTabGeometry->Geometry, ChildClipRect, OutDrawElements, MaxLayerId, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
		MaxLayerId = FMath::Max( MaxLayerId, CurWidgetsMaxLayerId );
	}

	return MaxLayerId;
}


FVector2D SDockingTabWell::ComputeDesiredSize( float ) const
{
	FVector2D DesiredSizeResult(0,0);

	for ( int32 TabIndex=0; TabIndex < Tabs.Num(); ++TabIndex )
	{
		// Currently not respecting Visibility because tabs cannot be invisible.
		const TSharedRef<SDockTab>& SomeTab = Tabs[TabIndex];
		const FVector2D SomeTabDesiredSize = SomeTab->GetDesiredSize();
		DesiredSizeResult.X += SomeTabDesiredSize.X;
		DesiredSizeResult.Y = FMath::Max(SomeTabDesiredSize.Y, DesiredSizeResult.Y);
	}

	TSharedPtr<SDockTab> TabBeingDragged = TabBeingDraggedPtr;
	if ( TabBeingDragged.IsValid() )
	{
		const FVector2D SomeTabDesiredSize = TabBeingDragged->GetDesiredSize();
		DesiredSizeResult.X += SomeTabDesiredSize.X;
		DesiredSizeResult.Y = FMath::Max(SomeTabDesiredSize.Y, DesiredSizeResult.Y);
	}

	return DesiredSizeResult;
}


FChildren* SDockingTabWell::GetChildren()
{
	return &Tabs;
}


FVector2D SDockingTabWell::ComputeChildSize( const FGeometry& AllottedGeometry ) const
{
	const int32 NumChildren = Tabs.Num();

	/** Assume all tabs overlap the same amount. */
	const float OverlapWidth = (NumChildren > 0)
		? Tabs[0]->GetOverlapWidth()
		: 0.0f;

	// All children shall be the same size: evenly divide the alloted area.
	// If we are dragging a tab, don't forget to take it into account when dividing.
	const FVector2D ChildSize = TabBeingDraggedPtr.IsValid()
		? FVector2D( (AllottedGeometry.GetLocalSize().X - OverlapWidth) / ( NumChildren + 1 ) + OverlapWidth, AllottedGeometry.GetLocalSize().Y )
		: FVector2D( (AllottedGeometry.GetLocalSize().X - OverlapWidth) / NumChildren + OverlapWidth, AllottedGeometry.GetLocalSize().Y );

	// Major vs. Minor tabs have different tab sizes.
	// We will make our choice based on the first tab we encounter.
	TSharedPtr<SDockTab> FirstTab = (NumChildren > 0)
		? Tabs[0]
		: TabBeingDraggedPtr;

	// If there are no tabs in this tabwell, assume minor tabs.
	FVector2D MaxTabSize(0,0);
	if ( FirstTab.IsValid() )
	{
		const ETabRole RoleToUse = FirstTab->GetVisualTabRole();
		MaxTabSize = FDockingConstants::GetMaxTabSizeFor(RoleToUse);
	}
	else
	{
		MaxTabSize = FDockingConstants::MaxMinorTabSize;
	}

	// Don't let the tabs get too big, or they'll look ugly.
	return FVector2D (
		FMath::Min( ChildSize.X, MaxTabSize.X ),
		FMath::Min( ChildSize.Y, MaxTabSize.Y )
	);
}


float SDockingTabWell::ComputeDraggedTabOffset( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, const FVector2D& InTabGrabOffsetFraction ) const
{
	const FVector2D ComputedChildSize = ComputeChildSize(MyGeometry);
	return MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() ).X - InTabGrabOffsetFraction .X * ComputedChildSize.X;
}


int32 SDockingTabWell::ComputeChildDropIndex(const FGeometry& MyGeometry, const TSharedRef<SDockTab>& TabBeingDragged)
{
	const float ChildWidth = ComputeChildSize(MyGeometry).X;
	const float ChildWidthWithOverlap = ChildWidth - TabBeingDragged->GetOverlapWidth();
	const float DraggedChildCenter = ChildBeingDraggedOffset + ChildWidth / 2;
	const int32 DropLocationIndex = FMath::Clamp(static_cast<int32>(DraggedChildCenter / ChildWidthWithOverlap), 0, Tabs.Num());
	return DropLocationIndex;
}


FReply SDockingTabWell::StartDraggingTab( TSharedRef<SDockTab> TabToStartDragging, FVector2D InTabGrabOffsetFraction, const FPointerEvent& MouseEvent )
{
	const bool bCanLeaveTabWell = TabToStartDragging->GetTabManager()->GetPrivateApi().CanTabLeaveTabWell( TabToStartDragging );

	// We are about to start dragging a tab, so make sure its offset is correct
	this->ChildBeingDraggedOffset = ComputeDraggedTabOffset( MouseEvent.FindGeometry(SharedThis(this)), MouseEvent, InTabGrabOffsetFraction );

	// Tha tab well keeps track of which tab we are dragging; we treat is specially during rendering and layout.
	TabBeingDraggedPtr = TabToStartDragging;	
	TabGrabOffsetFraction = InTabGrabOffsetFraction;
	Tabs.Remove(TabToStartDragging);
	

	if (bCanLeaveTabWell)
	{
		// We just removed the foreground tab.
		ForegroundTabIndex = INDEX_NONE;
		ParentTabStackPtr.Pin()->OnTabRemoved(TabToStartDragging->GetLayoutIdentifier());

#if PLATFORM_MAC
		// On Mac we need to activate the app as we may be dragging a window that is set to be invisible if the app is inactive
		FPlatformApplicationMisc::ActivateApplication();
#endif

		// Start dragging.
		TSharedRef<FDockingDragOperation> DragDropOperation =
			FDockingDragOperation::New(
				TabToStartDragging,
				InTabGrabOffsetFraction,
				GetDockArea().ToSharedRef(),
				ParentTabStackPtr.Pin()->GetTabStackGeometry().GetLocalSize()
			);

		return FReply::Handled().BeginDragDrop( DragDropOperation );
	}
	else
	{
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
}

void SDockingTabWell::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDockingDragOperation> DragDropOperation = DragDropEvent.GetOperationAs<FDockingDragOperation>();
	if ( DragDropOperation.IsValid() )
	{
		if (DragDropOperation->CanDockInNode(ParentTabStackPtr.Pin().ToSharedRef(), FDockingDragOperation::DockingViaTabWell))
		{
			// The user dragged a tab into this TabWell.

			// Update the state of the DragDropOperation to reflect this change.
			DragDropOperation->OnTabWellEntered( SharedThis(this) );

			// Preview the position of the tab in the TabWell
			this->TabBeingDraggedPtr = DragDropOperation->GetTabBeingDragged();
			this->TabGrabOffsetFraction = DragDropOperation->GetTabGrabOffsetFraction();
			
			// The user should see the contents of the tab that we're dragging.
			ParentTabStackPtr.Pin()->SetNodeContent(DragDropOperation->GetTabBeingDragged()->GetContent(), SNullWidget::NullWidget, SNullWidget::NullWidget, SNullWidget::NullWidget);
		}
	}
}

void SDockingTabWell::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDockingDragOperation> DragDropOperation = DragDropEvent.GetOperationAs<FDockingDragOperation>();
	if ( DragDropOperation.IsValid() )
	{
		TSharedRef<SDockingTabStack> ParentTabStack = ParentTabStackPtr.Pin().ToSharedRef();
		TSharedPtr<SDockTab> TabBeingDragged = this->TabBeingDraggedPtr;
		// Check for TabBeingDraggedPtr validity as it may no longer be valid when dragging tabs in game
		if ( TabBeingDragged.IsValid() && DragDropOperation->CanDockInNode(ParentTabStack, FDockingDragOperation::DockingViaTabWell) )
		{
			// Update the DragAndDrop operation based on this change.
			const int32 LastForegroundTabIndex = Tabs.Find(TabBeingDragged.ToSharedRef());

			// The user is pulling a tab out of this TabWell.
			TabBeingDragged->SetParent();

			// We are no longer dragging a tab in this tab well, so stop
			// showing it in the TabWell.
			this->TabBeingDraggedPtr.Reset();

			// Also stop showing its content; switch to the last tab that was active.
			BringTabToFront( FMath::Max(LastForegroundTabIndex-1, 0) );

			// We may have removed the last tab that this DockNode had.
			if ( Tabs.Num() == 0 )
			{
				// Let the DockNode know that it is no longer needed.
				ParentTabStack->OnLastTabRemoved();
			}

			GetDockArea()->CleanUp( SDockingNode::TabRemoval_DraggedOut );

			const FGeometry& DockNodeGeometry = ParentTabStack->GetTabStackGeometry();
			DragDropOperation->OnTabWellLeft( SharedThis(this), DockNodeGeometry );
		}
	}
}


FReply SDockingTabWell::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDockingDragOperation> DragDropOperation = DragDropEvent.GetOperationAs<FDockingDragOperation>();
	if ( DragDropOperation.IsValid() )
	{
		if (DragDropOperation->CanDockInNode(ParentTabStackPtr.Pin().ToSharedRef(), FDockingDragOperation::DockingViaTabWell))
		{
			// We are dragging the tab through a TabWell.
			// Update the position of the Tab that we are dragging in the panel.
			this->ChildBeingDraggedOffset = ComputeDraggedTabOffset(MyGeometry, DragDropEvent, TabGrabOffsetFraction);
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();	
}

FReply SDockingTabWell::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDockingDragOperation> DragDropOperation = DragDropEvent.GetOperationAs<FDockingDragOperation>();
	if (DragDropOperation.IsValid())
	{
		if (DragDropOperation->CanDockInNode(ParentTabStackPtr.Pin().ToSharedRef(), FDockingDragOperation::DockingViaTabWell))
		{
			// It's rare, but sometimes a drop operation can happen after we drag a tab out of a docking tab well but before the engine has a
			// chance to notify the next docking tab well that a drag operation has entered it. In this case, just use the tab referenced by the
			// drag/drop operation
			if (!TabBeingDraggedPtr.IsValid())
			{
				TabBeingDraggedPtr = DragDropOperation->GetTabBeingDragged();
			}
			
			if ( ensure( TabBeingDraggedPtr.IsValid() ) )
			{
				// We dropped a Tab into this TabWell.
				const TSharedRef<SDockTab> TabBeingDragged = TabBeingDraggedPtr.ToSharedRef();

				// Figure out where in this TabWell to drop the Tab.				
				const int32 DropLocationIndex = ComputeChildDropIndex(MyGeometry, TabBeingDragged);

				ensure( DragDropOperation->GetTabBeingDragged().ToSharedRef() == TabBeingDragged );

				// Actually insert the new tab.
				ParentTabStackPtr.Pin()->OpenTab(TabBeingDragged, DropLocationIndex);

				// We are no longer dragging a tab.
				TabBeingDraggedPtr.Reset();

				// We knew how to handled this drop operation!
				return FReply::Handled();
			}
		}
	}

	// Someone just dropped something in here, but we have no idea what to do with it.
	return FReply::Unhandled();
}

EWindowZone::Type SDockingTabWell::GetWindowZoneOverride() const
{
	// Pretend we are a title bar so the user can grab the area to move the window around
	return EWindowZone::TitleBar;
}

FReply SDockingTabWell::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (this->HasMouseCapture() && TabBeingDraggedPtr.IsValid()) 
	{
		const TSharedRef<SDockTab> TabBeingDragged = TabBeingDraggedPtr.ToSharedRef();
		this->TabBeingDraggedPtr.Reset();
		const int32 DropLocationIndex = ComputeChildDropIndex(MyGeometry, TabBeingDragged);
		Tabs.Insert( TabBeingDragged, DropLocationIndex );
		BringTabToFront(TabBeingDragged);
		// We are no longer dragging a tab in this tab well, so stop showing it in the TabWell.
		return FReply::Handled().ReleaseMouseCapture();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SDockingTabWell::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (this->HasMouseCapture())
	{
		// We are dragging the tab through a TabWell.
		// Update the position of the Tab that we are dragging in the panel.
		this->ChildBeingDraggedOffset = ComputeDraggedTabOffset(MyGeometry, MouseEvent, TabGrabOffsetFraction);
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}


void SDockingTabWell::BringTabToFront( int32 TabIndexToActivate )
{
	const bool bActiveIndexChanging = TabIndexToActivate != ForegroundTabIndex;
	if ( bActiveIndexChanging )
	{
		const int32 LastForegroundTabIndex = FMath::Min(ForegroundTabIndex, Tabs.Num()-1);

		// For positive indexes, don't go out of bounds on the array.
		ForegroundTabIndex = FMath::Min(TabIndexToActivate, Tabs.Num()-1);

		TSharedPtr<SDockingArea> MyDockArea = GetDockArea();
		if ( Tabs.Num() > 0 && MyDockArea.IsValid() )
		{
			const TSharedPtr<SDockTab> PreviousForegroundTab = (LastForegroundTabIndex == INDEX_NONE)
				? TSharedPtr<SDockTab>()
				: Tabs[LastForegroundTabIndex];

			const TSharedPtr<SDockTab> NewForegroundTab = (ForegroundTabIndex == INDEX_NONE)
				? TSharedPtr<SDockTab>()
				: Tabs[ForegroundTabIndex];
			
			MyDockArea->GetTabManager()->GetPrivateApi().OnTabForegrounded(NewForegroundTab, PreviousForegroundTab);

			FGlobalTabmanager::Get()->GetPrivateApi().OnTabForegrounded(NewForegroundTab, PreviousForegroundTab);
		}
	}

	// Always force a refresh, even if we don't think the active index changed.
	RefreshParentContent();

	// Update the native, global menu bar if a tab is in the foreground.
	if( ForegroundTabIndex != INDEX_NONE )
	{
		TSharedPtr<FTabManager> TabManager = Tabs[ForegroundTabIndex]->GetTabManager();
		if(TabManager == FGlobalTabmanager::Get())
		{
			FGlobalTabmanager::Get()->UpdateMainMenu(Tabs[ForegroundTabIndex], false);
		}
		else
		{
			TabManager->UpdateMainMenu(false);
		}
	}
}

/** Activate the tab specified by TabToActivate SDockTab. */
void SDockingTabWell::BringTabToFront( TSharedPtr<SDockTab> TabToActivate )
{
	if (Tabs.Num() > 0)
	{
		for (int32 TabIndex=0; TabIndex < Tabs.Num(); ++TabIndex )
		{
			if (Tabs[TabIndex] == TabToActivate)
			{
				BringTabToFront( TabIndex );
				return;
			}
		}
	}
}

/** Gets the currently active tab (or the currently dragged tab), or a null pointer if no tab is active. */
TSharedPtr<SDockTab> SDockingTabWell::GetForegroundTab() const
{
	if (TabBeingDraggedPtr.IsValid())
	{
		return TabBeingDraggedPtr;
	}
	return (Tabs.Num() > 0 && ForegroundTabIndex > INDEX_NONE) ? Tabs[ForegroundTabIndex] : TSharedPtr<SDockTab>();
}

/** Gets the index of the currently active tab, or INDEX_NONE if no tab is active or a tab is being dragged. */
int32 SDockingTabWell::GetForegroundTabIndex() const
{
	return (Tabs.Num() > 0) ? ForegroundTabIndex : INDEX_NONE;
}

void SDockingTabWell::RemoveAndDestroyTab(const TSharedRef<SDockTab>& TabToRemove, SDockingNode::ELayoutModification RemovalMethod)
{
	int32 TabIndex = Tabs.Find(TabToRemove);

	if (TabIndex != INDEX_NONE)
	{
		const TSharedPtr<SDockingTabStack> ParentTabStack = ParentTabStackPtr.Pin();

		// Remove the old tab from the list of tabs and activate the new tab.
		{
			BringTabToFront(TabIndex);
			Tabs.RemoveAt(TabIndex);
			// We no longer have a tab in the foreground.
			// This is important because BringTabToFront triggers notifications based on the difference in active tab indexes.
			ForegroundTabIndex = INDEX_NONE;

			// Now bring the last tab that we were on to the foreground
			BringTabToFront(FMath::Max(TabIndex-1, 0));
		}
		
		if ( ensure(ParentTabStack.IsValid()) )
		{
			TSharedPtr<SDockingArea> DockAreaPtr = ParentTabStack->GetDockArea();

			ParentTabStack->OnTabClosed( TabToRemove );
			
			// We might be closing down an entire dock area, if this is a major tab.
			// Use this opportunity to save its layout
			if (RemovalMethod == SDockingNode::TabRemoval_Closed)
			{
				if (DockAreaPtr.IsValid())
				{
					DockAreaPtr->GetTabManager()->GetPrivateApi().OnTabClosing( TabToRemove );
				}
			}

			if (Tabs.Num() == 0)
			{
				ParentTabStack->OnLastTabRemoved();
			}
			else
			{
				RefreshParentContent();
			}

			if (DockAreaPtr.IsValid())
			{
				DockAreaPtr->CleanUp( RemovalMethod );
			}
		}
	}
}

void SDockingTabWell::RefreshParentContent()
{
	if (Tabs.Num() > 0 && ForegroundTabIndex != INDEX_NONE)
	{
		const TSharedRef<SDockTab>& ForegroundTab = Tabs[ForegroundTabIndex];
		FGlobalTabmanager::Get()->SetActiveTab( ForegroundTab );

		TSharedPtr<SWindow> ParentWindowPtr = ForegroundTab->GetParentWindow();
		if (ParentWindowPtr.IsValid() && ParentWindowPtr != FGlobalTabmanager::Get()->GetRootWindow())
		{
			ParentWindowPtr->SetTitle( ForegroundTab->GetTabLabel() );
		}

		ParentTabStackPtr.Pin()->SetNodeContent( ForegroundTab->GetContent(), ForegroundTab->GetLeftContent(), ForegroundTab->GetRightContent(), ForegroundTab->GetBackgrounfContent() );
	}
	else
	{
		ParentTabStackPtr.Pin()->SetNodeContent(SNullWidget::NullWidget, SNullWidget::NullWidget, SNullWidget::NullWidget, SNullWidget::NullWidget);
	}
}

TSharedPtr<SDockingArea> SDockingTabWell::GetDockArea()
{
	return ParentTabStackPtr.IsValid() ? ParentTabStackPtr.Pin()->GetDockArea() : TSharedPtr<SDockingArea>();
}


TSharedPtr<SDockingTabStack> SDockingTabWell::GetParentDockTabStack()
{
	return ParentTabStackPtr.IsValid() ? ParentTabStackPtr.Pin() : TSharedPtr<SDockingTabStack>();
}
