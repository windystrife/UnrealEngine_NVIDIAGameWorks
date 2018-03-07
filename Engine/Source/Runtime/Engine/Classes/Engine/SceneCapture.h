// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Base class for all SceneCapture actors
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "SceneCapture.generated.h"

UCLASS(abstract, hidecategories=(Collision, Attachment, Actor), MinimalAPI)
class ASceneCapture : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	/** To display the 3d camera in the editor. */
	UPROPERTY()
	class UStaticMeshComponent* MeshComp;

public:
	/** Returns MeshComp subobject **/
	ENGINE_API class UStaticMeshComponent* GetMeshComp() const { return MeshComp; }
};



