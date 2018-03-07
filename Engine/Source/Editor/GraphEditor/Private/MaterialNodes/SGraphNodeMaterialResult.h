// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphNode.h"

class FGraphNodeMetaData;
class UMaterialGraphNode_Root;

class SGraphNodeMaterialResult : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeMaterialResult){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UMaterialGraphNode_Root* InNode);

	// SGraphNode interface
	virtual void CreatePinWidgets() override;
	virtual void PopulateMetaTag(FGraphNodeMetaData* TagMeta) const override;
	// End of SGraphNode interface

	// SNodePanel::SNode interface
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter) override;
	// End of SNodePanel::SNode interface

private:
	UMaterialGraphNode_Root* RootNode;
};
