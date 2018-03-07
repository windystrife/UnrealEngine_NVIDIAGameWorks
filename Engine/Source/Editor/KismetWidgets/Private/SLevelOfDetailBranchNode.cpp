// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SLevelOfDetailBranchNode.h"

/////////////////////////////////////////////////////
// SLevelOfDetailBranchNode

SLevelOfDetailBranchNode::SLevelOfDetailBranchNode()
	: LastCachedValue(INDEX_NONE)
	, ChildSlotLowDetail(SNullWidget::NullWidget)
	, ChildSlotHighDetail(SNullWidget::NullWidget)
{
}

void SLevelOfDetailBranchNode::Construct(const FArguments& InArgs)
{
	ShowLowDetailAttr = InArgs._UseLowDetailSlot;
	ChildSlotLowDetail = InArgs._LowDetail.Widget;
	ChildSlotHighDetail = InArgs._HighDetail.Widget;
	ChildSlot[ChildSlotHighDetail];
}

void SLevelOfDetailBranchNode::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	const int32 CurrentValue = ShowLowDetailAttr.Get() ? 1 : 0;
	if (CurrentValue != LastCachedValue)
	{
		LastCachedValue = CurrentValue;
		ChildSlot
		[
			(CurrentValue != 0) ? ChildSlotLowDetail : ChildSlotHighDetail
		];
	}
}
