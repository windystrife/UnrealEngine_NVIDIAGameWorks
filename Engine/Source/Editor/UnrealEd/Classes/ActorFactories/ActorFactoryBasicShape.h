// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactoryStaticMesh.h"
#include "ActorFactoryBasicShape.generated.h"

class AActor;
struct FAssetData;

UCLASS(MinimalAPI,config=Editor)
class UActorFactoryBasicShape : public UActorFactoryStaticMesh
{
	GENERATED_UCLASS_BODY()

	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;

	UNREALED_API static const FName BasicCube;
	UNREALED_API static const FName BasicSphere;
	UNREALED_API static const FName BasicCylinder;
	UNREALED_API static const FName BasicCone;
	UNREALED_API static const FName BasicPlane;
};
