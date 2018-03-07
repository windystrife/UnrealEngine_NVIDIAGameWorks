// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "KismetNodes/SGraphNodeK2Base.h"

class UK2Node;

class GRAPHEDITOR_API SGraphNodeK2Var : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2Var){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, UK2Node* InNode );

	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	// End of SGraphNode interface

protected:
	FSlateColor GetVariableColor() const;
};
