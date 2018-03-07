// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintFieldNodeSpawner.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "BlueprintFieldNodeSpawner"

//------------------------------------------------------------------------------
UBlueprintFieldNodeSpawner* UBlueprintFieldNodeSpawner::Create(TSubclassOf<UK2Node> NodeClass, UField const* const Field, UObject* Outer/* = nullptr*/)
{
	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}
	UBlueprintFieldNodeSpawner* NodeSpawner = NewObject<UBlueprintFieldNodeSpawner>(Outer);
	NodeSpawner->Field     = Field;
	NodeSpawner->NodeClass = NodeClass;
	
	return NodeSpawner;
}

//------------------------------------------------------------------------------
UBlueprintFieldNodeSpawner::UBlueprintFieldNodeSpawner(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, Field(nullptr)
{
}

//------------------------------------------------------------------------------
FBlueprintNodeSignature UBlueprintFieldNodeSpawner::GetSpawnerSignature() const
{
	FBlueprintNodeSignature SpawnerSignature(NodeClass);
	SpawnerSignature.AddSubObject(Field);

	return SpawnerSignature;
}

//------------------------------------------------------------------------------
UEdGraphNode* UBlueprintFieldNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	auto PostSpawnSetupLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, UField const* InField, FSetNodeFieldDelegate SetFieldDelegate, FCustomizeNodeDelegate UserDelegate)
	{
		SetFieldDelegate.ExecuteIfBound(NewNode, InField);
		UserDelegate.ExecuteIfBound(NewNode, bIsTemplateNode);
	};

	FCustomizeNodeDelegate PostSpawnSetupDelegate = FCustomizeNodeDelegate::CreateStatic(PostSpawnSetupLambda, GetField(), SetNodeFieldDelegate, CustomizeNodeDelegate);
	return Super::SpawnNode<UEdGraphNode>(NodeClass, ParentGraph, Bindings, Location, PostSpawnSetupDelegate);
}

//------------------------------------------------------------------------------
UField const* UBlueprintFieldNodeSpawner::GetField() const
{
	return Field;
}

#undef LOCTEXT_NAMESPACE
