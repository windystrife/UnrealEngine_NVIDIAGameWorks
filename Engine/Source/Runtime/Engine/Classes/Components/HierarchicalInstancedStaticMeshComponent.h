// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "Async/AsyncWork.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "StaticMeshResources.h"

#include "HierarchicalInstancedStaticMeshComponent.generated.h"

class FClusterBuilder;
class FStaticLightingTextureMapping_InstancedStaticMesh;

USTRUCT()
struct FClusterNode
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector BoundMin;
	UPROPERTY()
	int32 FirstChild;
	UPROPERTY()
	FVector BoundMax;
	UPROPERTY()
	int32 LastChild;
	UPROPERTY()
	int32 FirstInstance;
	UPROPERTY()
	int32 LastInstance;

	FClusterNode()
		: BoundMin(MAX_flt, MAX_flt, MAX_flt)
		, FirstChild(-1)
		, BoundMax(MIN_flt, MIN_flt, MIN_flt)
		, LastChild(-1)
		, FirstInstance(-1)
		, LastInstance(-1)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FClusterNode& NodeData)
	{
		// @warning BulkSerialize: FClusterNode is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << NodeData.BoundMin;
		Ar << NodeData.FirstChild;
		Ar << NodeData.BoundMax;
		Ar << NodeData.LastChild;
		Ar << NodeData.FirstInstance;
		Ar << NodeData.LastInstance;
		return Ar;
	}
};

UCLASS(ClassGroup=Rendering, meta=(BlueprintSpawnableComponent))
class ENGINE_API UHierarchicalInstancedStaticMeshComponent : public UInstancedStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

	~UHierarchicalInstancedStaticMeshComponent();

	TSharedPtr<TArray<FClusterNode>, ESPMode::ThreadSafe> ClusterTreePtr;

	// Table for remaping instances from cluster tree to PerInstanceSMData order
	UPROPERTY()
	TArray<int32> SortedInstances;

	// The number of instances in the ClusterTree. Subsequent instances will always be rendered.
	UPROPERTY()
	int32 NumBuiltInstances;

	// Normally equal to NumBuiltInstances, but can be lower if density scaling is in effect
	int32 NumBuiltRenderInstances;

	// Bounding box of any built instances (cached from the ClusterTree)
	UPROPERTY()
	FBox BuiltInstanceBounds;

	// Bounding box of any unbuilt instances
	UPROPERTY()
	FBox UnbuiltInstanceBounds;

	// Bounds of each individual unbuilt instance, used for LOD calculation
	UPROPERTY()
	TArray<FBox> UnbuiltInstanceBoundsList;

	// Instance Index of each individual unbuilt instance, used in unbuilt rendering during a wait for the build
	UPROPERTY()
	TArray<int32> UnbuiltInstanceIndexList;

	// Enable for detail meshes that don't really affect the game. Disable for anything important.
	// Typically, this will be enabled for small meshes without collision (e.g. grass) and disabled for large meshes with collision (e.g. trees)
	UPROPERTY()
	uint32 bEnableDensityScaling:1;

	// Which instances have been removed by foliage density scaling?
	TBitArray<> ExcludedDueToDensityScaling;

	// The number of nodes in the occlusion layer
	UPROPERTY()
	int32 OcclusionLayerNumNodes;

	// The last mesh bounds that was cache
	UPROPERTY()
	FBoxSphereBounds CacheMeshExtendedBounds;

	bool bIsAsyncBuilding;
	bool bDiscardAsyncBuildResults;
	bool bConcurrentRemoval;
	bool bAutoRebuildTreeOnInstanceChanges;

	UPROPERTY()
	bool bDisableCollision;

	// Apply the results of the async build
	void ApplyBuildTreeAsync(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, TSharedRef<FClusterBuilder, ESPMode::ThreadSafe> Builder, double StartTime);

public:

	//Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void PostLoad() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& BoundTransform) const override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	// UInstancedStaticMesh interface
	virtual int32 AddInstance(const FTransform& InstanceTransform) override;
	virtual bool RemoveInstance(int32 InstanceIndex) override;
	virtual bool UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty = false, bool bTeleport = false) override;
	virtual void ClearInstances() override;
	virtual TArray<int32> GetInstancesOverlappingSphere(const FVector& Center, float Radius, bool bSphereInWorldSpace = true) const override;
	virtual TArray<int32> GetInstancesOverlappingBox(const FBox& Box, bool bBoxInWorldSpace = true) const override;

	/** Removes all the instances with indices specified in the InstancesToRemove array. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	bool RemoveInstances(const TArray<int32>& InstancesToRemove);

	/** Get the number of instances that overlap a given sphere */
	int32 GetOverlappingSphereCount(const FSphere& Sphere) const;
	/** Get the number of instances that overlap a given box */
	int32 GetOverlappingBoxCount(const FBox& Box) const;
	/** Get the transforms of instances inside the provided box */
	void GetOverlappingBoxTransforms(const FBox& Box, TArray<FTransform>& OutTransforms) const;

	virtual bool ShouldCreatePhysicsState() const override;

	bool BuildTreeIfOutdated(bool Async, bool ForceUpdate);
	static void BuildTreeAnyThread(TArray<FMatrix>& InstanceTransforms, const FBox& MeshBox, TArray<FClusterNode>& OutClusterTree, TArray<int32>& OutSortedInstances, TArray<int32>& OutInstanceReorderTable, int32& OutOcclusionLayerNum, int32 MaxInstancesPerLeaf );
	void AcceptPrebuiltTree(TArray<FClusterNode>& InClusterTree, int InOcclusionLayerNumNodes);
	bool IsAsyncBuilding() const { return bIsAsyncBuilding; }
	bool IsTreeFullyBuilt() const { return NumBuiltInstances == PerInstanceSMData.Num() && RemovedInstances.Num() == 0; }

	/** Heuristic for the number of leaves in the tree **/
	int32 DesiredInstancesPerLeaf();

	virtual void ApplyComponentInstanceData(class FInstancedStaticMeshComponentInstanceData* InstancedMeshData) override;

protected:
	void BuildTree();
	void BuildTreeAsync();

	/** Removes a single instance without extra work such as rebuilding the tree or marking render state dirty. */
	void RemoveInstanceInternal(int32 InstanceIndex);
	
	/** Gets and approximate number of verts for each LOD to generate heuristics **/
	int32 GetVertsForLOD(int32 LODIndex);
	/** Average number of instances per leaf **/
	float ActualInstancesPerLeaf();
	/** For testing, prints some stats after any kind of build **/
	void PostBuildStats();

	virtual void GetNavigationPerInstanceTransforms(const FBox& AreaBox, TArray<FTransform>& InstanceData) const override;
	virtual void PartialNavigationUpdate(int32 InstanceIdx) override;
	void FlushAccumulatedNavigationUpdates();
	mutable FBox AccumulatedNavigationDirtyArea;

protected:
	friend FStaticLightingTextureMapping_InstancedStaticMesh;
	friend FInstancedLightMap2D;
	friend FInstancedShadowMap2D;
	friend class FClusterBuilder;
};

