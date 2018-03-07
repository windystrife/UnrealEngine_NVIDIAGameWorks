// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "KismetNodes/SGraphNodeK2Composite.h"

class UEdGraph;

class SGraphNodeStateMachineInstance : public SGraphNodeK2Composite
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeStateMachineInstance){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class UAnimGraphNode_StateMachineBase* InNode);
protected:
	// SGraphNodeK2Composite interface
	virtual UEdGraph* GetInnerGraph() const override;
	// End of SGraphNodeK2Composite interface
};
