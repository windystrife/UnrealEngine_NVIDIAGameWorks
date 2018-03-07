// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MessageFilter.h"
#include "Styling/SlateTypes.h"

FReply FMessageFilter::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	RefreshCallback.Broadcast();
	return FReply::Handled();
}

ECheckBoxState FMessageFilter::OnGetDisplayCheckState() const
{
	return Display ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FMessageFilter::OnDisplayCheckStateChanged(ECheckBoxState InNewState)
{
	Display = InNewState == ECheckBoxState::Checked;
	RefreshCallback.Broadcast();
}
