// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"

class IScrollableWidget;

/**
 * Shows a border above and below a scrollable area.
 */
class SLATE_API SScrollBorder : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SScrollBorder)
		: _Style( &FCoreStyle::Get().GetWidgetStyle<FScrollBorderStyle>("ScrollBorder") )
		, _BorderFadeDistance( FVector2D(0.01f, 0.01f) )
		{}

		/** Style used to draw this scroll border */
		SLATE_STYLE_ARGUMENT( FScrollBorderStyle, Style )

		/** The distance in normalized coordinate to begin fading the scroll border. */
		SLATE_ATTRIBUTE( FVector2D, BorderFadeDistance )

		/** Arbitrary content to be displayed in the overlay under the shadow layers; if not specified, the associated ScrollableWidget will be used instead. */
		SLATE_DEFAULT_SLOT( FArguments, Content )
	SLATE_END_ARGS()

	virtual ~SScrollBorder();

	/**
	  * Constructs a scrollable border overlay for the specified ScrollableWidget
	  */
	void Construct(const FArguments& InArgs, TSharedRef<IScrollableWidget> InScrollableWidget);
	
protected:

	/** Gets the top border opacity. */
	FSlateColor GetTopBorderOpacity() const;

	/** Gets the bottom border opacity. */
	FSlateColor GetBottomBorderOpacity() const;

	/** The border fade distance in normalized coordinates. */
	TAttribute<FVector2D> BorderFadeDistance;

	/** The widget that to watch for when the scrolling changes to show the border. */
	TAttribute< TWeakPtr< IScrollableWidget > > ScrollableWidget;
};
