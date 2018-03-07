// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "MaterialPins/SGraphPinMaterialInput.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"

void SGraphPinMaterialInput::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments().UsePinColorForText(true), InGraphPinObj);
}

FSlateColor SGraphPinMaterialInput::GetPinColor() const
{
	check(GraphPinObj);
	UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GraphPinObj->GetOwningNode()->GetGraph());
	const UMaterialGraphSchema* Schema = CastChecked<UMaterialGraphSchema>(MaterialGraph->GetSchema());

	if (MaterialGraph->IsInputActive(GraphPinObj))
	{
		return Schema->ActivePinColor;
	}
	else
	{
		return Schema->InactivePinColor;
	}
}
