// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Layout/WidgetPath.h"
#include "SlateGlobals.h"

DECLARE_CYCLE_STAT(TEXT("ToWidgetPath"), STAT_ToWidgetPath, STATGROUP_Slate);


FWidgetPath::FWidgetPath()
: Widgets( EVisibility::Visible )
, TopLevelWindow()
, VirtualPointerPositions()
{
}

FWidgetPath::FWidgetPath( TSharedPtr<SWindow> InTopLevelWindow, const FArrangedChildren& InWidgetPath )
: Widgets( InWidgetPath )
, TopLevelWindow(InTopLevelWindow)
, VirtualPointerPositions()
{
}

FWidgetPath::FWidgetPath( TArray<FWidgetAndPointer> InWidgetsAndPointers )
: Widgets( FArrangedChildren::Hittest2_FromArray(InWidgetsAndPointers) )
, TopLevelWindow( InWidgetsAndPointers.Num() > 0 ? StaticCastSharedRef<SWindow>(InWidgetsAndPointers[0].Widget) : TSharedPtr<SWindow>(nullptr) )
, VirtualPointerPositions( [&InWidgetsAndPointers]()
	{ 
		TArray< TSharedPtr<FVirtualPointerPosition> > Pointers;
		Pointers.Reserve(InWidgetsAndPointers.Num());
		for ( const FWidgetAndPointer& WidgetAndPointer : InWidgetsAndPointers )
		{
			Pointers.Add( WidgetAndPointer.PointerPosition );
		};
		return Pointers;
	}())
{
}

FWidgetPath FWidgetPath::GetPathDownTo( TSharedRef<const SWidget> MarkerWidget ) const
{
	FArrangedChildren ClippedPath(EVisibility::Visible);
	bool bCopiedMarker = false;
	for( int32 WidgetIndex = 0; !bCopiedMarker && WidgetIndex < Widgets.Num(); ++WidgetIndex )
	{
		ClippedPath.AddWidget( Widgets[WidgetIndex] );
		bCopiedMarker = (Widgets[WidgetIndex].Widget == MarkerWidget);
	}
		
	if ( bCopiedMarker )
	{
		// We found the MarkerWidget and copied the path down to (and including) it.
		return FWidgetPath( TopLevelWindow, ClippedPath );
	}
	else
	{
		// The MarkerWidget was not in the widget path. We failed.
		return FWidgetPath( nullptr, FArrangedChildren(EVisibility::Visible) );		
	}	
}

const TSharedPtr<FVirtualPointerPosition>& FWidgetPath::GetCursorAt( int32 Index ) const
{
	return VirtualPointerPositions[Index];
}


bool FWidgetPath::ContainsWidget( TSharedRef<const SWidget> WidgetToFind ) const
{
	for(int32 WidgetIndex = 0; WidgetIndex < Widgets.Num(); ++WidgetIndex)
	{
		if ( Widgets[WidgetIndex].Widget == WidgetToFind )
		{
			return true;
		}
	}
		
	return false;
}


TOptional<FArrangedWidget> FWidgetPath::FindArrangedWidget( TSharedRef<const SWidget> WidgetToFind ) const
{
	for(int32 WidgetIndex = 0; WidgetIndex < Widgets.Num(); ++WidgetIndex)
	{
		if ( Widgets[WidgetIndex].Widget == WidgetToFind )
		{
			return Widgets[WidgetIndex];
		}
	}

	return TOptional<FArrangedWidget>();
}

TOptional<FWidgetAndPointer> FWidgetPath::FindArrangedWidgetAndCursor( TSharedRef<const SWidget> WidgetToFind ) const
{
	const int32 Index = Widgets.IndexOfByPredicate( [&WidgetToFind]( const FArrangedWidget& SomeWidget )
	{
		return SomeWidget.Widget == WidgetToFind;
	} );

	return (Index != INDEX_NONE)
		? FWidgetAndPointer( Widgets[Index], VirtualPointerPositions[Index] )
		: FWidgetAndPointer();
}

	
TSharedRef<SWindow> FWidgetPath::GetWindow()
{
	check(IsValid());

	TSharedRef<SWindow> FirstWidgetWindow = StaticCastSharedRef<SWindow>(Widgets[0].Widget);
	return FirstWidgetWindow;
}


TSharedRef<SWindow> FWidgetPath::GetWindow() const
{
	check(IsValid());

	TSharedRef<SWindow> FirstWidgetWindow = StaticCastSharedRef<SWindow>(Widgets[0].Widget);
	return FirstWidgetWindow;
}


bool FWidgetPath::IsValid() const
{
	return Widgets.Num() > 0;
}

	
FString FWidgetPath::ToString() const
{
	FString StringBuffer;
	for( int32 WidgetIndex = Widgets.Num()-1; WidgetIndex >= 0; --WidgetIndex )
	{
		StringBuffer += Widgets[WidgetIndex].ToString();
		StringBuffer += TEXT("\n");
	}
	return StringBuffer;
}


/** Matches any widget that is focusable */
struct FFocusableWidgetMatcher
{
	bool IsMatch( const TSharedRef<const SWidget>& InWidget ) const
	{
		return InWidget->IsEnabled() && InWidget->SupportsKeyboardFocus();
	}
};

/**
 * Move focus either forward on backward in the path level specified by PathLevel.
 * That is, this movement of focus will modify the subtree under Widgets(PathLevel).
 *
 * @param PathLevel       The level in this WidgetPath whose focus to move.
 * @param MoveDirectin    Move focus forward or backward?
 *
 * @return true if the focus moved successfully, false if we were unable to move focus
 */
bool FWidgetPath::MoveFocus(int32 PathLevel, EUINavigation NavigationType)
{
	check(NavigationType == EUINavigation::Next || NavigationType == EUINavigation::Previous);

	const int32 MoveDirectionAsInt = (NavigationType == EUINavigation::Next)
		? +1
		: -1;


	if ( PathLevel == Widgets.Num()-1 )
	{
		// We are the currently focused widget because we are at the very bottom of focus path.
		if (NavigationType == EUINavigation::Next)
		{
			// EFocusMoveDirection::Next implies descend, so try to find a focusable descendant.
			return ExtendPathTo( FFocusableWidgetMatcher() );
		}
		else
		{
			// EFocusMoveDirection::Previous implies move focus up a level.
			return false;
		}
		
	}
	else if ( Widgets.Num() > 1 )
	{
		// We are not the last widget in the path.
		// GOAL: Look for a focusable descendant to the left or right of the currently focused path.
	
		// Arrange the children so we can iterate through them regardless of widget type.
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		Widgets[PathLevel].Widget->ArrangeChildren( Widgets[PathLevel].Geometry, ArrangedChildren );

		// Find the currently focused child among the children.
		int32 FocusedChildIndex = ArrangedChildren.FindItemIndex( Widgets[PathLevel+1] );
		FocusedChildIndex = (FocusedChildIndex) % ArrangedChildren.Num() + MoveDirectionAsInt;

		// Now actually search for the widget.
		for( ; FocusedChildIndex < ArrangedChildren.Num() && FocusedChildIndex >= 0; FocusedChildIndex += MoveDirectionAsInt )
		{
			// Neither disabled widgets nor their children can be focused.
			if ( ArrangedChildren[FocusedChildIndex].Widget->IsEnabled() )
			{
				// Look for a focusable descendant.
				FArrangedChildren PathToFocusableChild = GeneratePathToWidget(FFocusableWidgetMatcher(), ArrangedChildren[FocusedChildIndex], NavigationType);
				// Either we found a focusable descendant, or an immediate child that is focusable.
				const bool bFoundNextFocusable = ( PathToFocusableChild.Num() > 0 ) || ArrangedChildren[FocusedChildIndex].Widget->SupportsKeyboardFocus();
				if ( bFoundNextFocusable )
				{
					// We found the next focusable widget, so make this path point at the new widget by:
					// First, truncating the FocusPath up to the current level (i.e. PathLevel).
					Widgets.Remove( PathLevel+1, Widgets.Num()-PathLevel-1 );
					// Second, add the immediate child that is focus or whose descendant is focused.
					Widgets.AddWidget( ArrangedChildren[FocusedChildIndex] );
					// Add path to focused descendants if any.
					Widgets.Append( PathToFocusableChild );
					// We successfully moved focus!
					return true;
				}
			}
		}		
		
	}

	return false;
}

/** Construct a weak widget path from a widget path. Defaults to an invalid path. */
FWeakWidgetPath::FWeakWidgetPath( const FWidgetPath& InWidgetPath )
: Window( InWidgetPath.TopLevelWindow )
{
	for( int32 WidgetIndex = 0; WidgetIndex < InWidgetPath.Widgets.Num(); ++WidgetIndex )
	{
		Widgets.Add( TWeakPtr<SWidget>( InWidgetPath.Widgets[WidgetIndex].Widget ) );
	}
}

/** Make a non-weak WidgetPath out of this WeakWidgetPath. Do this by computing all the relevant geometries and converting the weak pointers to TSharedPtr. */	
FWidgetPath FWeakWidgetPath::ToWidgetPath(EInterruptedPathHandling::Type InterruptedPathHandling, const FPointerEvent* PointerEvent ) const
{
	FWidgetPath WidgetPath;
	ToWidgetPath( WidgetPath, InterruptedPathHandling, PointerEvent );
	return WidgetPath;
}

TSharedRef<FWidgetPath> FWeakWidgetPath::ToWidgetPathRef(EInterruptedPathHandling::Type InterruptedPathHandling, const FPointerEvent* PointerEvent) const
{
	TSharedRef<FWidgetPath> WidgetPath = MakeShareable(new FWidgetPath());
	ToWidgetPath(WidgetPath.Get(), InterruptedPathHandling, PointerEvent);
	return WidgetPath;
}

FWeakWidgetPath::EPathResolutionResult::Result FWeakWidgetPath::ToWidgetPath( FWidgetPath& WidgetPath, EInterruptedPathHandling::Type InterruptedPathHandling, const FPointerEvent* PointerEvent ) const
{
	SCOPE_CYCLE_COUNTER(STAT_ToWidgetPath);

	TArray<FWidgetAndPointer> PathWithGeometries;
	TArray< TSharedPtr<SWidget> > WidgetPtrs;
		
	// Convert the weak pointers into shared pointers because we are about to do something with this path instead of just observe it.
	TSharedPtr<SWindow> TopLevelWindowPtr = Window.Pin();
	for( TArray< TWeakPtr<SWidget> >::TConstIterator SomeWeakWidgetPtr( Widgets ); SomeWeakWidgetPtr; ++SomeWeakWidgetPtr )
	{
		WidgetPtrs.Add( SomeWeakWidgetPtr->Pin() );
	}	
	
	// The path can get interrupted if some subtree of widgets disappeared, but we still maintain weak references to it.
	bool bPathUninterrupted = false;

	// For each widget in the path compute the geometry. We are able to do this starting with the top-level window because it knows its own geometry.
	if ( TopLevelWindowPtr.IsValid() )
	{
		bPathUninterrupted = true;

		FGeometry ParentGeometry = TopLevelWindowPtr->GetWindowGeometryInScreen();
		PathWithGeometries.Add( FWidgetAndPointer(
			FArrangedWidget( TopLevelWindowPtr.ToSharedRef(), ParentGeometry ),
			// @todo slate: this should be the cursor's virtual position in window space.
			TSharedPtr<FVirtualPointerPosition>() ) );
		
		FArrangedChildren ArrangedChildren(EVisibility::Visible, true);
		
		TSharedPtr<FVirtualPointerPosition> VirtualPointerPos;
		// For every widget in the vertical slice...
		for( int32 WidgetIndex = 0; bPathUninterrupted && WidgetIndex < WidgetPtrs.Num()-1; ++WidgetIndex )
		{
			TSharedPtr<SWidget> CurWidget = WidgetPtrs[WidgetIndex];

			bool bFoundChild = false;
			if ( CurWidget.IsValid() )
			{
				// Arrange the widget's children to find their geometries.
				ArrangedChildren.Empty();
				CurWidget->ArrangeChildren(ParentGeometry, ArrangedChildren);
				
				// Find the next widget in the path among the arranged children.
				for( int32 SearchIndex = 0; !bFoundChild && SearchIndex < ArrangedChildren.Num(); ++SearchIndex )
				{					
					FArrangedWidget& ArrangedWidget = ArrangedChildren[SearchIndex];

					if ( ArrangedWidget.Widget == WidgetPtrs[WidgetIndex + 1] )
					{
						if( PointerEvent && !VirtualPointerPos.IsValid() )
						{
							VirtualPointerPos = CurWidget->TranslateMouseCoordinateFor3DChild( ArrangedWidget.Widget, ParentGeometry, PointerEvent->GetScreenSpacePosition(), PointerEvent->GetLastScreenSpacePosition() );
						}

						bFoundChild = true;
						// Remember the widget, the associated geometry, and the pointer position in a transformed space.
						PathWithGeometries.Add( FWidgetAndPointer(ArrangedChildren[SearchIndex], VirtualPointerPos) );
						// The next child in the vertical slice will be arranged with respect to its parent's geometry.
						ParentGeometry = ArrangedChildren[SearchIndex].Geometry;
					}
				}
			}

			bPathUninterrupted = bFoundChild;
			if (!bFoundChild && InterruptedPathHandling == EInterruptedPathHandling::ReturnInvalid )
			{
				return EPathResolutionResult::Truncated;
			}
		}			
	}
	
	WidgetPath = FWidgetPath( PathWithGeometries );
	return bPathUninterrupted ? EPathResolutionResult::Live : EPathResolutionResult::Truncated;
}

bool FWeakWidgetPath::ContainsWidget( const TSharedRef< const SWidget >& SomeWidget ) const
{
	for ( int32 WidgetIndex=0; WidgetIndex<Widgets.Num(); ++WidgetIndex )
	{
		if (Widgets[WidgetIndex].Pin() == SomeWidget)
		{
			return true;
		}
	}

	return false;
}

FWidgetPath FWeakWidgetPath::ToNextFocusedPath(EUINavigation NavigationType) const
{
	return ToNextFocusedPath(NavigationType, FNavigationReply::Escape(), FArrangedWidget::NullWidget);
}

FWidgetPath FWeakWidgetPath::ToNextFocusedPath(EUINavigation NavigationType, const FNavigationReply& NavigationReply, const FArrangedWidget& RuleWidget) const
{
	check(NavigationType == EUINavigation::Next || NavigationType == EUINavigation::Previous);

	// Make a copy of the focus path. We will mutate it until it meets the necessary requirements.
	FWidgetPath NewFocusPath = this->ToWidgetPath();
	TSharedPtr<SWidget> CurrentlyFocusedWidget = this->Widgets.Last().Pin();

	bool bMovedFocus = false;
	// Attempt to move the focus starting at the leafmost widget and bubbling up to the root (i.e. the window)
	for (int32 FocusNodeIndex=NewFocusPath.Widgets.Num()-1; !bMovedFocus && FocusNodeIndex >= 0; --FocusNodeIndex)
	{
		// We've reached the stop boundary and not yet moved focus, so don't advance.
		if ( NavigationReply.GetBoundaryRule() == EUINavigationRule::Stop && RuleWidget.Widget == NewFocusPath.Widgets[FocusNodeIndex].Widget )
		{
			break;
		}

		//TODO Slate Navigation Handle Wrap.

		bMovedFocus = NewFocusPath.MoveFocus(FocusNodeIndex, NavigationType);
	}

	return NewFocusPath;
}
