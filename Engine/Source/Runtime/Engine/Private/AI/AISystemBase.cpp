// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/AISystemBase.h"
#include "Templates/Casts.h"

UAISystemBase::UAISystemBase(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

FName UAISystemBase::GetAISystemModuleName()
{
	UAISystemBase* AISystemDefaultObject = Cast<UAISystemBase>(StaticClass()->GetDefaultObject());
	return AISystemDefaultObject != NULL ? AISystemDefaultObject->AISystemModuleName : TEXT("");
}

FSoftClassPath UAISystemBase::GetAISystemClassName()
{
	UAISystemBase* AISystemDefaultObject = Cast<UAISystemBase>(StaticClass()->GetDefaultObject());
	return AISystemDefaultObject != NULL ? AISystemDefaultObject->AISystemClassName : FSoftClassPath();
}

void UAISystemBase::StartPlay()
{

}

bool UAISystemBase::ShouldInstantiateInNetMode(ENetMode NetMode)
{
	UAISystemBase* AISystemDefaultObject = Cast<UAISystemBase>(StaticClass()->GetDefaultObject());
	return AISystemDefaultObject && (AISystemDefaultObject->bInstantiateAISystemOnClient == true || NetMode != NM_Client);
}
