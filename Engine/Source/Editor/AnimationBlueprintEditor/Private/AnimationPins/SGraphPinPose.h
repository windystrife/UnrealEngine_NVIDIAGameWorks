// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SGraphPin.h"

class SGraphPinPose : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinPose)	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:
	//~ Begin SGraphPin Interface
	virtual const FSlateBrush* GetPinIcon() const override;
	//~ End SGraphPin Interface

	mutable const FSlateBrush* CachedImg_Pin_ConnectedHovered;
	mutable const FSlateBrush* CachedImg_Pin_DisconnectedHovered;
};
