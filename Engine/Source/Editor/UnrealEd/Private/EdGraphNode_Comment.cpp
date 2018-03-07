// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EdGraphNode_Comment.h"
#include "Layout/SlateRect.h"

#include "GraphEditorSettings.h"
#include "Kismet2/Kismet2NameValidators.h"

#define LOCTEXT_NAMESPACE "EdGraph"

/////////////////////////////////////////////////////
// UEdGraphNode_Comment

UEdGraphNode_Comment::UEdGraphNode_Comment(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeWidth = 400.0f;
	NodeHeight = 100.0f;
	FontSize = 18;
	CommentColor = GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;
	bColorCommentBubble = false;
	MoveMode = ECommentBoxMode::GroupMovement;

	bCommentBubblePinned = true;
	bCommentBubbleVisible = true;
	bCommentBubbleVisible_InDetailsPanel = true;
	bCanResizeNode = true;
	bCanRenameNode = true;
	CommentDepth = -1;
}

void UEdGraphNode_Comment::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector) 
{
	UEdGraphNode_Comment* This = CastChecked<UEdGraphNode_Comment>(InThis);
	for (auto It = This->NodesUnderComment.CreateIterator(); It; ++It)
	{
		Collector.AddReferencedObject(*It, This);
	}

	Super::AddReferencedObjects(InThis, Collector);
}

void UEdGraphNode_Comment::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	bCommentBubbleVisible = bCommentBubbleVisible_InDetailsPanel;
	bCommentBubblePinned = bCommentBubbleVisible_InDetailsPanel;
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UEdGraphNode_Comment::PostPlacedNewNode()
{
	//@TODO: Consider making the default value we use here a preference
	// This is done here instead of in the constructor so we can later change the default for newly placed
	// instances without changing all of the existing ones (due to delta serialization)
	MoveMode = ECommentBoxMode::GroupMovement;

	NodeComment = NSLOCTEXT("K2Node", "CommentBlock_NewEmptyComment", "Comment").ToString();
}

FText UEdGraphNode_Comment::GetTooltipText() const
{
	if (CachedTooltip.IsOutOfDate(this))
	{
		CachedTooltip.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "CommentBlock_Tooltip", "Comment:\n{0}"), FText::FromString(NodeComment)), this);
	}
	return CachedTooltip;
}

FString UEdGraphNode_Comment::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/Common");
}

FString UEdGraphNode_Comment::GetDocumentationExcerptName() const
{
	return TEXT("UEdGraphNode_Comment");
}

FSlateIcon UEdGraphNode_Comment::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor::White;
	
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Comment_16x");
	return Icon;
}

FText UEdGraphNode_Comment::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if(TitleType == ENodeTitleType::MenuTitle)
	{
		return NSLOCTEXT("K2Node", "NoComment_ListTitle", "Add Comment...");
	}
	else if(TitleType == ENodeTitleType::ListView)
	{
		return NSLOCTEXT("K2Node", "CommentBlock_ListTitle", "Comment");	
	}

	return FText::FromString(NodeComment);
}

FText UEdGraphNode_Comment::GetPinNameOverride(const UEdGraphPin& Pin) const
{
	return GetNodeTitle(ENodeTitleType::ListView);
}

FLinearColor UEdGraphNode_Comment::GetNodeCommentColor() const
{
	// Only affects the 'zoomed out' comment bubble color, not the box itself
	return (bColorCommentBubble)
		? CommentColor 
		: FLinearColor::White;
}

void UEdGraphNode_Comment::ResizeNode(const FVector2D& NewSize)
{
	if (bCanResizeNode) 
	{
		NodeHeight = NewSize.Y;
		NodeWidth = NewSize.X;
	}
}

void UEdGraphNode_Comment::AddNodeUnderComment(UObject* Object)
{
	if( UEdGraphNode_Comment* ChildComment = Cast<UEdGraphNode_Comment>(Object))
	{
		CommentDepth = FMath::Min( CommentDepth, ChildComment->CommentDepth - 1 );
	}
	NodesUnderComment.Add(Object);
}

void UEdGraphNode_Comment::ClearNodesUnderComment()
{
	NodesUnderComment.Empty();
}

void UEdGraphNode_Comment::SetBounds(const class FSlateRect& Rect)
{
	NodePosX = Rect.Left;
	NodePosY = Rect.Top;

	FVector2D Size = Rect.GetSize();
	NodeWidth = Size.X;
	NodeHeight = Size.Y;
}

const FCommentNodeSet& UEdGraphNode_Comment::GetNodesUnderComment() const
{
	return NodesUnderComment;
}

void UEdGraphNode_Comment::OnRenameNode(const FString& NewName)
{
	NodeComment = NewName;
	CachedTooltip.MarkDirty();
}

TSharedPtr<class INameValidatorInterface> UEdGraphNode_Comment::MakeNameValidator() const
{
	// Comments can be duplicated, etc...
	return MakeShareable(new FDummyNameValidator(EValidatorResult::Ok));
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
