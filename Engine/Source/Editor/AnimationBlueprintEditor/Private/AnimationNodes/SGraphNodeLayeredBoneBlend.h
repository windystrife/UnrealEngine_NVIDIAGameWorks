// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "AnimationNodes/SAnimationGraphNode.h"

class SVerticalBox;
class UAnimGraphNode_LayeredBoneBlend;

class SGraphNodeLayeredBoneBlend : public SAnimationGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeLayeredBoneBlend){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UAnimGraphNode_LayeredBoneBlend* InNode);

protected:
	// SGraphNode interface
	virtual void CreateInputSideAddButton(TSharedPtr<SVerticalBox> InputBox) override;
	virtual FReply OnAddPin() override;
	// End of SGraphNode interface

private:

	// The node that we represent
	UAnimGraphNode_LayeredBoneBlend* Node;

};
