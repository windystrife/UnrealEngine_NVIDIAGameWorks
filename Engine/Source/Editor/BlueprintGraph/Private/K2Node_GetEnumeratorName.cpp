// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_GetEnumeratorName.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "Kismet/KismetNodeHelperLibrary.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

FString UK2Node_GetEnumeratorName::EnumeratorPinName = TEXT("Enumerator");

UK2Node_GetEnumeratorName::UK2Node_GetEnumeratorName(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_GetEnumeratorName::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	CreatePin(EGPD_Input, Schema->PC_Byte, FString(), nullptr, EnumeratorPinName);
	CreatePin(EGPD_Output, Schema->PC_Name, FString(), nullptr, Schema->PN_ReturnValue);
}

FText UK2Node_GetEnumeratorName::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "GetEnumeratorName_Tooltip", "Returns name of enumerator");
}

FText UK2Node_GetEnumeratorName::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "GetNode_Title", "Enum to Name");
}

FText UK2Node_GetEnumeratorName::GetCompactNodeTitle() const
{
	return NSLOCTEXT("K2Node", "CastSymbol", "\x2022");
}

UEnum* UK2Node_GetEnumeratorName::GetEnum() const
{
	const UEdGraphPin* InputPin = FindPinChecked(EnumeratorPinName);
	const UEdGraphPin* EnumPin = InputPin->LinkedTo.Num() ? InputPin->LinkedTo[0] : InputPin;
	return EnumPin ? Cast<UEnum>(EnumPin->PinType.PinSubCategoryObject.Get()) : nullptr;
}

void UK2Node_GetEnumeratorName::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	const UEdGraphPin* OutputPin = FindPinChecked(Schema->PN_ReturnValue); 
	/*Don't validate isolated nodes */
	if (0 != OutputPin->LinkedTo.Num())
	{
		EarlyValidation(MessageLog);
	}
}

FSlateIcon UK2Node_GetEnumeratorName::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Enum_16x");
	return Icon;
}

void UK2Node_GetEnumeratorName::EarlyValidation(class FCompilerResultsLog& MessageLog) const
{
	Super::EarlyValidation(MessageLog);

	const UEnum* Enum = GetEnum();
	if (nullptr == Enum)
	{
		MessageLog.Error(*NSLOCTEXT("K2Node", "GetNumEnumEntries_NoIntput_Error", "@@ Must have non-default Enum input").ToString(), this);
	}
}

bool UK2Node_GetEnumeratorName::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	const UEdGraphPin* InputPin = FindPinChecked(EnumeratorPinName);
	if((InputPin == MyPin) && OtherPin && (OtherPin->PinType.PinCategory == Schema->PC_Byte))
	{
		if(NULL == Cast<UEnum>(OtherPin->PinType.PinSubCategoryObject.Get()))
		{
			OutReason = NSLOCTEXT("K2Node", "GetNumEnumEntries_NotEnum_Msg", "Input is not an Enum.").ToString();
			return true;
		}
	}

	return false;
}

FName UK2Node_GetEnumeratorName::GetFunctionName() const
{
	const FName FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetNodeHelperLibrary, GetEnumeratorName);
	return FunctionName;
}

void UK2Node_GetEnumeratorName::UpdatePinType()
{
	UEdGraphPin* EnumPin = FindPinChecked(EnumeratorPinName);
	UEnum* Enum = GetEnum();
	if (EnumPin->PinType.PinSubCategoryObject != Enum)
	{
		EnumPin->PinType.PinSubCategoryObject = Enum;
		PinTypeChanged(EnumPin);
	}
}

void UK2Node_GetEnumeratorName::PostReconstructNode()
{
	UpdatePinType();

	Super::PostReconstructNode();
}

void UK2Node_GetEnumeratorName::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	if (Pin == FindPinChecked(EnumeratorPinName))
	{
		UpdatePinType();
	}
}

void UK2Node_GetEnumeratorName::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UEnum* Enum = GetEnum();
	if(NULL == Enum)
	{
		CompilerContext.MessageLog.Error(*FString::Printf(*NSLOCTEXT("K2Node", "GetEnumeratorNam_Error_MustHaveValidName", "@@ must have a valid enum defined").ToString()), this);
		return;
	}

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
		
	const UFunction* Function = UKismetNodeHelperLibrary::StaticClass()->FindFunctionByName( GetFunctionName() );
	check(NULL != Function);
	UK2Node_CallFunction* CallGetName = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph); 
	CallGetName->SetFromFunction(Function);
	CallGetName->AllocateDefaultPins();
	check(CallGetName->IsNodePure());
		
	//OPUTPUT PIN
	UEdGraphPin* OrgReturnPin = FindPinChecked(Schema->PN_ReturnValue);
	UEdGraphPin* NewReturnPin = CallGetName->GetReturnValuePin();
	check(NULL != NewReturnPin);
	CompilerContext.MovePinLinksToIntermediate(*OrgReturnPin, *NewReturnPin);

	//ENUM PIN
	UEdGraphPin* EnumPin = CallGetName->FindPinChecked(TEXT("Enum"));
	Schema->TrySetDefaultObject(*EnumPin, Enum);
	check(EnumPin->DefaultObject == Enum);

	//VALUE PIN
	UEdGraphPin* OrgInputPin = FindPinChecked(EnumeratorPinName);
	UEdGraphPin* IndexPin = CallGetName->FindPinChecked(TEXT("EnumeratorValue"));
	check(EGPD_Input == IndexPin->Direction && Schema->PC_Byte == IndexPin->PinType.PinCategory);
	CompilerContext.MovePinLinksToIntermediate(*OrgInputPin, *IndexPin);

	if (!IndexPin->LinkedTo.Num())
	{
		//MAKE LITERAL BYTE FROM LITERAL ENUM
		const FString EnumLiteral = IndexPin->GetDefaultAsString();
		const int32 NumericValue = Enum->GetValueByName(*EnumLiteral);
		if (NumericValue == INDEX_NONE) 
		{
			CompilerContext.MessageLog.Error(*FString::Printf(*NSLOCTEXT("K2Node", "GetEnumeratorNam_Error_InvalidName", "@@ has invalid enum value '%s'").ToString(), *EnumLiteral), this);
			return;
		}
		const FString DefaultByteValue = FString::FromInt(NumericValue);

		// LITERAL BYTE FUNCTION
		const FName FunctionName = GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralByte);
		UK2Node_CallFunction* MakeLiteralByte = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph); 
		MakeLiteralByte->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(FunctionName));
		MakeLiteralByte->AllocateDefaultPins();

		UEdGraphPin* MakeLiteralByteReturnPin = MakeLiteralByte->FindPinChecked(Schema->PN_ReturnValue);
		Schema->TryCreateConnection(MakeLiteralByteReturnPin, IndexPin);

		UEdGraphPin* MakeLiteralByteInputPin = MakeLiteralByte->FindPinChecked(TEXT("Value"));
		MakeLiteralByteInputPin->DefaultValue = DefaultByteValue;
	}

	BreakAllNodeLinks();
}

void UK2Node_GetEnumeratorName::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

FText UK2Node_GetEnumeratorName::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Name);
}
