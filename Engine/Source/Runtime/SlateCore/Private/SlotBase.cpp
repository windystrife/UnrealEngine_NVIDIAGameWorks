// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlotBase.h"
#include "Widgets/SNullWidget.h"

FSlotBase::FSlotBase()
: Widget( SNullWidget::NullWidget )
{

}

FSlotBase::FSlotBase( const TSharedRef<SWidget>& InWidget )
: Widget( InWidget )
{
	
}

const TSharedPtr<SWidget> FSlotBase::DetachWidget()
{
	if (Widget != SNullWidget::NullWidget)
	{
		const TSharedRef<SWidget> MyExWidget = Widget;
		Widget = SNullWidget::NullWidget;	
		return MyExWidget;
	}
	else
	{
		// Nothing to detach!
		return TSharedPtr<SWidget>();
	}
}

FSlotBase::~FSlotBase()
{
}
