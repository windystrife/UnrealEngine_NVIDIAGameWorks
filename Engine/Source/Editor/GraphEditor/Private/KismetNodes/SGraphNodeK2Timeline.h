// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SNodePanel.h"
#include "KismetNodes/SGraphNodeK2Default.h"

class UK2Node_Timeline;

class SGraphNodeK2Timeline : public SGraphNodeK2Default
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2Timeline){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node_Timeline* InNode);

	// SNodePanel::SNode interface
	void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;
	// End of SNodePanel::SNode interface
};
