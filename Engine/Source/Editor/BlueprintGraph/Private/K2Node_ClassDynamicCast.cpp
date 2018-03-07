// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_ClassDynamicCast.h"
#include "GraphEditorSettings.h"
#include "EdGraphSchema_K2.h"
#include "DynamicCastHandler.h"

#define LOCTEXT_NAMESPACE "K2Node_ClassDynamicCast"

struct FClassDynamicCastHelper
{
	static const FString CastSuccessPinName;

	static const FString& GetClassToCastName()
	{
		static FString ClassToCastName(TEXT("Class"));
		return ClassToCastName;
	}
};

const FString FClassDynamicCastHelper::CastSuccessPinName = TEXT("bSuccess");

UK2Node_ClassDynamicCast::UK2Node_ClassDynamicCast(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_ClassDynamicCast::AllocateDefaultPins()
{
	// Check to track down possible BP comms corruption
	//@TODO: Move this somewhere more sensible
	ensure((TargetType == NULL) || (!TargetType->HasAnyClassFlags(CLASS_NewerVersionExists)));

	const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(GetSchema());
	check(K2Schema != nullptr);
	if (!K2Schema->DoesGraphSupportImpureFunctions(GetGraph()))
	{
		bIsPureCast = true;
	}

	if (!bIsPureCast)
	{
		// Input - Execution Pin
		CreatePin(EGPD_Input, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Execute);

		// Output - Execution Pins
		CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_CastSucceeded);
		CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_CastFailed);
	}

	// Input - Source type Pin
	CreatePin(EGPD_Input, K2Schema->PC_Class, FString(), UObject::StaticClass(), FClassDynamicCastHelper::GetClassToCastName());

	// Output - Data Pin
	if (TargetType != NULL)
	{
		const FString CastResultPinName = K2Schema->PN_CastedValuePrefix + TargetType->GetDisplayNameText().ToString();
		CreatePin(EGPD_Output, K2Schema->PC_Class, FString(), *TargetType, CastResultPinName);
	}

	UEdGraphPin* BoolSuccessPin = CreatePin(EGPD_Output, K2Schema->PC_Boolean, FString(), nullptr, FClassDynamicCastHelper::CastSuccessPinName);
	BoolSuccessPin->bHidden = !bIsPureCast;

	UK2Node::AllocateDefaultPins();
}

FLinearColor UK2Node_ClassDynamicCast::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ClassPinTypeColor;
}

FText UK2Node_ClassDynamicCast::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedNodeTitle.IsOutOfDate(this))
	{
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("NodeTitle", "{0} Class"), Super::GetNodeTitle(TitleType)), this);
	}
	return CachedNodeTitle;
}

UEdGraphPin* UK2Node_ClassDynamicCast::GetCastSourcePin() const
{
	UEdGraphPin* Pin = FindPinChecked(FClassDynamicCastHelper::GetClassToCastName());
	check(Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin* UK2Node_ClassDynamicCast::GetBoolSuccessPin() const
{
	UEdGraphPin* Pin = FindPin(FClassDynamicCastHelper::CastSuccessPinName);
	check((Pin == nullptr) || (Pin->Direction == EGPD_Output));
	return Pin;
}

FNodeHandlingFunctor* UK2Node_ClassDynamicCast::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_DynamicCast(CompilerContext, KCST_MetaCast);
}

#undef LOCTEXT_NAMESPACE
