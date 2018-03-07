// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraNodeConvert.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraEditorUtilities.h"
#include "SNiagaraGraphNodeConvert.h"
#include "NiagaraHlslTranslator.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeConvert"

UNiagaraNodeConvert::UNiagaraNodeConvert() : UNiagaraNodeWithDynamicPins(), bIsWiringShown(true)
{

}

void UNiagaraNodeConvert::AllocateDefaultPins()
{
	CreateAddPin(EGPD_Input);
	CreateAddPin(EGPD_Output);
}

TSharedPtr<SGraphNode> UNiagaraNodeConvert::CreateVisualWidget()
{
	return SNew(SNiagaraGraphNodeConvert, this);
}

void UNiagaraNodeConvert::Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& CompileOutputs)
{
	TArray<UEdGraphPin*> InputPins;
	GetInputPins(InputPins);

	TArray<int32> CompileInputs;
	for(UEdGraphPin* InputPin : InputPins)
	{
		if (InputPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryType || 
			InputPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryEnum)
		{
			int32 CompiledInput = Translator->CompilePin(InputPin);
			if (CompiledInput == INDEX_NONE)
			{
				Translator->Error(LOCTEXT("InputError", "Error compiling input for convert node."), this, InputPin);
			}
			CompileInputs.Add(CompiledInput);
		}
	}

	Translator->Convert(this, CompileInputs, CompileOutputs);
}

void UNiagaraNodeConvert::AutowireNewNode(UEdGraphPin* FromPin)
{
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	check(Schema);

	FNiagaraTypeDefinition TypeDef;
	if (FromPin)
	{
		TypeDef = Schema->PinToTypeDefinition(FromPin);
	}
	EEdGraphPinDirection Dir = FromPin ? (EEdGraphPinDirection)FromPin->Direction : EGPD_Output;
	EEdGraphPinDirection OppositeDir = FromPin && (EEdGraphPinDirection)FromPin->Direction == EGPD_Input ? EGPD_Output : EGPD_Input;

	if (AutowireSwizzle.IsEmpty())
	{
		if (AutowireBreakType.GetStruct() != nullptr)
		{			
			TypeDef = AutowireBreakType;
			Dir = EGPD_Output;
			OppositeDir = EGPD_Input;
		}
		else if(AutowireMakeType.GetStruct() != nullptr)
		{
			TypeDef = AutowireMakeType;
			Dir = EGPD_Input;
			OppositeDir = EGPD_Output;
		}

		if (TypeDef.IsValid() == false)
		{
			return;
		}

		//No swizzle so we make or break the type.
		const UScriptStruct* Struct = TypeDef.GetScriptStruct();
		if (Struct)
		{
			UEdGraphPin* ConnectPin = RequestNewTypedPin(OppositeDir, TypeDef);
			check(ConnectPin);
			if(FromPin)
			{
				if (Dir == EGPD_Input)
				{
					FromPin->BreakAllPinLinks();
				}

				ConnectPin->MakeLinkTo(FromPin);
			}

			TArray<FName> SrcPath;
			TArray<FName> DestPath;
			//Add a corresponding pin for each property in the from Pin.
			for (TFieldIterator<UProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
			{
				SrcPath.Empty(SrcPath.Num());
				DestPath.Empty(DestPath.Num());
				const UProperty* Property = *PropertyIt;
				FNiagaraTypeDefinition PropType = Schema->GetTypeDefForProperty(Property);
				UEdGraphPin* NewPin = RequestNewTypedPin(Dir, PropType, Property->GetDisplayNameText().ToString());

				if (Dir == EGPD_Input)
				{
					if (FNiagaraTypeDefinition::IsScalarDefinition(PropType))
					{
						SrcPath.Add(TEXT("Value"));
					}
					DestPath.Add(*Property->GetName());
					Connections.Add(FNiagaraConvertConnection(NewPin->PinId, SrcPath, ConnectPin->PinId, DestPath));
					if (SrcPath.Num())
					{
						AddExpandedRecord(FNiagaraConvertPinRecord(NewPin->PinId, SrcPath).GetParent());
					}
					if (DestPath.Num())
					{
						AddExpandedRecord(FNiagaraConvertPinRecord(ConnectPin->PinId, DestPath).GetParent());
					}
				}
				else
				{
					SrcPath.Add(*Property->GetName());
					if (FNiagaraTypeDefinition::IsScalarDefinition(PropType))
					{
						DestPath.Add(TEXT("Value"));
					}
					Connections.Add(FNiagaraConvertConnection(ConnectPin->PinId, SrcPath, NewPin->PinId, DestPath));
					if (DestPath.Num())
					{
						AddExpandedRecord(FNiagaraConvertPinRecord(NewPin->PinId, DestPath).GetParent());
					}
					if (SrcPath.Num())
					{
						AddExpandedRecord(FNiagaraConvertPinRecord(ConnectPin->PinId, SrcPath).GetParent());
					}
				}
			}
		}
	}
	else
	{
		check(FromPin);
		check(OppositeDir == EGPD_Input);
		static FNiagaraTypeDefinition SwizTypes[4] =
		{
			FNiagaraTypeDefinition::GetFloatDef(),
			FNiagaraTypeDefinition::GetVec2Def(),
			FNiagaraTypeDefinition::GetVec3Def(),
			FNiagaraTypeDefinition::GetVec4Def()
		};
		static FName SwizComponents[4] =
		{
			TEXT("X"),
			TEXT("Y"),
			TEXT("Z"),
			TEXT("W")
		};

		UEdGraphPin* ConnectPin = RequestNewTypedPin(OppositeDir, TypeDef);
		check(ConnectPin);
		ConnectPin->MakeLinkTo(FromPin);
		
		check(AutowireSwizzle.Len() <= 4 && AutowireSwizzle.Len() > 0);
		FNiagaraTypeDefinition SwizType = SwizTypes[AutowireSwizzle.Len() - 1];
		UEdGraphPin* NewPin = RequestNewTypedPin(EGPD_Output, SwizType, SwizType.GetNameText().ToString());

		TArray<FName> SrcPath;
		TArray<FName> DestPath;
		for (int32 i = 0; i < AutowireSwizzle.Len(); ++i)
		{
			TCHAR Char = AutowireSwizzle[i];
			FString CharStr = FString(1, &Char);
			SrcPath.Empty(SrcPath.Num());
			DestPath.Empty(DestPath.Num());

			SrcPath.Add(*CharStr);
			DestPath.Add(FNiagaraTypeDefinition::IsScalarDefinition(SwizType) ? TEXT("Value") : SwizComponents[i]);
			Connections.Add(FNiagaraConvertConnection(ConnectPin->PinId, SrcPath, NewPin->PinId, DestPath));
			if (DestPath.Num())
			{
				AddExpandedRecord(FNiagaraConvertPinRecord(NewPin->PinId, DestPath).GetParent());
			}

			if (SrcPath.Num())
			{
				AddExpandedRecord(FNiagaraConvertPinRecord(ConnectPin->PinId, SrcPath).GetParent());
			}
		}
	}
	GetGraph()->NotifyGraphChanged();
}

FText UNiagaraNodeConvert::GetNodeTitle(ENodeTitleType::Type TitleType)const
{
	if (!AutowireSwizzle.IsEmpty())
	{
		return FText::FromString(AutowireSwizzle);
	}
	else if (AutowireMakeType.IsValid())
	{
		return FText::Format(LOCTEXT("MakeTitle", "Make {0}"), AutowireMakeType.GetNameText());
	}
	else if (AutowireBreakType.IsValid())
	{
		return FText::Format(LOCTEXT("BreakTitle", "Break {0}"), AutowireBreakType.GetNameText());
	}
	else
	{
		TArray<UEdGraphPin*> InPins;
		TArray<UEdGraphPin*> OutPins;
		GetInputPins(InPins);
		GetOutputPins(OutPins);
		if (InPins.Num() == 2 && OutPins.Num() == 2)
		{
			//We are converting one pin type directly to another so we can have a nice name.
			const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
			check(Schema);
			FNiagaraTypeDefinition AType = Schema->PinToTypeDefinition(InPins[0]);
			FNiagaraTypeDefinition BType = Schema->PinToTypeDefinition(OutPins[0]);
			return FText::Format(LOCTEXT("SpecificConvertTitle", "{0} -> {1}"), AType.GetNameText(), BType.GetNameText());
		}
		else
		{
			return LOCTEXT("DefaultTitle", "Convert");
		}

	}
}

TArray<FNiagaraConvertConnection>& UNiagaraNodeConvert::GetConnections()
{
	return Connections;
}

void UNiagaraNodeConvert::OnPinRemoved(UEdGraphPin* PinToRemove)
{
	TSet<FGuid> TypePinIds;
	for (UEdGraphPin* Pin : GetAllPins())
	{
		TypePinIds.Add(Pin->PinId);
	}

	auto RemovePredicate = [&](const FNiagaraConvertConnection& Connection)
	{
		return TypePinIds.Contains(Connection.SourcePinId) == false || TypePinIds.Contains(Connection.DestinationPinId) == false;
	};

	Connections.RemoveAll(RemovePredicate);
}

void UNiagaraNodeConvert::InitAsSwizzle(FString Swiz)
{
	AutowireSwizzle = Swiz;
}

void UNiagaraNodeConvert::InitAsMake(FNiagaraTypeDefinition Type)
{
	AutowireMakeType = Type;
}

void UNiagaraNodeConvert::InitAsBreak(FNiagaraTypeDefinition Type)
{
	AutowireBreakType = Type;
}

bool UNiagaraNodeConvert::InitConversion(UEdGraphPin* FromPin, UEdGraphPin* ToPin)
{
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	check(Schema);
	FNiagaraTypeDefinition FromType = Schema->PinToTypeDefinition(FromPin);
	FNiagaraTypeDefinition ToType = Schema->PinToTypeDefinition(ToPin);

	//Can only convert normal struct types.
	if (!FromType.IsValid() || !ToType.IsValid() || FromType.GetClass() || ToType.GetClass())
	{
		return false;
	}

	UEdGraphPin* ConnectFromPin = RequestNewTypedPin(EGPD_Input, FromType);
	FromPin->MakeLinkTo(ConnectFromPin);
	UEdGraphPin* ConnectToPin = RequestNewTypedPin(EGPD_Output, ToType);
	// Before we connect our new link, make sure that the old ones are gone.
	ToPin->BreakAllPinLinks();
	ToPin->MakeLinkTo(ConnectToPin);
	check(ConnectFromPin);
	check(ConnectToPin);

	TArray<FName> SrcPath;
	TArray<FName> DestPath;
	TFieldIterator<UProperty> FromPropertyIt(FromType.GetScriptStruct(), EFieldIteratorFlags::IncludeSuper);
	TFieldIterator<UProperty> ToPropertyIt(ToType.GetScriptStruct(), EFieldIteratorFlags::IncludeSuper);

	TFieldIterator<UProperty> NextFromPropertyIt(FromType.GetScriptStruct(), EFieldIteratorFlags::IncludeSuper);
	while (FromPropertyIt && ToPropertyIt)
	{
		UProperty* FromProp = *FromPropertyIt;
		UProperty* ToProp = *ToPropertyIt;
		if (NextFromPropertyIt)
		{
			++NextFromPropertyIt;
		}

		FNiagaraTypeDefinition FromPropType = Schema->GetTypeDefForProperty(FromProp);
		FNiagaraTypeDefinition ToPropType = Schema->GetTypeDefForProperty(ToProp);
		SrcPath.Empty();
		DestPath.Empty();
		if (FromPropType == ToPropType)
		{
			SrcPath.Add(*FromProp->GetName());
			DestPath.Add(*ToProp->GetName());
			Connections.Add(FNiagaraConvertConnection(ConnectFromPin->PinId, SrcPath, ConnectToPin->PinId, DestPath));

			if (SrcPath.Num())
			{
				AddExpandedRecord(FNiagaraConvertPinRecord(ConnectFromPin->PinId, SrcPath).GetParent());
			}

			if (DestPath.Num())
			{
				AddExpandedRecord(FNiagaraConvertPinRecord(ConnectToPin->PinId, DestPath).GetParent());
			}
		}
			
		//If there is no next From property, just keep with the same one and set it to all future To properties.
		if (NextFromPropertyIt)
		{
			++FromPropertyIt;
		}		

		++ToPropertyIt;
	}

	return Connections.Num() > 0;
}


bool UNiagaraNodeConvert::IsWiringShown() const
{
	return bIsWiringShown;
}

void UNiagaraNodeConvert::SetWiringShown(bool bInShown)
{
	bIsWiringShown = bInShown;
}

void UNiagaraNodeConvert::RemoveExpandedRecord(const FNiagaraConvertPinRecord& InRecord)
{
	if (HasExpandedRecord(InRecord) == true)
	{
		FScopedTransaction ConnectTransaction(NSLOCTEXT("NiagaraConvert", "ConvertNodeCollpaseTransaction", "Collapse node."));
		Modify();
		ExpandedItems.Remove(InRecord);
	}
}


bool UNiagaraNodeConvert::HasExpandedRecord(const FNiagaraConvertPinRecord& InRecord)
{
	for (const FNiagaraConvertPinRecord& ExpandedRecord : ExpandedItems)
	{
		if (ExpandedRecord.PinId == InRecord.PinId && ExpandedRecord.Path == InRecord.Path)
		{
			return true;
		}
	}
	return false;
}

void UNiagaraNodeConvert::AddExpandedRecord(const FNiagaraConvertPinRecord& InRecord)
{
	if (HasExpandedRecord(InRecord) == false)
	{
		Modify();
		FScopedTransaction ConnectTransaction(NSLOCTEXT("NiagaraConvert", "ConvertNodeExpandedTransaction", "Expand node."));
		ExpandedItems.AddUnique(InRecord);
	}
}

bool operator ==(const FNiagaraConvertPinRecord & A, const FNiagaraConvertPinRecord & B)
{
	return (A.PinId == B.PinId && A.Path == B.Path);
}

#undef LOCTEXT_NAMESPACE