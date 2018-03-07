// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FPaintArgs;
class FSlateWindowElementList;
class SSequencerTreeView;

/**
 * A wrapper widget responsible for positioning a track lane within the section area
 */
class SSequencerTrackLane : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSequencerTrackLane){}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<FSequencerDisplayNode>& InDisplayNode, const TSharedRef<SSequencerTreeView>& InTreeView);
	
	/** Paint this widget */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Get the desired physical vertical position of this track lane */
	float GetPhysicalPosition() const;

protected:
	virtual FVector2D ComputeDesiredSize(float LayoutScale) const override;

private:
	/** The authoritative display node that created us */
	TSharedPtr<FSequencerDisplayNode> DisplayNode;
	/** Pointer back to the tree view for virtual <-> physical space conversions. Important: weak ptr to avoid circular references */
	TWeakPtr<SSequencerTreeView> TreeView;
	/** Our desired size last frame */
	TOptional<FVector2D> LastDesiredSize;
};
