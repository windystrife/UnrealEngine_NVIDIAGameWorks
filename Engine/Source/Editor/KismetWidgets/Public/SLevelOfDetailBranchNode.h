// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"

/////////////////////////////////////////////////////
// SLevelOfDetailBranchNode

class KISMETWIDGETS_API SLevelOfDetailBranchNode : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLevelOfDetailBranchNode)
		: _UseLowDetailSlot(false)
		{}

		// Should the low detail or high detail slot be shown?
		SLATE_ATTRIBUTE(bool, UseLowDetailSlot)

		// The low-detail slot
		SLATE_NAMED_SLOT(FArguments, LowDetail)

		// The high-detail slot
		SLATE_NAMED_SLOT(FArguments, HighDetail)
	SLATE_END_ARGS()

protected:
	// What kind of slot was shown last frame
	int LastCachedValue;

	// The attribute indicating the kind of slot to show
	TAttribute<bool> ShowLowDetailAttr;

	// The low-detail child slot
	TSharedRef<SWidget> ChildSlotLowDetail;

	// The high-detail child slot
	TSharedRef<SWidget> ChildSlotHighDetail;

public:
	SLevelOfDetailBranchNode();

	void Construct(const FArguments& InArgs);

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface
};
