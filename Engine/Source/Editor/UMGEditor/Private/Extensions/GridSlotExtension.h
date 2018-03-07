// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "WidgetReference.h"
#include "DesignerExtension.h"

class FGridSlotExtension : public FDesignerExtension
{
public:
	FGridSlotExtension();

	virtual ~FGridSlotExtension() {}

	virtual bool CanExtendSelection(const TArray< FWidgetReference >& Selection) const override;
	
	virtual void ExtendSelection(const TArray< FWidgetReference >& Selection, TArray< TSharedRef<FDesignerSurfaceElement> >& SurfaceElements) override;

private:

	FReply HandleShiftRow(int32 ShiftAmount);
	FReply HandleShiftColumn(int32 ShiftAmount);

	void ShiftRow(UWidget* Widget, int32 ShiftAmount);
	void ShiftColumn(UWidget* Widget, int32 ShiftAmount);
};
