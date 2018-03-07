// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "EngineDefines.h"
#include "HitProxies.h"
#include "Misc/Guid.h"
#include "Engine/TextureStreamingTypes.h"
#include "Components/StaticMeshComponent.h"

#include "InstancedStaticMeshComponent.generated.h"

class FLightingBuildOptions;
class FPrimitiveSceneProxy;
class FStaticLightingTextureMapping_InstancedStaticMesh;
class ULightComponent;
struct FNavigableGeometryExport;
struct FNavigationRelevantData;
struct FPerInstanceRenderData;
struct FStaticLightingPrimitiveInfo;

DECLARE_STATS_GROUP(TEXT("Foliage"), STATGROUP_Foliage, STATCAT_Advanced);

class FStaticLightingTextureMapping_InstancedStaticMesh;
class FInstancedLightMap2D;
class FInstancedShadowMap2D;
class FStaticMeshInstanceData;

USTRUCT()
struct FInstancedStaticMeshInstanceData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Instances)
	FMatrix Transform;

	/** Legacy, this is now stored in FMeshMapBuildData.  Still serialized for backwards compatibility. */
	UPROPERTY()
	FVector2D LightmapUVBias_DEPRECATED;

	/** Legacy, this is now stored in FMeshMapBuildData.  Still serialized for backwards compatibility. */
	UPROPERTY()
	FVector2D ShadowmapUVBias_DEPRECATED;

	FInstancedStaticMeshInstanceData()
		: Transform(FMatrix::Identity)
		, LightmapUVBias_DEPRECATED(ForceInit)
		, ShadowmapUVBias_DEPRECATED(ForceInit)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FInstancedStaticMeshInstanceData& InstanceData)
	{
		// @warning BulkSerialize: FInstancedStaticMeshInstanceData is serialized as memory dump
		// See TArray::BulkSerialize for detailed description of implied limitations.
		Ar << InstanceData.Transform << InstanceData.LightmapUVBias_DEPRECATED << InstanceData.ShadowmapUVBias_DEPRECATED;
		return Ar;
	}
};

USTRUCT()
struct FInstancedStaticMeshMappingInfo
{
	GENERATED_USTRUCT_BODY()

	FStaticLightingTextureMapping_InstancedStaticMesh* Mapping;

	FInstancedStaticMeshMappingInfo()
		: Mapping(nullptr)
	{
	}
};

class FAsyncBuildInstanceBuffer : public FNonAbandonableTask
{
public:
	class UInstancedStaticMeshComponent* Component;
	class UWorld* World;

	FAsyncBuildInstanceBuffer(class UInstancedStaticMeshComponent* InComponent, class UWorld* InWorld)
		: Component(InComponent)
		, World(InWorld)
	{
	}
	void DoWork();
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncBuildInstanceBuffer, STATGROUP_ThreadPoolAsyncTasks);
	}
	static const TCHAR *Name()
	{
		return TEXT("FAsyncBuildInstanceBuffer");
	}
};

/** A component that efficiently renders multiple instances of the same StaticMesh. */
UCLASS(ClassGroup = Rendering, meta = (BlueprintSpawnableComponent), Blueprintable)
class ENGINE_API UInstancedStaticMeshComponent : public UStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

	virtual ~UInstancedStaticMeshComponent();

	/** Array of instances, bulk serialized. */
	UPROPERTY(EditAnywhere, SkipSerialization, DisplayName="Instances", Category=Instances, meta=(MakeEditWidget=true, EditFixedOrder))
	TArray<FInstancedStaticMeshInstanceData> PerInstanceSMData;

	/** Value used to seed the random number stream that generates random numbers for each of this mesh's instances.
		The random number is stored in a buffer accessible to materials through the PerInstanceRandom expression. If
		this is set to zero (default), it will be populated automatically by the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InstancedStaticMeshComponent)
	int32 InstancingRandomSeed;

	/** Distance from camera at which each instance begins to fade out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Culling)
	int32 InstanceStartCullDistance;

	/** Distance from camera at which each instance completely fades out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Culling)
	int32 InstanceEndCullDistance;

	/** Mapping from PerInstanceSMData order to instance render buffer order. If empty, the PerInstanceSMData order is used. */
	UPROPERTY()
	TArray<int32> InstanceReorderTable;

	// The render indices of any removed items we should not render.
	UPROPERTY()
	TArray<int32> RemovedInstances;

	/** Set to true to permit updating the vertex buffer used in the instance buffer without recreating it completely. This should be used if you plan on dynamically changing the instances at run-time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InstancedStaticMeshComponent)
	bool UseDynamicInstanceBuffer;

	/** Set to true to keep instance buffer accessible by the CPU, otherwise it's discarded and considered never changing, only GPU has a copy of the data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InstancedStaticMeshComponent)
	bool KeepInstanceBufferCPUAccess;

	/** Tracks outstanding proxysize, as this is a bit hard to do with the fire-and-forget grass. */
	SIZE_T ProxySize;

	// Temp hack, long term we will load data in the right format directly
	FAsyncTask<FAsyncBuildInstanceBuffer>* AsyncBuildInstanceBufferTask;

	/** Add an instance to this component. Transform is given in local space of this component. */
	UFUNCTION(BlueprintCallable, Category="Components|InstancedStaticMesh")
	virtual int32 AddInstance(const FTransform& InstanceTransform);

	/** Add an instance to this component. Transform is given in world space. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	int32 AddInstanceWorldSpace(const FTransform& WorldTransform);

	/** Get the transform for the instance specified. Instance is returned in local space of this component unless bWorldSpace is set.  Returns True on success. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	bool GetInstanceTransform(int32 InstanceIndex, FTransform& OutInstanceTransform, bool bWorldSpace = false) const;

	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;

	/** Get the scale comming form the component, when computing StreamingTexture data. Used to support instanced meshes. */
	virtual float GetTextureStreamingTransformScale() const override;
	/** Get material, UV density and bounds for a given material index. */
	virtual bool GetMaterialStreamingData(int32 MaterialIndex, FPrimitiveMaterialInfo& MaterialData) const override;
	/** Build the data to compute accuracte StreaminTexture data. */
	virtual bool BuildTextureStreamingData(ETextureStreamingBuildType BuildType, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TSet<FGuid>& DependentResources) override;
	/** Get the StreaminTexture data. */
	virtual void GetStreamingTextureInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const override;

	/**
	* Update the transform for the instance specified.
	*
	* @param InstanceIndex			The index of the instance to update
	* @param NewInstanceTransform	The new transform
	* @param bWorldSpace			If true, the new transform interpreted as a World Space transform, otherwise it is interpreted as Local Space
	* @param bMarkRenderStateDirty	If true, the change should be visible immediately. If you are updating many instances you should only set this to true for the last instance.
	* @param bTeleport				Whether or not the instance's physics should be moved normally, or teleported (moved instantly, ignoring velocity).
	* @return						True on success.
	*/
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual bool UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace=false, bool bMarkRenderStateDirty=false, bool bTeleport=false);

	/** Remove the instance specified. Returns True on success. Note that this will leave the array in order, but may shrink it. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual bool RemoveInstance(int32 InstanceIndex);
	
	/** Clear all instances being rendered by this component. */
	UFUNCTION(BlueprintCallable, Category="Components|InstancedStaticMesh")
	virtual void ClearInstances();
	
	/** Get the number of instances in this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	int32 GetInstanceCount() const;

	/** Sets the fading start and culling end distances for this component. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	void SetCullDistances(int32 StartCullDistance, int32 EndCullDistance);

	/** Returns the instances with instance bounds overlapping the specified sphere. The return value is an array of instance indices. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual TArray<int32> GetInstancesOverlappingSphere(const FVector& Center, float Radius, bool bSphereInWorldSpace=true) const;

	/** Returns the instances with instance bounds overlapping the specified box. The return value is an array of instance indices. */
	UFUNCTION(BlueprintCallable, Category = "Components|InstancedStaticMesh")
	virtual TArray<int32> GetInstancesOverlappingBox(const FBox& Box, bool bBoxInWorldSpace=true) const;

	virtual bool ShouldCreatePhysicsState() const override;

	virtual void PostLoad() override;
	virtual void OnComponentCreated() override;

public:
	/** Render data will be initialized on PostLoad or on demand. Released on the rendering thread. */
	TSharedPtr<FPerInstanceRenderData, ESPMode::ThreadSafe> PerInstanceRenderData;
	TSet<int32> NeedUpdatingInstanceIndexList;
	/** This was prebuilt, grass system use it, never destroy it. */
	bool bPerInstanceRenderDataWasPrebuilt;
		
#if WITH_EDITOR
	/** One bit per instance if the instance is selected. */
	TBitArray<> SelectedInstances;
#endif
	/** Physics representation of the instance bodies. */
	TArray<FBodyInstance*> InstanceBodies;

	/** Serialization of all the InstanceBodies. Helps speed up physics creation time. */
	UPROPERTY()
	class UPhysicsSerializer* PhysicsSerializer;

	//~ Begin UActorComponent Interface
	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;
	//~ End UActorComponent Interface

	//~ Begin UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
protected:
	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
public:
	virtual bool CanEditSimulatePhysics() override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& BoundTransform) const override;
	virtual bool SupportsStaticLighting() const override { return true; }
#if WITH_EDITOR
	virtual void GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options) override;
#endif
	virtual void GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const override;
	
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
	//~ End UPrimitiveComponent Interface
	
	//~ Begin UNavRelevantInterface Interface
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	//~ End UPrimitiveComponent Interface
	
	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	void BeginDestroy() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
	//~ End UObject Interface

	/** Applies the cached component instance data to a newly blueprint constructed component. */
	virtual void ApplyComponentInstanceData(class FInstancedStaticMeshComponentInstanceData* ComponentInstanceData);

	/** Check to see if an instance is selected. */
	bool IsInstanceSelected(int32 InInstanceIndex) const;

	/** Select/deselect an instance or group of instances. */
	void SelectInstance(bool bInSelected, int32 InInstanceIndex, int32 InInstanceCount = 1);

	/** Deselect all instances. */
	void ClearInstanceSelection();

	/** Initialize the Per Instance Render Data */
	void InitPerInstanceRenderData(bool InitializeFromCurrentData, FStaticMeshInstanceData* InSharedInstanceBufferData = nullptr);

	/** Transfers ownership of instance render data to a render thread. Instance render data will be released in scene proxy destructor or on render thread task. */
	void ReleasePerInstanceRenderData();

	virtual void PropagateLightingScenarioChange() override;

private:
	/** Creates body instances for all instances owned by this component. */
	void CreateAllInstanceBodies();

	/** Terminate all body instances owned by this component. */
	void ClearAllInstanceBodies();

	/** Sets up new instance data to sensible defaults, creates physics counterparts if possible. */
	void SetupNewInstanceData(FInstancedStaticMeshInstanceData& InOutNewInstanceData, int32 InInstanceIndex, const FTransform& InInstanceTransform);

protected:
	/** Request to navigation system to update only part of navmesh occupied by specified instance. */
	virtual void PartialNavigationUpdate(int32 InstanceIdx);

	/** Internal version of AddInstance */
	int32 AddInstanceInternal(int32 InstanceIndex, FInstancedStaticMeshInstanceData* InNewInstanceData, const FTransform& InstanceTransform);
	
	/** Internal version of RemoveInstance */	
	bool RemoveInstanceInternal(int32 InstanceIndex, bool ReorderInstances, bool InstanceAlreadyRemoved);
	
	/** Handles request from navigation system to gather instance transforms in a specific area box. */
	virtual void GetNavigationPerInstanceTransforms(const FBox& AreaBox, TArray<FTransform>& InstanceData) const;

	/** Initializes the body instance for the specified instance of the static mesh. */
	void InitInstanceBody(int32 InstanceIdx, FBodyInstance* InBodyInstance);

	/** Flush the asyc instance buffer task if we're running in async mode */
	void FlushAsyncBuildInstanceBufferTask();

	/** Number of pending lightmaps still to be calculated (Apply()'d). */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	int32 NumPendingLightmaps;

	/** The mappings for all the instances of this component. */
	UPROPERTY(Transient, DuplicateTransient, TextExportTransient)
	TArray<FInstancedStaticMeshMappingInfo> CachedMappings;

	void ApplyLightMapping(FStaticLightingTextureMapping_InstancedStaticMesh* InMapping, ULevel* LightingScenario);

	friend FStaticLightingTextureMapping_InstancedStaticMesh;
	friend FInstancedLightMap2D;
	friend FInstancedShadowMap2D;
};

/** InstancedStaticMeshInstance hit proxy */
struct HInstancedStaticMeshInstance : public HHitProxy
{
	UInstancedStaticMeshComponent* Component;
	int32 InstanceIndex;

	DECLARE_HIT_PROXY(ENGINE_API);
	HInstancedStaticMeshInstance(UInstancedStaticMeshComponent* InComponent, int32 InInstanceIndex) : HHitProxy(HPP_World), Component(InComponent), InstanceIndex(InInstanceIndex) {}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};
