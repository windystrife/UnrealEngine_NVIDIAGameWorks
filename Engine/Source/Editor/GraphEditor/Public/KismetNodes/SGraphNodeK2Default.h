// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphNodeK2Base.h"

class UK2Node;

class SGraphNodeK2Default : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2Default){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node* InNode);
};
