// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_CallArrayFunction.h"
#include "EdGraphSchema_K2.h"
#include "BlueprintNodeSpawner.h"
#include "K2Node_GetArrayItem.h"
#include "BlueprintsObjectVersion.h"
#include "Kismet/KismetArrayLibrary.h" // for Array_Get()

UK2Node_CallArrayFunction::UK2Node_CallArrayFunction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_CallArrayFunction::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* TargetArrayPin = GetTargetArrayPin();
	if (ensure(TargetArrayPin))
	{
		TargetArrayPin->PinType.ContainerType = EPinContainerType::Array;
		TargetArrayPin->PinType.bIsReference = true;
		TargetArrayPin->PinType.PinCategory = Schema->PC_Wildcard;
		TargetArrayPin->PinType.PinSubCategory.Reset();
		TargetArrayPin->PinType.PinSubCategoryObject = nullptr;
	}

	TArray< FArrayPropertyPinCombo > ArrayPins;
	GetArrayPins(ArrayPins);
	for(auto Iter = ArrayPins.CreateConstIterator(); Iter; ++Iter)
	{
		if(Iter->ArrayPropPin)
		{
			Iter->ArrayPropPin->bHidden = true;
			Iter->ArrayPropPin->bNotConnectable = true;
			Iter->ArrayPropPin->bDefaultValueIsReadOnly = true;
		}
	}

	PropagateArrayTypeInfo(TargetArrayPin);
}

void UK2Node_CallArrayFunction::PostReconstructNode()
{
	// cannot use a ranged for here, as PinConnectionListChanged() might end up 
	// collapsing split pins (subtracting elements from Pins)
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = Pins[PinIndex];
		if (Pin->LinkedTo.Num() > 0)
		{
			PinConnectionListChanged(Pin);
		}
	}

	Super::PostReconstructNode();
}

void UK2Node_CallArrayFunction::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	TArray<UEdGraphPin*> PinsToCheck;
	GetArrayTypeDependentPins(PinsToCheck);

	for (int32 Index = 0; Index < PinsToCheck.Num(); ++Index)
	{
		UEdGraphPin* PinToCheck = PinsToCheck[Index];
		if (PinToCheck->SubPins.Num() > 0)
		{
			PinsToCheck.Append(PinToCheck->SubPins);
		}
	}

	PinsToCheck.Add(GetTargetArrayPin());

	if (PinsToCheck.Contains(Pin))
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		bool bNeedToPropagate = false;

		if( Pin->LinkedTo.Num() > 0 )
		{
			if (Pin->PinType.PinCategory == Schema->PC_Wildcard)
			{
				UEdGraphPin* LinkedTo = Pin->LinkedTo[0];
				check(LinkedTo);
				check(Pin->PinType.ContainerType == LinkedTo->PinType.ContainerType);

				Pin->PinType.PinCategory = LinkedTo->PinType.PinCategory;
				Pin->PinType.PinSubCategory = LinkedTo->PinType.PinSubCategory;
				Pin->PinType.PinSubCategoryObject = LinkedTo->PinType.PinSubCategoryObject;

				bNeedToPropagate = true;
			}
		}
		else
		{
			bNeedToPropagate = true;

			for (UEdGraphPin* PinToCheck : PinsToCheck)
			{
				if (PinToCheck->LinkedTo.Num() > 0)
				{
					bNeedToPropagate = false;
					break;
				}
			}

			if (bNeedToPropagate)
			{
				Pin->PinType.PinCategory = Schema->PC_Wildcard;
				Pin->PinType.PinSubCategory = TEXT("");
				Pin->PinType.PinSubCategoryObject = NULL;
			}
		}

		if (bNeedToPropagate)
		{
			PropagateArrayTypeInfo(Pin);
			GetGraph()->NotifyGraphChanged();
		}
	}
}

void UK2Node_CallArrayFunction::ConvertDeprecatedNode(UEdGraph* Graph, bool bOnlySafeChanges)
{
	if (GetLinkerCustomVersion(FBlueprintsObjectVersion::GUID) < FBlueprintsObjectVersion::ArrayGetFuncsReplacedByCustomNode)
	{
		if (FunctionReference.GetMemberParentClass() == UKismetArrayLibrary::StaticClass() && 
			FunctionReference.GetMemberName() == GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Get))
		{
			UBlueprintNodeSpawner::FCustomizeNodeDelegate CustomizeToReturnByVal = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(
				[](UEdGraphNode* NewNode, bool /*bIsTemplateNode*/)
				{
					UK2Node_GetArrayItem* ArrayGetNode = CastChecked<UK2Node_GetArrayItem>(NewNode);
					ArrayGetNode->SetDesiredReturnType(/*bAsReference =*/false);
				}
			);
			UBlueprintNodeSpawner* GetItemNodeSpawner = UBlueprintNodeSpawner::Create(UK2Node_GetArrayItem::StaticClass(), /*Outer =*/nullptr, CustomizeToReturnByVal);

			FVector2D NodePos(NodePosX, NodePosY);
			IBlueprintNodeBinder::FBindingSet Bindings;
			UK2Node_GetArrayItem* GetItemNode = Cast<UK2Node_GetArrayItem>(GetItemNodeSpawner->Invoke(Graph, Bindings, NodePos));

			const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
			if (K2Schema && GetItemNode)
			{
				TMap<FString, FString> OldToNewPinMap;
				for (UEdGraphPin* Pin : Pins)
				{
					if (Pin->ParentPin != nullptr)
					{
						// ReplaceOldNodeWithNew() will take care of mapping split pins (as long as the parents are properly mapped)
						continue;
					}
					else if (Pin->PinName == UEdGraphSchema_K2::PN_Self)
					{
						// there's no analogous pin, signal that we're expecting this
						OldToNewPinMap.Add(Pin->PinName, FString());
					}
					else if (Pin->PinType.IsArray())
					{
						OldToNewPinMap.Add(Pin->PinName, GetItemNode->GetTargetArrayPin()->PinName);
					}
					else if (Pin->Direction == EGPD_Output)
					{
						OldToNewPinMap.Add(Pin->PinName, GetItemNode->GetResultPin()->PinName);
					}
					else
					{
						OldToNewPinMap.Add(Pin->PinName, GetItemNode->GetIndexPin()->PinName);
					}
				}
				K2Schema->ReplaceOldNodeWithNew(this, GetItemNode, OldToNewPinMap);
			}
		}
	}
}

UEdGraphPin* UK2Node_CallArrayFunction::GetTargetArrayPin() const
{
	TArray< FArrayPropertyPinCombo > ArrayParmPins;

	GetArrayPins(ArrayParmPins);

	if(ArrayParmPins.Num())
	{
		return ArrayParmPins[0].ArrayPin;
	}
	return nullptr;
}

void UK2Node_CallArrayFunction::GetArrayPins(TArray< FArrayPropertyPinCombo >& OutArrayPinInfo ) const
{
	OutArrayPinInfo.Empty();

	UFunction* TargetFunction = GetTargetFunction();
	if (ensure(TargetFunction))
	{
		const FString& ArrayPointerMetaData = TargetFunction->GetMetaData(FBlueprintMetadata::MD_ArrayParam);
		TArray<FString> ArrayPinComboNames;
		ArrayPointerMetaData.ParseIntoArray(ArrayPinComboNames, TEXT(","), true);

		for (auto Iter = ArrayPinComboNames.CreateConstIterator(); Iter; ++Iter)
		{
			TArray<FString> ArrayPinNames;
			Iter->ParseIntoArray(ArrayPinNames, TEXT("|"), true);

			FArrayPropertyPinCombo ArrayInfo;
			ArrayInfo.ArrayPin = FindPin(ArrayPinNames[0]);
			if (ArrayPinNames.Num() > 1)
			{
				ArrayInfo.ArrayPropPin = FindPin(ArrayPinNames[1]);
			}

			if (ArrayInfo.ArrayPin)
			{
				OutArrayPinInfo.Add(ArrayInfo);
			}
		}
	}
}

bool UK2Node_CallArrayFunction::IsWildcardProperty(UFunction* InArrayFunction, const UProperty* InProperty)
{
	if(InArrayFunction && InProperty)
	{
		const FString& ArrayPointerMetaData = InArrayFunction->GetMetaData(FBlueprintMetadata::MD_ArrayParam);
		if (!ArrayPointerMetaData.IsEmpty())
		{
			TArray<FString> ArrayPinComboNames;
			ArrayPointerMetaData.ParseIntoArray(ArrayPinComboNames, TEXT(","), true);

			for (auto Iter = ArrayPinComboNames.CreateConstIterator(); Iter; ++Iter)
			{
				TArray<FString> ArrayPinNames;
				Iter->ParseIntoArray(ArrayPinNames, TEXT("|"), true);

				if (ArrayPinNames[0] == InProperty->GetName())
				{
					return true;
				}
			}
		}
	}
	return false;
}

void UK2Node_CallArrayFunction::GetArrayTypeDependentPins(TArray<UEdGraphPin*>& OutPins) const
{
	OutPins.Empty();

	UFunction* TargetFunction = GetTargetFunction();
	if (ensure(TargetFunction))
	{
		const FString& DependentPinMetaData = TargetFunction->GetMetaData(FBlueprintMetadata::MD_ArrayDependentParam);
		TArray<FString> TypeDependentPinNames;
		DependentPinMetaData.ParseIntoArray(TypeDependentPinNames, TEXT(","), true);

		for (TArray<UEdGraphPin*>::TConstIterator it(Pins); it; ++it)
		{
			UEdGraphPin* CurrentPin = *it;
			int32 ItemIndex = 0;
			if (CurrentPin && TypeDependentPinNames.Find(CurrentPin->PinName, ItemIndex))
			{
				OutPins.Add(CurrentPin);
			}
		}
	}
}

void UK2Node_CallArrayFunction::PropagateArrayTypeInfo(const UEdGraphPin* SourcePin)
{
	if( SourcePin )
	{
		const UEdGraphSchema_K2* Schema = CastChecked<UEdGraphSchema_K2>(GetSchema());
		const FEdGraphPinType& SourcePinType = SourcePin->PinType;

		TArray<UEdGraphPin*> DependentPins;
		GetArrayTypeDependentPins(DependentPins);
		DependentPins.Add(GetTargetArrayPin());
	
		// Propagate pin type info (except for array info!) to pins with dependent types
		for (UEdGraphPin* CurrentPin : DependentPins)
		{
			if (CurrentPin != SourcePin)
			{
				CA_SUPPRESS(6011); // warning C6011: Dereferencing NULL pointer 'CurrentPin'.
				FEdGraphPinType& CurrentPinType = CurrentPin->PinType;

				bool const bHasTypeMismatch = (CurrentPinType.PinCategory != SourcePinType.PinCategory) ||
					(CurrentPinType.PinSubCategory != SourcePinType.PinSubCategory) ||
					(CurrentPinType.PinSubCategoryObject != SourcePinType.PinSubCategoryObject);

				if (!bHasTypeMismatch)
				{
					continue;
				}

				if (CurrentPin->SubPins.Num() > 0)
				{
					Schema->RecombinePin(CurrentPin->SubPins[0]);
				}

				CurrentPinType.PinCategory          = SourcePinType.PinCategory;
				CurrentPinType.PinSubCategory       = SourcePinType.PinSubCategory;
				CurrentPinType.PinSubCategoryObject = SourcePinType.PinSubCategoryObject;

				// Reset default values
				if (!Schema->IsPinDefaultValid(CurrentPin, CurrentPin->DefaultValue, CurrentPin->DefaultObject, CurrentPin->DefaultTextValue).IsEmpty())
				{
					Schema->ResetPinToAutogeneratedDefaultValue(CurrentPin);
				}
			}
		}
	}
}
