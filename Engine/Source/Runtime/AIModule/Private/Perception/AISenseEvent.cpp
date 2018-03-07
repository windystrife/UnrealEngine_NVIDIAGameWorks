// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Perception/AISenseEvent.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISenseEvent_Hearing.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISenseEvent_Damage.h"

//----------------------------------------------------------------------//
// UAISenseEvent_Hearing
//----------------------------------------------------------------------//
UAISenseEvent_Hearing::UAISenseEvent_Hearing(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

FAISenseID UAISenseEvent_Hearing::GetSenseID() const
{
	return UAISense::GetSenseID<UAISense_Hearing>();
}

//----------------------------------------------------------------------//
// UAISenseEvent_Damage
//----------------------------------------------------------------------//
FAISenseID UAISenseEvent_Damage::GetSenseID() const
{
	return UAISense::GetSenseID<UAISense_Damage>();
}
