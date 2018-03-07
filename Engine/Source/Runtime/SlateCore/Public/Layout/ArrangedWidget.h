// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "Input/Events.h"

class SWidget;

/**
 * A pair: Widget and its Geometry. Widgets populate an list of WidgetGeometries
 * when they arrange their children. See SWidget::ArrangeChildren.
 */
class FArrangedWidget
{
public:

	FArrangedWidget( TSharedRef<SWidget> InWidget, const FGeometry& InGeometry )
		: Geometry(InGeometry)
		, Widget(InWidget)
	{ }

	SLATECORE_API static FArrangedWidget NullWidget;

public:

	/**
	 * Gets the string representation of the Widget and corresponding Geometry.
	 *
	 * @return String representation.
	 */
	FString ToString( ) const;

public:

	/**
	 * Compares this widget arrangement with another for equality.
	 *
	 * @param Other The other arrangement to compare with.
	 * @return true if the two arrangements are equal, false otherwise.
	 */
	bool operator==( const FArrangedWidget& Other ) const 
	{
		return Widget == Other.Widget;
	}

public:

	/** The widget's geometry. */
	FGeometry Geometry;

	/** The widget that is being arranged. */
	TSharedRef<SWidget> Widget;
};

struct SLATECORE_API FWidgetAndPointer : public FArrangedWidget
{
	FWidgetAndPointer();

	FWidgetAndPointer( const FArrangedWidget& InWidget, const TSharedPtr<FVirtualPointerPosition>& InPosition );

	TSharedPtr<FVirtualPointerPosition> PointerPosition;
};
