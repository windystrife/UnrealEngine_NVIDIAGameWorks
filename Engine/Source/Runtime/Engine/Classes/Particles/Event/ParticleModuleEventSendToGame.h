// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ParticleModuleEventSendToGame.generated.h"

UCLASS(abstract, editinlinenew, hidecategories=Object)
class UParticleModuleEventSendToGame : public UObject
{
	GENERATED_UCLASS_BODY()


	/** This is our function to allow subclasses to "do the event action" **/
	virtual void DoEvent( const FVector& InCollideDirection, const FVector& InHitLocation, const FVector& InHitNormal, const FName& InBoneName ) {}
};

