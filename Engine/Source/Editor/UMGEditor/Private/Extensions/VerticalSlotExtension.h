// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "WidgetReference.h"
#include "DesignerExtension.h"

class FVerticalSlotExtension : public FDesignerExtension
{
public:
	FVerticalSlotExtension();

	virtual ~FVerticalSlotExtension() {}

	virtual bool CanExtendSelection(const TArray< FWidgetReference >& Selection) const override;
	
	virtual void ExtendSelection(const TArray< FWidgetReference >& Selection, TArray< TSharedRef<FDesignerSurfaceElement> >& SurfaceElements) override;

private:

	bool CanShift(int32 ShiftAmount) const;

	FReply HandleShiftVertical(int32 ShiftAmount);

	void ShiftVertical(UWidget* Widget, int32 ShiftAmount);
};
