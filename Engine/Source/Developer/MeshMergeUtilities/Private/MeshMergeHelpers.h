// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/MeshMerging.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class USplineMeshComponent;
class UBodySetup;
class ALandscapeProxy;
struct FSectionInfo;
struct FRawMesh;
struct FRawMeshExt;
struct FStaticMeshLODResources;
struct FKAggregateGeom;

class MESHMERGEUTILITIES_API FMeshMergeHelpers
{
public:
	/** Extracting section info data from static, skeletal mesh (components) */
	static void ExtractSections(const UStaticMeshComponent* Component, int32 LODIndex, TArray<FSectionInfo>& OutSections);
	static void ExtractSections(const USkeletalMeshComponent* Component, int32 LODIndex, TArray<FSectionInfo>& OutSections);
	static void ExtractSections(const UStaticMesh* StaticMesh, int32 LODIndex, TArray<FSectionInfo>& OutSections);

	/** Extracting mesh data in FRawMesh form from static, skeletal mesh (components) */
	static void RetrieveMesh(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FRawMesh& RawMesh, bool bPropagateVertexColours);
	static void RetrieveMesh(USkeletalMeshComponent* SkeletalMeshComponent, int32 LODIndex, FRawMesh& RawMesh, bool bPropagateVertexColours);
	static void RetrieveMesh(const UStaticMesh* StaticMesh, int32 LODIndex, FRawMesh& RawMesh);
	
	/** Exports static mesh LOD render data to a RawMesh */
	static void ExportStaticMeshLOD(const FStaticMeshLODResources& StaticMeshLOD, FRawMesh& OutRawMesh);

	/** Checks whether or not the texture coordinates are outside of 0-1 UV ranges */
	static bool CheckWrappingUVs(const TArray<FVector2D>& UVs);

	/** Culls away triangles which are inside culling volumes or completely underneath the landscape */
	static void CullTrianglesFromVolumesAndUnderLandscapes(const UWorld* World, const FBoxSphereBounds& Bounds, FRawMesh& InOutRawMesh);
	
	/** Propagates deformation along spline to raw mesh data */
	static void PropagateSplineDeformationToRawMesh(const USplineMeshComponent* InSplineMeshComponent, struct FRawMesh &OutRawMesh);
	
	/** Propagates deformation along spline to physics geometry data */
	static void PropagateSplineDeformationToPhysicsGeometry(USplineMeshComponent* SplineMeshComponent, FKAggregateGeom& InOutPhysicsGeometry);

	/** Transforms raw mesh data using InTransform*/
	static void TransformRawMeshVertexData(const FTransform& InTransform, FRawMesh &OutRawMesh);

	/** Retrieves all culling landscapes and volumes as FRawMesh structures */
	static void RetrieveCullingLandscapeAndVolumes(UWorld* InWorld, const FBoxSphereBounds& EstimatedMeshProxyBounds, const TEnumAsByte<ELandscapeCullingPrecision::Type> PrecisionType, TArray<FRawMesh*>& CullingRawMeshes);

	/** Transforms physics geometry data using InTransform */
	static void TransformPhysicsGeometry(const FTransform& InTransform, struct FKAggregateGeom& AggGeom);
	
	/** Extract physics geometry data from a body setup */
	static void ExtractPhysicsGeometry(UBodySetup* InBodySetup, const FTransform& ComponentToWorld, struct FKAggregateGeom& OutAggGeom);

	/** Ensure that UV is in valid 0-1 UV ranges */
	static FVector2D GetValidUV(const FVector2D& UV);
	
	/** Calculates UV coordinates bounds for the given Raw Mesh	*/
	static void CalculateTextureCoordinateBoundsForRawMesh(const FRawMesh& InRawMesh, TArray<FBox2D>& OutBounds);

	/** Propagates vertex painted colors from the StaticMeshComponent instance to RawMesh */
	static bool PropagatePaintedColorsToRawMesh(const UStaticMeshComponent* StaticMeshComponent, int32 LODIndex, FRawMesh& RawMesh);

	/** Checks whether or not the landscape proxy is hit given a ray start and end */
	static bool IsLandscapeHit(const FVector& RayOrigin, const FVector& RayEndPoint, const UWorld* World, const TArray<ALandscapeProxy*>& LandscapeProxies, FVector& OutHitLocation);
};
