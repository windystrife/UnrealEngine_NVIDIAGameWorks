// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldSurfaceCacheLighting.cpp
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldAmbientOcclusion.h"

float GAOMaxObjectBoundingRadius = 50000;
FAutoConsoleVariableRef CVarAOMaxObjectBoundingRadius(
	TEXT("r.AOMaxObjectBoundingRadius"),
	GAOMaxObjectBoundingRadius,
	TEXT("Objects larger than this will not contribute to AO calculations, to improve performance."),
	ECVF_RenderThreadSafe
	);

int32 GAOLogObjectBufferReallocation = 0;
FAutoConsoleVariableRef CVarAOLogObjectBufferReallocation(
	TEXT("r.AOLogObjectBufferReallocation"),
	GAOLogObjectBufferReallocation,
	TEXT(""),
	ECVF_RenderThreadSafe
	);

// Must match equivalent shader defines
int32 FDistanceFieldObjectBuffers::ObjectDataStride = 18;
int32 FDistanceFieldCulledObjectBuffers::ObjectDataStride = 16;
int32 FDistanceFieldCulledObjectBuffers::ObjectBoxBoundsStride = 5;

// In float4's.  Must match corresponding usf definition
int32 UploadObjectDataStride = 1 + FDistanceFieldObjectBuffers::ObjectDataStride;

class FDistanceFieldUploadDataResource : public FRenderResource
{
public:

	FCPUUpdatedBuffer UploadData;

	FDistanceFieldUploadDataResource()
	{
		// PS4 volatile only supports 8Mb, switch to volatile once that is fixed.
		UploadData.bVolatile = false;

		UploadData.Format = PF_A32B32G32R32F;
		UploadData.Stride = UploadObjectDataStride;
	}

	virtual void InitDynamicRHI()  override
	{
		UploadData.Initialize();
	}

	virtual void ReleaseDynamicRHI() override
	{
		UploadData.Release();
	}
};

TGlobalResource<FDistanceFieldUploadDataResource> GDistanceFieldUploadData;

class FDistanceFieldUploadIndicesResource : public FRenderResource
{
public:

	FCPUUpdatedBuffer UploadIndices;

	FDistanceFieldUploadIndicesResource()
	{
		// PS4 volatile only supports 8Mb, switch to volatile once that is fixed.
		UploadIndices.bVolatile = false;

		UploadIndices.Format = PF_R32_UINT;
		UploadIndices.Stride = 1;
	}

	virtual void InitDynamicRHI()  override
	{
		UploadIndices.Initialize();
	}

	virtual void ReleaseDynamicRHI() override
	{
		UploadIndices.Release();
	}
};

TGlobalResource<FDistanceFieldUploadIndicesResource> GDistanceFieldUploadIndices;

class FDistanceFieldRemoveIndicesResource : public FRenderResource
{
public:

	FCPUUpdatedBuffer RemoveIndices;

	FDistanceFieldRemoveIndicesResource()
	{
		RemoveIndices.Format = PF_R32G32B32A32_UINT;
		RemoveIndices.Stride = 1;
	}

	virtual void InitDynamicRHI()  override
	{
		RemoveIndices.Initialize();
	}

	virtual void ReleaseDynamicRHI() override
	{
		RemoveIndices.Release();
	}
};

TGlobalResource<FDistanceFieldRemoveIndicesResource> GDistanceFieldRemoveIndices;

const uint32 UpdateObjectsGroupSize = 64;

class FUploadObjectsToBufferCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FUploadObjectsToBufferCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATEOBJECTS_THREADGROUP_SIZE"), UpdateObjectsGroupSize);
	}

	FUploadObjectsToBufferCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		NumUploadOperations.Bind(Initializer.ParameterMap, TEXT("NumUploadOperations"));
		UploadOperationIndices.Bind(Initializer.ParameterMap, TEXT("UploadOperationIndices"));
		UploadOperationData.Bind(Initializer.ParameterMap, TEXT("UploadOperationData"));
		ObjectBufferParameters.Bind(Initializer.ParameterMap);
	}

	FUploadObjectsToBufferCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FScene* Scene, uint32 NumUploadOperationsValue, FShaderResourceViewRHIParamRef InUploadOperationIndices, FShaderResourceViewRHIParamRef InUploadOperationData)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetShaderValue(RHICmdList, ShaderRHI, NumUploadOperations, NumUploadOperationsValue);

		SetSRVParameter(RHICmdList, ShaderRHI, UploadOperationIndices, InUploadOperationIndices);
		SetSRVParameter(RHICmdList, ShaderRHI, UploadOperationData, InUploadOperationData);


		ObjectBufferParameters.Set(RHICmdList, ShaderRHI, *(Scene->DistanceFieldSceneData.ObjectBuffers), Scene->DistanceFieldSceneData.NumObjectsInBuffer, true);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FScene* Scene)
	{
		ObjectBufferParameters.UnsetParameters(RHICmdList, GetComputeShader(), *(Scene->DistanceFieldSceneData.ObjectBuffers), true);

		const FDistanceFieldObjectBuffers& ObjectBuffers = *(Scene->DistanceFieldSceneData.ObjectBuffers);
		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = ObjectBuffers.Bounds.UAV;
		OutUAVs[1] = ObjectBuffers.Data.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << NumUploadOperations;
		Ar << UploadOperationIndices;
		Ar << UploadOperationData;
		Ar << ObjectBufferParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter NumUploadOperations;
	FShaderResourceParameter UploadOperationIndices;
	FShaderResourceParameter UploadOperationData;
	FDistanceFieldObjectBufferParameters ObjectBufferParameters;
};

IMPLEMENT_SHADER_TYPE(,FUploadObjectsToBufferCS,TEXT("/Engine/Private/DistanceFieldObjectCulling.usf"),TEXT("UploadObjectsToBufferCS"),SF_Compute);

class FCopyObjectBufferCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopyObjectBufferCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATEOBJECTS_THREADGROUP_SIZE"), UpdateObjectsGroupSize);
	}

	FCopyObjectBufferCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		CopyObjectBounds.Bind(Initializer.ParameterMap, TEXT("CopyObjectBounds"));
		CopyObjectData.Bind(Initializer.ParameterMap, TEXT("CopyObjectData"));
		ObjectBufferParameters.Bind(Initializer.ParameterMap);
	}

	FCopyObjectBufferCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, FDistanceFieldObjectBuffers& ObjectBuffersSource, FDistanceFieldObjectBuffers& ObjectBuffersDest, int32 NumObjectsValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = ObjectBuffersDest.Bounds.UAV;
		OutUAVs[1] = ObjectBuffersDest.Data.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		CopyObjectBounds.SetBuffer(RHICmdList, ShaderRHI, ObjectBuffersDest.Bounds);
		CopyObjectData.SetBuffer(RHICmdList, ShaderRHI, ObjectBuffersDest.Data);

		ObjectBufferParameters.Set(RHICmdList, ShaderRHI, ObjectBuffersSource, NumObjectsValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FDistanceFieldObjectBuffers& ObjectBuffersDest)
	{
		ObjectBufferParameters.UnsetParameters(RHICmdList, GetComputeShader(), ObjectBuffersDest);
		CopyObjectBounds.UnsetUAV(RHICmdList, GetComputeShader());
		CopyObjectData.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = ObjectBuffersDest.Bounds.UAV;
		OutUAVs[1] = ObjectBuffersDest.Data.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CopyObjectBounds;
		Ar << CopyObjectData;
		Ar << ObjectBufferParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter CopyObjectBounds;
	FRWShaderParameter CopyObjectData;
	FDistanceFieldObjectBufferParameters ObjectBufferParameters;
};

IMPLEMENT_SHADER_TYPE(,FCopyObjectBufferCS,TEXT("/Engine/Private/DistanceFieldObjectCulling.usf"),TEXT("CopyObjectBufferCS"),SF_Compute);

class FCopySurfelBufferCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopySurfelBufferCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATEOBJECTS_THREADGROUP_SIZE"), UpdateObjectsGroupSize);
	}

	FCopySurfelBufferCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		CopyInterpolatedVertexData.Bind(Initializer.ParameterMap, TEXT("CopyInterpolatedVertexData"));
		CopySurfelData.Bind(Initializer.ParameterMap, TEXT("CopySurfelData"));
		SurfelBufferParameters.Bind(Initializer.ParameterMap);
		NumSurfels.Bind(Initializer.ParameterMap, TEXT("NumSurfels"));
	}

	FCopySurfelBufferCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSurfelBuffers& SurfelBuffersSource, const FInstancedSurfelBuffers& InstancedSurfelBuffersSource, FSurfelBuffers& SurfelBuffersDest, int32 NumSurfelsValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = SurfelBuffersDest.InterpolatedVertexData.UAV;
		OutUAVs[1] = SurfelBuffersDest.Surfels.UAV;		
		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));

		CopyInterpolatedVertexData.SetBuffer(RHICmdList, ShaderRHI, SurfelBuffersDest.InterpolatedVertexData);
		CopySurfelData.SetBuffer(RHICmdList, ShaderRHI, SurfelBuffersDest.Surfels);
		SurfelBufferParameters.Set(RHICmdList, ShaderRHI, SurfelBuffersSource, InstancedSurfelBuffersSource);
		SetShaderValue(RHICmdList, ShaderRHI, NumSurfels, NumSurfelsValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSurfelBuffers& SurfelBuffersDest)
	{
		SurfelBufferParameters.UnsetParameters(RHICmdList, GetComputeShader());
		CopyInterpolatedVertexData.UnsetUAV(RHICmdList, GetComputeShader());
		CopySurfelData.UnsetUAV(RHICmdList, GetComputeShader());

		FUnorderedAccessViewRHIParamRef OutUAVs[2];
		OutUAVs[0] = SurfelBuffersDest.InterpolatedVertexData.UAV;
		OutUAVs[1] = SurfelBuffersDest.Surfels.UAV;
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CopyInterpolatedVertexData;
		Ar << CopySurfelData;
		Ar << SurfelBufferParameters;
		Ar << NumSurfels;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter CopyInterpolatedVertexData;
	FRWShaderParameter CopySurfelData;
	FSurfelBufferParameters SurfelBufferParameters;
	FShaderParameter NumSurfels;
};

IMPLEMENT_SHADER_TYPE(,FCopySurfelBufferCS,TEXT("/Engine/Private/SurfelTree.usf"),TEXT("CopySurfelBufferCS"),SF_Compute);


class FCopyVPLFluxBufferCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopyVPLFluxBufferCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATEOBJECTS_THREADGROUP_SIZE"), UpdateObjectsGroupSize);
	}

	FCopyVPLFluxBufferCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		CopyVPLFlux.Bind(Initializer.ParameterMap, TEXT("CopyVPLFlux"));
		SurfelBufferParameters.Bind(Initializer.ParameterMap);
		NumSurfels.Bind(Initializer.ParameterMap, TEXT("NumSurfels"));
	}

	FCopyVPLFluxBufferCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSurfelBuffers& SurfelBuffersSource, const FInstancedSurfelBuffers& InstancedSurfelBuffersSource, FInstancedSurfelBuffers& InstancedSurfelBuffersDest, int32 NumSurfelsValue)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, InstancedSurfelBuffersDest.VPLFlux.UAV);
		CopyVPLFlux.SetBuffer(RHICmdList, ShaderRHI, InstancedSurfelBuffersDest.VPLFlux);
		SurfelBufferParameters.Set(RHICmdList, ShaderRHI, SurfelBuffersSource, InstancedSurfelBuffersSource);
		SetShaderValue(RHICmdList, ShaderRHI, NumSurfels, NumSurfelsValue);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FInstancedSurfelBuffers& InstancedSurfelBuffersDest)
	{
		SurfelBufferParameters.UnsetParameters(RHICmdList, GetComputeShader());
		CopyVPLFlux.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, InstancedSurfelBuffersDest.VPLFlux.UAV);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CopyVPLFlux;
		Ar << SurfelBufferParameters;
		Ar << NumSurfels;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter CopyVPLFlux;
	FSurfelBufferParameters SurfelBufferParameters;
	FShaderParameter NumSurfels;
};

IMPLEMENT_SHADER_TYPE(,FCopyVPLFluxBufferCS,TEXT("/Engine/Private/SurfelTree.usf"),TEXT("CopyVPLFluxBufferCS"),SF_Compute);

template<bool bRemoveFromSameBuffer>
class TRemoveObjectsFromBufferCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TRemoveObjectsFromBufferCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATEOBJECTS_THREADGROUP_SIZE"), UpdateObjectsGroupSize);
		OutEnvironment.SetDefine(TEXT("REMOVE_FROM_SAME_BUFFER"), bRemoveFromSameBuffer);
	}

	TRemoveObjectsFromBufferCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		NumRemoveOperations.Bind(Initializer.ParameterMap, TEXT("NumRemoveOperations"));
		RemoveOperationIndices.Bind(Initializer.ParameterMap, TEXT("RemoveOperationIndices"));
		ObjectBufferParameters.Bind(Initializer.ParameterMap);
		ObjectBounds2.Bind(Initializer.ParameterMap, TEXT("ObjectBounds2"));
		ObjectData2.Bind(Initializer.ParameterMap, TEXT("ObjectData2"));
	}

	TRemoveObjectsFromBufferCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FScene* Scene, 
		uint32 NumRemoveOperationsValue, 
		FShaderResourceViewRHIParamRef InRemoveOperationIndices, 
		FShaderResourceViewRHIParamRef InObjectBounds2, 
		FShaderResourceViewRHIParamRef InObjectData2)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetShaderValue(RHICmdList, ShaderRHI, NumRemoveOperations, NumRemoveOperationsValue);
		SetSRVParameter(RHICmdList, ShaderRHI, RemoveOperationIndices, InRemoveOperationIndices);
		ObjectBufferParameters.Set(RHICmdList, ShaderRHI, *(Scene->DistanceFieldSceneData.ObjectBuffers), Scene->DistanceFieldSceneData.NumObjectsInBuffer, true);
		SetSRVParameter(RHICmdList, ShaderRHI, ObjectBounds2, InObjectBounds2);
		SetSRVParameter(RHICmdList, ShaderRHI, ObjectData2, InObjectData2);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FScene* Scene)
	{
		ObjectBufferParameters.UnsetParameters(RHICmdList, GetComputeShader(), *(Scene->DistanceFieldSceneData.ObjectBuffers), true);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << NumRemoveOperations;
		Ar << RemoveOperationIndices;
		Ar << ObjectBufferParameters;
		Ar << ObjectBounds2;
		Ar << ObjectData2;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter NumRemoveOperations;
	FShaderResourceParameter RemoveOperationIndices;
	FDistanceFieldObjectBufferParameters ObjectBufferParameters;
	FShaderResourceParameter ObjectBounds2;
	FShaderResourceParameter ObjectData2;
};

IMPLEMENT_SHADER_TYPE(template<>,TRemoveObjectsFromBufferCS<true>,TEXT("/Engine/Private/DistanceFieldObjectCulling.usf"),TEXT("RemoveObjectsFromBufferCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TRemoveObjectsFromBufferCS<false>,TEXT("/Engine/Private/DistanceFieldObjectCulling.usf"),TEXT("RemoveObjectsFromBufferCS"),SF_Compute);

void FSurfelBufferAllocator::RemovePrimitive(const FPrimitiveSceneInfo* Primitive)
{
	FPrimitiveSurfelAllocation Allocation;

	if (Allocations.RemoveAndCopyValue(Primitive, Allocation))
	{
		bool bMergedWithExisting = false;

		FPrimitiveSurfelFreeEntry FreeEntry(Allocation.Offset, Allocation.GetTotalNumSurfels());

		// Note: only does one merge
		//@todo - keep free list sorted then can binary search
		for (int32 FreeIndex = 0; FreeIndex < FreeList.Num(); FreeIndex++)
		{
			if (FreeList[FreeIndex].Offset == FreeEntry.Offset + FreeEntry.NumSurfels)
			{
				FreeList[FreeIndex].Offset = FreeEntry.Offset;
				FreeList[FreeIndex].NumSurfels += FreeEntry.NumSurfels;
				bMergedWithExisting = true;
				break;
			}
			else if (FreeList[FreeIndex].Offset + FreeList[FreeIndex].NumSurfels == FreeEntry.Offset)
			{
				FreeList[FreeIndex].NumSurfels += FreeEntry.NumSurfels;
				bMergedWithExisting = true;
				break;
			}
		}

		if (!bMergedWithExisting)
		{
			FreeList.Add(FreeEntry);
		}
	}
}

void FSurfelBufferAllocator::AddPrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo, int32 PrimitiveLOD0Surfels, int32 PrimitiveNumSurfels, int32 NumInstances)
{
	int32 BestFreeAllocationIndex = -1;

	for (int32 FreeIndex = 0; FreeIndex < FreeList.Num(); FreeIndex++)
	{
		const FPrimitiveSurfelFreeEntry& CurrentFreeEntry = FreeList[FreeIndex];

		if (CurrentFreeEntry.NumSurfels >= PrimitiveNumSurfels * NumInstances
			&& (BestFreeAllocationIndex == -1 
				|| CurrentFreeEntry.NumSurfels < FreeList[BestFreeAllocationIndex].NumSurfels))
		{
			BestFreeAllocationIndex = FreeIndex;
		}
	}

	if (BestFreeAllocationIndex != -1)
	{
		FPrimitiveSurfelFreeEntry FreeEntry = FreeList[BestFreeAllocationIndex];
						
		if (FreeEntry.NumSurfels == PrimitiveNumSurfels * NumInstances)
		{
			// Existing allocation matches exactly, remove it from the free list
			FreeList.RemoveAtSwap(BestFreeAllocationIndex);
		}
		else
		{
			// Replace with the remaining free range
			FreeList[BestFreeAllocationIndex] = FPrimitiveSurfelFreeEntry(FreeEntry.Offset + PrimitiveNumSurfels * NumInstances, FreeEntry.NumSurfels - PrimitiveNumSurfels * NumInstances);
		}

		Allocations.Add(PrimitiveSceneInfo, FPrimitiveSurfelAllocation(FreeEntry.Offset, PrimitiveLOD0Surfels, PrimitiveNumSurfels, NumInstances));
	}
	else
	{
		// Add a new allocation to the end of the buffer
		Allocations.Add(PrimitiveSceneInfo, FPrimitiveSurfelAllocation(NumSurfelsInBuffer, PrimitiveLOD0Surfels, PrimitiveNumSurfels, NumInstances));
		NumSurfelsInBuffer += PrimitiveNumSurfels * NumInstances;
	}
}

void UpdateGlobalDistanceFieldObjectRemoves(FRHICommandListImmediate& RHICmdList, FScene* Scene)
{
	FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;

	TArray<FIntRect> RemoveObjectIndices;
	FDistanceFieldObjectBuffers* TemporaryCopySourceBuffers = NULL;

	if (DistanceFieldSceneData.PendingRemoveOperations.Num() > 0)
	{
		TArray<int32, SceneRenderingAllocator> PendingRemoveOperations;

		for (int32 RemoveIndex = 0; RemoveIndex < DistanceFieldSceneData.PendingRemoveOperations.Num(); RemoveIndex++)
		{
			// Can't dereference the primitive here, it has already been deleted
			const FPrimitiveSceneInfo* Primitive = DistanceFieldSceneData.PendingRemoveOperations[RemoveIndex].Primitive;
			DistanceFieldSceneData.SurfelAllocations.RemovePrimitive(Primitive);
			DistanceFieldSceneData.InstancedSurfelAllocations.RemovePrimitive(Primitive);
			const TArray<int32, TInlineAllocator<1>>& DistanceFieldInstanceIndices = DistanceFieldSceneData.PendingRemoveOperations[RemoveIndex].DistanceFieldInstanceIndices;

			for (int32 RemoveInstanceIndex = 0; RemoveInstanceIndex < DistanceFieldInstanceIndices.Num(); RemoveInstanceIndex++)
			{
				const int32 InstanceIndex = DistanceFieldInstanceIndices[RemoveInstanceIndex];

				// InstanceIndex will be -1 with zero scale meshes
				if (InstanceIndex >= 0)
				{
					FGlobalDFCacheType CacheType = DistanceFieldSceneData.PendingRemoveOperations[RemoveIndex].bOftenMoving ? GDF_Full : GDF_MostlyStatic;
					DistanceFieldSceneData.PrimitiveModifiedBounds[CacheType].Add(DistanceFieldSceneData.PrimitiveInstanceMapping[InstanceIndex].BoundingSphere);
					PendingRemoveOperations.Add(InstanceIndex);
				}
			}
		}

		DistanceFieldSceneData.PendingRemoveOperations.Reset();

		if (PendingRemoveOperations.Num() > 0)
		{
			check(DistanceFieldSceneData.NumObjectsInBuffer >= PendingRemoveOperations.Num());

			// Sort from smallest to largest
			PendingRemoveOperations.Sort();

			// We have multiple remove requests enqueued in PendingRemoveOperations, can only use the RemoveAtSwap version when there won't be collisions
			const bool bUseRemoveAtSwap = PendingRemoveOperations.Last() < DistanceFieldSceneData.NumObjectsInBuffer - PendingRemoveOperations.Num();

			if (bUseRemoveAtSwap)
			{
				// Remove everything in parallel in the same buffer with a RemoveAtSwap algorithm
				for (int32 RemovePrimitiveIndex = 0; RemovePrimitiveIndex < PendingRemoveOperations.Num(); RemovePrimitiveIndex++)
				{
					DistanceFieldSceneData.NumObjectsInBuffer--;
					const int32 RemoveIndex = PendingRemoveOperations[RemovePrimitiveIndex];
					const int32 MoveFromIndex = DistanceFieldSceneData.NumObjectsInBuffer;

					check(RemoveIndex != MoveFromIndex);
					// Queue a compute shader move
					RemoveObjectIndices.Add(FIntRect(RemoveIndex, MoveFromIndex, 0, 0));

					// Fixup indices of the primitive that is being moved
					FPrimitiveAndInstance& PrimitiveAndInstanceBeingMoved = DistanceFieldSceneData.PrimitiveInstanceMapping[MoveFromIndex];
					check(PrimitiveAndInstanceBeingMoved.Primitive && PrimitiveAndInstanceBeingMoved.Primitive->DistanceFieldInstanceIndices.Num() > 0);
					PrimitiveAndInstanceBeingMoved.Primitive->DistanceFieldInstanceIndices[PrimitiveAndInstanceBeingMoved.InstanceIndex] = RemoveIndex;

					DistanceFieldSceneData.PrimitiveInstanceMapping.RemoveAtSwap(RemoveIndex);
				}
			}
			else
			{
				const double StartTime = FPlatformTime::Seconds();

				// Have to copy the object data to allow parallel removing
				TemporaryCopySourceBuffers = DistanceFieldSceneData.ObjectBuffers;
				DistanceFieldSceneData.ObjectBuffers = new FDistanceFieldObjectBuffers();
				DistanceFieldSceneData.ObjectBuffers->MaxObjects = TemporaryCopySourceBuffers->MaxObjects;
				DistanceFieldSceneData.ObjectBuffers->Initialize();

				TArray<FPrimitiveAndInstance> OriginalPrimitiveInstanceMapping = DistanceFieldSceneData.PrimitiveInstanceMapping;
				DistanceFieldSceneData.PrimitiveInstanceMapping.Reset();

				const int32 NumDestObjects = DistanceFieldSceneData.NumObjectsInBuffer - PendingRemoveOperations.Num();
				int32 SourceIndex = 0;
				int32 NextPendingRemoveIndex = 0;

				for (int32 DestinationIndex = 0; DestinationIndex < NumDestObjects; DestinationIndex++)
				{
					while (NextPendingRemoveIndex < PendingRemoveOperations.Num()
						&& PendingRemoveOperations[NextPendingRemoveIndex] == SourceIndex)
					{
						NextPendingRemoveIndex++;
						SourceIndex++;
					}

					// Queue a compute shader move
					RemoveObjectIndices.Add(FIntRect(DestinationIndex, SourceIndex, 0, 0));

					// Fixup indices of the primitive that is being moved
					FPrimitiveAndInstance& PrimitiveAndInstanceBeingMoved = OriginalPrimitiveInstanceMapping[SourceIndex];
					check(PrimitiveAndInstanceBeingMoved.Primitive && PrimitiveAndInstanceBeingMoved.Primitive->DistanceFieldInstanceIndices.Num() > 0);
					PrimitiveAndInstanceBeingMoved.Primitive->DistanceFieldInstanceIndices[PrimitiveAndInstanceBeingMoved.InstanceIndex] = DestinationIndex;

					check(DistanceFieldSceneData.PrimitiveInstanceMapping.Num() == DestinationIndex);
					DistanceFieldSceneData.PrimitiveInstanceMapping.Add(PrimitiveAndInstanceBeingMoved);

					SourceIndex++;
				}

				DistanceFieldSceneData.NumObjectsInBuffer = NumDestObjects;

				if (GAOLogObjectBufferReallocation)
				{
					const float ElapsedTime = (float)(FPlatformTime::Seconds() - StartTime);
					UE_LOG(LogDistanceField,Warning,TEXT("Global object buffer realloc %.3fs"), ElapsedTime);
				}

				/*
				// Have to remove one at a time while any entries to remove are at the end of the buffer
				DistanceFieldSceneData.NumObjectsInBuffer--;
				const int32 RemoveIndex = DistanceFieldSceneData.PendingRemoveOperations[ParallelConflictIndex];
				const int32 MoveFromIndex = DistanceFieldSceneData.NumObjectsInBuffer;

				if (RemoveIndex != MoveFromIndex)
				{
					// Queue a compute shader move
					RemoveObjectIndices.Add(FIntRect(RemoveIndex, MoveFromIndex, 0, 0));

					// Fixup indices of the primitive that is being moved
					FPrimitiveAndInstance& PrimitiveAndInstanceBeingMoved = DistanceFieldSceneData.PrimitiveInstanceMapping[MoveFromIndex];
					check(PrimitiveAndInstanceBeingMoved.Primitive && PrimitiveAndInstanceBeingMoved.Primitive->DistanceFieldInstanceIndices.Num() > 0);
					PrimitiveAndInstanceBeingMoved.Primitive->DistanceFieldInstanceIndices[PrimitiveAndInstanceBeingMoved.InstanceIndex] = RemoveIndex;
				}

				DistanceFieldSceneData.PrimitiveInstanceMapping.RemoveAtSwap(RemoveIndex);
				DistanceFieldSceneData.PendingRemoveOperations.RemoveAtSwap(ParallelConflictIndex);
				*/
			}

			PendingRemoveOperations.Reset();

			if (RemoveObjectIndices.Num() > 0)
			{
				if (RemoveObjectIndices.Num() > GDistanceFieldRemoveIndices.RemoveIndices.MaxElements)
				{
					GDistanceFieldRemoveIndices.RemoveIndices.MaxElements = RemoveObjectIndices.Num() * 5 / 4;
					GDistanceFieldRemoveIndices.RemoveIndices.Release();
					GDistanceFieldRemoveIndices.RemoveIndices.Initialize();
				}

				void* LockedBuffer = RHILockVertexBuffer(GDistanceFieldRemoveIndices.RemoveIndices.Buffer, 0, GDistanceFieldRemoveIndices.RemoveIndices.Buffer->GetSize(), RLM_WriteOnly);
				const uint32 MemcpySize = RemoveObjectIndices.GetTypeSize() * RemoveObjectIndices.Num();
				check(GDistanceFieldRemoveIndices.RemoveIndices.Buffer->GetSize() >= MemcpySize);
				FPlatformMemory::Memcpy(LockedBuffer, RemoveObjectIndices.GetData(), MemcpySize);
				RHIUnlockVertexBuffer(GDistanceFieldRemoveIndices.RemoveIndices.Buffer);

				if (bUseRemoveAtSwap)
				{
					check(!TemporaryCopySourceBuffers);
					TShaderMapRef<TRemoveObjectsFromBufferCS<true> > ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, Scene, RemoveObjectIndices.Num(), GDistanceFieldRemoveIndices.RemoveIndices.BufferSRV, NULL, NULL);

					DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(RemoveObjectIndices.Num(), UpdateObjectsGroupSize), 1, 1);
					ComputeShader->UnsetParameters(RHICmdList, Scene);
				}
				else
				{
					check(TemporaryCopySourceBuffers);
					TShaderMapRef<TRemoveObjectsFromBufferCS<false> > ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, Scene, RemoveObjectIndices.Num(), GDistanceFieldRemoveIndices.RemoveIndices.BufferSRV, TemporaryCopySourceBuffers->Bounds.SRV, TemporaryCopySourceBuffers->Data.SRV);

					DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(RemoveObjectIndices.Num(), UpdateObjectsGroupSize), 1, 1);
					ComputeShader->UnsetParameters(RHICmdList, Scene);
				}
			}
			
			// make sure to delete the temporary buffer (even if RemoveObjectIndices is empty)
			if (TemporaryCopySourceBuffers)
			{
				check(bUseRemoveAtSwap == false);
				TemporaryCopySourceBuffers->Release();
				delete TemporaryCopySourceBuffers;
				TemporaryCopySourceBuffers = nullptr;
			}
		}
	}
}

/** Gathers the information needed to represent a single object's distance field and appends it to the upload buffers. */
void ProcessPrimitiveUpdate(
	bool bIsAddOperation,
	FRHICommandListImmediate& RHICmdList, 
	FSceneRenderer& SceneRenderer, 
	FPrimitiveSceneInfo* PrimitiveSceneInfo, 
	int32 OriginalNumObjects,
	FVector InvTextureDim,
	bool bPrepareForDistanceFieldGI, 
	TArray<FMatrix>& ObjectLocalToWorldTransforms,
	TArray<uint32>& UploadObjectIndices,
	TArray<FVector4>& UploadObjectData)
{
	FScene* Scene = SceneRenderer.Scene;
	FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;

	ObjectLocalToWorldTransforms.Reset();

	FBox LocalVolumeBounds;
	FVector2D DistanceMinMax;
	FIntVector BlockMin;
	FIntVector BlockSize;
	bool bBuiltAsIfTwoSided;
	bool bMeshWasPlane;
	float SelfShadowBias;
	PrimitiveSceneInfo->Proxy->GetDistancefieldAtlasData(LocalVolumeBounds, DistanceMinMax, BlockMin, BlockSize, bBuiltAsIfTwoSided, bMeshWasPlane, SelfShadowBias, ObjectLocalToWorldTransforms);

	if (BlockMin.X >= 0 
		&& BlockMin.Y >= 0 
		&& BlockMin.Z >= 0 
		&& ObjectLocalToWorldTransforms.Num() > 0)
	{
		const float BoundingRadius = PrimitiveSceneInfo->Proxy->GetBounds().SphereRadius;
		const FGlobalDFCacheType CacheType = PrimitiveSceneInfo->Proxy->IsOftenMoving() ? GDF_Full : GDF_MostlyStatic;

		// Proxy bounds are only useful if single instance
		if (ObjectLocalToWorldTransforms.Num() > 1 || BoundingRadius < GAOMaxObjectBoundingRadius)
		{
			FPrimitiveSurfelAllocation Allocation;
			FPrimitiveSurfelAllocation InstancedAllocation;
						
			if (bPrepareForDistanceFieldGI)
			{
				const FPrimitiveSurfelAllocation* AllocationPtr = Scene->DistanceFieldSceneData.SurfelAllocations.FindAllocation(PrimitiveSceneInfo);
				const FPrimitiveSurfelAllocation* InstancedAllocationPtr = Scene->DistanceFieldSceneData.InstancedSurfelAllocations.FindAllocation(PrimitiveSceneInfo);

				if (AllocationPtr)
				{
					checkSlow(InstancedAllocationPtr && InstancedAllocationPtr->NumInstances == ObjectLocalToWorldTransforms.Num());
					Allocation = *AllocationPtr;
					InstancedAllocation = *InstancedAllocationPtr;

					extern void GenerateSurfelRepresentation(FRHICommandListImmediate& RHICmdList, FSceneRenderer& Renderer, FViewInfo& View, FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMatrix& Instance0Transform, FPrimitiveSurfelAllocation& Allocation);
					// @todo - support surfel generation without a view
					GenerateSurfelRepresentation(RHICmdList, SceneRenderer, SceneRenderer.Views[0], PrimitiveSceneInfo, ObjectLocalToWorldTransforms[0], Allocation);

					if (Allocation.NumSurfels == 0)
					{
						InstancedAllocation.NumSurfels = 0;
						InstancedAllocation.NumInstances = 0;
						InstancedAllocation.NumLOD0 = 0;
					}
				}
			}

			if (bIsAddOperation)
			{
				PrimitiveSceneInfo->DistanceFieldInstanceIndices.Empty(ObjectLocalToWorldTransforms.Num());
				PrimitiveSceneInfo->DistanceFieldInstanceIndices.AddZeroed(ObjectLocalToWorldTransforms.Num());
			}

			for (int32 TransformIndex = 0; TransformIndex < ObjectLocalToWorldTransforms.Num(); TransformIndex++)
			{
				FMatrix LocalToWorld = ObjectLocalToWorldTransforms[TransformIndex];
				const float MaxScale = LocalToWorld.GetMaximumAxisScale();

				// Skip degenerate primitives
				if (MaxScale > 0)
				{
					uint32 UploadIndex;

					if (bIsAddOperation)
					{
						UploadIndex = OriginalNumObjects + UploadObjectIndices.Num();
						DistanceFieldSceneData.NumObjectsInBuffer++;
					}
					else
					{
						UploadIndex = PrimitiveSceneInfo->DistanceFieldInstanceIndices[TransformIndex];
					}

					UploadObjectIndices.Add(UploadIndex);

					if (bMeshWasPlane)
					{
						FVector LocalScales = LocalToWorld.GetScaleVector();
						FVector AbsLocalScales(FMath::Abs(LocalScales.X), FMath::Abs(LocalScales.Y), FMath::Abs(LocalScales.Z));
						float MidScale = FMath::Min(AbsLocalScales.X, AbsLocalScales.Y);
						float ScaleAdjust = FMath::Sign(LocalScales.Z) * MidScale / AbsLocalScales.Z;
						// The mesh was determined to be a plane flat in Z during the build process, so we can change the Z scale
						// Helps in cases with modular ground pieces with scales of (10, 10, 1) and some triangles just above Z=0
						LocalToWorld.SetAxis(2, LocalToWorld.GetScaledAxis(EAxis::Z) * ScaleAdjust);
					}

					const FMatrix VolumeToWorld = FScaleMatrix(LocalVolumeBounds.GetExtent()) 
						* FTranslationMatrix(LocalVolumeBounds.GetCenter())
						* LocalToWorld;

					const FVector4 ObjectBoundingSphere(VolumeToWorld.GetOrigin(), VolumeToWorld.GetScaleVector().Size());

					UploadObjectData.Add(ObjectBoundingSphere);

					const float MaxExtent = LocalVolumeBounds.GetExtent().GetMax();

					const FMatrix UniformScaleVolumeToWorld = FScaleMatrix(MaxExtent) 
						* FTranslationMatrix(LocalVolumeBounds.GetCenter())
						* LocalToWorld;

					const FVector InvBlockSize(1.0f / BlockSize.X, 1.0f / BlockSize.Y, 1.0f / BlockSize.Z);

					//float3 VolumeUV = (VolumePosition / LocalPositionExtent * .5f * UVScale + .5f * UVScale + UVAdd;
					const FVector LocalPositionExtent = LocalVolumeBounds.GetExtent() / FVector(MaxExtent);
					const FVector UVScale = FVector(BlockSize) * InvTextureDim;
					const float VolumeScale = UniformScaleVolumeToWorld.GetMaximumAxisScale();

					const FMatrix WorldToVolume = UniformScaleVolumeToWorld.Inverse();
					// WorldToVolume
					UploadObjectData.Add(*(FVector4*)&WorldToVolume.M[0]);
					UploadObjectData.Add(*(FVector4*)&WorldToVolume.M[1]);
					UploadObjectData.Add(*(FVector4*)&WorldToVolume.M[2]);
					UploadObjectData.Add(*(FVector4*)&WorldToVolume.M[3]);

					// Clamp to texel center by subtracting a half texel in the [-1,1] position space
					// LocalPositionExtent
					UploadObjectData.Add(FVector4(LocalPositionExtent - InvBlockSize, 0));

					// UVScale, VolumeScale and sign gives bGeneratedAsTwoSided
					const float WSign = bBuiltAsIfTwoSided ? -1 : 1;
					UploadObjectData.Add(FVector4(FVector(BlockSize) * InvTextureDim * .5f / LocalPositionExtent, WSign * VolumeScale));

					// UVAdd
					UploadObjectData.Add(FVector4(FVector(BlockMin) * InvTextureDim + .5f * UVScale, SelfShadowBias));

					// DistanceFieldMAD
					// [0, 1] -> [MinVolumeDistance, MaxVolumeDistance]
					UploadObjectData.Add(FVector4(DistanceMinMax.Y - DistanceMinMax.X, DistanceMinMax.X, 0, 0));

					UploadObjectData.Add(*(FVector4*)&UniformScaleVolumeToWorld.M[0]);
					UploadObjectData.Add(*(FVector4*)&UniformScaleVolumeToWorld.M[1]);
					UploadObjectData.Add(*(FVector4*)&UniformScaleVolumeToWorld.M[2]);

					UploadObjectData.Add(*(FVector4*)&LocalToWorld.M[0]);
					UploadObjectData.Add(*(FVector4*)&LocalToWorld.M[1]);
					UploadObjectData.Add(*(FVector4*)&LocalToWorld.M[2]);
					UploadObjectData.Add(*(FVector4*)&LocalToWorld.M[3]);

					UploadObjectData.Add(FVector4(Allocation.Offset, Allocation.NumLOD0, Allocation.NumSurfels, InstancedAllocation.Offset + InstancedAllocation.NumSurfels * TransformIndex));

					UploadObjectData.Add(FVector4(LocalVolumeBounds.Min, 0));

					// Box bounds
					const float OftenMovingWSign = CacheType == GDF_Full ? 1.0f : -1.0f;
					UploadObjectData.Add(FVector4(LocalVolumeBounds.Max, OftenMovingWSign));

					checkSlow(UploadObjectData.Num() % UploadObjectDataStride == 0);

					if (bIsAddOperation)
					{
						const int32 AddIndex = UploadIndex;
						DistanceFieldSceneData.PrimitiveInstanceMapping.Add(FPrimitiveAndInstance(ObjectBoundingSphere, PrimitiveSceneInfo, TransformIndex));
						PrimitiveSceneInfo->DistanceFieldInstanceIndices[TransformIndex] = AddIndex;
					}
					else 
					{
						// InstanceIndex will be -1 with zero scale meshes
						const int32 InstanceIndex = PrimitiveSceneInfo->DistanceFieldInstanceIndices[TransformIndex];
						if (InstanceIndex >= 0)
						{
							// For an update transform we have to dirty the previous bounds and the new bounds, in case of large movement (teleport)
							DistanceFieldSceneData.PrimitiveModifiedBounds[CacheType].Add(DistanceFieldSceneData.PrimitiveInstanceMapping[InstanceIndex].BoundingSphere);
							DistanceFieldSceneData.PrimitiveInstanceMapping[InstanceIndex].BoundingSphere = ObjectBoundingSphere;
						}
					}

					DistanceFieldSceneData.PrimitiveModifiedBounds[CacheType].Add(ObjectBoundingSphere);

					extern int32 GAOLogGlobalDistanceFieldModifiedPrimitives;

					if (GAOLogGlobalDistanceFieldModifiedPrimitives)
					{
						UE_LOG(LogDistanceField,Log,TEXT("Global Distance Field %s primitive %s %s %s bounding radius %.1f"), PrimitiveSceneInfo->Proxy->IsOftenMoving() ? TEXT("CACHED") : TEXT("Movable"), (bIsAddOperation ? TEXT("add") : TEXT("update")), *PrimitiveSceneInfo->Proxy->GetOwnerName().ToString(), *PrimitiveSceneInfo->Proxy->GetResourceName().ToString(), BoundingRadius);
					}
				}
				else if (bIsAddOperation)
				{
					// Set to -1 for zero scale meshes
					PrimitiveSceneInfo->DistanceFieldInstanceIndices[TransformIndex] = -1;
				}
			}
		}
		else
		{
			UE_LOG(LogDistanceField,Log,TEXT("Primitive %s %s excluded due to bounding radius %f"), *PrimitiveSceneInfo->Proxy->GetOwnerName().ToString(), *PrimitiveSceneInfo->Proxy->GetResourceName().ToString(), BoundingRadius);
		}
	}
}

void FDeferredShadingSceneRenderer::UpdateGlobalDistanceFieldObjectBuffers(FRHICommandListImmediate& RHICmdList) 
{
	FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;

	if (GDistanceFieldVolumeTextureAtlas.VolumeTextureRHI
		&& (DistanceFieldSceneData.HasPendingOperations() || DistanceFieldSceneData.AtlasGeneration != GDistanceFieldVolumeTextureAtlas.GetGeneration()))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateObjectData);
		SCOPED_DRAW_EVENT(RHICmdList, UpdateSceneObjectData);

		if (!DistanceFieldSceneData.ObjectBuffers)
		{
			DistanceFieldSceneData.ObjectBuffers = new FDistanceFieldObjectBuffers();
		}

		if (!DistanceFieldSceneData.SurfelBuffers)
		{
			DistanceFieldSceneData.SurfelBuffers = new FSurfelBuffers();
		}

		if (!DistanceFieldSceneData.InstancedSurfelBuffers)
		{
			DistanceFieldSceneData.InstancedSurfelBuffers = new FInstancedSurfelBuffers();
		}

		if (DistanceFieldSceneData.AtlasGeneration != GDistanceFieldVolumeTextureAtlas.GetGeneration())
		{
			DistanceFieldSceneData.AtlasGeneration = GDistanceFieldVolumeTextureAtlas.GetGeneration();

			for (int32 PrimitiveInstanceIndex = 0; PrimitiveInstanceIndex < DistanceFieldSceneData.PrimitiveInstanceMapping.Num(); PrimitiveInstanceIndex++)
			{
				FPrimitiveAndInstance& PrimitiveInstance = DistanceFieldSceneData.PrimitiveInstanceMapping[PrimitiveInstanceIndex];

				// Queue an update of all primitives, since the atlas layout has changed
				if (PrimitiveInstance.InstanceIndex == 0 
					&& !DistanceFieldSceneData.HasPendingRemovePrimitive(PrimitiveInstance.Primitive)
					&& !DistanceFieldSceneData.PendingAddOperations.Contains(PrimitiveInstance.Primitive)
					&& !DistanceFieldSceneData.PendingUpdateOperations.Contains(PrimitiveInstance.Primitive))
				{
					DistanceFieldSceneData.PendingUpdateOperations.Add(PrimitiveInstance.Primitive);
				}
			}
		}

		// Process removes before adds, as the adds will overwrite primitive allocation info in DistanceFieldSceneData.SurfelAllocations
		UpdateGlobalDistanceFieldObjectRemoves(RHICmdList, Scene);

		extern int32 GVPLMeshGlobalIllumination;
		TArray<uint32> UploadObjectIndices;
		TArray<FVector4> UploadObjectData;
		const bool bPrepareForDistanceFieldGI = GVPLMeshGlobalIllumination && SupportsDistanceFieldGI(Scene->GetFeatureLevel(), Scene->GetShaderPlatform());

		if (DistanceFieldSceneData.PendingAddOperations.Num() > 0 || DistanceFieldSceneData.PendingUpdateOperations.Num() > 0)
		{
			TArray<FMatrix> ObjectLocalToWorldTransforms;

			const int32 NumUploadOperations = DistanceFieldSceneData.PendingAddOperations.Num() + DistanceFieldSceneData.PendingUpdateOperations.Num();
			UploadObjectData.Empty(NumUploadOperations * UploadObjectDataStride);
			UploadObjectIndices.Empty(NumUploadOperations);

			const int32 NumTexelsOneDimX = GDistanceFieldVolumeTextureAtlas.GetSizeX();
			const int32 NumTexelsOneDimY = GDistanceFieldVolumeTextureAtlas.GetSizeY();
			const int32 NumTexelsOneDimZ = GDistanceFieldVolumeTextureAtlas.GetSizeZ();
			const FVector InvTextureDim(1.0f / NumTexelsOneDimX, 1.0f / NumTexelsOneDimY, 1.0f / NumTexelsOneDimZ);

			int32 OriginalNumObjects = DistanceFieldSceneData.NumObjectsInBuffer;
			int32 OriginalNumSurfels = DistanceFieldSceneData.SurfelAllocations.GetNumSurfelsInBuffer();
			int32 OriginalNumInstancedSurfels = DistanceFieldSceneData.InstancedSurfelAllocations.GetNumSurfelsInBuffer();
			
			if (bPrepareForDistanceFieldGI)
			{
				for (int32 UploadPrimitiveIndex = 0; UploadPrimitiveIndex < DistanceFieldSceneData.PendingAddOperations.Num(); UploadPrimitiveIndex++)
				{
					FPrimitiveSceneInfo* PrimitiveSceneInfo = DistanceFieldSceneData.PendingAddOperations[UploadPrimitiveIndex];

					int32 NumInstances = 0;
					float BoundsSurfaceArea = 0;
					PrimitiveSceneInfo->Proxy->GetDistanceFieldInstanceInfo(NumInstances, BoundsSurfaceArea);

					extern void ComputeNumSurfels(float BoundsSurfaceArea, int32& PrimitiveNumSurfels, int32& PrimitiveLOD0Surfels);
					int32 PrimitiveNumSurfels;
					int32 PrimitiveLOD0Surfels;
					ComputeNumSurfels(BoundsSurfaceArea, PrimitiveNumSurfels, PrimitiveLOD0Surfels);

					if (PrimitiveNumSurfels > 0 && NumInstances > 0)
					{
						const int32 PrimitiveTotalNumSurfels = PrimitiveNumSurfels * NumInstances;

						if (PrimitiveNumSurfels > 5000)
						{
							UE_LOG(LogDistanceField,Warning,TEXT("Primitive %s %s used %u Surfels"), *PrimitiveSceneInfo->Proxy->GetOwnerName().ToString(), *PrimitiveSceneInfo->Proxy->GetResourceName().ToString(), PrimitiveNumSurfels);
						}

						DistanceFieldSceneData.SurfelAllocations.AddPrimitive(PrimitiveSceneInfo, PrimitiveLOD0Surfels, PrimitiveNumSurfels, 1);
						DistanceFieldSceneData.InstancedSurfelAllocations.AddPrimitive(PrimitiveSceneInfo, PrimitiveLOD0Surfels, PrimitiveNumSurfels, NumInstances);
					}
				}

				if (DistanceFieldSceneData.SurfelBuffers->MaxSurfels < DistanceFieldSceneData.SurfelAllocations.GetNumSurfelsInBuffer())
				{
					if (DistanceFieldSceneData.SurfelBuffers->MaxSurfels > 0)
					{
						// Realloc
						FSurfelBuffers* NewSurfelBuffers = new FSurfelBuffers();
						NewSurfelBuffers->MaxSurfels = DistanceFieldSceneData.SurfelAllocations.GetNumSurfelsInBuffer() * 5 / 4;
						NewSurfelBuffers->Initialize();

						{
							TShaderMapRef<FCopySurfelBufferCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
							RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
							ComputeShader->SetParameters(RHICmdList, *(DistanceFieldSceneData.SurfelBuffers), *(DistanceFieldSceneData.InstancedSurfelBuffers), *NewSurfelBuffers, OriginalNumSurfels);

							DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(OriginalNumSurfels, UpdateObjectsGroupSize), 1, 1);
							ComputeShader->UnsetParameters(RHICmdList, *NewSurfelBuffers);
						}

						DistanceFieldSceneData.SurfelBuffers->Release();
						delete DistanceFieldSceneData.SurfelBuffers;
						DistanceFieldSceneData.SurfelBuffers = NewSurfelBuffers;
					}
					else
					{
						// First time allocate
						DistanceFieldSceneData.SurfelBuffers->MaxSurfels = DistanceFieldSceneData.SurfelAllocations.GetNumSurfelsInBuffer() * 5 / 4;
						DistanceFieldSceneData.SurfelBuffers->Initialize();
					}
				}
				
				if (DistanceFieldSceneData.InstancedSurfelBuffers->MaxSurfels < DistanceFieldSceneData.InstancedSurfelAllocations.GetNumSurfelsInBuffer())
				{
					if (DistanceFieldSceneData.InstancedSurfelBuffers->MaxSurfels > 0)
					{
						// Realloc
						FInstancedSurfelBuffers* NewInstancedSurfelBuffers = new FInstancedSurfelBuffers();
						NewInstancedSurfelBuffers->MaxSurfels = DistanceFieldSceneData.InstancedSurfelAllocations.GetNumSurfelsInBuffer() * 5 / 4;
						NewInstancedSurfelBuffers->Initialize();
						
						{
							TShaderMapRef<FCopyVPLFluxBufferCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
							RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
							ComputeShader->SetParameters(RHICmdList, *(DistanceFieldSceneData.SurfelBuffers), *(DistanceFieldSceneData.InstancedSurfelBuffers), *NewInstancedSurfelBuffers, OriginalNumInstancedSurfels);

							DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(OriginalNumInstancedSurfels, UpdateObjectsGroupSize), 1, 1);
							ComputeShader->UnsetParameters(RHICmdList, *NewInstancedSurfelBuffers);
						}

						DistanceFieldSceneData.InstancedSurfelBuffers->Release();
						delete DistanceFieldSceneData.InstancedSurfelBuffers;
						DistanceFieldSceneData.InstancedSurfelBuffers = NewInstancedSurfelBuffers;
					}
					else
					{
						// First time allocate
						DistanceFieldSceneData.InstancedSurfelBuffers->MaxSurfels = DistanceFieldSceneData.InstancedSurfelAllocations.GetNumSurfelsInBuffer() * 5 / 4;
						DistanceFieldSceneData.InstancedSurfelBuffers->Initialize();
					}
				}
			}

			for (int32 UploadPrimitiveIndex = 0; UploadPrimitiveIndex < DistanceFieldSceneData.PendingAddOperations.Num(); UploadPrimitiveIndex++)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = DistanceFieldSceneData.PendingAddOperations[UploadPrimitiveIndex];

				ProcessPrimitiveUpdate(
					true,
					RHICmdList, 
					*this, 
					PrimitiveSceneInfo, 
					OriginalNumObjects, 
					InvTextureDim, 
					bPrepareForDistanceFieldGI, 
					ObjectLocalToWorldTransforms, 
					UploadObjectIndices, 
					UploadObjectData);
			}

			for (TSet<FPrimitiveSceneInfo*>::TIterator It(DistanceFieldSceneData.PendingUpdateOperations); It; ++It)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = *It;

				ProcessPrimitiveUpdate(
					false,
					RHICmdList, 
					*this, 
					PrimitiveSceneInfo, 
					OriginalNumObjects, 
					InvTextureDim, 
					bPrepareForDistanceFieldGI, 
					ObjectLocalToWorldTransforms, 
					UploadObjectIndices, 
					UploadObjectData);
			}

			DistanceFieldSceneData.PendingAddOperations.Reset();
			DistanceFieldSceneData.PendingUpdateOperations.Empty();

			if (DistanceFieldSceneData.ObjectBuffers->MaxObjects < DistanceFieldSceneData.NumObjectsInBuffer)
			{
				if (DistanceFieldSceneData.ObjectBuffers->MaxObjects > 0)
				{
					// Realloc
					FDistanceFieldObjectBuffers* NewObjectBuffers = new FDistanceFieldObjectBuffers();
					NewObjectBuffers->MaxObjects = DistanceFieldSceneData.NumObjectsInBuffer * 5 / 4;
					NewObjectBuffers->Initialize();

					{
						TShaderMapRef<FCopyObjectBufferCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
						ComputeShader->SetParameters(RHICmdList, *(DistanceFieldSceneData.ObjectBuffers), *NewObjectBuffers, OriginalNumObjects);

						DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(OriginalNumObjects, UpdateObjectsGroupSize), 1, 1);
						ComputeShader->UnsetParameters(RHICmdList, *NewObjectBuffers);
					}

					DistanceFieldSceneData.ObjectBuffers->Release();
					delete DistanceFieldSceneData.ObjectBuffers;
					DistanceFieldSceneData.ObjectBuffers = NewObjectBuffers;
				}
				else
				{
					// First time allocate
					DistanceFieldSceneData.ObjectBuffers->MaxObjects = DistanceFieldSceneData.NumObjectsInBuffer * 5 / 4;
					DistanceFieldSceneData.ObjectBuffers->Initialize();
				}
			}
		}

		if (UploadObjectIndices.Num() > 0)
		{
			if (UploadObjectIndices.Num() > GDistanceFieldUploadIndices.UploadIndices.MaxElements
				// Shrink if very large
				|| (GDistanceFieldUploadIndices.UploadIndices.MaxElements > 1000 && GDistanceFieldUploadIndices.UploadIndices.MaxElements > UploadObjectIndices.Num() * 2))
			{
				GDistanceFieldUploadIndices.UploadIndices.MaxElements = UploadObjectIndices.Num() * 5 / 4;
				GDistanceFieldUploadIndices.UploadIndices.Release();
				GDistanceFieldUploadIndices.UploadIndices.Initialize();

				GDistanceFieldUploadData.UploadData.MaxElements = UploadObjectIndices.Num() * 5 / 4;
				GDistanceFieldUploadData.UploadData.Release();
				GDistanceFieldUploadData.UploadData.Initialize();
			}

			void* LockedBuffer = RHILockVertexBuffer(GDistanceFieldUploadIndices.UploadIndices.Buffer, 0, GDistanceFieldUploadIndices.UploadIndices.Buffer->GetSize(), RLM_WriteOnly);
			const uint32 MemcpySize = UploadObjectIndices.GetTypeSize() * UploadObjectIndices.Num();
			check(GDistanceFieldUploadIndices.UploadIndices.Buffer->GetSize() >= MemcpySize);
			FPlatformMemory::Memcpy(LockedBuffer, UploadObjectIndices.GetData(), MemcpySize);
			RHIUnlockVertexBuffer(GDistanceFieldUploadIndices.UploadIndices.Buffer);

			LockedBuffer = RHILockVertexBuffer(GDistanceFieldUploadData.UploadData.Buffer, 0, GDistanceFieldUploadData.UploadData.Buffer->GetSize(), RLM_WriteOnly);
			const uint32 MemcpySize2 = UploadObjectData.GetTypeSize() * UploadObjectData.Num();
			check(GDistanceFieldUploadData.UploadData.Buffer->GetSize() >= MemcpySize2);
			FPlatformMemory::Memcpy(LockedBuffer, UploadObjectData.GetData(), MemcpySize2);
			RHIUnlockVertexBuffer(GDistanceFieldUploadData.UploadData.Buffer);

			{
				TShaderMapRef<FUploadObjectsToBufferCS> ComputeShader(GetGlobalShaderMap(Scene->GetFeatureLevel()));
				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, Scene, UploadObjectIndices.Num(), GDistanceFieldUploadIndices.UploadIndices.BufferSRV, GDistanceFieldUploadData.UploadData.BufferSRV);

				DispatchComputeShader(RHICmdList, *ComputeShader, FMath::DivideAndRoundUp<uint32>(UploadObjectIndices.Num(), UpdateObjectsGroupSize), 1, 1);
				ComputeShader->UnsetParameters(RHICmdList, Scene);
			}
		}

		check(DistanceFieldSceneData.NumObjectsInBuffer == DistanceFieldSceneData.PrimitiveInstanceMapping.Num());

		DistanceFieldSceneData.VerifyIntegrity();
	}
}

FString GetObjectBufferMemoryString()
{
	return FString::Printf(TEXT("Temp object buffers %.3fMb"), 
		(GDistanceFieldUploadIndices.UploadIndices.GetSizeBytes() + GDistanceFieldUploadData.UploadData.GetSizeBytes() + GDistanceFieldRemoveIndices.RemoveIndices.GetSizeBytes()) / 1024.0f / 1024.0f);
}
