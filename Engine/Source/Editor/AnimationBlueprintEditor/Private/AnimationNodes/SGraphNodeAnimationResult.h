// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "AnimationNodes/SAnimationGraphNode.h"

class SGraphNodeAnimationResult : public SAnimationGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeAnimationResult){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UAnimGraphNode_Base* InNode);

protected:
	// SGraphNode interface
	virtual TSharedRef<SWidget> CreateNodeContentArea() override;
	// End of SGraphNode interface
};
