// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactorySkeletalMesh.generated.h"

class AActor;
struct FAssetData;
class USkeletalMesh;

UCLASS(MinimalAPI, config=Editor)
class UActorFactorySkeletalMesh : public UActorFactory
{
	GENERATED_UCLASS_BODY()

protected:
	//~ Begin UActorFactory Interface
	virtual void PostSpawnActor( UObject* Asset, AActor* NewActor ) override;
	virtual void PostCreateBlueprint( UObject* Asset, AActor* CDO ) override;
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg ) override;
	virtual FQuat AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation) const override;
	//~ End UActorFactory Interface

	virtual USkeletalMesh* GetSkeletalMeshFromAsset( UObject* Asset ) const;
};



