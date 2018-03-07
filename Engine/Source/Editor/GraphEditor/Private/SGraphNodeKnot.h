// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphPin.h"
#include "SGraphNodeDefault.h"

class SCommentBubble;

/** The visual representation of a control point meant to adjust how connections are routed, also known as a Reroute node.
 * The input knot node should have properly implemented ShouldDrawNodeAsControlPointOnly to return true with valid indices for its pins.
 */
class SGraphNodeKnot : public SGraphNodeDefault
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeKnot) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UEdGraphNode* InKnot);

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual void RequestRenameOnSpawn() override { }
	// End of SGraphNode interface

	// SWidget interface
	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;
	// End of SWidget interface

protected:
	/** Returns Offset to center comment on the node's only pin */
	FVector2D GetCommentOffset() const;
protected:

	/** Toggles the hovered visibility state */
	virtual void OnCommentBubbleToggled(bool bInCommentBubbleVisible) override;

	/** If bHoveredCommentVisibility is true, hides the comment bubble after a change is committed */
	virtual void OnCommentTextCommitted(const FText& NewComment, ETextCommit::Type CommitInfo) override;

	/** The hovered visibility state. If false, comment bubble will only appear on hover. */
	bool bAlwaysShowCommentBubble;

	/** SharedPtr to comment bubble */
	TSharedPtr<SCommentBubble> CommentBubble;

	const FSlateBrush* ShadowBrush;
	const FSlateBrush* ShadowBrushSelected;
};
