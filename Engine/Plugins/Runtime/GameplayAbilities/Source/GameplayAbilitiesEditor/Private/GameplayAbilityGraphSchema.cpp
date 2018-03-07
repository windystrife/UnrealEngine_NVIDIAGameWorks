// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayAbilityGraphSchema.h"
#include "EdGraphSchema_K2_Actions.h"
#include "GameplayEffect.h"
#include "Kismet2/BlueprintEditorUtils.h"

UGameplayAbilityGraphSchema::UGameplayAbilityGraphSchema(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

UK2Node_VariableGet* UGameplayAbilityGraphSchema::SpawnVariableGetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableGetNode(GraphPosition, ParentGraph, VariableName, Source);
}

UK2Node_VariableSet* UGameplayAbilityGraphSchema::SpawnVariableSetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const
{
	return Super::SpawnVariableSetNode(GraphPosition, ParentGraph, VariableName, Source);
}
