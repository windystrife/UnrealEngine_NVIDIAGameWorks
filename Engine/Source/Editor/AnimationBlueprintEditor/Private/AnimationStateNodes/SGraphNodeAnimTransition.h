// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SNodePanel.h"
#include "SGraphNode.h"

class SToolTip;
class UAnimStateTransitionNode;

class SGraphNodeAnimTransition : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeAnimTransition){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UAnimStateTransitionNode* InNode);

	// SNodePanel::SNode interface
	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter) override;
	virtual bool RequiresSecondPassLayout() const override;
	virtual void PerformSecondPassLayout(const TMap< UObject*, TSharedRef<SNode> >& NodeToWidgetLookup) const override;
	// End of SNodePanel::SNode interface

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	// End of SGraphNode interface

	// SWidget interface
	void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	// End of SWidget interface

	// Calculate position for multiple nodes to be placed between a start and end point, by providing this nodes index and max expected nodes 
	void PositionBetweenTwoNodesWithOffset(const FGeometry& StartGeom, const FGeometry& EndGeom, int32 NodeIndex, int32 MaxNodes) const;

	static FLinearColor StaticGetTransitionColor(UAnimStateTransitionNode* TransNode, bool bIsHovered);
private:
	TSharedPtr<STextEntryPopup> TextEntryWidget;

private:
	FText GetPreviewCornerText(bool reverse) const;
	FSlateColor GetTransitionColor() const;

	TSharedRef<SWidget> GenerateInlineDisplayOrEditingWidget(bool bShowGraphPreview);
	TSharedRef<SWidget> GenerateRichTooltip();

	FString GetCurrentDuration() const;
};
