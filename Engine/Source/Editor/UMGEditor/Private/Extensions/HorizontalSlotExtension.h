// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "WidgetReference.h"
#include "DesignerExtension.h"

class FHorizontalSlotExtension : public FDesignerExtension
{
public:
	FHorizontalSlotExtension();

	virtual ~FHorizontalSlotExtension() {}

	virtual bool CanExtendSelection(const TArray< FWidgetReference >& Selection) const override;
	
	virtual void ExtendSelection(const TArray< FWidgetReference >& Selection, TArray< TSharedRef<FDesignerSurfaceElement> >& SurfaceElements) override;

private:

	bool CanShift(int32 ShiftAmount) const;

	FReply HandleShift(int32 ShiftAmount);
	
	void ShiftHorizontal(UWidget* Widget, int32 ShiftAmount);
};
