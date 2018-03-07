// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Layout/ArrangedWidget.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWidget.h"


FArrangedWidget FArrangedWidget::NullWidget(SNullWidget::NullWidget, FGeometry());

FString FArrangedWidget::ToString( ) const
{
	return FString::Printf(TEXT("%s @ %s"), *Widget->ToString(), *Geometry.ToString());
}


FWidgetAndPointer::FWidgetAndPointer()
: FArrangedWidget(FArrangedWidget::NullWidget)
, PointerPosition(TSharedPtr<FVirtualPointerPosition>())
{}

FWidgetAndPointer::FWidgetAndPointer( const FArrangedWidget& InWidget, const TSharedPtr<FVirtualPointerPosition>& InPosition )
: FArrangedWidget(InWidget)
, PointerPosition(InPosition)
{}
