// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWidget;

class SLATECORE_API FSlotBase
{
public:

	FSlotBase();

	FSlotBase( const TSharedRef<SWidget>& InWidget );

	virtual ~FSlotBase();

	FORCEINLINE_DEBUGGABLE void AttachWidget( const TSharedRef<SWidget>& InWidget )
	{
		Widget = InWidget;
	}

	/**
	 * Access the widget in the current slot.
	 * There will always be a widget in the slot; sometimes it is
	 * the SNullWidget instance.
	 */
	FORCEINLINE_DEBUGGABLE const TSharedRef<SWidget>& GetWidget() const
	{
		return Widget;
	}

	/**
	 * Remove the widget from its current slot.
	 * The removed widget is returned so that operations could be performed on it.
	 * If the null widget was being stored, an invalid shared ptr is returned instead.
	 */
	const TSharedPtr<SWidget> DetachWidget();

private:
	// non-copyable
	FSlotBase& operator=(const FSlotBase&);
	FSlotBase(const FSlotBase&);

private:
	/**
	 *
	 * Widget IS NOW PRIVATE!
	 *
	 */
	TSharedRef<SWidget> Widget;
};


template<typename SlotType>
class TSlotBase : public FSlotBase
{
public:

	TSlotBase()
	: FSlotBase()
	{}

	TSlotBase( const TSharedRef<SWidget>& InWidget )
	: FSlotBase( InWidget )
	{}

	SlotType& operator[]( const TSharedRef<SWidget>& InChildWidget )
	{
		this->AttachWidget(InChildWidget);
		return (SlotType&)(*this);
	}

	SlotType& Expose( SlotType*& OutVarToInit )
	{
		OutVarToInit = (SlotType*)this;
		return (SlotType&)(*this);
	}
};
