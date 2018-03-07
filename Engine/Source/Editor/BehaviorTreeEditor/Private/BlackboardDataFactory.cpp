// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlackboardDataFactory.h"
#include "BehaviorTree/BlackboardData.h"

#define LOCTEXT_NAMESPACE "BlackboardDataFactory"

UBlackboardDataFactory::UBlackboardDataFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UBlackboardData::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

bool UBlackboardDataFactory::CanCreateNew() const
{
	return true;
}

UObject* UBlackboardDataFactory::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UBlackboardData::StaticClass()));
	return NewObject<UBlackboardData>(InParent, Class, Name, Flags);
}

#undef LOCTEXT_NAMESPACE
