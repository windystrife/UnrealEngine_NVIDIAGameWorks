// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Styling/SlateColor.h"
#include "SNodePanel.h"
#include "KismetNodes/SGraphNodeK2Base.h"
#include "Engine/PoseWatch.h"

class UAnimGraphNode_Base;

class SAnimationGraphNode : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SAnimationGraphNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UAnimGraphNode_Base* InNode);

protected:
	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	// End of SWidget interface

	// SGraphNode interface
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const override;
	// End of SGraphNode interface

private:
	// Return Pose View Colour for slate indicator
	FSlateColor GetPoseViewColour() const;

	FReply SpawnColourPicker();

	/** Keep a reference to the indicator widget handing around */
	TSharedPtr<SWidget> IndicatorWidget;

	/** Keep a reference to the pose view indicator widget handing around */
	TSharedPtr<SWidget> PoseViewWidget;

	TWeakObjectPtr<class UPoseWatch> PoseWatch;
};
