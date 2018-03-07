// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "HLODSelectionActor.generated.h"

UCLASS(notplaceable, hidecategories = (Object, Collision, Display, Input, Blueprint, Transform))
class AHLODSelectionActor : public AActor
{
	GENERATED_UCLASS_BODY()

protected:
#if WITH_EDITORONLY_DATA
	/** Visualization component for rendering the LODActors bounds (cluster bounds)*/
	UPROPERTY()
	class UDrawSphereComponent* DrawSphereComponent;
#endif // WITH_EDITORONLY_DATA
public:
#if WITH_EDITORONLY_DATA
	/** Returns StaticMeshComponent subobject **/
	class UDrawSphereComponent* GetDrawSphereComponent() const { return DrawSphereComponent; }

	/** LOD actor this is representing */
	TWeakObjectPtr<AActor> RepresentedActor;
#endif // WITH_EDITORONLY_DATA
};
