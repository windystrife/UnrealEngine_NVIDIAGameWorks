// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "KismetNodes/SGraphNodeK2Event.h"
#include "SGraphPin.h"
#include "K2Node_Event.h"

void SGraphNodeK2Event::AddPin( const TSharedRef<SGraphPin>& PinToAdd ) 
{
	const UEdGraphPin* PinObj = PinToAdd->GetPinObj();
	const bool bDelegateOutput = (PinObj != nullptr) && (UK2Node_Event::DelegateOutputName == PinObj->PinName);

	if (bDelegateOutput && TitleAreaWidget.IsValid())
	{
		PinToAdd->SetOwner(SharedThis(this));

		bHasDelegateOutputPin = true;
		PinToAdd->SetShowLabel(false);
		TitleAreaWidget->AddSlot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(FMargin(4))
		[
			PinToAdd
		];

		OutputPins.Add(PinToAdd);
	}
	else
	{
		SGraphNodeK2Default::AddPin(PinToAdd);
	}
}

bool SGraphNodeK2Event::UseLowDetailNodeTitles() const
{
	return (!bHasDelegateOutputPin) && ParentUseLowDetailNodeTitles();
}

EVisibility SGraphNodeK2Event::GetTitleVisibility() const
{
	return ParentUseLowDetailNodeTitles() ? EVisibility::Hidden : EVisibility::Visible;
}

TSharedRef<SWidget> SGraphNodeK2Event::CreateTitleWidget(TSharedPtr<SNodeTitle> NodeTitle)
{
	auto WidgetRef = SGraphNodeK2Default::CreateTitleWidget(NodeTitle);
	auto VisibilityAttribute = TAttribute<EVisibility>::Create(
		TAttribute<EVisibility>::FGetter::CreateSP(this, &SGraphNodeK2Event::GetTitleVisibility));
	WidgetRef->SetVisibility(VisibilityAttribute);
	if (NodeTitle.IsValid())
	{
		NodeTitle->SetVisibility(VisibilityAttribute);
	}

	return WidgetRef;
}
