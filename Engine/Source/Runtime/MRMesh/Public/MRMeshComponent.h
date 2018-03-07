// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "PackedNormal.h"

#include "MRMeshComponent.generated.h"




class UMaterial;
class FBaseMeshReconstructorModule;
class UMeshReconstructorBase;
struct FDynamicMeshVertex;

DECLARE_STATS_GROUP(TEXT("MRMesh"), STATGROUP_MRMESH, STATCAT_Advanced);

DECLARE_DELEGATE(FOnProcessingComplete)

class IMRMesh
{
public:
	struct FSendBrickDataArgs
	{
		FIntVector BrickCoords;
		TArray<FVector>& PositionData;
		//TArray<FVector2D>& UVData;
		//TArray<FPackedNormal>& TangentXData;
		//TArray<FPackedNormal>& TangentZData;
		TArray<FColor>& ColorData;
		TArray<uint32>& Indices;
	};

	virtual void SendBrickData(FSendBrickDataArgs Args, const FOnProcessingComplete& OnProcessingComplete = FOnProcessingComplete()) = 0;
	virtual void ClearAllBrickData() = 0;
};



UCLASS(meta = (BlueprintSpawnableComponent, Experimental), ClassGroup = Rendering)
class MRMESH_API UMRMeshComponent : public UPrimitiveComponent, public IMRMesh
{
public:
	friend class FMRMeshProxy;

	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category = "Mesh Reconstruction")
	void ConnectReconstructor( UMeshReconstructorBase* Reconstructor );

	UFUNCTION(BlueprintCallable, Category = "Mesh Reconstruction")
	UMeshReconstructorBase* GetReconstructor() const;

private:
	//~ UPrimitiveComponent
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;
	virtual void SetMaterial(int32 ElementIndex, class UMaterialInterface* InMaterial) override;
	//~ UPrimitiveComponent

	//~ IMRMesh
	virtual void SendBrickData(FSendBrickDataArgs Args, const FOnProcessingComplete& OnProcessingComplete = FOnProcessingComplete()) override;
	virtual void ClearAllBrickData() override;
	//~ IMRMesh

private:
	void SendBrickData_Internal(IMRMesh::FSendBrickDataArgs Args, FOnProcessingComplete OnProcessingComplete);

	void RemoveBodyInstance(int32 BodyIndex);

	void ClearAllBrickData_Internal();

	UPROPERTY(EditAnywhere, Category = Appearance)
	UMaterialInterface* Material;

	UPROPERTY(EditAnywhere, Category = "Mesh Reconstruction")
	UMeshReconstructorBase* MeshReconstructor;

	UPROPERTY(EditAnywhere, Category = "Mesh Reconstruction")
	bool bEnableCollision = false;

	UPROPERTY(transient)
	TArray<UBodySetup*> BodySetups;
	TArray<FBodyInstance*> BodyInstances;
	TArray<FIntVector> BodyIds;


};