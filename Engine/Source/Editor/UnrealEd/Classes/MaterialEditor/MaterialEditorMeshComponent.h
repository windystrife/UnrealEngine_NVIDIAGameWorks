// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/StaticMeshComponent.h"
#include "MaterialEditorMeshComponent.generated.h"

UCLASS()
class UNREALED_API UMaterialEditorMeshComponent : public UStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

	// USceneComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
};

