// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "UObject/WeakObjectPtr.h"
#include "GameFramework/Actor.h"
#include "EnvironmentQuery/EnvQueryTypes.h"

UEnvQueryItemType_Actor::UEnvQueryItemType_Actor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(FWeakObjectPtr);
}

AActor* UEnvQueryItemType_Actor::GetValue(const uint8* RawData)
{
	FWeakObjectPtr WeakObjPtr = GetValueFromMemory<FWeakObjectPtr>(RawData);
	return (AActor*)(WeakObjPtr.Get());
}

void UEnvQueryItemType_Actor::SetValue(uint8* RawData, const FWeakObjectPtr& Value)
{
	FWeakObjectPtr WeakObjPtr(Value);
	SetValueInMemory<FWeakObjectPtr>(RawData, WeakObjPtr);
}

FVector UEnvQueryItemType_Actor::GetItemLocation(const uint8* RawData) const
{
	AActor* MyActor = UEnvQueryItemType_Actor::GetValue(RawData);
	return MyActor ? MyActor->GetActorLocation() : FVector::ZeroVector;
}

FRotator UEnvQueryItemType_Actor::GetItemRotation(const uint8* RawData) const
{
	AActor* MyActor = UEnvQueryItemType_Actor::GetValue(RawData);
	return MyActor ? MyActor->GetActorRotation() : FRotator::ZeroRotator;
}

AActor* UEnvQueryItemType_Actor::GetActor(const uint8* RawData) const
{
	return UEnvQueryItemType_Actor::GetValue(RawData);
}

void UEnvQueryItemType_Actor::SetContextHelper(FEnvQueryContextData& ContextData, const AActor* SingleActor)
{
	ContextData.ValueType = UEnvQueryItemType_Actor::StaticClass();
	ContextData.NumValues = 1;
	ContextData.RawData.SetNumUninitialized(sizeof(FWeakObjectPtr));

	UEnvQueryItemType_Actor::SetValue(ContextData.RawData.GetData(), SingleActor);
}

void UEnvQueryItemType_Actor::SetContextHelper(FEnvQueryContextData& ContextData, const TArray<const AActor*>& MultipleActors)
{
	ContextData.ValueType = UEnvQueryItemType_Actor::StaticClass();
	ContextData.NumValues = MultipleActors.Num();
	ContextData.RawData.SetNumUninitialized(sizeof(FWeakObjectPtr) * MultipleActors.Num());

	uint8* RawData = (uint8*)ContextData.RawData.GetData();
	for (int32 ActorIndex = 0; ActorIndex < MultipleActors.Num(); ActorIndex++)
	{
		UEnvQueryItemType_Actor::SetValue(RawData, MultipleActors[ActorIndex]);
		RawData += sizeof(FWeakObjectPtr);
	}
}

void UEnvQueryItemType_Actor::SetContextHelper(FEnvQueryContextData& ContextData, const TArray<AActor*>& MultipleActors)
{
	ContextData.ValueType = UEnvQueryItemType_Actor::StaticClass();
	ContextData.NumValues = MultipleActors.Num();
	ContextData.RawData.SetNumUninitialized(sizeof(FWeakObjectPtr)* MultipleActors.Num());

	uint8* RawData = (uint8*)ContextData.RawData.GetData();
	for (int32 ActorIndex = 0; ActorIndex < MultipleActors.Num(); ActorIndex++)
	{
		UEnvQueryItemType_Actor::SetValue(RawData, MultipleActors[ActorIndex]);
		RawData += sizeof(FWeakObjectPtr);
	}
}
