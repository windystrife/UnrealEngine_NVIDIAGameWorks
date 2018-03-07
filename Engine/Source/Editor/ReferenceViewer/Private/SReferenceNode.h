// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphNode.h"

class FAssetThumbnail;
class UEdGraphNode_Reference;

/**
 * 
 */
class SReferenceNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS( SReferenceNode ){}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs, UEdGraphNode_Reference* InNode );

	// SGraphNode implementation
	virtual void UpdateGraphNode() override;
	virtual bool IsNodeEditable() const override { return false; }
	// End SGraphNode implementation

private:
	TSharedPtr<class FAssetThumbnail> AssetThumbnail;
};
