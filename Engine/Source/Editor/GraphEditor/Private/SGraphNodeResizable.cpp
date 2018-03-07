// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SGraphNodeResizable.h"
#include "ScopedTransaction.h"
#include "Framework/Application/SlateApplication.h"

namespace GraphNodeResizableDefs
{
	/** Size of the hit result border for the window borders */
	static const FSlateRect HitResultBorderSize( 10, 10, 10, 10 );

	/** Default Title Bar Size */
	static const float DefaultTitleBarHeight = 12.f;

	/** Minimum size for node */
	static const FVector2D MinNodeSize( 30.0f, 30.0f );

	/** Maximum size for node */
	static const FVector2D MaxNodeSize( 400.0f, 400.0f );
}

bool SGraphNodeResizable::InSelectionArea(EResizableWindowZone InMouseZone) const
{
	return (	(InMouseZone == CRWZ_RightBorder) || (InMouseZone == CRWZ_BottomBorder) || (InMouseZone == CRWZ_BottomRightBorder) || 
				(InMouseZone == CRWZ_LeftBorder) || (InMouseZone == CRWZ_TopBorder) || (InMouseZone == CRWZ_TopLeftBorder) ||
				(InMouseZone == CRWZ_TopRightBorder) || (InMouseZone == CRWZ_BottomLeftBorder) );
}

void SGraphNodeResizable::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{	
	// Determine the zone the mouse is in
	if( !bUserIsDragging )
	{
		FVector2D LocalMouseCoordinates = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
		MouseZone = FindMouseZone(LocalMouseCoordinates);
		SNodePanel::SNode::OnMouseEnter( MyGeometry, MouseEvent );
	}
}

void SGraphNodeResizable::OnMouseLeave( const FPointerEvent& MouseEvent ) 
{
	if( !bUserIsDragging )
	{
		// Reset our mouse zone
		MouseZone = CRWZ_NotInWindow;
		SNodePanel::SNode::OnMouseLeave( MouseEvent );
	}
}

FCursorReply SGraphNodeResizable::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	if (MouseZone == CRWZ_RightBorder || MouseZone == CRWZ_LeftBorder)
	{
		// right/left of node
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}
	else if (MouseZone == CRWZ_BottomRightBorder || MouseZone == CRWZ_TopLeftBorder)
	{
		// bottom right / top left hand corner
		return FCursorReply::Cursor( EMouseCursor::ResizeSouthEast );
	}
	else if (MouseZone == CRWZ_BottomBorder || MouseZone == CRWZ_TopBorder)
	{
		// bottom / top of node
		return FCursorReply::Cursor(EMouseCursor::ResizeUpDown);
	}
	else if (MouseZone == CRWZ_BottomLeftBorder || MouseZone == CRWZ_TopRightBorder)
	{
		// bottom left / top right hand corner
		return FCursorReply::Cursor( EMouseCursor::ResizeSouthWest );
	}
	else if (MouseZone == CRWZ_TitleBar) 
	{
		return FCursorReply::Cursor(EMouseCursor::CardinalCross);
	}

	return FCursorReply::Unhandled();
}

FReply SGraphNodeResizable::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( InSelectionArea() && (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && IsEditable.Get() ) 
	{
		bUserIsDragging = true;
		StoredUserSize = UserSize;
		DragSize = UserSize;
		//Find node anchor point
		InitNodeAnchorPoint();

		return FReply::Handled().CaptureMouse( SharedThis(this) );
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SGraphNodeResizable::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && bUserIsDragging )
	{
		bUserIsDragging = false;

		// Resize the node	
		UserSize.X = FMath::RoundToFloat(UserSize.X);
		UserSize.Y = FMath::RoundToFloat(UserSize.Y);

		GetNodeObj()->ResizeNode(UserSize);

		// End resize transaction
		ResizeTransactionPtr.Reset();

		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}


FReply SGraphNodeResizable::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bUserIsDragging)
	{
		FVector2D GraphSpaceCoordinates = NodeCoordToGraphCoord( MouseEvent.GetScreenSpacePosition() );
		FVector2D OldGraphSpaceCoordinates = NodeCoordToGraphCoord( MouseEvent.GetLastScreenSpacePosition() );
		TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		FVector2D Delta = (GraphSpaceCoordinates - OldGraphSpaceCoordinates) / (OwnerWindow.IsValid() ? OwnerWindow->GetDPIScaleFactor() : 1.0f);
		

		//Clamp delta value based on resizing direction
		if( MouseZone == CRWZ_LeftBorder || MouseZone == CRWZ_RightBorder )
		{
			Delta.Y = 0.0f;
		}
		else if( MouseZone == CRWZ_TopBorder || MouseZone == CRWZ_BottomBorder )
		{
			Delta.X = 0.0f;
		}

		//Resize node delta value
		FVector2D DeltaNodeSize = Delta;

		//Modify node size delta value based on resizing direction
		if(	(MouseZone == CRWZ_LeftBorder) || (MouseZone == CRWZ_TopBorder) || (MouseZone == CRWZ_TopLeftBorder) )
		{
			DeltaNodeSize = -DeltaNodeSize;
		}
		else if( MouseZone == CRWZ_TopRightBorder )
		{
			DeltaNodeSize.Y = -DeltaNodeSize.Y;
		}
		else if( MouseZone == CRWZ_BottomLeftBorder )
		{
			DeltaNodeSize.X = -DeltaNodeSize.X;
		}
		// Apply delta unfiltered to DragSize
		DragSize.X += DeltaNodeSize.X;
		DragSize.Y += DeltaNodeSize.Y;
		// apply snap
		const float SnapSize = SNodePanel::GetSnapGridSize();
		FVector2D SnappedSize;
		SnappedSize.X = SnapSize * FMath::RoundToFloat( DragSize.X/SnapSize );
		SnappedSize.Y = SnapSize * FMath::RoundToFloat( DragSize.Y/SnapSize );

		// Enforce min/max sizing
		const FVector2D MinSize = GetNodeMinimumSize();
		SnappedSize.X = FMath::Max( SnappedSize.X, MinSize.X );
		SnappedSize.Y = FMath::Max( SnappedSize.Y, MinSize.Y );

		const FVector2D MaxSize = GetNodeMaximumSize();
		SnappedSize.X = FMath::Min( SnappedSize.X, MaxSize.X );
		SnappedSize.Y = FMath::Min( SnappedSize.Y, MaxSize.Y );

		FVector2D DeltaNodePos(0,0);

		if( UserSize != SnappedSize )
		{
			//Modify node position (resizing top and left sides)
			if( MouseZone != CRWZ_BottomBorder && MouseZone != CRWZ_RightBorder && MouseZone != CRWZ_BottomRightBorder )
			{
				//Delta value to move graph node position
				DeltaNodePos = UserSize - SnappedSize;

				//Clamp position delta based on resizing direction
				if( MouseZone == CRWZ_BottomLeftBorder )
				{
					DeltaNodePos.Y = 0.0f;
				}
				else if( MouseZone == CRWZ_TopRightBorder )
				{
					DeltaNodePos.X = 0.0f;
				}
			}
			UserSize = SnappedSize;
			GraphNode->ResizeNode( UserSize );
			DeltaNodePos = GetCorrectedNodePosition() - GetPosition();
		}

		if (!ResizeTransactionPtr.IsValid() && UserSize != StoredUserSize)
		{
			// Start resize transaction.  The transaction is started here so all MoveTo actions are captured while empty
			//	transactions are not created
			ResizeTransactionPtr = MakeShareable(new FScopedTransaction(NSLOCTEXT("GraphEditor", "ResizeNodeAction", "Resize Node")));
		}

		SGraphNode::FNodeSet NodeFilter;
		SGraphNode::MoveTo( GetPosition() + DeltaNodePos, NodeFilter );
	}
	else
	{
		const FVector2D LocalMouseCoordinates = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
		MouseZone = FindMouseZone(LocalMouseCoordinates);
	}
	return SGraphNode::OnMouseMove( MyGeometry, MouseEvent );
}

void SGraphNodeResizable::InitNodeAnchorPoint()
{
	NodeAnchorPoint = GetPosition();
	
	if( (MouseZone == CRWZ_LeftBorder) || (MouseZone == CRWZ_TopBorder) || (MouseZone == CRWZ_TopLeftBorder) )
	{
		NodeAnchorPoint += UserSize;
	}
	else if( MouseZone == CRWZ_BottomLeftBorder )
	{
		NodeAnchorPoint.X += UserSize.X;
	}
	else if( MouseZone == CRWZ_TopRightBorder )
	{
		NodeAnchorPoint.Y += UserSize.Y;
	}
}


FVector2D SGraphNodeResizable::GetCorrectedNodePosition() const
{
	FVector2D CorrectedPos = NodeAnchorPoint;
	
	if( (MouseZone == CRWZ_LeftBorder) || (MouseZone == CRWZ_TopBorder) || (MouseZone == CRWZ_TopLeftBorder) )
	{
		CorrectedPos -= UserSize;
	}
	else if( MouseZone == CRWZ_BottomLeftBorder )
	{
		CorrectedPos.X -= UserSize.X;
	}
	else if( MouseZone == CRWZ_TopRightBorder )
	{
		CorrectedPos.Y -= UserSize.Y;
	}

	return CorrectedPos;
}

SGraphNodeResizable::EResizableWindowZone SGraphNodeResizable::FindMouseZone(const FVector2D& LocalMouseCoordinates) const
{
	EResizableWindowZone OutMouseZone = CRWZ_NotInWindow;
	const FSlateRect HitResultBorderSize = GetHitTestingBorder();
	const FVector2D NodeSize = GetDesiredSize();

	// Test for hit in location of 'grab' zone
	if (LocalMouseCoordinates.Y > ( NodeSize.Y - HitResultBorderSize.Bottom))
	{
		OutMouseZone = CRWZ_BottomBorder;
	}
	else if (LocalMouseCoordinates.Y <= (HitResultBorderSize.Top))
	{
		OutMouseZone = CRWZ_TopBorder;
	}
	else if (LocalMouseCoordinates.Y <= GetTitleBarHeight())
	{
		OutMouseZone = CRWZ_TitleBar;
	}

	if (LocalMouseCoordinates.X > (NodeSize.X - HitResultBorderSize.Right))
	{
		if (OutMouseZone == CRWZ_BottomBorder)
		{
			OutMouseZone = CRWZ_BottomRightBorder;
		}
		else if (OutMouseZone == CRWZ_TopBorder)
		{
			OutMouseZone = CRWZ_TopRightBorder;
		}
		else
		{
			OutMouseZone = CRWZ_RightBorder;
		}
	}
	else if (LocalMouseCoordinates.X <= HitResultBorderSize.Left)
	{
		if (OutMouseZone == CRWZ_TopBorder)
		{
			OutMouseZone = CRWZ_TopLeftBorder;
		}
		else if (OutMouseZone == CRWZ_BottomBorder)
		{
			OutMouseZone = CRWZ_BottomLeftBorder;
		}
		else
		{
			OutMouseZone = CRWZ_LeftBorder;
		}
	}
	
	// Test for hit on rest of frame
	if (OutMouseZone == CRWZ_NotInWindow)
	{
		if (LocalMouseCoordinates.Y > HitResultBorderSize.Top) 
		{
			OutMouseZone = CRWZ_InWindow;
		}
		else if (LocalMouseCoordinates.X > HitResultBorderSize.Left) 
		{
			OutMouseZone = CRWZ_InWindow;
		}
	}
	return OutMouseZone;
}

float SGraphNodeResizable::GetTitleBarHeight() const
{
	// this can probably just be SGraphNode::GetTitleRect().Height()
	return GraphNodeResizableDefs::DefaultTitleBarHeight;
}

FVector2D SGraphNodeResizable::GetNodeMinimumSize() const
{
	return GraphNodeResizableDefs::MinNodeSize;
}

FVector2D SGraphNodeResizable::GetNodeMaximumSize() const
{
	return GraphNodeResizableDefs::MaxNodeSize;
}

FSlateRect SGraphNodeResizable::GetHitTestingBorder() const
{
	return GraphNodeResizableDefs::HitResultBorderSize;
}
