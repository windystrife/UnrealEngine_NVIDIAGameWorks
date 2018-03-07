// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SScrollBox.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SPanel.h"
#include "Types/SlateConstants.h"
#include "Layout/LayoutUtils.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"


SScrollBox::FSlot& SScrollBox::Slot()
{
	return *(new SScrollBox::FSlot());
}

class SScrollPanel : public SPanel
{
public:
	
	SLATE_BEGIN_ARGS(SScrollPanel)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_ARGUMENT(EOrientation, Orientation)

	SLATE_END_ARGS()

	SScrollPanel()
	: Children()
	{
	}

	void Construct( const FArguments& InArgs, const TArray<SScrollBox::FSlot*>& InSlots )
	{
		PhysicalOffset = 0;
		Children.Reserve( InSlots.Num() );
		for ( int32 SlotIndex=0; SlotIndex < InSlots.Num(); ++SlotIndex )
		{
			Children.Add( InSlots[ SlotIndex ] );
		}
		Orientation = InArgs._Orientation;
	}

public:

	EOrientation GetOrientation()
	{
		return Orientation;
	}

	void SetOrientation(EOrientation InOrientation)
	{
		Orientation = InOrientation;
	}

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override
	{
		float CurChildOffset = -PhysicalOffset;

		for( int32 SlotIndex=0; SlotIndex < Children.Num(); ++SlotIndex )
		{
			const SScrollBox::FSlot& ThisSlot = Children[SlotIndex];
			const EVisibility ChildVisibility = ThisSlot.GetWidget()->GetVisibility();

			if ( ChildVisibility != EVisibility::Collapsed )
			{
				if (Orientation == Orient_Vertical)
				{
					CurChildOffset = ArrangeChildVerticalAndReturnOffset(AllottedGeometry, ArrangedChildren, ThisSlot, CurChildOffset);
				}
				else
				{
					CurChildOffset = ArrangeChildHorizontalAndReturnOffset(AllottedGeometry, ArrangedChildren, ThisSlot, CurChildOffset);
				}
			}
		}
	}

	virtual FChildren* GetChildren() override
	{
		return &Children;
	}

	float PhysicalOffset;
	TPanelChildren<SScrollBox::FSlot> Children;

protected:
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		FVector2D ThisDesiredSize = FVector2D::ZeroVector;
		for ( int32 SlotIndex=0; SlotIndex < Children.Num(); ++SlotIndex )
		{
			const SScrollBox::FSlot& ThisSlot = Children[SlotIndex];
			if ( ThisSlot.GetWidget()->GetVisibility() != EVisibility::Collapsed )
			{
				const FVector2D ChildDesiredSize = ThisSlot.GetWidget()->GetDesiredSize();
				if ( Orientation == Orient_Vertical )
				{
					ThisDesiredSize.X = FMath::Max(ChildDesiredSize.X, ThisDesiredSize.X);
					ThisDesiredSize.Y += ChildDesiredSize.Y + ThisSlot.SlotPadding.Get().GetTotalSpaceAlong<Orient_Vertical>();
				}
				else
				{
					ThisDesiredSize.X += ChildDesiredSize.X + ThisSlot.SlotPadding.Get().GetTotalSpaceAlong<Orient_Horizontal>();
					ThisDesiredSize.Y = FMath::Max(ChildDesiredSize.Y, ThisDesiredSize.Y);
				}
			}
		}

		return ThisDesiredSize;
	}

private:
	
	float ArrangeChildVerticalAndReturnOffset(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, const SScrollBox::FSlot& ThisSlot, float CurChildOffset) const
	{
		const FMargin& ThisPadding = ThisSlot.SlotPadding.Get();
		const FVector2D& WidgetDesiredSize = ThisSlot.GetWidget()->GetDesiredSize();
		const float ThisSlotDesiredHeight = WidgetDesiredSize.Y + ThisPadding.GetTotalSpaceAlong<Orient_Vertical>();

		// Figure out the size and local position of the child within the slot.  There is no vertical alignment, because 
		// it does not make sense in a panel where items are stacked vertically end-to-end.
		AlignmentArrangeResult XAlignmentResult = AlignChild<Orient_Horizontal>(AllottedGeometry.GetLocalSize().X, ThisSlot, ThisPadding);

		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ThisSlot.GetWidget(), FVector2D(XAlignmentResult.Offset, CurChildOffset + ThisPadding.Top), FVector2D(XAlignmentResult.Size, WidgetDesiredSize.Y)));
		return CurChildOffset + ThisSlotDesiredHeight;
	}

	float ArrangeChildHorizontalAndReturnOffset(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, const SScrollBox::FSlot& ThisSlot, float CurChildOffset) const
	{
		const FMargin& ThisPadding = ThisSlot.SlotPadding.Get();
		const FVector2D& WidgetDesiredSize = ThisSlot.GetWidget()->GetDesiredSize();
		const float ThisSlotDesiredWidth = WidgetDesiredSize.X + ThisPadding.GetTotalSpaceAlong<Orient_Horizontal>();

		// Figure out the size and local position of the child within the slot.  There is no horizontal alignment, because
		// it doesn't make sense in a panel where items are stacked horizontally end-to-end.
		AlignmentArrangeResult YAlignmentResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, ThisSlot, ThisPadding);

		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ThisSlot.GetWidget(), FVector2D(CurChildOffset + ThisPadding.Left, YAlignmentResult.Offset), FVector2D(WidgetDesiredSize.X, YAlignmentResult.Size)));
		return CurChildOffset + ThisSlotDesiredWidth;
	}

private:

	EOrientation Orientation;
};


SScrollBox::SScrollBox()
{
	bClippingProxy = true;
}

void SScrollBox::Construct( const FArguments& InArgs )
{
	check(InArgs._Style);

	Style = InArgs._Style;
	ScrollBarStyle = InArgs._ScrollBarStyle;
	DesiredScrollOffset = 0;
	bIsScrolling = false;
	bAnimateScroll = false;
	AmountScrolledWhileRightMouseDown = 0;
	PendingScrollTriggerAmount = 0;
	bShowSoftwareCursor = false;
	SoftwareCursorPosition = FVector2D::ZeroVector;
	OnUserScrolled = InArgs._OnUserScrolled;
	Orientation = InArgs._Orientation;
	bScrollToEnd = false;
	bIsScrollingActiveTimerRegistered = false;
	ConsumeMouseWheel = InArgs._ConsumeMouseWheel;
	TickScrollDelta = 0;
	AllowOverscroll = InArgs._AllowOverscroll;
	NavigationScrollPadding = InArgs._NavigationScrollPadding;
	NavigationDestination = InArgs._NavigationDestination;
	bTouchPanningCapture = false;

	if (InArgs._ExternalScrollbar.IsValid())
	{
		// An external scroll bar was specified by the user
		ScrollBar = InArgs._ExternalScrollbar;
		ScrollBar->SetOnUserScrolled(FOnUserScrolled::CreateSP(this, &SScrollBox::ScrollBar_OnUserScrolled));
		bScrollBarIsExternal = true;
	}
	else
	{
		// Make a scroll bar 
		ScrollBar = ConstructScrollBar();
		ScrollBar->SetThickness(InArgs._ScrollBarThickness);
		ScrollBar->SetUserVisibility(InArgs._ScrollBarVisibility);
		ScrollBar->SetScrollBarAlwaysVisible(InArgs._ScrollBarAlwaysVisible);

		bScrollBarIsExternal = false;
	}

	SAssignNew(ScrollPanel, SScrollPanel, InArgs.Slots)
		.Clipping(InArgs._Clipping)
		.Orientation(Orientation);

	if (Orientation == Orient_Vertical)
	{
		ConstructVerticalLayout();
	}
	else
	{
		ConstructHorizontalLayout();
	}

	ScrollBar->SetState( 0.0f, 1.0f );
}

void SScrollBox::OnClippingChanged()
{
	ScrollPanel->SetClipping(Clipping);
}

TSharedPtr<SScrollBar> SScrollBox::ConstructScrollBar()
{
	return TSharedPtr<SScrollBar>(SNew(SScrollBar)
		.Style(ScrollBarStyle)
		.Orientation(Orientation)
		.OnUserScrolled(this, &SScrollBox::ScrollBar_OnUserScrolled));
}

void SScrollBox::ConstructVerticalLayout()
{
	TSharedPtr<SHorizontalBox> PanelAndScrollbar;
	this->ChildSlot
	[
		SAssignNew(PanelAndScrollbar, SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 1.0f))
			[
				// Scroll panel that presents the scrolled content
				ScrollPanel.ToSharedRef()
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			[
				// Shadow: Hint to scroll up
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(this, &SScrollBox::GetStartShadowOpacity)
				.Image(&Style->TopShadowBrush)
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			[
				// Shadow: a hint to scroll down
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(this, &SScrollBox::GetEndShadowOpacity)
				.Image(&Style->BottomShadowBrush)
			]
		]
	];

	if (!bScrollBarIsExternal)
	{
		PanelAndScrollbar->AddSlot()
		.AutoWidth()
		[
			ScrollBar.ToSharedRef()
		];
	}
}

void SScrollBox::ConstructHorizontalLayout()
{
	TSharedPtr<SVerticalBox> PanelAndScrollbar;
	this->ChildSlot
	[
		SAssignNew(PanelAndScrollbar, SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.Padding(FMargin(0.0f, 0.0f, 1.0f, 0.0f))
			[
				// Scroll panel that presents the scrolled content
				ScrollPanel.ToSharedRef()
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Fill)
			[
				// Shadow: Hint to left
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(this, &SScrollBox::GetStartShadowOpacity)
				.Image(&Style->LeftShadowBrush)
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			[
				// Shadow: a hint to scroll right
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(this, &SScrollBox::GetEndShadowOpacity)
				.Image(&Style->RightShadowBrush)
			]
		]
	];

	if (!bScrollBarIsExternal)
	{
		PanelAndScrollbar->AddSlot()
			.AutoHeight()
			[
				ScrollBar.ToSharedRef()
			];
	}
}

/** Adds a slot to SScrollBox */
SScrollBox::FSlot& SScrollBox::AddSlot()
{
	SScrollBox::FSlot& NewSlot = *new SScrollBox::FSlot();
	ScrollPanel->Children.Add( &NewSlot );

	return NewSlot;
}

/** Removes a slot at the specified location */
void SScrollBox::RemoveSlot( const TSharedRef<SWidget>& WidgetToRemove )
{
	TPanelChildren<SScrollBox::FSlot>& Children = ScrollPanel->Children;
	for( int32 SlotIndex=0; SlotIndex < Children.Num(); ++SlotIndex )
	{
		if ( Children[SlotIndex].GetWidget() == WidgetToRemove )
		{
			Children.RemoveAt(SlotIndex);
			return;
		}
	}
}

void SScrollBox::ClearChildren()
{
	ScrollPanel->Children.Empty();
}

bool SScrollBox::IsRightClickScrolling() const
{
	return FSlateApplication::IsInitialized() && AmountScrolledWhileRightMouseDown >= FSlateApplication::Get().GetDragTriggerDistance() && this->ScrollBar->IsNeeded();
}

float SScrollBox::GetScrollOffset() const
{
	return DesiredScrollOffset;
}

void SScrollBox::SetScrollOffset( float NewScrollOffset )
{
	DesiredScrollOffset = NewScrollOffset;
	bScrollToEnd = false;
}

void SScrollBox::ScrollToStart()
{
	SetScrollOffset(0);
}

void SScrollBox::ScrollToEnd()
{
	bScrollToEnd = true;

	Invalidate(EInvalidateWidget::Layout);
}

void SScrollBox::ScrollDescendantIntoView(const TSharedPtr<SWidget>& WidgetToScrollIntoView, bool InAnimateScroll, EDescendantScrollDestination InDestination, float InScrollPadding)
{
	ScrollIntoViewRequest = [this, WidgetToScrollIntoView, InAnimateScroll, InDestination, InScrollPadding] (FGeometry AllottedGeometry) {
		InternalScrollDescendantIntoView(AllottedGeometry, WidgetToScrollIntoView, InAnimateScroll, InDestination, InScrollPadding);
	};

	BeginInertialScrolling();
}

bool SScrollBox::InternalScrollDescendantIntoView(const FGeometry& MyGeometry, const TSharedPtr<SWidget>& WidgetToFind, bool InAnimateScroll, EDescendantScrollDestination InDestination, float InScrollPadding)
{
	// We need to safely find the one WidgetToFind among our descendants.
	TSet< TSharedRef<SWidget> > WidgetsToFind;
	{
		WidgetsToFind.Add( WidgetToFind.ToSharedRef() );
	}
	TMap<TSharedRef<SWidget>, FArrangedWidget> Result;

	FindChildGeometries( MyGeometry, WidgetsToFind, Result );

	FArrangedWidget* WidgetGeometry = Result.Find( WidgetToFind.ToSharedRef() );
	if (!WidgetGeometry)
	{
		UE_LOG(LogSlate, Warning, TEXT("Unable to scroll to descendant as it's not a child of the scrollbox"));
	}

	if ( WidgetGeometry )
	{
		float ScrollOffset = 0.0f;
		if ( InDestination == EDescendantScrollDestination::TopOrLeft )
		{
			// Calculate how much we would need to scroll to bring this to the top/left of the scroll box
			const float WidgetPosition = GetScrollComponentFromVector(MyGeometry.AbsoluteToLocal(WidgetGeometry->Geometry.GetAbsolutePosition()));
			const float MyPosition = InScrollPadding;
			ScrollOffset = WidgetPosition - MyPosition;
		}
		else if ( InDestination == EDescendantScrollDestination::Center )
		{
			// Calculate how much we would need to scroll to bring this to the top/left of the scroll box
			const float WidgetPosition = GetScrollComponentFromVector(MyGeometry.AbsoluteToLocal(WidgetGeometry->Geometry.GetAbsolutePosition()) + (WidgetGeometry->Geometry.GetLocalSize() / 2));
			const float MyPosition = GetScrollComponentFromVector(MyGeometry.GetLocalSize() * FVector2D(0.5f, 0.5f));
			ScrollOffset = WidgetPosition - MyPosition;
		}
		else
		{
			const float WidgetStartPosition = GetScrollComponentFromVector(MyGeometry.AbsoluteToLocal(WidgetGeometry->Geometry.GetAbsolutePosition()));
			const float WidgetEndPosition = WidgetStartPosition + GetScrollComponentFromVector(WidgetGeometry->Geometry.GetLocalSize());
			const float ViewStartPosition = InScrollPadding;
			const float ViewEndPosition = GetScrollComponentFromVector(MyGeometry.GetLocalSize() - InScrollPadding);

			const float ViewDelta = ( ViewEndPosition - ViewStartPosition );
			const float WidgetDelta = ( WidgetEndPosition - WidgetStartPosition );

			if ( WidgetStartPosition < ViewStartPosition )
			{
				ScrollOffset = WidgetStartPosition - ViewStartPosition;
			}
			else if ( WidgetEndPosition > ViewEndPosition )
			{
				ScrollOffset = ( WidgetEndPosition - ViewDelta ) - ViewStartPosition;
			}
		}

		if ( ScrollOffset != 0.0f )
		{
			DesiredScrollOffset = ScrollPanel->PhysicalOffset;
			ScrollBy(MyGeometry, ScrollOffset, EAllowOverscroll::No, InAnimateScroll);
		}

		return true;
	}

	return false;
}

EOrientation SScrollBox::GetOrientation()
{
	return Orientation;
}

void SScrollBox::SetOrientation(EOrientation InOrientation)
{
	if (Orientation != InOrientation)
	{
		Orientation = InOrientation;
		if (!bScrollBarIsExternal)
		{
			ScrollBar = ConstructScrollBar();
		}
		ScrollPanel->SetOrientation(Orientation);
		if (Orientation == Orient_Vertical)
		{
			ConstructVerticalLayout();
		}
		else
		{
			ConstructHorizontalLayout();
		}
	}
}

void SScrollBox::SetScrollBarVisibility(EVisibility InVisibility)
{
	ScrollBar->SetUserVisibility(InVisibility);
}

void SScrollBox::SetScrollBarAlwaysVisible(bool InAlwaysVisible)
{
	ScrollBar->SetScrollBarAlwaysVisible(InAlwaysVisible);
}

void SScrollBox::SetScrollBarThickness(FVector2D InThickness)
{
	ScrollBar->SetThickness(InThickness);
}

void SScrollBox::SetScrollBarRightClickDragAllowed(bool bIsAllowed)
{
	bAllowsRightClickDragScrolling = bIsAllowed;
}

EActiveTimerReturnType SScrollBox::UpdateInertialScroll(double InCurrentTime, float InDeltaTime)
{
	bool bKeepTicking = bIsScrolling;

	if ( bIsScrolling )
	{
		InertialScrollManager.UpdateScrollVelocity(InDeltaTime);
		const float ScrollVelocityLocal = InertialScrollManager.GetScrollVelocity() / CachedGeometry.Scale;

		if (ScrollVelocityLocal != 0.f )
		{
			if ( CanUseInertialScroll(ScrollVelocityLocal) )
			{
				bKeepTicking = true;
				ScrollBy(CachedGeometry, ScrollVelocityLocal * InDeltaTime, AllowOverscroll, false);
			}
			else
			{
				InertialScrollManager.ClearScrollVelocity();
			}
		}
	}

	if ( AllowOverscroll == EAllowOverscroll::Yes )
	{
		// If we are currently in overscroll, the list will need refreshing.
		// Do this before UpdateOverscroll, as that could cause GetOverscroll() to be 0
		if ( Overscroll.GetOverscroll(CachedGeometry) != 0.0f )
		{
			bKeepTicking = true;
		}

		Overscroll.UpdateOverscroll(InDeltaTime);
	}

	TickScrollDelta = 0.f;

	if ( !bKeepTicking )
	{
		bIsScrolling = false;
		bIsScrollingActiveTimerRegistered = false;
		UpdateInertialScrollHandle.Reset();
	}

	return bKeepTicking ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
}

void SScrollBox::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	CachedGeometry = AllottedGeometry;

	if ( bTouchPanningCapture && (FSlateApplication::Get().GetCurrentTime() - LastScrollTime) > 0.10 )
	{
		InertialScrollManager.ClearScrollVelocity();
	}

	// If we needed a widget to be scrolled into view, make that happen.
	if ( ScrollIntoViewRequest )
	{
		ScrollIntoViewRequest(AllottedGeometry);
		ScrollIntoViewRequest = nullptr;
	}

	const FGeometry ScrollPanelGeometry = FindChildGeometry( AllottedGeometry, ScrollPanel.ToSharedRef() );
	const float ContentSize = GetScrollComponentFromVector(ScrollPanel->GetDesiredSize());

	if ( bScrollToEnd )
	{
		DesiredScrollOffset = FMath::Max(ContentSize - GetScrollComponentFromVector(ScrollPanelGeometry.GetLocalSize()), 0.0f);
		bScrollToEnd = false;
	}

	// If this scroll box has no size, do not compute a view fraction because it will be wrong and causes pop in when the size is available
	const float ViewFraction = GetScrollComponentFromVector(AllottedGeometry.GetLocalSize()) > 0 ? GetScrollComponentFromVector(ScrollPanelGeometry.GetLocalSize()) / ContentSize : 1;
	const float ViewOffset = FMath::Clamp<float>( DesiredScrollOffset/ContentSize, 0.0, 1.0 - ViewFraction );
	
	// Update the scrollbar with the clamped version of the offset
	float TargetPhysicalOffset = GetScrollComponentFromVector(ViewOffset*ScrollPanel->GetDesiredSize());
	if ( AllowOverscroll == EAllowOverscroll::Yes )
	{
		TargetPhysicalOffset += Overscroll.GetOverscroll(AllottedGeometry);
	}

	const bool bWasScrolling = bIsScrolling;
	bIsScrolling = !FMath::IsNearlyEqual(TargetPhysicalOffset, ScrollPanel->PhysicalOffset, 0.001f);
	ScrollPanel->PhysicalOffset = (bAnimateScroll)
		? FMath::FInterpTo(ScrollPanel->PhysicalOffset, TargetPhysicalOffset, InDeltaTime, 15.f)
		: TargetPhysicalOffset;

	if (bWasScrolling && !bIsScrolling)
	{
		Invalidate(EInvalidateWidget::Layout);
	}
	
	ScrollBar->SetState(ViewOffset, ViewFraction);
	if (!ScrollBar->IsNeeded())
	{
		// We cannot scroll, so ensure that there is no offset.
		ScrollPanel->PhysicalOffset = 0.0f;
	}
}

bool SScrollBox::ComputeVolatility() const
{
	return bIsScrolling || IsRightClickScrolling();
}

FReply SScrollBox::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsTouchEvent() && !bFingerOwningTouchInteraction.IsSet())
	{
		// Clear any inertia 
		InertialScrollManager.ClearScrollVelocity();
		// We have started a new interaction; track how far the user has moved since they put their finger down.
		AmountScrolledWhileRightMouseDown = 0;
		PendingScrollTriggerAmount = 0;
		// Someone put their finger down in this list, so they probably want to drag the list.
		bFingerOwningTouchInteraction = MouseEvent.GetPointerIndex();

		Invalidate(EInvalidateWidget::Layout);
	}
	return FReply::Unhandled();
}

FReply SScrollBox::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( !bFingerOwningTouchInteraction.IsSet() )
	{
		EndInertialScrolling();
	}

	if ( MouseEvent.IsTouchEvent() )
	{
		return FReply::Handled();
	}
	else
	{
		if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && ScrollBar->IsNeeded()  && bAllowsRightClickDragScrolling)
		{
			AmountScrolledWhileRightMouseDown = 0;

			Invalidate(EInvalidateWidget::Layout);

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();	
}

FReply SScrollBox::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && bAllowsRightClickDragScrolling)
	{
		if ( !bIsScrollingActiveTimerRegistered && IsRightClickScrolling() )
		{
			// Register the active timer to handle the inertial scrolling
			CachedGeometry = MyGeometry;
			BeginInertialScrolling();
		}

		AmountScrolledWhileRightMouseDown = 0;

		Invalidate(EInvalidateWidget::Layout);

		FReply Reply = FReply::Handled().ReleaseMouseCapture();
		bShowSoftwareCursor = false;

		// If we have mouse capture, snap the mouse back to the closest location that is within the panel's bounds
		if ( HasMouseCapture() )
		{
			FSlateRect PanelScreenSpaceRect = MyGeometry.GetLayoutBoundingRect();
			FVector2D CursorPosition = MyGeometry.LocalToAbsolute( SoftwareCursorPosition );

			FIntPoint BestPositionInPanel(
				FMath::RoundToInt( FMath::Clamp( CursorPosition.X, PanelScreenSpaceRect.Left, PanelScreenSpaceRect.Right ) ),
				FMath::RoundToInt( FMath::Clamp( CursorPosition.Y, PanelScreenSpaceRect.Top, PanelScreenSpaceRect.Bottom ) )
				);

			Reply.SetMousePos(BestPositionInPanel);
		}

		return Reply;
	}

	return FReply::Unhandled();
}

FReply SScrollBox::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	const float ScrollByAmountScreen = GetScrollComponentFromVector(MouseEvent.GetCursorDelta());
	const float ScrollByAmountLocal = ScrollByAmountScreen / MyGeometry.Scale;

	if ( MouseEvent.IsTouchEvent() )
	{
		FReply Reply = FReply::Unhandled();

		if ( !bTouchPanningCapture )
		{
			if ( bFingerOwningTouchInteraction.IsSet() && MouseEvent.IsTouchEvent() && !HasMouseCapture() )
			{
				PendingScrollTriggerAmount += ScrollByAmountScreen;

				if ( FMath::Abs(PendingScrollTriggerAmount) > FSlateApplication::Get().GetDragTriggerDistance() )
				{
					bTouchPanningCapture = true;
					ScrollBar->BeginScrolling();

					// The user has moved the list some amount; they are probably
					// trying to scroll. From now on, the list assumes the user is scrolling
					// until they lift their finger.
					Reply = FReply::Handled().CaptureMouse(AsShared());
				}
				else
				{
					Reply = FReply::Handled();
				}
			}
		}
		else
		{
			if ( bFingerOwningTouchInteraction.IsSet() && HasMouseCaptureByUser(MouseEvent.GetUserIndex(), MouseEvent.GetPointerIndex()) )
			{
				LastScrollTime = FSlateApplication::Get().GetCurrentTime();
				InertialScrollManager.AddScrollSample(-ScrollByAmountScreen, FSlateApplication::Get().GetCurrentTime());
				ScrollBy(MyGeometry, -ScrollByAmountLocal, EAllowOverscroll::Yes, false);

				Reply = FReply::Handled();
			}
		}

		return Reply;
	}
	else
	{
		if ( MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton)  && bAllowsRightClickDragScrolling)
		{
			// If scrolling with the right mouse button, we need to remember how much we scrolled.
			// If we did not scroll at all, we will bring up the context menu when the mouse is released.
			AmountScrolledWhileRightMouseDown += FMath::Abs(ScrollByAmountScreen);

			// Has the mouse moved far enough with the right mouse button held down to start capturing
			// the mouse and dragging the view?
			if ( IsRightClickScrolling() )
			{
				InertialScrollManager.AddScrollSample(-ScrollByAmountScreen, FPlatformTime::Seconds());
				const bool bDidScroll = ScrollBy(MyGeometry, -ScrollByAmountLocal, AllowOverscroll, false);

				FReply Reply = FReply::Handled();

				// Capture the mouse if we need to
				if ( HasMouseCapture() == false )
				{
					Reply.CaptureMouse(AsShared()).UseHighPrecisionMouseMovement(AsShared());
					SoftwareCursorPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
					bShowSoftwareCursor = true;
				}

				// Check if the mouse has moved.
				if ( bDidScroll )
				{
					SetScrollComponentOnVector(SoftwareCursorPosition, GetScrollComponentFromVector(SoftwareCursorPosition) + GetScrollComponentFromVector(MouseEvent.GetCursorDelta()));
				}

				return Reply;
			}
		}
	}

	return FReply::Unhandled();
}

void SScrollBox::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	if ( HasMouseCapture() == false )
	{
		// No longer scrolling (unless we have mouse capture)
		if ( AmountScrolledWhileRightMouseDown != 0 )
		{
			AmountScrolledWhileRightMouseDown = 0;
			Invalidate(EInvalidateWidget::Layout);
		}

		if ( MouseEvent.IsTouchEvent() )
		{
			bFingerOwningTouchInteraction.Reset();
		}
	}
}

FReply SScrollBox::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (ScrollBar->IsNeeded() && ConsumeMouseWheel != EConsumeMouseWheel::Never )
	{
		// Make sure scroll velocity is cleared so it doesn't fight with the mouse wheel input
		InertialScrollManager.ClearScrollVelocity();

		const bool bScrollWasHandled = ScrollBy(MyGeometry, -MouseEvent.GetWheelDelta() * GetGlobalScrollAmount(), EAllowOverscroll::No, false);

		if ( bScrollWasHandled && !bIsScrollingActiveTimerRegistered )
		{
			// Register the active timer to handle the inertial scrolling
			CachedGeometry = MyGeometry;
			BeginInertialScrolling();
		}

		return bScrollWasHandled ? FReply::Handled() : FReply::Unhandled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

bool SScrollBox::ScrollBy(const FGeometry& AllottedGeometry, float LocalScrollAmount, EAllowOverscroll Overscrolling, bool InAnimateScroll)
{
	Invalidate(EInvalidateWidget::LayoutAndVolatility);

	bAnimateScroll = InAnimateScroll;

	const float ContentSize = GetScrollComponentFromVector(ScrollPanel->GetDesiredSize());
	const FGeometry ScrollPanelGeometry = FindChildGeometry( AllottedGeometry, ScrollPanel.ToSharedRef() );

	const float PreviousScrollOffset = DesiredScrollOffset;

	if (LocalScrollAmount != 0 )
	{
		const float ScrollMin = 0.0f;
		const float ScrollMax = ContentSize - GetScrollComponentFromVector(ScrollPanelGeometry.GetLocalSize());

		if ( AllowOverscroll == EAllowOverscroll::Yes && Overscrolling == EAllowOverscroll::Yes && Overscroll.ShouldApplyOverscroll(DesiredScrollOffset == 0, DesiredScrollOffset == ScrollMax, LocalScrollAmount) )
		{
			Overscroll.ScrollBy(AllottedGeometry, LocalScrollAmount);
		}
		else
		{
			DesiredScrollOffset = FMath::Clamp(DesiredScrollOffset + LocalScrollAmount, ScrollMin, ScrollMax);
		}
	}

	OnUserScrolled.ExecuteIfBound(DesiredScrollOffset);

	return ConsumeMouseWheel == EConsumeMouseWheel::Always || DesiredScrollOffset != PreviousScrollOffset;
}

FCursorReply SScrollBox::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	if ( IsRightClickScrolling() )
	{
		// We hide the native cursor as we'll be drawing the software EMouseCursor::GrabHandClosed cursor
		return FCursorReply::Cursor( EMouseCursor::None );
	}
	else
	{
		return FCursorReply::Unhandled();
	}
}

FReply SScrollBox::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	CachedGeometry = MyGeometry;

	if ( HasMouseCaptureByUser(InTouchEvent.GetUserIndex(), InTouchEvent.GetPointerIndex()) )
	{
		AmountScrolledWhileRightMouseDown = 0;
		PendingScrollTriggerAmount = 0;
		bFingerOwningTouchInteraction.Reset();
		bTouchPanningCapture = false;

		ScrollBar->EndScrolling();

		Invalidate(EInvalidateWidget::Layout);

		BeginInertialScrolling();

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

void SScrollBox::OnMouseCaptureLost()
{
	SCompoundWidget::OnMouseCaptureLost();
}

FNavigationReply SScrollBox::OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent)
{
	TSharedPtr<SWidget> FocusedChild;
	int32 FocusedChildIndex = -1;
	int32 FocusedChildDirection = 0;

	// Find the child with focus currently so that we can find the next logical child we're going to move to.
	TPanelChildren<SScrollBox::FSlot>& Children = ScrollPanel->Children;
	for ( int32 SlotIndex=0; SlotIndex < Children.Num(); ++SlotIndex )
	{
		if ( Children[SlotIndex].GetWidget()->HasUserFocus(InNavigationEvent.GetUserIndex()).IsSet() ||
			 Children[SlotIndex].GetWidget()->HasUserFocusedDescendants(InNavigationEvent.GetUserIndex()) )
		{
			FocusedChild = Children[SlotIndex].GetWidget();
			FocusedChildIndex = SlotIndex;
			break;
		}
	}

	if ( FocusedChild.IsValid() )
	{
		if ( Orientation == Orient_Vertical )
		{
			switch ( InNavigationEvent.GetNavigationType() )
			{
			case EUINavigation::Up:
				FocusedChildDirection = -1;
				break;
			case EUINavigation::Down:
				FocusedChildDirection = 1;
				break;
			default:
				// If we don't handle this direction in our current orientation we can 
				// just allow the behavior of the boundary rule take over.
				return SCompoundWidget::OnNavigation(MyGeometry, InNavigationEvent);
			}
		}
		else // Orient_Horizontal
		{
			switch ( InNavigationEvent.GetNavigationType() )
			{
			case EUINavigation::Left:
				FocusedChildDirection = -1;
				break;
			case EUINavigation::Right:
				FocusedChildDirection = 1;
				break;
			default:
				// If we don't handle this direction in our current orientation we can 
				// just allow the behavior of the boundary rule take over.
				return SCompoundWidget::OnNavigation(MyGeometry, InNavigationEvent);
			}
		}

		// If the focused child index is in a valid range we know we can successfully focus
		// the new child we're moving focus to.
		if ( FocusedChildDirection != 0 )
		{
			TSharedPtr<SWidget> NextFocusableChild;

			// Search in the direction we need to move for the next focusable child of the scrollbox.
			for ( int32 ChildIndex = FocusedChildIndex + FocusedChildDirection; ChildIndex >= 0 && ChildIndex < Children.Num(); ChildIndex += FocusedChildDirection )
			{
				TSharedRef<SWidget> PossiblyFocusableChild = Children[ChildIndex].GetWidget();
				if ( PossiblyFocusableChild->SupportsKeyboardFocus() )
				{
					NextFocusableChild = PossiblyFocusableChild;
					break;
				}
			}

			// If we found a focusable child, scroll to it, and shift focus.
			if ( NextFocusableChild.IsValid() )
			{
				InternalScrollDescendantIntoView(MyGeometry, NextFocusableChild, false, NavigationDestination, NavigationScrollPadding);
				return FNavigationReply::Explicit(NextFocusableChild);
			}
		}
	}

	return SCompoundWidget::OnNavigation(MyGeometry, InNavigationEvent);
}

int32 SScrollBox::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
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

void SScrollBox::ScrollBar_OnUserScrolled( float InScrollOffsetFraction )
{
	const float ContentSize = GetScrollComponentFromVector(ScrollPanel->GetDesiredSize());
	const FGeometry ScrollPanelGeometry = FindChildGeometry(CachedGeometry, ScrollPanel.ToSharedRef());

	// Clamp to max scroll offset
	DesiredScrollOffset = FMath::Min(InScrollOffsetFraction * ContentSize, ContentSize - GetScrollComponentFromVector(ScrollPanelGeometry.GetLocalSize()));
	OnUserScrolled.ExecuteIfBound(DesiredScrollOffset);

	Invalidate(EInvalidateWidget::Layout);
}

const float ShadowFadeDistance = 32.0f;

FSlateColor SScrollBox::GetStartShadowOpacity() const
{
	// The shadow should only be visible when the user needs a hint that they can scroll up.
	const float ShadowOpacity = FMath::Clamp( ScrollPanel->PhysicalOffset/ShadowFadeDistance, 0.0f, 1.0f);
	
	return FLinearColor(1.0f, 1.0f, 1.0f, ShadowOpacity);
}

FSlateColor SScrollBox::GetEndShadowOpacity() const
{
	// The shadow should only be visible when the user needs a hint that they can scroll down.
	const float ShadowOpacity = (ScrollBar->DistanceFromBottom() * GetScrollComponentFromVector(ScrollPanel->GetDesiredSize()) / ShadowFadeDistance);
	
	return FLinearColor(1.0f, 1.0f, 1.0f, ShadowOpacity);
}

bool SScrollBox::CanUseInertialScroll(float ScrollAmount) const
{
	const auto CurrentOverscroll = Overscroll.GetOverscroll(CachedGeometry);

	// We allow sampling for the inertial scroll if we are not in the overscroll region,
	// Or if we are scrolling outwards of the overscroll region
	return CurrentOverscroll == 0.f || FMath::Sign(CurrentOverscroll) != FMath::Sign(ScrollAmount);
}

EAllowOverscroll SScrollBox::GetAllowOverscroll() const
{
	return AllowOverscroll;
}

void SScrollBox::SetAllowOverscroll(EAllowOverscroll NewAllowOverscroll)
{
	AllowOverscroll = NewAllowOverscroll;

	if (AllowOverscroll == EAllowOverscroll::No)
	{
		Overscroll.ResetOverscroll();
	}
}

void SScrollBox::BeginInertialScrolling()
{
	if ( !UpdateInertialScrollHandle.IsValid() )
	{
		bIsScrolling = true;
		bIsScrollingActiveTimerRegistered = true;
		UpdateInertialScrollHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SScrollBox::UpdateInertialScroll));
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

void SScrollBox::EndInertialScrolling()
{
	bIsScrolling = false;
	bIsScrollingActiveTimerRegistered = false;

	if ( UpdateInertialScrollHandle.IsValid() )
	{
		UnRegisterActiveTimer(UpdateInertialScrollHandle.ToSharedRef());
		UpdateInertialScrollHandle.Reset();
	}

	// Zero the scroll velocity so the panel stops immediately on mouse down, even if the user does not drag
	InertialScrollManager.ClearScrollVelocity();
}