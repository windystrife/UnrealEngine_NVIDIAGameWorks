// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
FlowGridSceneProxy.h:
=============================================================================*/

// NvFlow begin

/*=============================================================================
FFlowGridSceneProxy
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PrimitiveSceneProxy.h"

#include "GameWorks/GridInteractionNvFlow.h"

#define NV_FLOW_SHADER_UTILS 0
#include "NvFlow.h"

namespace NvFlow
{
	const float scale = 100.f;
	const float scaleInv = 1.0f / scale;
	const float sdfRadius = 0.8f;
	const float angularScale = PI / 180.f;
}

struct FFlowGridRenderParams
{
	// NvFlowVolumeRenderParams
	NvFlowVolumeRenderMode RenderMode;
	NvFlowGridTextureChannel RenderChannel;
	uint32 bAdaptiveScreenPercentage : 1;
	float AdaptiveTargetFrameTime;
	float MaxScreenPercentage;
	float MinScreenPercentage;
	uint32 bDebugWireframe : 1;
	uint32 bGenerateDepth : 1;
	float DepthAlphaThreshold;
	float DepthIntensityThreshold;

	uint32 bVolumeShadowEnabled : 1;
	float ShadowIntensityScale;
	float ShadowMinIntensity;
	NvFlowFloat4 ShadowBlendCompMask;
	float ShadowBlendBias;

	uint32 ShadowResolution;
	float ShadowFrustrumScale;
	float ShadowMinResidentScale;
	float ShadowMaxResidentScale;

	int32 ShadowChannel;
	float ShadowNearDistance;
};

typedef void* FlowMaterialKeyType;
typedef void* FlowRenderMaterialKeyType;

struct FFlowRenderMaterialParams : NvFlowRenderMaterialParams
{
	FlowRenderMaterialKeyType Key;
	// Color map
	TArray<FLinearColor> ColorMap;
};

struct FFlowMaterialParams
{
	NvFlowGridMaterialParams GridParams;

	TArray<FFlowRenderMaterialParams> RenderMaterials;
};

struct FFlowDistanceFieldParams
{
	const class UStaticMesh* StaticMesh;

	FIntVector Size;
	FVector2D DistanceMinMax;
	TArray<uint8> CompressedDistanceFieldVolume;
};

#define LOG_FLOW_GRID_PROPERTIES 0

struct FFlowGridPropertiesObject
{
	volatile int32 RefCount = 1;

#if LOG_FLOW_GRID_PROPERTIES
	static volatile int32 LogRefCount;
	static void LogCreate(FFlowGridProperties* Ptr);
	static void LogRelease(FFlowGridProperties* Ptr);
#endif

	int32 AddRef()
	{
		return FPlatformAtomics::InterlockedIncrement(&RefCount);
	}

	int32 Release()
	{
		int32 ref = FPlatformAtomics::InterlockedDecrement(&RefCount);
		if (ref == 0)
		{
			delete this;
		}
		return ref;
	}

#if LOG_FLOW_GRID_PROPERTIES
	FFlowGridPropertiesObject() { LogCreate(this); }
	~FFlowGridPropertiesObject() { LogRelease(this); }
#else
	FFlowGridPropertiesObject() {}
	~FFlowGridPropertiesObject() {}
#endif

	FFlowGridPropertiesObject(const FFlowGridPropertiesObject& rhs) = delete;
	FFlowGridPropertiesObject& operator=(const FFlowGridPropertiesObject& rhs) = delete;
};

struct FFlowGridProperties : public FFlowGridPropertiesObject
{
	uint64 Version = 0ul;

	uint32 NumScheduledSubsteps = 1u;

	// indicates if grid should be allocated
	int32 bActive : 1;

	// multi-GPU enable, requires reset if changed
	uint32 bMultiAdapterEnabled : 1;
	uint32 bAsyncComputeEnabled : 1;

	uint32 bParticlesInteractionEnabled : 1;
	TEnumAsByte<enum EInteractionChannelNvFlow> InteractionChannel;
	struct FInteractionResponseContainerNvFlow ResponseToInteractionChannels;

	uint32 bParticleModeEnabled : 1;

	float ParticleToGridAccelTimeConstant;
	float ParticleToGridDecelTimeConstant;
	float ParticleToGridThresholdMultiplier;
	float GridToParticleAccelTimeConstant;
	float GridToParticleDecelTimeConstant;
	float GridToParticleThresholdMultiplier;

	// target simulation time step
	float SubstepSize;

	// virtual extents
	FVector VirtualGridExtents;

	float GridCellSize;

	// simulation parameters
	NvFlowGridDesc GridDesc;
	NvFlowGridParams GridParams;
	TArray<NvFlowGridEmitParams> GridEmitParams;
	TArray<NvFlowGridEmitParams> GridCollideParams;
	TArray<NvFlowShapeDesc> GridEmitShapeDescs;
	TArray<NvFlowShapeDesc> GridCollideShapeDescs;


	uint32 bDistanceFieldCollisionEnabled : 1;
	float MinActiveDistance;
	float MaxActiveDistance;
	float VelocitySlipFactor;
	float VelocitySlipThickness;

	// rendering parameters
	int32 ColorMapResolution;
	FFlowGridRenderParams RenderParams;

	TArray<FlowMaterialKeyType> GridEmitMaterialKeys;
	FlowMaterialKeyType DefaultMaterialKey;

	TArray<TPair<FlowMaterialKeyType, FFlowMaterialParams> > Materials;

	TArray<FFlowDistanceFieldParams> NewDistanceFieldList;

	TArray<const class UStaticMesh*> DistanceFieldKeys;
};

struct FFlowGridPropertiesRef
{
	FFlowGridProperties* Ref = nullptr;
	void Initialize(FFlowGridProperties* InRef)
	{
		Ref = InRef;
		if (Ref)
		{
			Ref->AddRef();
		}
	}
	FFlowGridPropertiesRef(FFlowGridProperties* InRef)
	{
		Initialize(InRef);
	}
	FFlowGridPropertiesRef(const FFlowGridPropertiesRef& InRef)
	{
		Initialize(InRef.Ref);
	}
	FFlowGridPropertiesRef& operator=(const FFlowGridPropertiesRef& InRef)
	{
		Initialize(InRef.Ref);
		return *this;
	}
	~FFlowGridPropertiesRef()
	{
		if (Ref)
		{
			Ref->Release();
			Ref = nullptr;
		}
	}
};

class UFlowGridComponent;

class FFlowGridSceneProxy : public FPrimitiveSceneProxy
{
public:

	FFlowGridSceneProxy(UFlowGridComponent* Component);

	virtual ~FFlowGridSceneProxy();

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View) {}

	virtual void CreateRenderThreadResources() override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const;

	virtual uint32 GetMemoryFootprint(void) const { return(sizeof(*this) + GetAllocatedSize()); }

	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

	void SetDynamicData_RenderThread(FFlowGridProperties* InFlowGridProperties);

public:

	// resources managed by game thread
	FFlowGridProperties* FlowGridProperties;

	// resources managed in render thread
	void* scenePtr;
	void(*cleanupSceneFunc)(void*);
};

// NvFlow end
