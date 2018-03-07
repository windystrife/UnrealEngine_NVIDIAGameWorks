// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "SGraphNode.h"

class SVerticalBox;
class USoundCueGraphNode;

class SGraphNodeSoundBase : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeSoundBase){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, class USoundCueGraphNode* InNode);

protected:
	// SGraphNode Interface
	virtual void CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox) override;
	virtual EVisibility IsAddPinButtonVisible() const override;
	virtual FReply OnAddPin() override;

private:
	USoundCueGraphNode* SoundNode;
};
