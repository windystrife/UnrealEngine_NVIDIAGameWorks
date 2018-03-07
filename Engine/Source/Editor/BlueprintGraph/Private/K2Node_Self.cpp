// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "BPTerminal.h"
#include "KismetCompiledFunctionContext.h"
#include "KismetCompilerMisc.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node_Self"

class FKCHandler_Self : public FNodeHandlingFunctor
{
public:
	FKCHandler_Self(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_Self* SelfNode = CastChecked<UK2Node_Self>(Node);
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

		UEdGraphPin* VarPin = SelfNode->FindPin(Schema->PN_Self);
		check( VarPin );

		FBPTerminal* Term = new (Context.Literals) FBPTerminal();
		Term->CopyFromPin(VarPin, VarPin->PinName);
		Term->bIsLiteral = true;
		Context.NetMap.Add(VarPin, Term);		
	}
};

UK2Node_Self::UK2Node_Self(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_Self::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	CreatePin(EGPD_Output, K2Schema->PC_Object, K2Schema->PSC_Self, nullptr, K2Schema->PN_Self);

	Super::AllocateDefaultPins();
}

FText UK2Node_Self::GetTooltipText() const
{
	return NSLOCTEXT("K2Node", "GetSelfReference", "Gets a reference to this instance of the blueprint");
}

FText UK2Node_Self::GetKeywords() const
{
	return LOCTEXT("SelfKeywords", "This");
}

FText UK2Node_Self::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText NodeTitle = NSLOCTEXT("K2Node", "SelfReferenceName", "Self-Reference");
	if (TitleType == ENodeTitleType::MenuTitle)
	{
		NodeTitle = LOCTEXT("ListTitle", "Get a reference to self");
	}
	return NodeTitle;	
}

FNodeHandlingFunctor* UK2Node_Self::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_Self(CompilerContext);
}

void UK2Node_Self::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if(Schema->IsStaticFunctionGraph(GetGraph()))
	{
		MessageLog.Warning(*NSLOCTEXT("K2Node", "InvalidSelfNode", "Self node @@ cannot be used in a static function.").ToString(), this);
	}
}

void UK2Node_Self::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

FText UK2Node_Self::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Variables);
}

#undef LOCTEXT_NAMESPACE
