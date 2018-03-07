// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "TargetPoint.generated.h"

UCLASS(MinimalAPI)
class ATargetPoint : public AActor
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Display, meta = (AllowPrivateAccess = "true"))
	class UBillboardComponent* SpriteComponent;

	UPROPERTY()
	class UArrowComponent* ArrowComponent;

public:
	/** Returns SpriteComponent subobject **/
	ENGINE_API class UBillboardComponent* GetSpriteComponent() const;
	/** Returns ArrowComponent subobject **/
	ENGINE_API class UArrowComponent* GetArrowComponent() const;
#endif
};



