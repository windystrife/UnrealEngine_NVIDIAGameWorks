// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Input/Events.h"
#include "Layout/ArrangedWidget.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"


/* Static initialization
 *****************************************************************************/

const FTouchKeySet FTouchKeySet::StandardSet(EKeys::LeftMouseButton);
const FTouchKeySet FTouchKeySet::EmptySet(EKeys::Invalid);

FGeometry FInputEvent::FindGeometry(const TSharedRef<SWidget>& WidgetToFind) const
{
	return EventPath->FindArrangedWidget(WidgetToFind).Get(FArrangedWidget::NullWidget).Geometry;
}

TSharedRef<SWindow> FInputEvent::GetWindow() const
{
	return EventPath->GetWindow();
}

FText FInputEvent::ToText() const
{
	return NSLOCTEXT("Events", "Unimplemented", "Unimplemented");
}

bool FInputEvent::IsPointerEvent() const
{
	return false;
}

FText FCharacterEvent::ToText() const
{
	return FText::Format( NSLOCTEXT("Events", "Char", "Char({0})"), FText::FromString(FString(1, &Character)) );
}

FText FKeyEvent::ToText() const
{
	return FText::Format( NSLOCTEXT("Events", "Key", "Key({0})"), Key.GetDisplayName() );
}

FText FAnalogInputEvent::ToText() const
{
	return FText::Format(NSLOCTEXT("Events", "AnalogInput", "AnalogInput Key({0})"), GetKey().GetDisplayName());
}

FText FPointerEvent::ToText() const
{
	return FText::Format( NSLOCTEXT("Events", "Pointer", "Pointer({0}, {1})"), EffectingButton.GetDisplayName() );
}

bool FPointerEvent::IsPointerEvent() const
{
	return true;
}
