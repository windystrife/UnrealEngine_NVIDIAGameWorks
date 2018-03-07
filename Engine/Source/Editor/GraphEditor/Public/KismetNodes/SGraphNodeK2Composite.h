// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "KismetNodes/SGraphNodeK2Base.h"

class SToolTip;
class UEdGraph;
class UK2Node_Composite;

class GRAPHEDITOR_API SGraphNodeK2Composite : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2Composite){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node_Composite* InNode);

	// SGraphNode interface
	 virtual void UpdateGraphNode() override;
	virtual TSharedPtr<SToolTip> GetComplexTooltip() override;
	// End of SGraphNode interface
protected:
	virtual UEdGraph* GetInnerGraph() const;

private:
	FText GetPreviewCornerText() const;
	FText GetTooltipTextForNode() const;

	TSharedRef<SWidget> CreateNodeBody();
};
