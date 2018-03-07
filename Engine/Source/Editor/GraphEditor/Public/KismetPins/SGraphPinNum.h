// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "KismetPins/SGraphPinString.h"

class GRAPHEDITOR_API SGraphPinNum : public SGraphPinString
{
public:
	SLATE_BEGIN_ARGS(SGraphPinNum) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

protected:
	// SGraphPinString interface
	virtual void SetTypeInValue(const FText& NewTypeInValue, ETextCommit::Type CommitInfo) override;
	// End of SGraphPinString interface
};
