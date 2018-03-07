// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "SGraphNode.h"

class SGraphNodeSoundResult : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeSoundResult){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class USoundCueGraphNode_Base* InNode);

protected:
	// SGraphNode interface
	virtual TSharedRef<SWidget> CreateNodeContentArea() override;
	// End of SGraphNode interface
};
