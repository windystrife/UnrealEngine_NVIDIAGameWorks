// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_CallMaterialParameterCollectionFunction.h"
#include "EdGraph/EdGraph.h"


UK2Node_CallMaterialParameterCollectionFunction::UK2Node_CallMaterialParameterCollectionFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_CallMaterialParameterCollectionFunction::PinDefaultValueChanged(UEdGraphPin* Pin) 
{
	Super::PinDefaultValueChanged(Pin);

	if (Pin->PinName == TEXT("Collection") )
	{
		// When the Collection pin gets a new value assigned, we need to update the Slate UI so that SGraphNodeCallParameterCollectionFunction will update the ParameterName drop down
		GetGraph()->NotifyGraphChanged();
	}
}

