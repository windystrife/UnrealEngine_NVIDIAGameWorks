// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "BlueprintGraphPanelPinFactory.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_GetDataTableRow.h"
#include "SGraphPin.h"
#include "SGraphPinNameList.h"
#include "SGraphPinDataTableRowName.h"
	
TSharedPtr<class SGraphPin> FBlueprintGraphPanelPinFactory::CreatePin(class UEdGraphPin* InPin) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (InPin->PinType.PinCategory == K2Schema->PC_Name)
	{
		UObject* Outer = InPin->GetOuter();

		// Create drop down combo boxes for DataTable and CurveTable RowName pins
		const UEdGraphPin* DataTablePin = nullptr;
		if (Outer->IsA(UK2Node_CallFunction::StaticClass()))
		{
			const UK2Node_CallFunction* CallFunctionNode = CastChecked<UK2Node_CallFunction>(Outer);
			if (CallFunctionNode)
			{
				const UFunction* FunctionToCall = CallFunctionNode->GetTargetFunction();
				if (FunctionToCall)
				{
					const FString& DataTablePinName = FunctionToCall->GetMetaData(FBlueprintMetadata::MD_DataTablePin);
					DataTablePin = CallFunctionNode->FindPin(DataTablePinName);
				}
			}
		}
		else if (Outer->IsA(UK2Node_GetDataTableRow::StaticClass()))
		{
			const UK2Node_GetDataTableRow* GetDataTableRowNode = CastChecked<UK2Node_GetDataTableRow>(Outer);
			DataTablePin = GetDataTableRowNode->GetDataTablePin();
		}

		if (DataTablePin)
		{
			if (DataTablePin->DefaultObject != nullptr && DataTablePin->LinkedTo.Num() == 0)
			{
				if (auto DataTable = Cast<UDataTable>(DataTablePin->DefaultObject))
				{
					return SNew(SGraphPinDataTableRowName, InPin, DataTable);
				}
				if (DataTablePin->DefaultObject->IsA(UCurveTable::StaticClass()))
				{
					UCurveTable* CurveTable = (UCurveTable*)DataTablePin->DefaultObject;
					if (CurveTable)
					{
						TArray<TSharedPtr<FName>> RowNames;
						/** Extract all the row names from the RowMap */
						for (TMap<FName, FRichCurve*>::TConstIterator Iterator(CurveTable->RowMap); Iterator; ++Iterator)
						{
							/** Create a simple array of the row names */
							TSharedPtr<FName> RowNameItem = MakeShareable(new FName(Iterator.Key()));
							RowNames.Add(RowNameItem);
						}
						return SNew(SGraphPinNameList, InPin, RowNames);
					}
				}
			}
		}
	}

	return nullptr;
}
