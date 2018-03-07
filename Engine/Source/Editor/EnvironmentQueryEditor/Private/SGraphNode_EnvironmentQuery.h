// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphNodeAI.h"

class SVerticalBox;
class UEnvironmentQueryGraphNode;

class SGraphNode_EnvironmentQuery : public SGraphNodeAI
{
public:
	SLATE_BEGIN_ARGS(SGraphNode_EnvironmentQuery){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEnvironmentQueryGraphNode* InNode);

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual void CreatePinWidgets() override;
	// End of SGraphNode interface

	/** adds decorator widget inside current node */
	virtual void AddSubNode(TSharedPtr<SGraphNode> SubNodeWidget) override;

	EVisibility GetWeightMarkerVisibility() const;
	TOptional<float> GetWeightProgressBarPercent() const;
	FSlateColor GetWeightProgressBarColor() const; 

	EVisibility GetTestToggleVisibility() const;
	ECheckBoxState IsTestToggleChecked() const;
	void OnTestToggleChanged(ECheckBoxState NewState);

	FSlateColor GetProfilerTestSlateColor() const;
	EVisibility GetProfilerTestVisibility() const;
	EVisibility GetProfilerOptionVisibility() const;
	FText GetProfilerDescAverage() const;
	FText GetProfilerDescWorst() const;
	FText GetProfilerDescOption() const;

protected:
	TSharedPtr<SVerticalBox> TestBox;

	FSlateColor GetBorderBackgroundColor() const;
	FSlateColor GetBackgroundColor() const;
};
