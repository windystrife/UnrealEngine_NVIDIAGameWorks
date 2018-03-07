// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeFactory.h"
#include "BehaviorTree/BehaviorTree.h"

#define LOCTEXT_NAMESPACE "BehaviorTreeFactory"

UBehaviorTreeFactory::UBehaviorTreeFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UBehaviorTree::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

bool UBehaviorTreeFactory::CanCreateNew() const
{
	return true;
}

UObject* UBehaviorTreeFactory::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UBehaviorTree::StaticClass()));
	return NewObject<UBehaviorTree>(InParent, Class, Name, Flags);;
}

#undef LOCTEXT_NAMESPACE
