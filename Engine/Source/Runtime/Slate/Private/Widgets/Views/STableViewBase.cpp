// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Views/STableViewBase.h"
#include "Rendering/DrawElements.h"
#include "Types/SlateConstants.h"
#include "Types/SlateStructs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Framework/Layout/Overscroll.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListPanel.h"

namespace ListConstants
{
	static const float OvershootMax = 150.0f;
	static const float OvershootBounceRate = 250.0f;
}

void STableViewBase::ConstructChildren( const TAttribute<float>& InItemWidth, const TAttribute<float>& InItemHeight, const TAttribute<EListItemAlignment>& InItemAlignment, const TSharedPtr<SHeaderRow>& InHeaderRow, const TSharedPtr<SScrollBar>& InScrollBar, const FOnTableViewScrolled& InOnTableViewScrolled )
{
	bItemsNeedRefresh = true;
	
	HeaderRow = InHeaderRow;

	OnTableViewScrolled = InOnTableViewScrolled;

	// If the user provided a scrollbar, we do not need to make one of our own.
	if (InScrollBar.IsValid())
	{
		ScrollBar = InScrollBar;
		ScrollBar->SetOnUserScrolled( FOnUserScrolled::CreateSP(this, &STableViewBase::ScrollBar_OnUserScrolled) );
	}
	
	TSharedRef<SWidget> ListAndScrollbar = (!ScrollBar.IsValid())
		// We need to make our own scrollbar
		? StaticCastSharedRef<SWidget>
		(
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SAssignNew( ItemsPanel, SListPanel )
				.ItemWidth( InItemWidth )
				.ItemHeight( InItemHeight )
				.NumDesiredItems( this, &STableViewBase::GetNumItemsBeingObserved )
				.ItemAlignment( InItemAlignment )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SBox )
				.WidthOverride( FOptionalSize( 16 ) )
				[
					SAssignNew(ScrollBar, SScrollBar)
					.OnUserScrolled( this, &STableViewBase::ScrollBar_OnUserScrolled )
				]
			]
		)
		// The user provided us with a scrollbar; we will rely on it.
		: StaticCastSharedRef<SWidget>
		(
			SAssignNew( ItemsPanel, SListPanel )
			.ItemWidth( InItemWidth )
			.ItemHeight( InItemHeight )
			.NumDesiredItems( this, &STableViewBase::GetNumItemsBeingObserved )
			.ItemAlignment( InItemAlignment )
		);

	if (InHeaderRow.IsValid())
	{
		// Only associate the scrollbar if we created it.
		// If the scrollbar was passed in from outside then it won't appear under our header row so doesn't need compensating for.
		if (!InScrollBar.IsValid())
		{
			InHeaderRow->SetAssociatedVerticalScrollBar( ScrollBar.ToSharedRef(), 16 );
		}

		this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				InHeaderRow.ToSharedRef()
			]
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				ListAndScrollbar
			]
		];
	}
	else
	{
		this->ChildSlot
		[
			ListAndScrollbar
		];
	}
	
}


bool STableViewBase::SupportsKeyboardFocus() const
{
	// The ListView is focusable.
	return true;
}


void STableViewBase::OnFocusLost( const FFocusEvent& InFocusEvent )
{
	bShowSoftwareCursor = false;
}


void STableViewBase::OnMouseCaptureLost()
{
	bShowSoftwareCursor = false;
}


struct FEndOfListResult
{
	FEndOfListResult( float InOffset, float InItemsAboveView )
	: OffsetFromEndOfList(InOffset)
	, ItemsAboveView(InItemsAboveView)
	{

	}

	float OffsetFromEndOfList;
	float ItemsAboveView;
};

static FEndOfListResult ComputeOffsetForEndOfList( const FGeometry& ListPanelGeometry, const FChildren& ListPanelChildren )
{
	float OffsetFromEndOfList = 0.0f;
	float AvailableSpace = ListPanelGeometry.GetLocalSize().Y;
	float ItemsAboveView = 0.0f;
	for ( int ChildIndex=ListPanelChildren.Num()-1; ChildIndex >= 0; --ChildIndex )
	{
		const float CurChildHeight = ListPanelChildren.GetChildAt(ChildIndex)->GetDesiredSize().Y;
		if (AvailableSpace == 0)
		{
			ItemsAboveView ++;
		}

		if ( CurChildHeight < AvailableSpace )
		{
			// This whole child fits
			OffsetFromEndOfList += 1;
			AvailableSpace -= CurChildHeight;
		}
		else
		{
			OffsetFromEndOfList += AvailableSpace / CurChildHeight;
			ItemsAboveView += (CurChildHeight - AvailableSpace)/CurChildHeight;
			AvailableSpace = 0;
		}
	}

	return FEndOfListResult( OffsetFromEndOfList, ItemsAboveView );
}

EActiveTimerReturnType STableViewBase::UpdateInertialScroll(double InCurrentTime, float InDeltaTime)
{
	bool bKeepTicking = false;
	if (ItemsPanel.IsValid())
	{
		if (IsRightClickScrolling())
		{
			bKeepTicking = true;

			// We sample for the inertial scroll on tick rather than on mouse/touch move so
			// that we still get samples even if the mouse has not moved.
			if (CanUseInertialScroll(TickScrollDelta))
			{
				InertialScrollManager.AddScrollSample(TickScrollDelta, InCurrentTime);
			}
		}
		else
		{
			InertialScrollManager.UpdateScrollVelocity(InDeltaTime);
			const float ScrollVelocity = InertialScrollManager.GetScrollVelocity();

			if (ScrollVelocity != 0.f)
			{
				if (CanUseInertialScroll(ScrollVelocity))
				{
					bKeepTicking = true;
					ScrollBy(GetCachedGeometry(), ScrollVelocity * InDeltaTime, AllowOverscroll);
				}
				else
				{
					InertialScrollManager.ClearScrollVelocity();
				}
			}

			if (AllowOverscroll == EAllowOverscroll::Yes)
			{
				// If we are currently in overscroll, the list will need refreshing.
				// Do this before UpdateOverscroll, as that could cause GetOverscroll() to be 0
				if (Overscroll.GetOverscroll(GetCachedGeometry()) != 0.0f)
				{
					bKeepTicking = true;
					RequestListRefresh();
				}

				Overscroll.UpdateOverscroll(InDeltaTime);
			}
		}

		TickScrollDelta = 0.f;
	}

	bIsScrollingActiveTimerRegistered = bKeepTicking;
	return bKeepTicking ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
}

EActiveTimerReturnType STableViewBase::EnsureTickToRefresh(double InCurrentTime, float InDeltaTime)
{
	// Actual refresh isn't implemented here as it can be needed in response to changes in the panel geometry.
	// Since that isn't known until Tick (called after this when registered), refreshing here could result in two refreshes in one frame.

	return EActiveTimerReturnType::Stop;
}

void STableViewBase::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (ItemsPanel.IsValid())
	{
		FGeometry PanelGeometry = FindChildGeometry( AllottedGeometry, ItemsPanel.ToSharedRef() );
		if ( bItemsNeedRefresh || PanelGeometryLastTick.GetLocalSize() != PanelGeometry.GetLocalSize())
		{
			PanelGeometryLastTick = PanelGeometry;

			// We never create the ItemsPanel if the user did not specify all the parameters required to successfully make a list.
			const EScrollIntoViewResult ScrollIntoViewResult = ScrollIntoView( PanelGeometry );

			const FReGenerateResults ReGenerateResults = ReGenerateItems( PanelGeometry );
			LastGenerateResults = ReGenerateResults;

			const int32 NumItemsBeingObserved = GetNumItemsBeingObserved();

			const int32 NumItemsWide = GetNumItemsWide();
			const int32 NumItemRows = NumItemsBeingObserved / NumItemsWide;

			const bool bEnoughRoomForAllItems = ReGenerateResults.ExactNumRowsOnScreen >= NumItemRows;
			if (bEnoughRoomForAllItems)
			{
				// We can show all the items, so make sure there is no scrolling.
				ScrollOffset = 0;
			}
			else if ( ReGenerateResults.bGeneratedPastLastItem )
			{
				ScrollOffset = ReGenerateResults.NewScrollOffset;
			}
			
			
			SetScrollOffset( FMath::Max(0.0, ScrollOffset) );
			ItemsPanel->SmoothScrollOffset( FMath::Fractional(ScrollOffset / GetNumItemsWide()) );

			if (AllowOverscroll == EAllowOverscroll::Yes)
			{
				const float OverscrollAmount = Overscroll.GetOverscroll(GetCachedGeometry());
				ItemsPanel->SetOverscrollAmount( OverscrollAmount );
			}

			UpdateSelectionSet();

			// Update scrollbar
			if (NumItemsBeingObserved > 0)
			{
				if (ReGenerateResults.ExactNumRowsOnScreen < 1.0f)
				{
					// We are be observing a single row which is larger than the available visible area, so we should calculate thumb size based on that
					const double VisibleSizeFraction = AllottedGeometry.GetLocalSize().Y / ReGenerateResults.HeightOfGeneratedItems;
					const double ThumbSizeFraction = FMath::Min(VisibleSizeFraction, 1.0);
					const double OffsetFraction = ScrollOffset / NumItemsBeingObserved;
					ScrollBar->SetState( OffsetFraction, ThumbSizeFraction );
				}
				else
				{
					// The thumb size is whatever fraction of the items we are currently seeing (including partially seen items).
					// e.g. if we are seeing 0.5 of the first generated widget and 0.75 of the last widget, that's 1.25 widgets.
					const double ThumbSizeFraction = ReGenerateResults.ExactNumRowsOnScreen / NumItemRows;
					const double OffsetFraction = ScrollOffset / NumItemsBeingObserved;
					ScrollBar->SetState( OffsetFraction, ThumbSizeFraction );
				}
			}
			else
			{
				const double ThumbSizeFraction = 1;
				const double OffsetFraction = 0;
				ScrollBar->SetState( OffsetFraction, ThumbSizeFraction );
			}

			bWasAtEndOfList = (ScrollBar->DistanceFromBottom() < KINDA_SMALL_NUMBER);

			bItemsNeedRefresh = false;
			ItemsPanel->SetRefreshPending(false);

			if (ScrollIntoViewResult == EScrollIntoViewResult::Deferred)
			{
				// We call this rather than just leave bItemsNeedRefresh as true to ensure that EnsureTickToRefresh is registered
				RequestListRefresh();
			}
			else
			{
				NotifyItemScrolledIntoView();
			}
		}
	}
}


void STableViewBase::ScrollBar_OnUserScrolled( float InScrollOffsetFraction )
{
	const double ClampedScrollOffsetInItems = FMath::Clamp<double>( InScrollOffsetFraction, 0.0, 1.0 )* GetNumItemsBeingObserved();
	ScrollTo( ClampedScrollOffsetInItems );
}


FReply STableViewBase::OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.IsTouchEvent())
	{
		// Clear any inertia 
		this->InertialScrollManager.ClearScrollVelocity();
		// We have started a new interaction; track how far the user has moved since they put their finger down.
		AmountScrolledWhileRightMouseDown = 0;
		// Someone put their finger down in this list, so they probably want to drag the list.
		bStartedTouchInteraction = true;
		return FReply::Unhandled();
	}
	else
	{
		return FReply::Unhandled();
	}
}


FReply STableViewBase::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Zero the scroll velocity so the list stops immediately on mouse down, even if the user does not drag
	this->InertialScrollManager.ClearScrollVelocity();

	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && ScrollBar->IsNeeded() )
	{
		AmountScrolledWhileRightMouseDown = 0;

		// NOTE: We don't bother capturing the mouse, unless the user starts dragging a few pixels (see the
		// mouse move handling here.)  This is important so that the item row has a chance to select
		// items when the right mouse button is released.  Just keep in mind that you might not get
		// an OnMouseButtonUp event for the right mouse button if the user moves off of the table before
		// they reach our scroll threshold
		return FReply::Handled();
	}
	else if ( this->HasMouseCapture() )
	{
		// Consume all mouse buttons while we are RMB-dragging.
		return FReply::Handled();
	}
	return FReply::Unhandled();			
}

FReply STableViewBase::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	if ( this->HasMouseCapture() )
	{
		// Consume all other mouse buttons while we are RMB-dragging.
		return FReply::Handled();
	}
	return FReply::Unhandled();			

}


FReply STableViewBase::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{

		OnRightMouseButtonUp( MouseEvent );

		FReply Reply = FReply::Handled().ReleaseMouseCapture();
		bShowSoftwareCursor = false;

		// If we have mouse capture, snap the mouse back to the closest location that is within the list's bounds
		if ( HasMouseCapture() )
		{
			FSlateRect ListScreenSpaceRect = MyGeometry.GetLayoutBoundingRect();
			FVector2D CursorPosition = MyGeometry.LocalToAbsolute( SoftwareCursorPosition );

			FIntPoint BestPositionInList(
				FMath::RoundToInt( FMath::Clamp( CursorPosition.X, ListScreenSpaceRect.Left, ListScreenSpaceRect.Right ) ),
				FMath::RoundToInt( FMath::Clamp( CursorPosition.Y, ListScreenSpaceRect.Top, ListScreenSpaceRect.Bottom ) )
				);

			Reply.SetMousePos(BestPositionInList);
		}

		return Reply;
	}
	return FReply::Unhandled();
}


FReply STableViewBase::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{	
	if( MouseEvent.IsMouseButtonDown( EKeys::RightMouseButton ) )
	{
		const float ScrollByAmount = MouseEvent.GetCursorDelta().Y / MyGeometry.Scale;
		// If scrolling with the right mouse button, we need to remember how much we scrolled.
		// If we did not scroll at all, we will bring up the context menu when the mouse is released.
		AmountScrolledWhileRightMouseDown += FMath::Abs( ScrollByAmount );

		// Has the mouse moved far enough with the right mouse button held down to start capturing
		// the mouse and dragging the view?
		if( IsRightClickScrolling() )
		{
			// Make sure the active timer is registered to update the inertial scroll
			if (!bIsScrollingActiveTimerRegistered)
			{
				bIsScrollingActiveTimerRegistered = true;
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &STableViewBase::UpdateInertialScroll));
			}

			TickScrollDelta -= ScrollByAmount;

			const float AmountScrolled = this->ScrollBy( MyGeometry, -ScrollByAmount, AllowOverscroll );

			FReply Reply = FReply::Handled();

			// The mouse moved enough that we're now dragging the view. Capture the mouse
			// so the user does not have to stay within the bounds of the list while dragging.
			if(this->HasMouseCapture() == false)
			{
				Reply.CaptureMouse( AsShared() ).UseHighPrecisionMouseMovement( AsShared() );
				SoftwareCursorPosition = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
				bShowSoftwareCursor = true;
			}

			// Check if the mouse has moved.
			if( AmountScrolled != 0 )
			{
				SoftwareCursorPosition.Y += ScrollByAmount;
			}

			return Reply;
		}
	}

	return FReply::Unhandled();
}


void STableViewBase::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	bStartedTouchInteraction = false;
	if(this->HasMouseCapture() == false)
	{
		// No longer scrolling (unless we have mouse capture)
		AmountScrolledWhileRightMouseDown = 0;
	}
}


FReply STableViewBase::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( !MouseEvent.IsControlDown() )
	{
		// Make sure scroll velocity is cleared so it doesn't fight with the mouse wheel input
		this->InertialScrollManager.ClearScrollVelocity();

		const float AmountScrolledInItems = this->ScrollBy( MyGeometry, -MouseEvent.GetWheelDelta() * WheelScrollMultiplier, EAllowOverscroll::No );

		switch ( ConsumeMouseWheel )
		{
		case EConsumeMouseWheel::Always:
			return FReply::Handled();
		case EConsumeMouseWheel::WhenScrollingPossible: //default behavior
		default:
			if ( FMath::Abs( AmountScrolledInItems ) > 0.0f )
			{
				return FReply::Handled();
			}
		}
		
	}
	return FReply::Unhandled();
}


FReply STableViewBase::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if ( InKeyEvent.IsControlDown() && InKeyEvent.GetKey() == EKeys::End )
	{
		ScrollToBottom();
		return FReply::Handled();
	}


	return FReply::Unhandled();
}

FCursorReply STableViewBase::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	if ( IsRightClickScrolling() && CursorEvent.IsMouseButtonDown(EKeys::RightMouseButton) )
	{
		// We hide the native cursor as we'll be drawing the software EMouseCursor::GrabHandClosed cursor
		return FCursorReply::Cursor( EMouseCursor::None );
	}
	else
	{
		return FCursorReply::Unhandled();
	}
}

FReply STableViewBase::OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	// See OnPreviewMouseButtonDown()
	//     if (MouseEvent.IsTouchEvent())

	return FReply::Unhandled();
}

FReply STableViewBase::OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	if (bStartedTouchInteraction)
	{
		const float ScrollByAmount = InTouchEvent.GetCursorDelta().Y / MyGeometry.Scale;
		AmountScrolledWhileRightMouseDown += FMath::Abs( ScrollByAmount );
		TickScrollDelta -= ScrollByAmount;

		if (AmountScrolledWhileRightMouseDown > FSlateApplication::Get().GetDragTriggerDistance())
		{
			// Make sure the active timer is registered to update the inertial scroll
			if ( !bIsScrollingActiveTimerRegistered )
			{
				bIsScrollingActiveTimerRegistered = true;
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &STableViewBase::UpdateInertialScroll));
			}

			const float AmountScrolled = this->ScrollBy( MyGeometry, -ScrollByAmount, EAllowOverscroll::Yes );

			ScrollBar->BeginScrolling();

			// The user has moved the list some amount; they are probably
			// trying to scroll. From now on, the list assumes the user is scrolling
			// until they lift their finger.
			return FReply::Handled().CaptureMouse( AsShared() );
		}
		return FReply::Handled();
	}
	else
	{
		return FReply::Handled();
	}
}

FReply STableViewBase::OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	AmountScrolledWhileRightMouseDown = 0;
	bStartedTouchInteraction = false;

	ScrollBar->EndScrolling();

	if (HasMouseCapture())
	{
		return FReply::Handled().ReleaseMouseCapture();
	}
	else
	{
		return FReply::Handled();
	}
}


int32 STableViewBase::GetNumGeneratedChildren() const
{
	return (ItemsPanel.IsValid())
		? ItemsPanel->GetChildren()->Num()
		: 0;
}

TSharedPtr<SHeaderRow> STableViewBase::GetHeaderRow() const
{
	return HeaderRow;
}

bool STableViewBase::IsRightClickScrolling() const
{
	return AmountScrolledWhileRightMouseDown >= FSlateApplication::Get().GetDragTriggerDistance() &&
		(this->ScrollBar->IsNeeded() || AllowOverscroll == EAllowOverscroll::Yes);
}

bool STableViewBase::IsUserScrolling() const
{
	bool bUserScroll = this->ScrollBar->IsNeeded() && this->ScrollBar->IsScrolling();
	return bUserScroll || IsRightClickScrolling();
}

void STableViewBase::RequestListRefresh()
{
	if (!bItemsNeedRefresh)
	{
		bItemsNeedRefresh = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &STableViewBase::EnsureTickToRefresh));
	}
	ItemsPanel->SetRefreshPending(true);
}

bool STableViewBase::IsPendingRefresh() const
{
	return bItemsNeedRefresh || ItemsPanel->IsRefreshPending();
}

int32 STableViewBase::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 NewLayerId = SCompoundWidget::OnPaint( Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );

	if( !bShowSoftwareCursor )
	{
		return NewLayerId;
	}

	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("SoftwareCursor_Grab"));

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		++NewLayerId,
		AllottedGeometry.ToPaintGeometry( SoftwareCursorPosition - ( Brush->ImageSize / 2 ), Brush->ImageSize ),
		Brush
		);

	return NewLayerId;
}

STableViewBase::STableViewBase( ETableViewMode::Type InTableViewMode )
	: TableViewMode( InTableViewMode )
	, ScrollOffset( 0 )
	, bStartedTouchInteraction( false )
	, AmountScrolledWhileRightMouseDown( 0 )
	, TickScrollDelta( 0 )
	, LastGenerateResults( 0,0,0,false )
	, bWasAtEndOfList(false)
	, SelectionMode( ESelectionMode::Multi )
	, SoftwareCursorPosition( ForceInitToZero )
	, bShowSoftwareCursor( false )
	, WheelScrollMultiplier(GetGlobalScrollAmount())
	, bIsScrollingActiveTimerRegistered( false )
	, Overscroll()
	, AllowOverscroll(EAllowOverscroll::Yes)
	, ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
	, bItemsNeedRefresh( false )	
{
}

float STableViewBase::ScrollBy(const FGeometry& MyGeometry, float ScrollByAmountInSlateUnits, EAllowOverscroll InAllowOverscroll)
{
	const int32 NumItemsBeingObserved = GetNumItemsBeingObserved();
	const float FractionalScrollOffsetInItems = (ScrollOffset + GetScrollRateInItems() * ScrollByAmountInSlateUnits) / NumItemsBeingObserved;
	const double ClampedScrollOffsetInItems = FMath::Clamp<double>( FractionalScrollOffsetInItems*NumItemsBeingObserved, -10.0f, NumItemsBeingObserved+10.0f ) * NumItemsBeingObserved;
	if (InAllowOverscroll == EAllowOverscroll::Yes)
	{
		Overscroll.ScrollBy(MyGeometry, ClampedScrollOffsetInItems - ScrollByAmountInSlateUnits );
	}
	return ScrollTo( ClampedScrollOffsetInItems );
}

float STableViewBase::ScrollTo( float InScrollOffset )
{
	const float NewScrollOffset = FMath::Clamp( InScrollOffset, -10.0f, GetNumItemsBeingObserved()+10.0f );
	float AmountScrolled = FMath::Abs( ScrollOffset - NewScrollOffset );
	SetScrollOffset( NewScrollOffset );
	
	if ( bWasAtEndOfList && NewScrollOffset >= ScrollOffset )
	{
		AmountScrolled = 0;
	}

	return AmountScrolled;
}

float STableViewBase::GetScrollOffset() const
{
	return ScrollOffset;
}

void STableViewBase::SetScrollOffset( const float InScrollOffset )
{
	if ( ScrollOffset != InScrollOffset )
	{
		ScrollOffset = InScrollOffset;
		OnTableViewScrolled.ExecuteIfBound( ScrollOffset );
		RequestListRefresh();
	}
}

void STableViewBase::AddScrollOffset(const float InScrollOffsetDelta, bool RefreshList)
{
	if (FMath::IsNearlyEqual(InScrollOffsetDelta, 0.0f) == false)
	{
		ScrollOffset += InScrollOffsetDelta;
		if (RefreshList)
		{
			OnTableViewScrolled.ExecuteIfBound(ScrollOffset);
			RequestListRefresh();
		}
	}
}

void STableViewBase::InsertWidget( const TSharedRef<ITableRow> & WidgetToInset )
{
	ItemsPanel->AddSlot(0)
	[
		WidgetToInset->AsWidget()
	];
}

/**
 * Add a widget to the view.
 *
 * @param WidgetToAppend   Widget to append to the view.
 */
void STableViewBase::AppendWidget( const TSharedRef<ITableRow>& WidgetToAppend )
{
	ItemsPanel->AddSlot()
	[
		WidgetToAppend->AsWidget()
	];
}

/**
 * Remove all the widgets from the view.
 */
void STableViewBase::ClearWidgets()
{
	ItemsPanel->ClearItems();
}

float STableViewBase::GetItemWidth() const
{
	return GetItemSize().X;
}

float STableViewBase::GetItemHeight() const
{
	return GetItemSize().Y;
}

FVector2D STableViewBase::GetItemSize() const
{
	return ItemsPanel->GetItemSize(PanelGeometryLastTick) + FVector2D(ItemsPanel->GetItemPadding(PanelGeometryLastTick), 0.0f);
}

void STableViewBase::SetItemHeight(TAttribute<float> Height)
{
	ItemsPanel->SetItemHeight(Height);
}

void STableViewBase::SetItemWidth(TAttribute<float> Width)
{
	ItemsPanel->SetItemWidth(Width);
}

float STableViewBase::GetNumLiveWidgets() const
{
	return ItemsPanel->GetChildren()->Num();
}

int32 STableViewBase::GetNumItemsWide() const
{
	return 1;
}

void STableViewBase::NavigateToWidget(const uint32 UserIndex, const TSharedPtr<SWidget>& NavigationDestination, ENavigationSource NavigationSource) const
{
	FSlateApplication::Get().NavigateToWidget(UserIndex, NavigationDestination, NavigationSource);
}

void STableViewBase::OnRightMouseButtonUp(const FPointerEvent& MouseEvent)
{
	const FVector2D& SummonLocation = MouseEvent.GetScreenSpacePosition();
	const bool bShouldOpenContextMenu = !IsRightClickScrolling();
	const bool bContextMenuOpeningBound = OnContextMenuOpening.IsBound();

	if (bShouldOpenContextMenu && bContextMenuOpeningBound)
	{
		// Get the context menu content. If NULL, don't open a menu.
		TSharedPtr<SWidget> MenuContent = OnContextMenuOpening.Execute();

		if (MenuContent.IsValid())
		{
			bShowSoftwareCursor = false;

			FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuContent.ToSharedRef(), SummonLocation, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}
	}

	AmountScrolledWhileRightMouseDown = 0;
}

float STableViewBase::GetScrollRateInItems() const
{
	return (LastGenerateResults.HeightOfGeneratedItems != 0 && LastGenerateResults.ExactNumRowsOnScreen != 0)
		// Approximate a consistent scrolling rate based on the average item height.
		? LastGenerateResults.ExactNumRowsOnScreen / LastGenerateResults.HeightOfGeneratedItems
		// Scroll 1/2 an item at a time as a default.
		: 0.5f;
}

void STableViewBase::ScrollToTop()
{
	SetScrollOffset( 0 );
	RequestListRefresh();
}

void STableViewBase::ScrollToBottom()
{
	SetScrollOffset( GetNumItemsBeingObserved() );
	RequestListRefresh();
}

FVector2D STableViewBase::GetScrollDistance()
{
	return FVector2D( 0, ScrollBar->DistanceFromTop() );
}

FVector2D STableViewBase::GetScrollDistanceRemaining()
{
	return FVector2D( 0, ScrollBar->DistanceFromBottom() );
}

TSharedRef<class SWidget> STableViewBase::GetScrollWidget()
{
	return SharedThis(this);
}

bool STableViewBase::CanUseInertialScroll( float ScrollAmount ) const
{
	const auto CurrentOverscroll = Overscroll.GetOverscroll(GetCachedGeometry());

	// We allow sampling for the inertial scroll if we are not in the overscroll region,
	// Or if we are scrolling outwards of the overscroll region
	return CurrentOverscroll == 0.f || FMath::Sign(CurrentOverscroll) != FMath::Sign(ScrollAmount);
}
