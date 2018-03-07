// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_StructOperation.h"
#include "Engine/UserDefinedStruct.h"
#include "EdGraphSchema_K2.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "Kismet2/StructureEditorUtils.h"
#include "BlueprintActionFilter.h"

//////////////////////////////////////////////////////////////////////////
// UK2Node_StructOperation

UK2Node_StructOperation::UK2Node_StructOperation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_StructOperation::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	// Skip UK2Node_Variable's validation because it doesn't need a property (see CL# 1756451)
	Super::Super::ValidateNodeDuringCompilation(MessageLog);
}

bool UK2Node_StructOperation::HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const
{
	const bool bResult = nullptr != StructType;
	if (bResult && OptionalOutput)
	{
		OptionalOutput->AddUnique(StructType);
	}

	const bool bSuperResult = Super::HasExternalDependencies(OptionalOutput);
	return bSuperResult || bResult;
}

void UK2Node_StructOperation::FStructOperationOptionalPinManager::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex, UProperty* Property) const
{
	FOptionalPinManager::CustomizePinData(Pin, SourcePropertyName, ArrayIndex, Property);

	if (Pin && Property)
	{
		const UUserDefinedStruct* UDStructure = Cast<const UUserDefinedStruct>(Property->GetOwnerStruct());
		if (UDStructure)
		{
			const FStructVariableDescription* VarDesc = FStructureEditorUtils::GetVarDesc(UDStructure).FindByPredicate(
				FStructureEditorUtils::FFindByNameHelper<FStructVariableDescription>(Property->GetFName()));
			if (VarDesc)
			{
				Pin->PersistentGuid = VarDesc->VarGuid;
			}
		}
	}
}

bool UK2Node_StructOperation::DoRenamedPinsMatch(const UEdGraphPin* NewPin, const UEdGraphPin* OldPin, bool bStructInVaraiablesOut)
{
	bool bResult = false;
	if (NewPin && OldPin && (OldPin->Direction == NewPin->Direction))
	{
		const EEdGraphPinDirection StructDirection = bStructInVaraiablesOut ? EGPD_Input : EGPD_Output;
		const EEdGraphPinDirection VariablesDirection = bStructInVaraiablesOut ? EGPD_Output : EGPD_Input;
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>(); 
		const bool bCompatible = K2Schema && K2Schema->ArePinTypesCompatible(NewPin->PinType, OldPin->PinType);

		if (bCompatible && (StructDirection == OldPin->Direction))
		{
			// Struct name was changed
			bResult = true;
		}
		else if (bCompatible && (VariablesDirection == OldPin->Direction))
		{
			// name of a member variable was changed
			if ((NewPin->PersistentGuid == OldPin->PersistentGuid) && OldPin->PersistentGuid.IsValid())
			{
				bResult = true;
			}
		}
	}
	return bResult;
}

FString UK2Node_StructOperation::GetPinMetaData(FString InPinName, FName InKey)
{
	FString ReturnValue;

	for (TFieldIterator<UProperty> It(StructType); It; ++It)
	{
		const UProperty* Property = *It;
		if(Property && Property->GetName() == InPinName)
		{
			ReturnValue = Property->GetMetaData(InKey);
			break;
		}
	}
	return ReturnValue;
}

FString UK2Node_StructOperation::GetFindReferenceSearchString() const
{
	return UEdGraphNode::GetFindReferenceSearchString();
}

bool UK2Node_StructOperation::IsActionFilteredOut(const FBlueprintActionFilter& Filter)
{
	bool bIsFiltered = false;
	if (StructType)
	{
		if (StructType->GetBoolMetaData(FBlueprintMetadata::MD_BlueprintInternalUseOnly))
		{
			bIsFiltered = true;
		}
		else if (!StructType->GetBoolMetaData(FBlueprintMetadata::MD_AllowableBlueprintVariableType))
		{
			bIsFiltered = true;
			for (UEdGraphPin* ContextPin : Filter.Context.Pins)
			{
				if (ContextPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && ContextPin->PinType.PinSubCategoryObject == StructType)
				{
					bIsFiltered = false;
					break;
				}
			}
		}
	}
	return bIsFiltered;
}
