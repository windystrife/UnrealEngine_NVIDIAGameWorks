// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SNodePanel.h"
#include "SGraphNode.h"

class SToolTip;

class GRAPHEDITOR_API SGraphNodeK2Base : public SGraphNode
{
public:

	//~ Begin SGraphNode Interface
	virtual void UpdateGraphNode() override;

	//~ Begin SNodePanel::SNode Interface
	virtual bool RequiresSecondPassLayout() const override;
	virtual void GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const override;
	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;
	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;
	void PerformSecondPassLayout(const TMap< UObject*, TSharedRef<SNode> >& NodeToWidgetLookup) const override;

protected :
	//~ Begin SGraphNode Interface
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	//~ End SGraphNode Interface

	/** Set up node in 'standard' mode */
	void UpdateStandardNode();
	/** Set up node in 'compact' mode */
	void UpdateCompactNode();

	/** Get title in compact mode */
	FText GetNodeCompactTitle() const;

	/** Retrieves text to tack on to the top of the tooltip (above the standard text) */
	FText GetToolTipHeading() const;

protected:
	static const FLinearColor BreakpointHitColor;
	static const FLinearColor LatentBubbleColor;
	static const FLinearColor TimelineBubbleColor;
	static const FLinearColor PinnedWatchColor;
};
