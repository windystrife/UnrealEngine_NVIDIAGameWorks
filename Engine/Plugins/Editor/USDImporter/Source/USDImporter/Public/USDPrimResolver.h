// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "SubclassOf.h"
#include "USDPrimResolver.generated.h"

class IUsdPrim;
class IAssetRegistry;
class UActorFactory;
struct FUsdImportContext;
struct FUsdGeomData;
struct FUSDSceneImportContext;

struct FUsdPrimToImport
{
	FUsdPrimToImport()
		: Prim(nullptr)
		, NumLODs(0)
		, CustomPrimTransform(FMatrix::Identity)
	{}

	IUsdPrim* Prim;
	int32 NumLODs;
	FMatrix CustomPrimTransform;

	const FUsdGeomData* GetGeomData(int32 LODIndex, double Time) const;
};

struct FActorSpawnData
{
	FActorSpawnData()
		: ActorPrim(nullptr)
		, AttachParentPrim(nullptr)
		, MeshPrim(nullptr)
	{}

	FMatrix WorldTransform;
	/** The prim that represents this actor */
	IUsdPrim* ActorPrim;
	/** The prim that represents the parent of this actor for attachment (not necessarily the parent of this prim) */
	IUsdPrim* AttachParentPrim;
	/** The prim that represents the mesh to import and apply to this actor */
	IUsdPrim* MeshPrim;
	FString ActorClassName;
	FString AssetPath;
	FName ActorName;
};

/** Base class for all evaluation of prims for geometry and actors */
UCLASS(transient, MinimalAPI)
class UUSDPrimResolver : public UObject
{
	GENERATED_BODY()

public:
	virtual void Init();

	virtual void FindPrimsToImport(FUsdImportContext& ImportContext, TArray<FUsdPrimToImport>& OutPrimsToImport);

	virtual void FindActorsToSpawn(FUSDSceneImportContext& ImportContext, TArray<FActorSpawnData>& OutActorSpawnDatas);

	virtual AActor* SpawnActor(FUSDSceneImportContext& ImportContext, const FActorSpawnData& SpawnData);

	virtual TSubclassOf<AActor> FindActorClass(FUSDSceneImportContext& ImportContext, const FActorSpawnData& SpawnData) const;

protected:
	void FindPrimsToImport_Recursive(FUsdImportContext& ImportContext, IUsdPrim* Prim, TArray<FUsdPrimToImport>& OutTopLevelPrims);
	virtual void FindActorsToSpawn_Recursive(FUSDSceneImportContext& ImportContext, IUsdPrim* Prim, IUsdPrim* ParentPrim, TArray<FActorSpawnData>& OutSpawnDatas);
	bool IsValidPathForImporting(const FString& TestPath) const;
protected:
	IAssetRegistry* AssetRegistry;
	TMap<IUsdPrim*, AActor*> PrimToActorMap;
};