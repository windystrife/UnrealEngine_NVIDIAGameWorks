// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Copyright (C) Microsoft. All rights reserved.

/*=============================================================================
	GPUSkinCache.cpp: Performs skinning on a compute shader into a buffer to avoid vertex buffer skinning.
=============================================================================*/

#include "GPUSkinCache.h"
#include "RawIndexBuffer.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "GlobalShader.h"
#include "SkeletalRenderGPUSkin.h"
#include "ShaderParameterUtils.h"
#include "ClearQuad.h"
#include "Shader.h"

DEFINE_STAT(STAT_GPUSkinCache_TotalNumChunks);
DEFINE_STAT(STAT_GPUSkinCache_TotalNumVertices);
DEFINE_STAT(STAT_GPUSkinCache_TotalMemUsed);
DEFINE_STAT(STAT_GPUSkinCache_TangentsIntermediateMemUsed);
DEFINE_STAT(STAT_GPUSkinCache_NumTrianglesForRecomputeTangents);
DEFINE_STAT(STAT_GPUSkinCache_NumSectionsProcessed);
DEFINE_STAT(STAT_GPUSkinCache_NumSetVertexStreams);
DEFINE_STAT(STAT_GPUSkinCache_NumPreGDME);
DEFINE_LOG_CATEGORY_STATIC(LogSkinCache, Log, All);

static int32 GEnableGPUSkinCacheShaders = 0;
static FAutoConsoleVariableRef CVarEnableGPUSkinCacheShaders(
	TEXT("r.SkinCache.CompileShaders"),
	GEnableGPUSkinCacheShaders,
	TEXT("Whether or not to compile the GPU compute skinning cache shaders.\n")
	TEXT("This will compile the shaders for skinning on a compute job and not skin on the vertex shader.\n")
	TEXT("GPUSkinVertexFactory.usf needs to be touched to cause a recompile if this changes.\n")
	TEXT("0 is off(default), 1 is on"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
	);

// 0/1
int32 GEnableGPUSkinCache = 1;
static TAutoConsoleVariable<int32> CVarEnableGPUSkinCache(
	TEXT("r.SkinCache.Mode"),
	1,
	TEXT("Whether or not to use the GPU compute skinning cache.\n")
	TEXT("This will perform skinning on a compute job and not skin on the vertex shader.\n")
	TEXT("Requires r.SkinCache.CompileShaders=1\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on(default)\n")
	TEXT(" 2: only use skin cache for skinned meshes that ticked the Recompute Tangents checkbox (unavailable in shipping builds)"),
	ECVF_RenderThreadSafe
	);

int32 GSkinCacheRecomputeTangents = 2;
TAutoConsoleVariable<int32> CVarGPUSkinCacheRecomputeTangents(
	TEXT("r.SkinCache.RecomputeTangents"),
	2,
	TEXT("This option enables recomputing the vertex tangents on the GPU.\n")
	TEXT("Can be changed at runtime, requires both r.SkinCache.CompileShaders=1 and r.SkinCache.Mode=1\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on, forces all skinned object to Recompute Tangents\n")
	TEXT(" 2: on, only recompute tangents on skinned objects who ticked the Recompute Tangents checkbox(default)\n"),
	ECVF_RenderThreadSafe
	);

static int32 GForceRecomputeTangents = 0;
FAutoConsoleVariableRef CVarGPUSkinCacheForceRecomputeTangents(
	TEXT("r.SkinCache.ForceRecomputeTangents"),
	GForceRecomputeTangents,
	TEXT("0: off (default)\n")
	TEXT("1: Forces enabling and using the skincache and forces all skinned object to Recompute Tangents\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

static int32 GNumTangentIntermediateBuffers = 1;
static TAutoConsoleVariable<float> CVarGPUSkinNumTangentIntermediateBuffers(
	TEXT("r.SkinCache.NumTangentIntermediateBuffers"),
	1,
	TEXT("How many intermediate buffers to use for intermediate results while\n")
	TEXT("doing Recompute Tangents; more may allow the GPU to overlap compute jobs."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarGPUSkinCacheDebug(
	TEXT("r.SkinCache.Debug"),
	1.0f,
	TEXT("A scaling constant passed to the SkinCache shader, useful for debugging"),
	ECVF_RenderThreadSafe
	);

static float GSkinCacheSceneMemoryLimitInMB = 128.0f;
static TAutoConsoleVariable<float> CVarGPUSkinCacheSceneMemoryLimitInMB(
	TEXT("r.SkinCache.SceneMemoryLimitInMB"),
	128.0f,
	TEXT("Maximum memory allowed to be allocated per World/Scene in Megs"),
	ECVF_RenderThreadSafe
);

static int32 GGPUSkinCacheFlushCounter = 0;

bool IsGPUSkinCacheAvailable()
{
	return GEnableGPUSkinCacheShaders != 0 || GForceRecomputeTangents != 0;
}

static inline bool DoesPlatformSupportGPUSkinCache(EShaderPlatform Platform)
{
	return Platform == SP_PCD3D_SM5 || Platform == SP_METAL_SM5 || Platform == SP_METAL_MRT_MAC || Platform == SP_METAL_MRT || Platform == SP_VULKAN_SM5;
}

// We don't have it always enabled as it's not clear if this has a performance cost
// Call on render thread only!
// Should only be called if SM5 (compute shaders, atomics) are supported.
ENGINE_API bool DoSkeletalMeshIndexBuffersNeedSRV()
{
	// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
//#todo-gpuskin: Enable on PS4 when SRVs for IB exist
	return DoesPlatformSupportGPUSkinCache(GMaxRHIShaderPlatform) && IsGPUSkinCacheAvailable();
}

ENGINE_API bool DoRecomputeSkinTangentsOnGPU_RT()
{
	// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
//#todo-gpuskin: Enable on PS4 when SRVs for IB exist
	return DoesPlatformSupportGPUSkinCache(GMaxRHIShaderPlatform) && GEnableGPUSkinCacheShaders != 0 && ((GEnableGPUSkinCache && GSkinCacheRecomputeTangents != 0) || GForceRecomputeTangents != 0);
}


class FGPUSkinCacheEntry
{
public:
	FGPUSkinCacheEntry(FGPUSkinCache* InSkinCache, FSkeletalMeshObjectGPUSkin* InGPUSkin, FGPUSkinCache::FRWBuffersAllocation* InAllocation)
		: Allocation(InAllocation)
		, SkinCache(InSkinCache)
		, GPUSkin(InGPUSkin)
		, MorphBuffer(0)
		, LOD(InGPUSkin->GetLOD())
	{
		
		const TArray<FSkelMeshSection>& Sections = InGPUSkin->GetRenderSections(LOD);
		DispatchData.AddDefaulted(Sections.Num());
		BatchElementsUserData.AddZeroed(Sections.Num());
		for (int32 Index = 0; Index < Sections.Num(); ++Index)
		{
			BatchElementsUserData[Index].Entry = this;
			BatchElementsUserData[Index].Section = Index;
		}

		FSkinWeightVertexBuffer* WeightBuffer = GPUSkin->GetSkinWeightVertexBuffer(LOD);
		InputWeightStride = WeightBuffer->GetStride();
		InputWeightStreamSRV = WeightBuffer->GetSRV();
	}

	~FGPUSkinCacheEntry()
	{
		check(!Allocation);
	}

	struct FSectionDispatchData
	{
		FGPUSkinCache::FRWBufferTracker AllocationTracker;

		FGPUBaseSkinVertexFactory* SourceVertexFactory;
		FGPUSkinPassthroughVertexFactory* TargetVertexFactory;

		// triangle index buffer (input for the RecomputeSkinTangents, might need special index buffer unique to position and normal, not considering UV/vertex color)
		FShaderResourceViewRHIParamRef IndexBuffer;

		const FSkelMeshSection* Section;

		// for debugging / draw events, -1 if not set
		uint32 SectionIndex;

		// 0:normal, 1:with morph target, 2:with APEX cloth (not yet implemented)
		uint32 SkinType;
		//
		bool bExtraBoneInfluences;

		// in floats (4 bytes)
		uint32 OutputStreamStart;
		uint32 NumVertices;

		// in floats (4 bytes)
		uint32 InputStreamStart;
		// in bytes
		uint32 InputStreamStride;
		FShaderResourceViewRHIRef InputVertexBufferSRV;

		// skin weight input
		uint32 InputWeightStart;

		// morph input
		uint32 MorphBufferOffset;

		// triangle index buffer (input for the RecomputeSkinTangents, might need special index buffer unique to position and normal, not considering UV/vertex color)
		uint32 IndexBufferOffsetValue;
		uint32 NumTriangles;

		FRWBuffer* BoneBuffer;
		FRWBuffer* PreviousBoneBuffer;

		FSectionDispatchData()
			: SourceVertexFactory(nullptr)
			, TargetVertexFactory(nullptr)
			, IndexBuffer(nullptr)
			, Section(nullptr)
			, SectionIndex(-1)
			, SkinType(0)
			, bExtraBoneInfluences(false)
			, OutputStreamStart(0)
			, NumVertices(0)
			, InputStreamStart(0)
			, InputStreamStride(0)
			, MorphBufferOffset(0)
			, IndexBufferOffsetValue(0)
			, NumTriangles(0)
			, BoneBuffer(nullptr)
			, PreviousBoneBuffer(nullptr)
		{
		}

		inline FRWBuffer* GetPreviousRWBuffer()
		{
			check(PreviousBoneBuffer);
			return PreviousBoneBuffer;
		}

		inline FRWBuffer* GetRWBuffer()
		{
			check(BoneBuffer);
			return BoneBuffer;
		}

		void UpdateVertexFactoryDeclaration()
		{
			TargetVertexFactory->UpdateVertexDeclaration(SourceVertexFactory, GetRWBuffer());
		}
	};

	void UpdateVertexFactoryDeclaration(int32 Section)
	{
		DispatchData[Section].UpdateVertexFactoryDeclaration();
	}

	bool IsSectionValid(int32 Section) const
	{
		const FSectionDispatchData& SectionData = DispatchData[Section];
		return SectionData.SectionIndex == Section;
	}

	bool IsSourceFactoryValid(int32 Section, FGPUBaseSkinVertexFactory* SourceVertexFactory) const
	{
		const FSectionDispatchData& SectionData = DispatchData[Section];
		return SectionData.SourceVertexFactory == SourceVertexFactory;
	}

	bool IsValid(FSkeletalMeshObjectGPUSkin* InSkin) const
	{
		return GPUSkin == InSkin && GPUSkin->GetLOD() == LOD;
	}

	void SetupSection(int32 SectionIndex, FGPUSkinCache::FRWBuffersAllocation* InAllocation, FSkelMeshSection* Section, const FMorphVertexBuffer* MorphVertexBuffer, uint32 NumVertices,
		uint32 InputStreamStart, uint32 InputStreamStride, FGPUBaseSkinVertexFactory* InSourceVertexFactory, FGPUSkinPassthroughVertexFactory* InTargetVertexFactory)
	{
		//UE_LOG(LogSkinCache, Warning, TEXT("*** SetupSection E %p Alloc %p Sec %d(%p) LOD %d"), this, InAllocation, SectionIndex, Section, LOD);
		FSectionDispatchData& Data = DispatchData[SectionIndex];
		check(!Data.AllocationTracker.Allocation || Data.AllocationTracker.Allocation == InAllocation);
		Data.AllocationTracker.Allocation = InAllocation;
		Data.SectionIndex = SectionIndex;
		Data.Section = Section;

		check(GPUSkin->GetLOD() == LOD);
		FSkeletalMeshResource& SkeletalMeshResource = GPUSkin->GetSkeletalMeshResource();
		FStaticLODModel& LodModel = SkeletalMeshResource.LODModels[LOD];
		check(Data.SectionIndex == LodModel.FindSectionIndex(*Section));

		Data.NumVertices = NumVertices;

		if (MorphVertexBuffer)
		{
			// in bytes
			const uint32 MorphStride = sizeof(FMorphGPUSkinVertex);

			// see GPU code "check(MorphStride == sizeof(float) * 6);"
			check(MorphStride == sizeof(float) * 6);

			Data.MorphBufferOffset = (MorphStride * Section->BaseVertexIndex) / sizeof(float);
		}

		//INC_DWORD_STAT(STAT_GPUSkinCache_TotalNumChunks);

		// SkinType 0:normal, 1:with morph target, 2:with APEX cloth (not yet implemented)
		Data.SkinType = MorphVertexBuffer ? 1 : 0;
		Data.InputStreamStart = InputStreamStart;
		Data.OutputStreamStart = Section->BaseVertexIndex * FGPUSkinCache::RWStrideInFloats;

		Data.InputStreamStride = InputStreamStride;
		Data.InputVertexBufferSRV = InSourceVertexFactory->GetSkinVertexBuffer()->GetSRV();
		Data.bExtraBoneInfluences = InSourceVertexFactory->UsesExtraBoneInfluences();
		check(Data.InputVertexBufferSRV);

		// weight buffer
		Data.InputWeightStart = (InputWeightStride * Section->BaseVertexIndex) / sizeof(float);
		Data.SourceVertexFactory = InSourceVertexFactory;
		Data.TargetVertexFactory = InTargetVertexFactory;

		int32 RecomputeTangentsMode = GForceRecomputeTangents > 0 ? 1 : GSkinCacheRecomputeTangents;
		if (RecomputeTangentsMode > 0)
		{
			if (Section->bRecomputeTangent || RecomputeTangentsMode == 1)
			{
				FRawStaticIndexBuffer16or32Interface* IndexBuffer = LodModel.MultiSizeIndexContainer.GetIndexBuffer();
				Data.IndexBuffer = IndexBuffer->GetSRV();
				if (Data.IndexBuffer)
				{
					Data.NumTriangles = Section->NumTriangles;
					Data.IndexBufferOffsetValue = Section->BaseIndex;
				}
			}
		}
	}

protected:
	FGPUSkinCache::FRWBuffersAllocation* Allocation;
	FGPUSkinCache* SkinCache;
	TArray<FGPUSkinBatchElementUserData> BatchElementsUserData;
	TArray<FSectionDispatchData> DispatchData;
	FSkeletalMeshObjectGPUSkin* GPUSkin;
	uint32 InputWeightStride;
	FShaderResourceViewRHIRef InputWeightStreamSRV;
	FShaderResourceViewRHIParamRef MorphBuffer;
	int32 LOD;

	friend class FGPUSkinCache;
	friend class FBaseGPUSkinCacheCS;
	friend class FBaseRecomputeTangents;
};

class FBaseGPUSkinCacheCS : public FGlobalShader
{
public:
	FBaseGPUSkinCacheCS() {} 

	FBaseGPUSkinCacheCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SkinMeshOriginParameter.Bind(Initializer.ParameterMap, TEXT("MeshOrigin"));
		SkinMeshExtensionParameter.Bind(Initializer.ParameterMap, TEXT("MeshExtension"));

		//DebugParameter.Bind(Initializer.ParameterMap, TEXT("DebugParameter"));

		InputStreamStride.Bind(Initializer.ParameterMap, TEXT("InputStreamStride"));
		NumVertices.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
		SkinCacheStart.Bind(Initializer.ParameterMap, TEXT("SkinCacheStart"));
		BoneMatrices.Bind(Initializer.ParameterMap, TEXT("BoneMatrices"));
		SkinInputStream.Bind(Initializer.ParameterMap, TEXT("SkinStreamInputBuffer"));
		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));

		InputWeightStart.Bind(Initializer.ParameterMap, TEXT("InputWeightStart"));
		InputWeightStride.Bind(Initializer.ParameterMap, TEXT("InputWeightStride"));
		InputWeightStream.Bind(Initializer.ParameterMap, TEXT("InputWeightStream"));
		
		SkinCacheBufferUAV.Bind(Initializer.ParameterMap, TEXT("SkinCacheBufferUAV"));

		MorphBuffer.Bind(Initializer.ParameterMap, TEXT("MorphBuffer"));
		MorphBufferOffset.Bind(Initializer.ParameterMap, TEXT("MorphBufferOffset"));
		SkinCacheDebug.Bind(Initializer.ParameterMap, TEXT("SkinCacheDebug"));
	}

	void SetParameters(FRHICommandListImmediate& RHICmdList, const FVertexBufferAndSRV& BoneBuffer, 
		const FVector& MeshOrigin, const FVector& MeshExtension,
		FGPUSkinCacheEntry* Entry,
		FGPUSkinCacheEntry::FSectionDispatchData& DispatchData,
		FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetShaderValue(RHICmdList, ShaderRHI, SkinMeshOriginParameter, MeshOrigin);
		SetShaderValue(RHICmdList, ShaderRHI, SkinMeshExtensionParameter, MeshExtension);

		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStride, DispatchData.InputStreamStride);
		SetShaderValue(RHICmdList, ShaderRHI, NumVertices, DispatchData.NumVertices);
		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStart, DispatchData.InputStreamStart);

		check(BoneBuffer.VertexBufferSRV);
		SetSRVParameter(RHICmdList, ShaderRHI, BoneMatrices, BoneBuffer.VertexBufferSRV);

		SetSRVParameter(RHICmdList, ShaderRHI, SkinInputStream, DispatchData.InputVertexBufferSRV);


		SetShaderValue(RHICmdList, ShaderRHI, InputWeightStart, DispatchData.InputWeightStart);
		SetShaderValue(RHICmdList, ShaderRHI, InputWeightStride, Entry->InputWeightStride);
		SetSRVParameter(RHICmdList, ShaderRHI, InputWeightStream, Entry->InputWeightStreamSRV);

		// output UAV
		SetUAVParameter(RHICmdList, ShaderRHI, SkinCacheBufferUAV, UnorderedAccessViewRHI);
		SetShaderValue(RHICmdList, ShaderRHI, SkinCacheStart, DispatchData.OutputStreamStart);
	
		const bool bMorph = DispatchData.SkinType == 1;
		if(bMorph)
		{
			SetSRVParameter(RHICmdList, ShaderRHI, MorphBuffer, Entry->MorphBuffer);
			SetShaderValue(RHICmdList, ShaderRHI, MorphBufferOffset, DispatchData.MorphBufferOffset);
		}

		SetShaderValue(RHICmdList, ShaderRHI, SkinCacheDebug, CVarGPUSkinCacheDebug.GetValueOnRenderThread());
	}


	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		
		SetUAVParameter(RHICmdList, ShaderRHI, SkinCacheBufferUAV, 0);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << SkinMeshOriginParameter << SkinMeshExtensionParameter << InputStreamStride
			<< NumVertices << InputStreamStart << SkinCacheStart
			<< SkinInputStream << SkinCacheBufferUAV << BoneMatrices << MorphBuffer << MorphBufferOffset
			<< SkinCacheDebug;

		Ar << InputWeightStart << InputWeightStride << InputWeightStream;

		//Ar << DebugParameter;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter SkinMeshOriginParameter;
	FShaderParameter SkinMeshExtensionParameter;
	
	FShaderParameter InputStreamStride;
	FShaderParameter NumVertices;
	FShaderParameter SkinCacheDebug;
	FShaderParameter InputStreamStart;
	FShaderParameter SkinCacheStart;

	//FShaderParameter DebugParameter;

	FShaderUniformBufferParameter SkinUniformBuffer;

	FShaderResourceParameter BoneMatrices;
	FShaderResourceParameter SkinInputStream;
	FShaderResourceParameter SkinCacheBufferUAV;

	FShaderParameter InputWeightStart;
	FShaderParameter InputWeightStride;
	FShaderResourceParameter InputWeightStream;

	FShaderResourceParameter MorphBuffer;
	FShaderParameter MorphBufferOffset;
};

/** Compute shader that skins a batch of vertices. */
// @param SkinType 0:normal, 1:with morph targets calculated outside the cache, 2:with morph target calculated insde the cache (not yet implemented), 3:with APEX cloth (not yet implemented)
template <bool bUseExtraBoneInfluencesT, uint32 SkinType>
class TGPUSkinCacheCS : public FBaseGPUSkinCacheCS
{
	DECLARE_SHADER_TYPE(TGPUSkinCacheCS, Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsGPUSkinCacheAvailable() && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		const uint32 UseExtraBoneInfluences = bUseExtraBoneInfluencesT;
		OutEnvironment.SetDefine(TEXT("GPUSKIN_USE_EXTRA_INFLUENCES"), UseExtraBoneInfluences);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_MORPH_BLEND"), SkinType);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_POSITION"), FGPUSkinCache::RWPositionOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_X"), FGPUSkinCache::RWTangentXOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_Z"), FGPUSkinCache::RWTangentZOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_NUM_FLOATS"), FGPUSkinCache::RWStrideInFloats);
	}

	TGPUSkinCacheCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseGPUSkinCacheCS(Initializer)
	{
	}

	TGPUSkinCacheCS()
	{
	}
	
	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/GpuSkinCacheComputeShader.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("SkinCacheUpdateBatchCS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A)		VARIATION2(A,0)			VARIATION2(A,1)
#define VARIATION2(A, B) typedef TGPUSkinCacheCS<A, B> TGPUSkinCacheCS##A##B; \
	IMPLEMENT_SHADER_TYPE2(TGPUSkinCacheCS##A##B, SF_Compute);
	VARIATION1(0) VARIATION1(1)
#undef VARIATION1
#undef VARIATION2

FGPUSkinCache::FGPUSkinCache(bool bInRequiresMemoryLimit)
	: UsedMemoryInBytes(0)
	, ExtraRequiredMemory(0)
	, FlushCounter(0)
	, bRequiresMemoryLimit(bInRequiresMemoryLimit)
	, CurrentStagingBufferIndex(0)
{
}

FGPUSkinCache::~FGPUSkinCache()
{
	Cleanup();
}

void FGPUSkinCache::Cleanup()
{
	for (int32 Index = 0; Index < StagingBuffers.Num(); ++Index)
	{
		StagingBuffers[Index].Release();
	}

	while (Entries.Num() > 0)
	{
		ReleaseSkinCacheEntry(Entries.Last());
	}
	ensure(Allocations.Num() == 0);
}

void FGPUSkinCache::TransitionAllToReadable(FRHICommandList& RHICmdList)
{
	if (BuffersToTransition.Num() > 0)
	{
		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, BuffersToTransition.GetData(), BuffersToTransition.Num());
		BuffersToTransition.SetNum(0, false);
	}
}

#if 0
void FGPUSkinCache::TransitionToWriteable(FRHICommandList& RHICmdList)
{
	int32 BufferIndex = InternalUpdateCount % GPUSKINCACHE_FRAMES;
	FUnorderedAccessViewRHIParamRef OutUAVs[] = {SkinCacheBuffer[BufferIndex].UAV};
	RHICmdList.TransitionResources(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
}

void FGPUSkinCache::TransitionAllToWriteable(FRHICommandList& RHICmdList)
{
	if (bInitialized)
	{
		FUnorderedAccessViewRHIParamRef OutUAVs[GPUSKINCACHE_FRAMES];

		for (int32 Index = 0; Index < GPUSKINCACHE_FRAMES; ++Index)
		{
			OutUAVs[Index] = SkinCacheBuffer[Index].UAV;
		}

		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EGfxToCompute, OutUAVs, ARRAY_COUNT(OutUAVs));
	}
}
#endif

/** base of the FRecomputeTangentsPerTrianglePassCS class */
class FBaseRecomputeTangents : public FGlobalShader
{
public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
		return DoesPlatformSupportGPUSkinCache(Platform) && IsGPUSkinCacheAvailable();
	}

	static const uint32 ThreadGroupSizeX = 64;

	FShaderResourceParameter IntermediateAccumBufferUAV;
	FShaderParameter NumTriangles;
	FShaderResourceParameter GPUSkinCacheBuffer;
	FShaderParameter SkinCacheStart;
	FShaderResourceParameter IndexBuffer;
	FShaderParameter IndexBufferOffset;
	FShaderParameter InputStreamStart;
	FShaderParameter InputStreamStride;
	FShaderResourceParameter SkinInputStream;

	FBaseRecomputeTangents()
	{}

	FBaseRecomputeTangents(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IntermediateAccumBufferUAV.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferUAV"));
		NumTriangles.Bind(Initializer.ParameterMap, TEXT("NumTriangles"));
		GPUSkinCacheBuffer.Bind(Initializer.ParameterMap,TEXT("GPUSkinCacheBuffer"));
		SkinCacheStart.Bind(Initializer.ParameterMap,TEXT("SkinCacheStart"));
		IndexBuffer.Bind(Initializer.ParameterMap,TEXT("IndexBuffer"));
		IndexBufferOffset.Bind(Initializer.ParameterMap,TEXT("IndexBufferOffset"));

		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));
		InputStreamStride.Bind(Initializer.ParameterMap, TEXT("InputStreamStride"));
		SkinInputStream.Bind(Initializer.ParameterMap, TEXT("SkinStreamInputBuffer"));
	}

	void SetParameters(FRHICommandListImmediate& RHICmdList, FGPUSkinCacheEntry* Entry, FGPUSkinCacheEntry::FSectionDispatchData& DispatchData, FRWBuffer& StagingBuffer)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

//later		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, NumTriangles, DispatchData.NumTriangles);

		SetSRVParameter(RHICmdList, ShaderRHI, GPUSkinCacheBuffer, DispatchData.GetRWBuffer()->SRV);
		SetShaderValue(RHICmdList, ShaderRHI, SkinCacheStart, DispatchData.OutputStreamStart);

		SetSRVParameter(RHICmdList, ShaderRHI, IndexBuffer, DispatchData.IndexBuffer);
		SetShaderValue(RHICmdList, ShaderRHI, IndexBufferOffset, DispatchData.IndexBufferOffsetValue);
		
		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStart, DispatchData.InputStreamStart);
		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStride, DispatchData.InputStreamStride);
		SetSRVParameter(RHICmdList, ShaderRHI, SkinInputStream, DispatchData.InputVertexBufferSRV);

		// UAV
		SetUAVParameter(RHICmdList, ShaderRHI, IntermediateAccumBufferUAV, StagingBuffer.UAV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetUAVParameter(RHICmdList, ShaderRHI, IntermediateAccumBufferUAV, 0);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << IntermediateAccumBufferUAV << NumTriangles << GPUSkinCacheBuffer << SkinCacheStart << IndexBuffer << IndexBufferOffset
			<< InputStreamStart << InputStreamStride << SkinInputStream;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/RecomputeTangentsPerTrianglePass.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainCS");
	}
};

/** Encapsulates the RecomputeSkinTangents compute shader. */
template <bool bUseExtraBoneInfluencesT, bool bFullPrecisionUV>
class FRecomputeTangentsPerTrianglePassCS : public FBaseRecomputeTangents
{
	DECLARE_SHADER_TYPE(FRecomputeTangentsPerTrianglePassCS, Global);

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		const uint32 UseExtraBoneInfluences = bUseExtraBoneInfluencesT;
		OutEnvironment.SetDefine(TEXT("GPUSKIN_USE_EXTRA_INFLUENCES"), UseExtraBoneInfluences);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("INTERMEDIATE_ACCUM_BUFFER_NUM_INTS"), FGPUSkinCache::IntermediateAccumBufferNumInts);
		OutEnvironment.SetDefine(TEXT("FULL_PRECISION_UV"), bFullPrecisionUV);
	}

	FRecomputeTangentsPerTrianglePassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseRecomputeTangents(Initializer)
	{
	}

	FRecomputeTangentsPerTrianglePassCS()
	{}
};

// #define avoids a lot of code duplication
#define VARIATION1(A)		VARIATION2(A,0)			VARIATION2(A,1)
#define VARIATION2(A, B) typedef FRecomputeTangentsPerTrianglePassCS<A, B> FRecomputeTangentsPerTrianglePassCS##A##B; \
	IMPLEMENT_SHADER_TYPE2(FRecomputeTangentsPerTrianglePassCS##A##B, SF_Compute);
	VARIATION1(0) VARIATION1(1)
#undef VARIATION1
#undef VARIATION2

/** Encapsulates the RecomputeSkinTangentsResolve compute shader. */
class FRecomputeTangentsPerVertexPassCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRecomputeTangentsPerVertexPassCS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// currently only implemented and tested on Window SM5 (needs Compute, Atomics, SRV for index buffers, UAV for VertexBuffers)
		return DoesPlatformSupportGPUSkinCache(Platform) && IsGPUSkinCacheAvailable();
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		// this pass cannot read the input as it doesn't have the permutation
		OutEnvironment.SetDefine(TEXT("GPUSKIN_USE_EXTRA_INFLUENCES"), (uint32)0);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_X"), FGPUSkinCache::RWTangentXOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_OFFSET_TANGENT_Z"), FGPUSkinCache::RWTangentZOffsetInFloats);
		OutEnvironment.SetDefine(TEXT("GPUSKIN_RWBUFFER_NUM_FLOATS"), FGPUSkinCache::RWStrideInFloats);
		OutEnvironment.SetDefine(TEXT("INTERMEDIATE_ACCUM_BUFFER_NUM_INTS"), FGPUSkinCache::IntermediateAccumBufferNumInts);
	}

	static const uint32 ThreadGroupSizeX = 64;

	FRecomputeTangentsPerVertexPassCS() {}

public:
	FShaderResourceParameter IntermediateAccumBufferUAV;
	FShaderResourceParameter SkinCacheBufferUAV;
	FShaderParameter SkinCacheStart;
	FShaderParameter NumVertices;
	FShaderParameter InputStreamStart;
	FShaderParameter InputStreamStride;

	FRecomputeTangentsPerVertexPassCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		IntermediateAccumBufferUAV.Bind(Initializer.ParameterMap, TEXT("IntermediateAccumBufferUAV"));
		SkinCacheBufferUAV.Bind(Initializer.ParameterMap,TEXT("SkinCacheBufferUAV"));
		SkinCacheStart.Bind(Initializer.ParameterMap,TEXT("SkinCacheStart"));
		NumVertices.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
		InputStreamStart.Bind(Initializer.ParameterMap, TEXT("InputStreamStart"));
		InputStreamStride.Bind(Initializer.ParameterMap, TEXT("InputStreamStride"));
	}

	void SetParameters(FRHICommandListImmediate& RHICmdList, FGPUSkinCacheEntry* Entry, FGPUSkinCacheEntry::FSectionDispatchData& DispatchData, FRWBuffer& StagingBuffer)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		
		check(StagingBuffer.UAV);

//later		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, SkinCacheStart, DispatchData.OutputStreamStart);
		SetShaderValue(RHICmdList, ShaderRHI, NumVertices, DispatchData.NumVertices);
		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStart, DispatchData.InputStreamStart);
		SetShaderValue(RHICmdList, ShaderRHI, InputStreamStride, DispatchData.InputStreamStride);

		// UAVs
		SetUAVParameter(RHICmdList, ShaderRHI, IntermediateAccumBufferUAV, StagingBuffer.UAV);
		SetUAVParameter(RHICmdList, ShaderRHI, SkinCacheBufferUAV, DispatchData.GetRWBuffer()->UAV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		SetUAVParameter(RHICmdList, ShaderRHI, SkinCacheBufferUAV, 0);
		SetUAVParameter(RHICmdList, ShaderRHI, IntermediateAccumBufferUAV, 0);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << IntermediateAccumBufferUAV << SkinCacheBufferUAV << SkinCacheStart << NumVertices << InputStreamStart << InputStreamStride;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(, FRecomputeTangentsPerVertexPassCS, TEXT("/Engine/Private/RecomputeTangentsPerVertexPass.usf"), TEXT("MainCS"), SF_Compute);

void FGPUSkinCache::DispatchUpdateSkinTangents(FRHICommandListImmediate& RHICmdList, FGPUSkinCacheEntry* Entry, int32 SectionIndex)
{
	FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[SectionIndex];

	{
		// no need to clear the intermediate buffer because we create it cleared and clear it after each usage in the per vertex pass

		FSkeletalMeshResource& SkeletalMeshResource = Entry->GPUSkin->GetSkeletalMeshResource();
		int32 LODIndex = Entry->LOD;
		FStaticLODModel& LodModel = SkeletalMeshResource.LODModels[LODIndex];

		//SetRenderTarget(RHICmdList, FTextureRHIRef(), FTextureRHIRef());

		FRawStaticIndexBuffer16or32Interface* IndexBuffer = LodModel.MultiSizeIndexContainer.GetIndexBuffer();
		FIndexBufferRHIParamRef IndexBufferRHI = IndexBuffer->IndexBufferRHI;

		const uint32 RequiredVertexCount = LodModel.NumVertices;

		uint32 MaxVertexCount = RequiredVertexCount;

		if (StagingBuffers.Num() != GNumTangentIntermediateBuffers)
		{
			// Release extra buffers if shrinking
			for (int32 Index = GNumTangentIntermediateBuffers; Index < StagingBuffers.Num(); ++Index)
			{
				StagingBuffers[Index].Release();
			}
			StagingBuffers.SetNum(GNumTangentIntermediateBuffers, false);
		}

		uint32 NumIntsPerBuffer = DispatchData.NumTriangles * 3 * FGPUSkinCache::IntermediateAccumBufferNumInts;
		CurrentStagingBufferIndex = (CurrentStagingBufferIndex + 1) % StagingBuffers.Num();
		FRWBuffer& StagingBuffer = StagingBuffers[CurrentStagingBufferIndex];
		if (StagingBuffers[CurrentStagingBufferIndex].NumBytes < NumIntsPerBuffer * sizeof(uint32))
		{
			StagingBuffer.Release();
			StagingBuffer.Initialize(sizeof(int32), NumIntsPerBuffer, PF_R32_SINT, BUF_UnorderedAccess);
			RHICmdList.BindDebugLabelName(StagingBuffer.UAV, TEXT("SkinTangentIntermediate"));
			SET_MEMORY_STAT(STAT_GPUSkinCache_TangentsIntermediateMemUsed, (NumIntsPerBuffer * sizeof(uint32)));
		}

		// This code can be optimized by batched up and doing it with less Dispatch calls (costs more memory)
		{
			auto* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<0, 0> > ComputeShader00(GlobalShaderMap);
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<0, 1> > ComputeShader01(GlobalShaderMap);
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<1, 0> > ComputeShader10(GlobalShaderMap);
			TShaderMapRef<FRecomputeTangentsPerTrianglePassCS<1, 1> > ComputeShader11(GlobalShaderMap);
			
			bool bFullPrecisionUV = LodModel.VertexBufferGPUSkin.GetUseFullPrecisionUVs();

			FBaseRecomputeTangents* Shader = 0;

			switch((uint32)bFullPrecisionUV)
			{
				case 0: Shader = DispatchData.bExtraBoneInfluences ? (FBaseRecomputeTangents*)*ComputeShader10 : (FBaseRecomputeTangents*)*ComputeShader00; break;
				case 1: Shader = DispatchData.bExtraBoneInfluences ? (FBaseRecomputeTangents*)*ComputeShader11 : (FBaseRecomputeTangents*)*ComputeShader01; break;
				default:
					check(0);
			}
	
			check(Shader);

			uint32 NumTriangles = DispatchData.NumTriangles;
			uint32 ThreadGroupCountValue = FMath::DivideAndRoundUp(NumTriangles, FBaseRecomputeTangents::ThreadGroupSizeX);

			SCOPED_DRAW_EVENTF(RHICmdList, SkinTangents_PerTrianglePass, TEXT("TangentsTri IndexStart=%d Tri=%d ExtraBoneInfluences=%d UVPrecision=%d"),
				DispatchData.IndexBufferOffsetValue, DispatchData.NumTriangles, DispatchData.bExtraBoneInfluences, bFullPrecisionUV);

			const FComputeShaderRHIParamRef ShaderRHI = Shader->GetComputeShader();
			RHICmdList.SetComputeShader(ShaderRHI);

			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EComputeToCompute, StagingBuffer.UAV.GetReference());

			INC_DWORD_STAT_BY(STAT_GPUSkinCache_NumTrianglesForRecomputeTangents, NumTriangles);
			Shader->SetParameters(RHICmdList, Entry, DispatchData, StagingBuffer);
			DispatchComputeShader(RHICmdList, Shader, ThreadGroupCountValue, 1, 1);
			Shader->UnsetParameters(RHICmdList);
		}

		{
			SCOPED_DRAW_EVENTF(RHICmdList, SkinTangents_PerVertexPass, TEXT("TangentsVertex InputStreamStart=%d, OutputStreamStart=%d, Vert=%d"),
				DispatchData.InputStreamStart, DispatchData.OutputStreamStart, DispatchData.NumVertices);
			//#todo-gpuskin Feature level?
			TShaderMapRef<FRecomputeTangentsPerVertexPassCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			const FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

			uint32 VertexCount = DispatchData.NumVertices;
			uint32 ThreadGroupCountValue = FMath::DivideAndRoundUp(VertexCount, FRecomputeTangentsPerVertexPassCS::ThreadGroupSizeX);

			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, StagingBuffer.UAV.GetReference());

			ComputeShader->SetParameters(RHICmdList, Entry, DispatchData, StagingBuffer);
			DispatchComputeShader(RHICmdList, *ComputeShader, ThreadGroupCountValue, 1, 1);
			ComputeShader->UnsetParameters(RHICmdList);
		}
// todo				RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, TangentsBlendBuffer.VertexBufferSRV);
//			ensureMsgf(DestRenderTarget.TargetableTexture == DestRenderTarget.ShaderResourceTexture, TEXT("%s should be resolved to a separate SRV"), *DestRenderTarget.TargetableTexture->GetName().ToString());	
	}
}

FGPUSkinCache::FRWBuffersAllocation* FGPUSkinCache::TryAllocBuffer(uint32 NumFloatsRequired)
{
	uint64 MaxSizeInBytes =  (uint64)(GSkinCacheSceneMemoryLimitInMB * 1024.0f * 1024.0f);
	uint64 RequiredMemInBytes = FRWBuffersAllocation::CalculateRequiredMemory(NumFloatsRequired);
	if (bRequiresMemoryLimit && UsedMemoryInBytes + RequiredMemInBytes >= MaxSizeInBytes)
	{
		ExtraRequiredMemory += RequiredMemInBytes;

		// Can't fit
		return nullptr;
	}

	FRWBuffersAllocation* NewAllocation = new FRWBuffersAllocation(NumFloatsRequired);
	Allocations.Add(NewAllocation);

	UsedMemoryInBytes += RequiredMemInBytes;
	INC_MEMORY_STAT_BY(STAT_GPUSkinCache_TotalMemUsed, RequiredMemInBytes);

	return NewAllocation;
}


void FGPUSkinCache::DoDispatch(FRHICommandListImmediate& RHICmdList, FGPUSkinCacheEntry* SkinCacheEntry, int32 Section, int32 FrameNumber)
{
	INC_DWORD_STAT(STAT_GPUSkinCache_TotalNumChunks);
	FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = SkinCacheEntry->DispatchData[Section];
	DispatchUpdateSkinning(RHICmdList, SkinCacheEntry, Section, FrameNumber);
	//RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DispatchData.GetRWBuffer());
	SkinCacheEntry->UpdateVertexFactoryDeclaration(Section);

	if (SkinCacheEntry->DispatchData[Section].IndexBuffer)
	{
		DispatchUpdateSkinTangents(RHICmdList, SkinCacheEntry, Section);
	}
}

void FGPUSkinCache::ProcessEntry(FRHICommandListImmediate& RHICmdList, FGPUBaseSkinVertexFactory* VertexFactory,
	FGPUSkinPassthroughVertexFactory* TargetVertexFactory, const FSkelMeshSection& BatchElement, FSkeletalMeshObjectGPUSkin* Skin,
	const FMorphVertexBuffer* MorphVertexBuffer, uint32 FrameNumber, int32 Section, FGPUSkinCacheEntry*& InOutEntry)
{
	INC_DWORD_STAT(STAT_GPUSkinCache_NumSectionsProcessed);

	const uint32 NumVertices = BatchElement.GetNumVertices();
	uint32 StreamStrides[MaxVertexElementCount];
	//#todo-gpuskin Check that stream 0 is the position stream
	const uint32 StreamStrideCount = VertexFactory->GetStreamStrides(StreamStrides);
	const uint32 InputStreamStart = (StreamStrides[0] * BatchElement.BaseVertexIndex) / sizeof(float);

	FSkeletalMeshResource& SkeletalMeshResource = Skin->GetSkeletalMeshResource();
	int32 LODIndex = Skin->GetLOD();
	FStaticLODModel& LodModel = SkeletalMeshResource.LODModels[LODIndex];

	if (FlushCounter < GGPUSkinCacheFlushCounter)
	{
		FlushCounter = GGPUSkinCacheFlushCounter;
		InvalidateAllEntries();
	}

	if (InOutEntry)
	{
		// If the LOD changed, the entry has to be invalidated
		if (!InOutEntry->IsValid(Skin))
		{
			Release(InOutEntry);
			InOutEntry = nullptr;
		}
		else
		{
			if (!InOutEntry->IsSectionValid(Section) || !InOutEntry->IsSourceFactoryValid(Section, VertexFactory))
			{
				// This section might not be valid yet, so set it up
				InOutEntry->SetupSection(Section, InOutEntry->Allocation, &LodModel.Sections[Section], MorphVertexBuffer, NumVertices, InputStreamStart, StreamStrides[0], VertexFactory, TargetVertexFactory);
			}
		}
	}

	// Try to allocate a new entry
	if (!InOutEntry)
	{
		int32 TotalNumVertices = VertexFactory->GetSkinVertexBuffer()->GetNumVertices();
		const uint32 NumUAVFloats = (RWStrideInFloats * TotalNumVertices);
		FRWBuffersAllocation* NewAllocation = TryAllocBuffer(NumUAVFloats);
		if (!NewAllocation)
		{
			// Couldn't fit; caller will notify OOM
			InOutEntry = nullptr;
			return;
		}

		InOutEntry = new FGPUSkinCacheEntry(this, Skin, NewAllocation);
		InOutEntry->GPUSkin = Skin;

		InOutEntry->SetupSection(Section, NewAllocation, &LodModel.Sections[Section], MorphVertexBuffer, NumVertices, InputStreamStart, StreamStrides[0], VertexFactory, TargetVertexFactory);
		Entries.Add(InOutEntry);
	}

	if (MorphVertexBuffer)
	{
		InOutEntry->MorphBuffer = MorphVertexBuffer->GetSRV();
		check(InOutEntry->MorphBuffer);

		const uint32 MorphStride = sizeof(FMorphGPUSkinVertex);

		// see GPU code "check(MorphStride == sizeof(float) * 6);"
		check(MorphStride == sizeof(float) * 6);

		InOutEntry->DispatchData[Section].MorphBufferOffset = (MorphStride * BatchElement.BaseVertexIndex) / sizeof(float);

		// weight buffer
		FSkinWeightVertexBuffer* WeightBuffer = Skin->GetSkinWeightVertexBuffer(LODIndex);
		uint32 WeightStride = WeightBuffer->GetStride();
		InOutEntry->DispatchData[Section].InputWeightStart = (WeightStride * BatchElement.BaseVertexIndex) / sizeof(float);
		InOutEntry->InputWeightStride = WeightStride;
		InOutEntry->InputWeightStreamSRV = WeightBuffer->GetSRV();
	}
	InOutEntry->DispatchData[Section].SkinType = MorphVertexBuffer ? 1 : 0;

	DoDispatch(RHICmdList, InOutEntry, Section, FrameNumber);

	InOutEntry->UpdateVertexFactoryDeclaration(Section);
}

void FGPUSkinCache::Release(FGPUSkinCacheEntry*& SkinCacheEntry)
{
	if (SkinCacheEntry)
	{
		ReleaseSkinCacheEntry(SkinCacheEntry);
		SkinCacheEntry = nullptr;
	}
}

void FGPUSkinCache::SetVertexStreams(FGPUSkinCacheEntry* Entry, int32 Section, FRHICommandList& RHICmdList, uint32 FrameNumber,
	FShader* Shader, const FGPUSkinPassthroughVertexFactory* VertexFactory,
	uint32 BaseVertexIndex, FShaderParameter PreviousStreamFloatOffset, FShaderResourceParameter PreviousStreamBuffer)
{
	INC_DWORD_STAT(STAT_GPUSkinCache_NumSetVertexStreams);
	check(Entry);
	check(Entry->IsSectionValid(Section));

	FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[Section];

	//UE_LOG(LogSkinCache, Warning, TEXT("*** SetVertexStreams E %p All %p Sec %d(%p) LOD %d"), Entry, Entry->DispatchData[Section].Allocation, Section, Entry->DispatchData[Section].Section, Entry->LOD);
	RHICmdList.SetStreamSource(VertexFactory->GetStreamIndex(), DispatchData.GetRWBuffer()->Buffer, 0);

	FVertexShaderRHIParamRef ShaderRHI = Shader->GetVertexShader();
	if (ShaderRHI && PreviousStreamBuffer.IsBound())
	{
		SetShaderValue(RHICmdList, ShaderRHI, PreviousStreamFloatOffset, 0);
		RHICmdList.SetShaderResourceViewParameter(ShaderRHI, PreviousStreamBuffer.GetBaseIndex(), DispatchData.GetPreviousRWBuffer()->SRV);
	}
}

void FGPUSkinCache::DispatchUpdateSkinning(FRHICommandListImmediate& RHICmdList, FGPUSkinCacheEntry* Entry, int32 Section, uint32 FrameNumber)
{
	FGPUSkinCacheEntry::FSectionDispatchData& DispatchData = Entry->DispatchData[Section];
	FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = DispatchData.SourceVertexFactory->GetShaderData();

	SCOPED_DRAW_EVENTF(RHICmdList, SkinCacheDispatch,
		TEXT("Skinning%d%d Chunk=%d InStreamStart=%d OutStart=%d Vert=%d Morph=%d/%d StrideInFloats:%d"),
		(int32)DispatchData.bExtraBoneInfluences, DispatchData.SkinType,
		DispatchData.SectionIndex, DispatchData.InputStreamStart, DispatchData.OutputStreamStart, DispatchData.NumVertices, Entry->MorphBuffer != 0, DispatchData.MorphBufferOffset,
		DispatchData.InputStreamStride / sizeof(float));
	auto* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<TGPUSkinCacheCS<true, 0> > SkinCacheCS10(GlobalShaderMap);
	TShaderMapRef<TGPUSkinCacheCS<false, 0> > SkinCacheCS00(GlobalShaderMap);
	TShaderMapRef<TGPUSkinCacheCS<true, 1> > SkinCacheCS11(GlobalShaderMap);
	TShaderMapRef<TGPUSkinCacheCS<false, 1> > SkinCacheCS01(GlobalShaderMap);

	FBaseGPUSkinCacheCS* Shader = 0;

	switch (DispatchData.SkinType)
	{
	case 0: Shader = DispatchData.bExtraBoneInfluences ? (FBaseGPUSkinCacheCS*)*SkinCacheCS10 : (FBaseGPUSkinCacheCS*)*SkinCacheCS00;
		break;
	case 1: Shader = DispatchData.bExtraBoneInfluences ? (FBaseGPUSkinCacheCS*)*SkinCacheCS11 : (FBaseGPUSkinCacheCS*)*SkinCacheCS01;
		break;
	default:
		check(0);
	}
	check(Shader);

	const FVertexBufferAndSRV& BoneBuffer = ShaderData.GetBoneBufferForReading(false, FrameNumber);
	const FVertexBufferAndSRV& PrevBoneBuffer = ShaderData.GetBoneBufferForReading(true, FrameNumber);

	uint32 CurrentRevision = FrameNumber;
	uint32 PreviousRevision = FrameNumber - 1;
	DispatchData.PreviousBoneBuffer = DispatchData.AllocationTracker.Find(PrevBoneBuffer, PreviousRevision);
	if (!DispatchData.PreviousBoneBuffer)
	{
		DispatchData.AllocationTracker.Advance(PrevBoneBuffer, PreviousRevision, BoneBuffer, CurrentRevision);
		DispatchData.PreviousBoneBuffer = DispatchData.AllocationTracker.Find(PrevBoneBuffer, PreviousRevision);
		check(DispatchData.PreviousBoneBuffer)

		RHICmdList.SetComputeShader(Shader->GetComputeShader());
		Shader->SetParameters(RHICmdList, PrevBoneBuffer, ShaderData.MeshOrigin, ShaderData.MeshExtension, Entry, DispatchData, DispatchData.GetPreviousRWBuffer()->UAV);

		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, DispatchData.GetPreviousRWBuffer()->UAV.GetReference());

		uint32 VertexCountAlign64 = FMath::DivideAndRoundUp(DispatchData.NumVertices, (uint32)64);
		INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumVertices, VertexCountAlign64 * 64);
		RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);
		Shader->UnsetParameters(RHICmdList);

		BuffersToTransition.Add(DispatchData.GetPreviousRWBuffer()->UAV);
	}

	DispatchData.BoneBuffer = DispatchData.AllocationTracker.Find(BoneBuffer, CurrentRevision);
	if (!DispatchData.BoneBuffer)
	{
		DispatchData.AllocationTracker.Advance(BoneBuffer, CurrentRevision, PrevBoneBuffer, PreviousRevision);
		DispatchData.BoneBuffer = DispatchData.AllocationTracker.Find(BoneBuffer, CurrentRevision);
		check(DispatchData.BoneBuffer);
		
		RHICmdList.SetComputeShader(Shader->GetComputeShader());
		Shader->SetParameters(RHICmdList, BoneBuffer, ShaderData.MeshOrigin, ShaderData.MeshExtension, Entry, DispatchData, DispatchData.GetRWBuffer()->UAV.GetReference());

		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, DispatchData.GetRWBuffer()->UAV.GetReference());

		uint32 VertexCountAlign64 = FMath::DivideAndRoundUp(DispatchData.NumVertices, (uint32)64);
		INC_DWORD_STAT_BY(STAT_GPUSkinCache_TotalNumVertices, VertexCountAlign64 * 64);
		RHICmdList.DispatchComputeShader(VertexCountAlign64, 1, 1);
		Shader->UnsetParameters(RHICmdList);

		BuffersToTransition.Add(DispatchData.GetRWBuffer()->UAV);
	}

	check(DispatchData.PreviousBoneBuffer != DispatchData.BoneBuffer);
}

void FGPUSkinCache::ReleaseSkinCacheEntry(FGPUSkinCacheEntry* SkinCacheEntry)
{
	FGPUSkinCache* SkinCache = SkinCacheEntry->SkinCache;
	FRWBuffersAllocation* Allocation = SkinCacheEntry->Allocation;
	if (Allocation)
	{
		uint64 RequiredMemInBytes = Allocation->GetNumBytes();
		SkinCache->UsedMemoryInBytes -= RequiredMemInBytes;
		DEC_MEMORY_STAT_BY(STAT_GPUSkinCache_TotalMemUsed, RequiredMemInBytes);

		SkinCache->Allocations.Remove(Allocation);

		for (uint32 i = 0; i < NUM_BUFFERS; i++)
		{
			FRWBuffer& RWBuffer = Allocation->RWBuffers[i];
			if (RWBuffer.UAV.IsValid())
			{
				SkinCache->BuffersToTransition.Remove(RWBuffer.UAV);
			}
		}

		delete Allocation;

		SkinCacheEntry->Allocation = nullptr;
	}

	SkinCache->Entries.RemoveSingleSwap(SkinCacheEntry, false);
	delete SkinCacheEntry;
}

bool FGPUSkinCache::IsEntryValid(FGPUSkinCacheEntry* SkinCacheEntry, int32 Section)
{
	return SkinCacheEntry->IsSectionValid(Section);
}

FGPUSkinBatchElementUserData* FGPUSkinCache::InternalGetFactoryUserData(FGPUSkinCacheEntry* Entry, int32 Section)
{
	return &Entry->BatchElementsUserData[Section];
}

void FGPUSkinCache::InvalidateAllEntries()
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		Entries[Index]->LOD = -1;
	}

	for (int32 Index = 0; Index < StagingBuffers.Num(); ++Index)
	{
		StagingBuffers[Index].Release();
	}
	StagingBuffers.SetNum(0, false);
	SET_MEMORY_STAT(STAT_GPUSkinCache_TangentsIntermediateMemUsed, 0);
}

void FGPUSkinCache::CVarSinkFunction()
{
	int32 NewGPUSkinCacheValue = CVarEnableGPUSkinCache.GetValueOnAnyThread() != 0;
	int32 NewRecomputeTangentsValue = CVarGPUSkinCacheRecomputeTangents.GetValueOnAnyThread();
	float NewSceneMaxSizeInMb = CVarGPUSkinCacheSceneMemoryLimitInMB.GetValueOnAnyThread();
	int32 NewNumTangentIntermediateBuffers = CVarGPUSkinNumTangentIntermediateBuffers.GetValueOnAnyThread();

	if (!GEnableGPUSkinCacheShaders)
	{
		NewGPUSkinCacheValue = 0;
		NewRecomputeTangentsValue = 0;
	}

	if (NewGPUSkinCacheValue != GEnableGPUSkinCache || NewRecomputeTangentsValue != GSkinCacheRecomputeTangents
		|| NewSceneMaxSizeInMb != GSkinCacheSceneMemoryLimitInMB || NewNumTangentIntermediateBuffers != GNumTangentIntermediateBuffers)
	{
		ENQUEUE_RENDER_COMMAND(DoEnableSkinCaching)(
			[NewRecomputeTangentsValue, NewGPUSkinCacheValue, NewSceneMaxSizeInMb, NewNumTangentIntermediateBuffers](FRHICommandList& RHICmdList)
			{
				GNumTangentIntermediateBuffers = FMath::Max(NewNumTangentIntermediateBuffers, 1);
				GEnableGPUSkinCache = NewGPUSkinCacheValue;
				GSkinCacheRecomputeTangents = NewRecomputeTangentsValue;
				GSkinCacheSceneMemoryLimitInMB = NewSceneMaxSizeInMb;
				++GGPUSkinCacheFlushCounter;
			}
		);
	}
}

FAutoConsoleVariableSink FGPUSkinCache::CVarSink(FConsoleCommandDelegate::CreateStatic(&CVarSinkFunction));
