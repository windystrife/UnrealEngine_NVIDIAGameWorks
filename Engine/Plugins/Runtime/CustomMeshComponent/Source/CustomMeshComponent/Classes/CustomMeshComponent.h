// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "CustomMeshComponent.generated.h"

class FPrimitiveSceneProxy;

USTRUCT(BlueprintType)
struct FCustomMeshTriangle
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Triangle)
	FVector Vertex0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Triangle)
	FVector Vertex1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Triangle)
	FVector Vertex2;
};

/** Component that allows you to specify custom triangle mesh geometry */
UCLASS(hidecategories=(Object,LOD, Physics, Collision), editinlinenew, meta=(BlueprintSpawnableComponent), ClassGroup=Rendering)
class CUSTOMMESHCOMPONENT_API UCustomMeshComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** Set the geometry to use on this triangle mesh */
	UFUNCTION(BlueprintCallable, Category="Components|CustomMesh")
	bool SetCustomMeshTriangles(const TArray<FCustomMeshTriangle>& Triangles);

	/** Add to the geometry to use on this triangle mesh.  This may cause an allocation.  Use SetCustomMeshTriangles() instead when possible to reduce allocations. */
	UFUNCTION(BlueprintCallable, Category = "Components|CustomMesh")
	void AddCustomMeshTriangles(const TArray<FCustomMeshTriangle>& Triangles);

	/** Removes all geometry from this triangle mesh.  Does not deallocate memory, allowing new geometry to reuse the existing allocation. */
	UFUNCTION(BlueprintCallable, Category = "Components|CustomMesh")
	void ClearCustomMeshTriangles();

private:

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ Begin USceneComponent Interface.

	/** */
	TArray<FCustomMeshTriangle> CustomMeshTris;

	friend class FCustomMeshSceneProxy;
};


