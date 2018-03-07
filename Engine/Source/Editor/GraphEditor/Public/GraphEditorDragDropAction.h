// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphPin.h"
#include "Styling/SlateColor.h"
#include "Input/DragAndDrop.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "EdGraph/EdGraphSchema.h"

class SGraphNode;
class SGraphPanel;
class SWidget;
class UEdGraph;
struct FSlateBrush;
class UEdGraphPin;
class UEdGraphNode;
class UEdGraphSchema;

// Base class for drag-drop actions that pass into the graph editor and perform an action when dropped
class GRAPHEDITOR_API FGraphEditorDragDropAction : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FGraphEditorDragDropAction, FDragDropOperation)

	void SetHoveredPin(UEdGraphPin* InPin);
	void SetHoveredNode(const TSharedPtr<SGraphNode>& InNode);
	void SetHoveredNode(UEdGraphNode* InNode);
	void SetHoveredGraph(const TSharedPtr<SGraphPanel>& InGraph);
	void SetHoveredCategoryName(const FText& InHoverCategoryName);
	void SetHoveredAction(TSharedPtr<struct FEdGraphSchemaAction> Action);
	void SetDropTargetValid( bool bValid ) { bDropTargetValid = bValid; }

	// Interface to override
	virtual void HoverTargetChanged() {}
	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) { return FReply::Unhandled(); }
	virtual FReply DroppedOnNode(FVector2D ScreenPosition, FVector2D GraphPosition) { return FReply::Unhandled(); }
	virtual FReply DroppedOnPanel( const TSharedRef< SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) { return FReply::Unhandled(); }
	virtual FReply DroppedOnAction(TSharedRef<struct FEdGraphSchemaAction> Action) { return FReply::Unhandled(); }
	virtual FReply DroppedOnCategory(FText Category) { return FReply::Unhandled(); }
	// End of interface to override
	
	virtual bool IsSupportedBySchema(const UEdGraphSchema* Schema) const { return true; }

	bool HasFeedbackMessage();
	void SetFeedbackMessage(const TSharedPtr<SWidget>& Message);
	void SetSimpleFeedbackMessage(const FSlateBrush* Icon, const FSlateColor& IconColor, const FText& Message, const FSlateBrush* SecondaryIcon = nullptr, const FSlateColor SecondaryColor = FSlateColor());

protected:
	UEdGraphPin* GetHoveredPin() const;
	UEdGraphNode* GetHoveredNode() const;
	UEdGraph* GetHoveredGraph() const;

	/** Constructs the window and widget if applicable */
	virtual void Construct() override;

private:

	EVisibility GetIconVisible() const;
	EVisibility GetErrorIconVisible() const;

	// The pin that the drag action is currently hovering over
	FEdGraphPinReference HoveredPin;

	// The node that the drag action is currently hovering over
	TWeakObjectPtr<UEdGraphNode> HoveredNode;

	// The graph that the drag action is currently hovering over
	TSharedPtr<SGraphPanel> HoveredGraph;

protected:

	// Name of category we are hovering over
	FText HoveredCategoryName;

	// Action we are hovering over
	TWeakPtr<struct FEdGraphSchemaAction> HoveredAction;

	// drop target status
	bool bDropTargetValid;
};

// Drag-drop action where an FEdGraphSchemaAction should be performed when dropped
class GRAPHEDITOR_API FGraphSchemaActionDragDropAction : public FGraphEditorDragDropAction
{
public:
	
	DRAG_DROP_OPERATOR_TYPE(FGraphSchemaActionDragDropAction, FGraphEditorDragDropAction)

	// FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnPanel(const TSharedRef< SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	// End of FGraphEditorDragDropAction

	static TSharedRef<FGraphSchemaActionDragDropAction> New(TSharedPtr<FEdGraphSchemaAction> InActionNode )
	{
		TSharedRef<FGraphSchemaActionDragDropAction> Operation = MakeShareable(new FGraphSchemaActionDragDropAction);
		Operation->SourceAction = InActionNode;
		Operation->Construct();
		return Operation;
	}

protected:
	virtual void GetDefaultStatusSymbol(const FSlateBrush*& PrimaryBrushOut, FSlateColor& IconColorOut, FSlateBrush const*& SecondaryBrushOut, FSlateColor& SecondaryColorOut) const;

	/** */
	TSharedPtr<FEdGraphSchemaAction> SourceAction;
};
