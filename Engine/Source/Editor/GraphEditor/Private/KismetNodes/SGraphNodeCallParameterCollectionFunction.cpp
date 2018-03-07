// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "KismetNodes/SGraphNodeCallParameterCollectionFunction.h"
#include "Materials/MaterialParameterCollection.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallMaterialParameterCollectionFunction.h"
#include "NodeFactory.h"
#include "SGraphPinNameList.h"

//////////////////////////////////////////////////////////////////////////
// SGraphNodeCallParameterCollectionFunction

TSharedPtr<SGraphPin> SGraphNodeCallParameterCollectionFunction::CreatePinWidget(UEdGraphPin* Pin) const
{
	UK2Node_CallMaterialParameterCollectionFunction* CallFunctionNode = Cast<UK2Node_CallMaterialParameterCollectionFunction>(GraphNode);

	// Create a special pin class for the ParameterName pin
	if (CallFunctionNode
		&& Pin->PinName == TEXT("ParameterName") 
		&& Pin->PinType.PinCategory == GetDefault<UEdGraphSchema_K2>()->PC_Name)
	{
		TArray<FName> NameList;

		UEdGraphPin* CollectionPin = GraphNode->FindPin(TEXT("Collection"));

		if (CollectionPin)
		{
			UMaterialParameterCollection* Collection = Cast<UMaterialParameterCollection>(CollectionPin->DefaultObject);

			if (Collection)
			{
				// Populate the ParameterName pin combobox with valid options from the Collection
				const bool bVectorParameters = CallFunctionNode->FunctionReference.GetMemberName().ToString().Contains(TEXT("Vector"));
				Collection->GetParameterNames(NameList, bVectorParameters);
			}
		}

		TArray<TSharedPtr<FName>> NamePtrList;

		for (FName NameItem : NameList)
		{
			NamePtrList.Add(MakeShareable( new FName(NameItem)));
		}

		TSharedPtr<SGraphPin> NewPin = SNew(SGraphPinNameList, Pin, NamePtrList);
		return NewPin;
	}
	else
	{
		return FNodeFactory::CreatePinWidget(Pin);
	}
}

