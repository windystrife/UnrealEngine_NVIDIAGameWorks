// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphNode.h"

class UBehaviorTreeDecoratorGraphNode_Decorator;

class SGraphNode_Decorator : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_Decorator){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UBehaviorTreeDecoratorGraphNode_Decorator* InNode);

	virtual FString GetNodeComment() const override;
};
