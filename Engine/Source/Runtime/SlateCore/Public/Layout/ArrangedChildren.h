// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Layout/ArrangedWidget.h"

/**
 * The results of an ArrangeChildren are always returned as an FArrangedChildren.
 * FArrangedChildren supports a filter that is useful for excluding widgets with unwanted
 * visibilities.
 */
class SLATECORE_API FArrangedChildren
{
	private:
	
	EVisibility VisibilityFilter;
	
	public:

	typedef TArray<FArrangedWidget, TInlineAllocator<16>> FArrangedWidgetArray;

	/**
	 * Construct a new container for arranged children that only accepts children that match the VisibilityFilter.
	 * e.g.
	 *  FArrangedChildren ArrangedChildren( VIS_All ); // Children will be included regardless of visibility
	 *  FArrangedChildren ArrangedChildren( EVisibility::Visible ); // Only visible children will be included
	 *  FArrangedChildren ArrangedChildren( EVisibility::Collapsed | EVisibility::Hidden ); // Only hidden and collapsed children will be included.
	 */
	FArrangedChildren( EVisibility InVisibilityFilter, bool bInAllow3DWidgets = false )
	: VisibilityFilter( InVisibilityFilter )
	, bAllow3DWidgets( bInAllow3DWidgets )
	{
	}

	// @todo hittest2.0 : we should get rid of this eventually.
	static FArrangedChildren Hittest2_FromArray(const TArray<FWidgetAndPointer>& InWidgets)
	{
		FArrangedChildren Temp( EVisibility::All );
		Temp.Array.Reserve(InWidgets.Num());
		for (const FWidgetAndPointer& WidgetAndPointer : InWidgets)
		{
			Temp.Array.Add(WidgetAndPointer);
		}		
		return Temp;
	}

	/** Reverse the order of the arranged children */
	void Reverse()
	{
		int32 LastElementIndex = Array.Num() - 1;
		for (int32 WidgetIndex = 0; WidgetIndex < Array.Num()/2; ++WidgetIndex )
		{
			Array.Swap( WidgetIndex, LastElementIndex - WidgetIndex );
		}
	}

	/**
	 * Add an arranged widget (i.e. widget and its resulting geometry) to the list of Arranged children.
	 *
	 * @param VisibilityOverride   The arrange function may override the visibility of the widget for the purposes
	 *                             of layout or performance (i.e. prevent redundant call to Widget->GetVisibility())
	 * @param InWidgetGeometry     The arranged widget (i.e. widget and its geometry)
	 */
	FORCEINLINE_DEBUGGABLE void AddWidget(EVisibility VisibilityOverride, const FArrangedWidget& InWidgetGeometry)
	{
		if ( Accepts(VisibilityOverride) )
		{
			Array.Add(InWidgetGeometry);
		}
	}
	
	/**
	 * Add an arranged widget (i.e. widget and its resulting geometry) to the list of Arranged children
	 * based on the the visibility filter and the arranged widget's visibility
	 */
	void AddWidget(const FArrangedWidget& InWidgetGeometry);

	FORCEINLINE_DEBUGGABLE EVisibility GetFilter() const
	{
		return VisibilityFilter;
	}

	FORCEINLINE_DEBUGGABLE bool Accepts(EVisibility InVisibility) const
	{
		return 0 != (InVisibility.Value & VisibilityFilter.Value);
	}

	FORCEINLINE_DEBUGGABLE bool Allows3DWidgets() const
	{
		return bAllow3DWidgets;
	}

	// We duplicate parts of TArray's interface here!
	// Inheriting from "public TArray<FArrangedWidget>"
	// saves us this boilerplate, but causes instantiation of
	// many template combinations that we do not want.

private:
	/** Internal representation of the array widgets */
	FArrangedWidgetArray Array;

	bool bAllow3DWidgets;
public:

	FORCEINLINE FArrangedWidgetArray& GetInternalArray()
	{
		return Array;
	}

	FORCEINLINE const FArrangedWidgetArray& GetInternalArray() const
	{
		return Array;
	}

	FORCEINLINE int32 Num() const
	{
		return Array.Num();
	}

	FORCEINLINE const FArrangedWidget& operator[]( int32 Index ) const
	{
		return Array[Index];
	}

	FORCEINLINE FArrangedWidget& operator[](int32 Index)
	{
		return Array[Index];
	}

	FORCEINLINE const FArrangedWidget& Last() const
	{
		return Array.Last();
	}

	FORCEINLINE FArrangedWidget& Last()
	{
		return Array.Last();
	}

	FORCEINLINE int32 FindItemIndex(const FArrangedWidget& ItemToFind) const
	{
		return Array.Find(ItemToFind);
	}

	template<typename PredicateType>
	FORCEINLINE int32 IndexOfByPredicate(const PredicateType& Pred) const
	{
		return Array.IndexOfByPredicate( Pred );
	}

	FORCEINLINE void Remove(int32 Index, int32 Count=1)
	{
		Array.RemoveAt(Index, Count);
	}

	FORCEINLINE void Append(const FArrangedChildren& Source)
	{
		Array.Append( Source.Array );
	}

	FORCEINLINE void Empty()
	{
		Array.Reset();
	}

	FORCEINLINE bool IsValidIndex(int32 Index)
	{
		return Array.IsValidIndex(Index);
	}
};
