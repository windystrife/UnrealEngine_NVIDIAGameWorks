// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDPrimResolver.h"
#include "USDPrimResolverKind.generated.h"


/** Evaluates USD prims based on USD kind metadata */
UCLASS(transient, MinimalAPI)
class UUSDPrimResolverKind : public UUSDPrimResolver
{
	GENERATED_BODY()

public:
	virtual void FindActorsToSpawn(FUSDSceneImportContext& ImportContext, TArray<FActorSpawnData>& OutActorSpawnData) override;

private:

	void FindActorsToSpawn_Recursive(FUSDSceneImportContext& ImportContext, class IUsdPrim* Prim, class IUsdPrim* ParentPrim, TArray<FActorSpawnData>& OutSpawnDatas);
	IUsdPrim* FindMeshPrim(IUsdPrim* StartPrim) const;
};