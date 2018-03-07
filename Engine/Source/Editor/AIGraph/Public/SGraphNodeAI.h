// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Styling/SlateColor.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphNode.h"
#include "SGraphPin.h"
#include "Editor/GraphEditor/Private/DragNode.h"

class SGraphPanel;
class SToolTip;
class UAIGraphNode;

class FDragAIGraphNode : public FDragNode
{
public:
	DRAG_DROP_OPERATOR_TYPE(FDragAIGraphNode, FDragNode)

	static TSharedRef<FDragAIGraphNode> New(const TSharedRef<SGraphPanel>& InGraphPanel, const TSharedRef<SGraphNode>& InDraggedNode);
	static TSharedRef<FDragAIGraphNode> New(const TSharedRef<SGraphPanel>& InGraphPanel, const TArray< TSharedRef<SGraphNode> >& InDraggedNodes);

	UAIGraphNode* GetDropTargetNode() const;

	double StartTime;

protected:
	typedef FDragNode Super;
};

class AIGRAPH_API SGraphNodeAI : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeAI){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UAIGraphNode* InNode);

	//~ Begin SGraphNode Interface
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnMouseMove(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent) override;
	virtual void SetOwner(const TSharedRef<SGraphPanel>& OwnerPanel) override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	//~ End SGraphNode Interface

	/** handle mouse down on the node */
	FReply OnMouseDown(const FGeometry& SenderGeometry, const FPointerEvent& MouseEvent);

	/** adds subnode widget inside current node */
	virtual void AddSubNode(TSharedPtr<SGraphNode> SubNodeWidget);

	/** gets decorator or service node if one is found under mouse cursor */
	TSharedPtr<SGraphNode> GetSubNodeUnderCursor(const FGeometry& WidgetGeometry, const FPointerEvent& MouseEvent);

	/** gets drag over marker visibility */
	EVisibility GetDragOverMarkerVisibility() const;

	/** sets drag marker visible or collapsed on this node */
	void SetDragMarker(bool bEnabled);

protected:
	TArray< TSharedPtr<SGraphNode> > SubNodes;

	uint32 bDragMarkerVisible : 1;

	virtual FText GetDescription() const;
	virtual EVisibility GetDescriptionVisibility() const;

	virtual FText GetPreviewCornerText() const;
	virtual const FSlateBrush* GetNameIcon() const;
};

class AIGRAPH_API SGraphPinAI : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinAI){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);
protected:
	//~ Begin SGraphPin Interface
	virtual FSlateColor GetPinColor() const override;
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	//~ End SGraphPin Interface

	const FSlateBrush* GetPinBorder() const;
};
