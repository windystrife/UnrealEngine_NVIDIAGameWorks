// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "KismetPins/SGraphPinIndex.h"
#include "EdGraphSchema_K2.h"
#include "SPinTypeSelector.h"

void SGraphPinIndex::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinIndex::GetDefaultValueWidget()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	return SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(Schema, &UEdGraphSchema_K2::GetVariableTypeTree))
		.TargetPinType(this, &SGraphPinIndex::OnGetPinType)
		.OnPinTypeChanged(this, &SGraphPinIndex::OnTypeChanged)
		.Schema(Schema)
		.TypeTreeFilter(ETypeTreeFilter::IndexTypesOnly)
		.IsEnabled(true)
		.bAllowArrays(false);
}

FEdGraphPinType SGraphPinIndex::OnGetPinType() const
{
	return GraphPinObj->PinType;
}

void SGraphPinIndex::OnTypeChanged(const FEdGraphPinType& PinType)
{
	if (GraphPinObj)
	{
		GraphPinObj->Modify();
		GraphPinObj->PinType = PinType;
		// Let the node know that one of its' pins had their pin type changed
		if (GraphPinObj->GetOwningNode())
		{
			GraphPinObj->GetOwningNode()->PinTypeChanged(GraphPinObj);
		}
	}
}
