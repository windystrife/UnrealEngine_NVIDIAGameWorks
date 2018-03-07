// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/MemberReference.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallFunctionOnMember.generated.h"

class UEdGraph;

UCLASS(MinimalAPI)
class UK2Node_CallFunctionOnMember : public UK2Node_CallFunction
{
	GENERATED_UCLASS_BODY()

	/** Reference to member variable to call function on */
	UPROPERTY()
	FMemberReference				MemberVariableToCallOn;

	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const override;

	//~ Begin UK2Node_CallFunction Interface
	virtual UEdGraphPin* CreateSelfPin(const UFunction* Function) override;
	virtual FText GetFunctionContextString() const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	//~ End UK2Node_CallFunction Interface

};

