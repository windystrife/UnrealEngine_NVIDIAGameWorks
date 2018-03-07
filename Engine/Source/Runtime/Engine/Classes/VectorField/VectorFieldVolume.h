// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VectorFieldVolume: Volume encompassing a vector field.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "VectorFieldVolume.generated.h"

class UBillboardComponent;

UCLASS(hidecategories=(Object, Advanced, Collision), MinimalAPI)
class AVectorFieldVolume : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = VectorFieldVolume, meta = (AllowPrivateAccess = "true"))
	class UVectorFieldComponent* VectorFieldComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UBillboardComponent* SpriteComponent;
#endif

public:
	/** Returns VectorFieldComponent subobject **/
	ENGINE_API class UVectorFieldComponent* GetVectorFieldComponent() const { return VectorFieldComponent; }
#if WITH_EDITORONLY_DATA
	/** Returns SpriteComponent subobject **/
	ENGINE_API UBillboardComponent* GetSpriteComponent() const { return SpriteComponent; }
#endif
};



