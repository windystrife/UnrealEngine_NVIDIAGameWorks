// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderGPUSkin.cpp: GPU skinned skeletal mesh rendering code.
=============================================================================*/

#include "SkeletalRenderGPUSkin.h"
#include "Components/SkeletalMeshComponent.h"
#include "SceneUtils.h"
#include "SkeletalRender.h"
#include "GPUSkinCache.h"
#include "Animation/MorphTarget.h"
#include "ClearQuad.h"
#include "ShaderParameterUtils.h"


DEFINE_LOG_CATEGORY_STATIC(LogSkeletalGPUSkinMesh, Warning, All);

// 0/1
#define UPDATE_PER_BONE_DATA_ONLY_FOR_OBJECT_BEEN_VISIBLE 1

DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Update"),STAT_MorphVertexBuffer_Update,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Init"),STAT_MorphVertexBuffer_Init,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Apply Delta"),STAT_MorphVertexBuffer_ApplyDelta,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Alloc"), STAT_MorphVertexBuffer_Alloc, STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer RHI Lock and copy"), STAT_MorphVertexBuffer_RhiLockAndCopy, STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer RHI Unlock"), STAT_MorphVertexBuffer_RhiUnlock, STATGROUP_MorphTarget);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Morph Target Compute"), Stat_GPU_MorphTargets, STATGROUP_GPU);

static TAutoConsoleVariable<int32> CVarMotionBlurDebug(
	TEXT("r.MotionBlurDebug"),
	0,
	TEXT("Defines if we log debugging output for motion blur rendering.\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: on"),
	ECVF_Cheat | ECVF_RenderThreadSafe);

static int32 GUseGPUMorphTargets = 1;
static FAutoConsoleVariableRef CVarUseGPUMorphTargets(
	TEXT("r.MorphTarget.Mode"),
	GUseGPUMorphTargets,
	TEXT("Use GPU for computing morph targets.\n")
	TEXT(" 0: Use original CPU method (loop per morph then by vertex)\n")
	TEXT(" 1: Enable GPU method (default)\n"),
	ECVF_Default
	);

static float GMorphTargetWeightThreshold = SMALL_NUMBER;
static FAutoConsoleVariableRef CVarMorphTargetWeightThreshold(
	TEXT("r.MorphTarget.WeightThreshold"),
	GMorphTargetWeightThreshold,
	*FString::Printf(TEXT("Set MorphTarget Weight Threshold (Default : %f).\n"), SMALL_NUMBER), 
	ECVF_Default
);

/*-----------------------------------------------------------------------------
FMorphVertexBuffer
-----------------------------------------------------------------------------*/

void FMorphVertexBuffer::InitDynamicRHI()
{
	// LOD of the skel mesh is used to find number of vertices in buffer
	FStaticLODModel& LodModel = SkelMeshResource->LODModels[LODIdx];

	// Create the buffer rendering resource
	uint32 Size = LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);
	FRHIResourceCreateInfo CreateInfo;

	bool bSupportsComputeShaders = RHISupportsComputeShaders(GMaxRHIShaderPlatform);
	bUsesComputeShader = GUseGPUMorphTargets != 0 && bSupportsComputeShaders;

#if PLATFORM_PS4
	// PS4 requires non-static buffers in order to be updated on the GPU while the CPU is writing into it
	EBufferUsageFlags Flags = bUsesComputeShader ? (EBufferUsageFlags)(BUF_Dynamic | BUF_UnorderedAccess) : BUF_Dynamic;
#else
	EBufferUsageFlags Flags = bUsesComputeShader ? (EBufferUsageFlags)(BUF_Static | BUF_UnorderedAccess) : BUF_Dynamic;
#endif

	// BUF_ShaderResource is needed for Morph support of the SkinCache
	Flags = (EBufferUsageFlags)(Flags | BUF_ShaderResource);

	VertexBufferRHI = RHICreateVertexBuffer(Size, Flags, CreateInfo);
	bool bUsesSkinCache = bSupportsComputeShaders && IsGPUSkinCacheAvailable() && GEnableGPUSkinCache;
	if (bUsesSkinCache)
	{
		SRVValue = RHICreateShaderResourceView(VertexBufferRHI, 4, PF_R32_FLOAT);
	}

	if (!bUsesComputeShader)
	{
		// Lock the buffer.
		void* BufferData = RHILockVertexBuffer(VertexBufferRHI, 0, sizeof(FMorphGPUSkinVertex)*LodModel.NumVertices, RLM_WriteOnly);
		FMorphGPUSkinVertex* Buffer = (FMorphGPUSkinVertex*)BufferData;
		FMemory::Memzero(&Buffer[0], sizeof(FMorphGPUSkinVertex)*LodModel.NumVertices);
		// Unlock the buffer.
		RHIUnlockVertexBuffer(VertexBufferRHI);
		bNeedsInitialClear = false;
	}
	else
	{
		UAVValue = RHICreateUnorderedAccessView(VertexBufferRHI, PF_R32_UINT);
		bNeedsInitialClear = true;
	}

	// hasn't been updated yet
	bHasBeenUpdated = false;
}

void FMorphVertexBuffer::ReleaseDynamicRHI()
{
	UAVValue.SafeRelease();
	VertexBufferRHI.SafeRelease();
	SRVValue.SafeRelease();
}

// @third party code - BEGIN HairWorks
void FMorphVertexBuffer::RequireSRV()
{
	if (SRVValue != nullptr)
		return;

	SRVValue = RHICreateShaderResourceView(VertexBufferRHI, 4, PF_R32_FLOAT);
}
// @third party code - END HairWorks

/*-----------------------------------------------------------------------------
FSkeletalMeshObjectGPUSkin
-----------------------------------------------------------------------------*/

FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectGPUSkin(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshResource* InSkeletalMeshResource, ERHIFeatureLevel::Type InFeatureLevel)
	: FSkeletalMeshObject(InMeshComponent, InSkeletalMeshResource, InFeatureLevel)
	,	DynamicData(NULL)
	,	bNeedsUpdateDeferred(false)
	,	bMorphNeedsUpdateDeferred(false)
	,	bMorphResourcesInitialized(false)
{
	// create LODs to match the base mesh
	LODs.Empty(SkeletalMeshResource->LODModels.Num());
	for( int32 LODIndex=0;LODIndex < SkeletalMeshResource->LODModels.Num();LODIndex++ )
	{
		new(LODs) FSkeletalMeshObjectLOD(SkeletalMeshResource,LODIndex);
	}

	InitResources(InMeshComponent);
}


FSkeletalMeshObjectGPUSkin::~FSkeletalMeshObjectGPUSkin()
{
	check(!RHIThreadFenceForDynamicData.GetReference());
	if (DynamicData)
	{
		FDynamicSkelMeshObjectDataGPUSkin::FreeDynamicSkelMeshObjectDataGPUSkin(DynamicData);
	}
	DynamicData = nullptr;
}


void FSkeletalMeshObjectGPUSkin::InitResources(USkinnedMeshComponent* InMeshComponent)
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];

		FSkelMeshComponentLODInfo* CompLODInfo = nullptr;
		if (InMeshComponent->LODInfo.IsValidIndex(LODIndex))
		{
			CompLODInfo = &InMeshComponent->LODInfo[LODIndex];
		}

		SkelLOD.InitResources(MeshLODInfo, CompLODInfo, FeatureLevel);
	}
}

void FSkeletalMeshObjectGPUSkin::ReleaseResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		SkelLOD.ReleaseResources();
	}
	// also release morph resources
	ReleaseMorphResources();
	FSkeletalMeshObjectGPUSkin* MeshObject = this;
	FGPUSkinCacheEntry** PtrSkinCacheEntry = &SkinCacheEntry;
	ENQUEUE_RENDER_COMMAND(WaitRHIThreadFenceForDynamicData)(
		[MeshObject, PtrSkinCacheEntry](FRHICommandList& RHICmdList)
		{
			FGPUSkinCacheEntry*& LocalSkinCacheEntry = *PtrSkinCacheEntry;
			FGPUSkinCache::Release(LocalSkinCacheEntry);

			FScopeCycleCounter Context(MeshObject->GetStatId());
			MeshObject->WaitForRHIThreadFenceForDynamicData();
			*PtrSkinCacheEntry = nullptr;
		}
	);
}

void FSkeletalMeshObjectGPUSkin::InitMorphResources(bool bInUsePerBoneMotionBlur, const TArray<float>& MorphTargetWeights)
{
	if( bMorphResourcesInitialized )
	{
		// release first if already initialized
		ReleaseMorphResources();
	}

	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		// init any morph vertex buffers for each LOD
		const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];
		SkelLOD.InitMorphResources(MeshLODInfo,bInUsePerBoneMotionBlur, FeatureLevel);
	}
	bMorphResourcesInitialized = true;
}

void FSkeletalMeshObjectGPUSkin::ReleaseMorphResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		// release morph vertex buffers and factories if they were created
		SkelLOD.ReleaseMorphResources();
	}

	bMorphResourcesInitialized = false;
}


void FSkeletalMeshObjectGPUSkin::Update(int32 LODIndex,USkinnedMeshComponent* InMeshComponent,const TArray<FActiveMorphTarget>& ActiveMorphTargets, const TArray<float>& MorphTargetWeights)
{
	// make sure morph data has been initialized for each LOD
	if(InMeshComponent && !bMorphResourcesInitialized && ActiveMorphTargets.Num() > 0 )
	{
		// initialized on-the-fly in order to avoid creating extra vertex streams for each skel mesh instance
		InitMorphResources(InMeshComponent->bPerBoneMotionBlur, MorphTargetWeights);		
	}

	// create the new dynamic data for use by the rendering thread
	// this data is only deleted when another update is sent
	FDynamicSkelMeshObjectDataGPUSkin* NewDynamicData = FDynamicSkelMeshObjectDataGPUSkin::AllocDynamicSkelMeshObjectDataGPUSkin();		
	NewDynamicData->InitDynamicSkelMeshObjectDataGPUSkin(InMeshComponent,SkeletalMeshResource,LODIndex,ActiveMorphTargets, MorphTargetWeights);

	// We prepare the next frame but still have the value from the last one
	uint32 FrameNumberToPrepare = GFrameNumber + 1;

	FGPUSkinCache* GPUSkinCache = nullptr;
	if (InMeshComponent && InMeshComponent->SceneProxy)
	{
		// We allow caching of per-frame, per-scene data
		FrameNumberToPrepare = InMeshComponent->SceneProxy->GetScene().GetFrameNumber() + 1;
		GPUSkinCache = InMeshComponent->SceneProxy->GetScene().GetGPUSkinCache();
	}

	// queue a call to update this data
	FSkeletalMeshObjectGPUSkin* MeshObject = this;
	ENQUEUE_RENDER_COMMAND(SkelMeshObjectUpdateDataCommand)(
		[MeshObject, FrameNumberToPrepare, NewDynamicData, GPUSkinCache](FRHICommandListImmediate& RHICmdList)
		{
			FScopeCycleCounter Context(MeshObject->GetStatId());
			MeshObject->UpdateDynamicData_RenderThread(GPUSkinCache, RHICmdList, NewDynamicData, nullptr, FrameNumberToPrepare);
		}
	);

	if( GIsEditor && InMeshComponent)
	{
		// this does not need thread-safe update
#if WITH_EDITORONLY_DATA
		ProgressiveDrawingFraction = InMeshComponent->ProgressiveDrawingFraction;
#endif // #if WITH_EDITORONLY_DATA
		CustomSortAlternateIndexMode = (ECustomSortAlternateIndexMode)InMeshComponent->CustomSortAlternateIndexMode;
	}
}

void FSkeletalMeshObjectGPUSkin::UpdateRecomputeTangent(int32 MaterialIndex, int32 LODIndex, bool bRecomputeTangent)
{
	// queue a call to update this data
	FSkeletalMeshObjectGPUSkin* MeshObject = this;
	ENQUEUE_RENDER_COMMAND(SkelMeshObjectUpdateMaterialDataCommand)(
		[MeshObject, MaterialIndex, LODIndex, bRecomputeTangent](FRHICommandList& RHICmdList)
		{
			// iterate through section and find the section that matches MaterialIndex, if so, set that flag
			for (int32 LodIdx = 0; LodIdx < MeshObject->SkeletalMeshResource->LODModels.Num(); ++LodIdx)
			{
				if (LODIndex != INDEX_NONE && LODIndex != LodIdx)
					continue;
				FStaticLODModel& LODModel = MeshObject->SkeletalMeshResource->LODModels[LodIdx];
				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
				{
					// @todo there can be more than one section that can use same material? If not, please break. 
					if (LODModel.Sections[SectionIndex].MaterialIndex == MaterialIndex)
					{
						LODModel.Sections[SectionIndex].bRecomputeTangent = bRecomputeTangent;
					}
				}
			}
		}
	);
}

static TAutoConsoleVariable<int32> CVarDeferSkeletalDynamicDataUpdateUntilGDME(
	TEXT("r.DeferSkeletalDynamicDataUpdateUntilGDME"),
	0,
	TEXT("If > 0, then do skeletal mesh dynamic data updates will be deferred until GDME. Experimental option."));

void FSkeletalMeshObjectGPUSkin::UpdateDynamicData_RenderThread(FGPUSkinCache* GPUSkinCache, FRHICommandListImmediate& RHICmdList, FDynamicSkelMeshObjectDataGPUSkin* InDynamicData, FSceneInterface* Scene, uint32 FrameNumberToPrepare)
{
	SCOPE_CYCLE_COUNTER(STAT_GPUSkinUpdateRTTime);
	check(InDynamicData);
	bool bMorphNeedsUpdate=false;
	// figure out if the morphing vertex buffer needs to be updated. compare old vs new active morphs
	bMorphNeedsUpdate = 
		(bMorphNeedsUpdateDeferred && bNeedsUpdateDeferred) || // the need for an update sticks
		(DynamicData ? (DynamicData->LODIndex != InDynamicData->LODIndex ||
		!DynamicData->ActiveMorphTargetsEqual(InDynamicData->ActiveMorphTargets, InDynamicData->MorphTargetWeights))
		: true);

	WaitForRHIThreadFenceForDynamicData();
	if (DynamicData)
	{
		FDynamicSkelMeshObjectDataGPUSkin::FreeDynamicSkelMeshObjectDataGPUSkin(DynamicData);
	}
	// update with new data
	DynamicData = InDynamicData;

	if (CVarDeferSkeletalDynamicDataUpdateUntilGDME.GetValueOnRenderThread())
	{
		bMorphNeedsUpdateDeferred = bMorphNeedsUpdate;
		bNeedsUpdateDeferred = true;
	}
	else
	{
		ProcessUpdatedDynamicData(GPUSkinCache, RHICmdList, FrameNumberToPrepare, bMorphNeedsUpdate);
	}
}

void FSkeletalMeshObjectGPUSkin::PreGDMECallback(FGPUSkinCache* GPUSkinCache, uint32 FrameNumber)
{
	if (bNeedsUpdateDeferred)
	{
		ProcessUpdatedDynamicData(GPUSkinCache, FRHICommandListExecutor::GetImmediateCommandList(), FrameNumber, bMorphNeedsUpdateDeferred);
	}
}

void FSkeletalMeshObjectGPUSkin::WaitForRHIThreadFenceForDynamicData()
{
	// we should be done with the old data at this point
	if (RHIThreadFenceForDynamicData.GetReference())
	{
		FRHICommandListExecutor::WaitOnRHIThreadFence(RHIThreadFenceForDynamicData);
		RHIThreadFenceForDynamicData = nullptr;
	}
}

void FSkeletalMeshObjectGPUSkin::ProcessUpdatedDynamicData(FGPUSkinCache* GPUSkinCache, FRHICommandListImmediate& RHICmdList, uint32 FrameNumberToPrepare, bool bMorphNeedsUpdate)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSkeletalMeshObjectGPUSkin_ProcessUpdatedDynamicData);
	bNeedsUpdateDeferred = false;
	bMorphNeedsUpdateDeferred = false;

	FSkeletalMeshObjectLOD& LOD = LODs[DynamicData->LODIndex];

	// if hasn't been updated, force update again
	bMorphNeedsUpdate = LOD.MorphVertexBuffer.bHasBeenUpdated ? bMorphNeedsUpdate : true;

	bool bMorph = DynamicData->NumWeightedActiveMorphTargets > 0;

	const FStaticLODModel& LODModel = SkeletalMeshResource->LODModels[DynamicData->LODIndex];
	const TArray<FSkelMeshSection>& Sections = GetRenderSections(DynamicData->LODIndex);

	// use correct vertex factories based on alternate weights usage
	FVertexFactoryData& VertexFactoryData = LOD.GPUSkinVertexFactories;

	bool bDataPresent = false;

	bool bGPUSkinCacheEnabled = GEnableGPUSkinCache && (FeatureLevel >= ERHIFeatureLevel::SM5);

	if (LOD.MorphVertexBuffer.bNeedsInitialClear && !(bMorph && bMorphNeedsUpdate))
	{
		if (IsValidRef(LOD.MorphVertexBuffer.GetUAV()))
		{
			ClearUAV(RHICmdList, LOD.MorphVertexBuffer.GetUAV(), LOD.MorphVertexBuffer.GetUAVSize(), 0);
		}
	}
	LOD.MorphVertexBuffer.bNeedsInitialClear = false;

	if(bMorph) 
	{
		bDataPresent = true;
		checkSlow((VertexFactoryData.MorphVertexFactories.Num() == Sections.Num()));
		
		// only update if the morph data changed and there are weighted morph targets
		if(bMorphNeedsUpdate)
		{
			if (GUseGPUMorphTargets && RHISupportsComputeShaders(GMaxRHIShaderPlatform))
			{
				ensureAlways(DynamicData->MorphTargetWeights.Num() == LODModel.MorphTargetVertexInfoBuffers.GetNumMorphs());
				// update the morph data for the lod (before SkinCache)
				LOD.UpdateMorphVertexBufferGPU(RHICmdList, DynamicData->MorphTargetWeights, LODModel.MorphTargetVertexInfoBuffers);
			}
			else
			{
				// update the morph data for the lod (before SkinCache)
				LOD.UpdateMorphVertexBufferCPU(DynamicData->ActiveMorphTargets, DynamicData->MorphTargetWeights);
			}		
		}
	}
	else
	{
//		checkSlow(VertexFactoryData.MorphVertexFactories.Num() == 0);
		bDataPresent = VertexFactoryData.VertexFactories.Num() > 0;
	}

	if (bDataPresent)
	{
		for (int32 SectionIdx = 0; SectionIdx < Sections.Num(); SectionIdx++)
		{
			const FSkelMeshSection& Section = Sections[SectionIdx];

			bool bClothFactory = (FeatureLevel >= ERHIFeatureLevel::SM4) && (DynamicData->ClothingSimData.Num() > 0) && Section.HasClothingData();

			FGPUBaseSkinVertexFactory* VertexFactory;
			{
				if(bClothFactory)
				{
					VertexFactory = VertexFactoryData.ClothVertexFactories[SectionIdx]->GetVertexFactory();
				}
				else
				{
					if(DynamicData->NumWeightedActiveMorphTargets > 0)
					{
						VertexFactory = VertexFactoryData.MorphVertexFactories[SectionIdx].Get();
					}
					else
					{
						VertexFactory = VertexFactoryData.VertexFactories[SectionIdx].Get();
					}
				}
			}

			FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = VertexFactory->GetShaderData();

			bool bUseSkinCache = bGPUSkinCacheEnabled;
			if (bUseSkinCache)
			{
				if (bClothFactory)
				{
					//INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForCloth);
					bUseSkinCache = false;
				}
				else if (Section.MaxBoneInfluences == 0)
				{
					//INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForZeroInfluences);
					bUseSkinCache = false;
				}

				{
#if (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
					// In this mode the SkinCache should only be used for RecomputeTangent meshes
					if (GEnableGPUSkinCache == 2)
					{
						if (!Section.bRecomputeTangent)
						{
							bUseSkinCache = false;
						}
					}
#endif
				}
			}

			// Create a uniform buffer from the bone transforms.
			TArray<FMatrix>& ReferenceToLocalMatrices = DynamicData->ReferenceToLocal;
			bool bNeedFence = ShaderData.UpdateBoneData(RHICmdList, ReferenceToLocalMatrices, Section.BoneMap, FrameNumberToPrepare, FeatureLevel, bUseSkinCache);
			// Try to use the GPU skinning cache if possible
			if (bUseSkinCache)
			{
				GPUSkinCache->ProcessEntry(RHICmdList, VertexFactory,
					VertexFactoryData.PassthroughVertexFactories[SectionIdx].Get(), Section, this, bMorph ? &LOD.MorphVertexBuffer : 0,
					FrameNumberToPrepare, SectionIdx, SkinCacheEntry);
			}
#if WITH_APEX_CLOTHING
			// Update uniform buffer for APEX cloth simulation mesh positions and normals
			if( bClothFactory )
			{				
				FGPUBaseSkinAPEXClothVertexFactory::ClothShaderType& ClothShaderData = VertexFactoryData.ClothVertexFactories[SectionIdx]->GetClothShaderData();
				ClothShaderData.ClothBlendWeight = DynamicData->ClothBlendWeight;
				int16 ActorIdx = Section.CorrespondClothAssetIndex;
				if(FClothSimulData* SimData = DynamicData->ClothingSimData.Find(ActorIdx))
				{
					bNeedFence = ClothShaderData.UpdateClothSimulData(RHICmdList, DynamicData->ClothingSimData[ActorIdx].Positions, DynamicData->ClothingSimData[ActorIdx].Normals, FrameNumberToPrepare, FeatureLevel) || bNeedFence;
				}
			}
#endif // WITH_APEX_CLOTHING
			if (bNeedFence)
			{
				RHIThreadFenceForDynamicData = RHICmdList.RHIThreadFence(true);
			}
		}
	}
}

TArray<float> FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::MorphAccumulatedWeightArray;

void FGPUMorphUpdateCS::SetParameters(FRHICommandList& RHICmdList, const FVector4& LocalScale, const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers, FMorphVertexBuffer& MorphVertexBuffer)
{
	FComputeShaderRHIRef CS = GetComputeShader();
	RHICmdList.SetComputeShader(CS);

	SetUAVParameter(RHICmdList, CS, MorphVertexBufferParameter, MorphVertexBuffer.GetUAV());

	SetShaderValue(RHICmdList, CS, PositionScaleParameter, LocalScale);

	SetSRVParameter(RHICmdList, CS, VertexIndicesParameter, MorphTargetVertexInfoBuffers.VertexIndicesSRV);
	SetSRVParameter(RHICmdList, CS, MorphDeltasParameter, MorphTargetVertexInfoBuffers.MorphDeltasSRV);
}

void FGPUMorphUpdateCS::SetOffsetAndSize(FRHICommandList& RHICmdList, uint32 Offset, uint32 Size, float Weight)
{
	FComputeShaderRHIRef CS = GetComputeShader();
	uint32 OffsetAndSize[2] = { Offset, Offset + Size };
	SetShaderValue(RHICmdList, CS, OffsetAndSizeParameter, OffsetAndSize);
	SetShaderValue(RHICmdList, CS, MorphTargetWeightParameter, Weight);
}


void FGPUMorphUpdateCS::Dispatch(FRHICommandList& RHICmdList, uint32 Size)
{
	RHICmdList.DispatchComputeShader(1, (Size + 31) / 32, 1);	
}

void FGPUMorphUpdateCS::EndAllDispatches(FRHICommandList& RHICmdList)
{
	FComputeShaderRHIRef CS = GetComputeShader();
	SetUAVParameter(RHICmdList, CS, MorphVertexBufferParameter, nullptr);
}

IMPLEMENT_SHADER_TYPE(, FGPUMorphUpdateCS, TEXT("/Engine/Private/MorphTargets.usf"), TEXT("GPUMorphUpdateCS"), SF_Compute);

void FGPUMorphNormalizeCS::SetParameters(FRHICommandList& RHICmdList, uint32 NumVerticies, const FVector4& InvLocalScale, const float AccumulatedWeight, FMorphVertexBuffer& MorphVertexBuffer)
{
	FComputeShaderRHIRef CS = GetComputeShader();
	RHICmdList.SetComputeShader(CS);

	SetUAVParameter(RHICmdList, CS, MorphVertexBufferParameter, MorphVertexBuffer.GetUAV());

	SetShaderValue(RHICmdList, CS, MorphTargetWeightParameter, AccumulatedWeight);
	SetShaderValue(RHICmdList, CS, MorphWorkItemsParameter, NumVerticies);
	SetShaderValue(RHICmdList, CS, PositionScaleParameter, InvLocalScale);
}

void FGPUMorphNormalizeCS::Dispatch(FRHICommandList& RHICmdList, uint32 NumVerticies, const FVector4& InvLocalScale, const float AccumulatedWeight, FMorphVertexBuffer& MorphVertexBuffer)
{
	FComputeShaderRHIRef CS = GetComputeShader();
	FGPUMorphNormalizeCS::SetParameters(RHICmdList, NumVerticies, InvLocalScale, AccumulatedWeight, MorphVertexBuffer);
	RHICmdList.DispatchComputeShader(1, (NumVerticies + 31) / 32, 1);
	SetUAVParameter(RHICmdList, CS, MorphVertexBufferParameter, nullptr);
}

IMPLEMENT_SHADER_TYPE(, FGPUMorphNormalizeCS, TEXT("/Engine/Private/MorphTargets.usf"), TEXT("GPUMorphNormalizeCS"), SF_Compute);

static const float MorphTargetWeightCutoffThreshold = 0.00000001f;

static void CalculateMorphDeltaBounds(const TArray<float>& MorphTargetWeights, const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers, FVector4& MorphScale, FVector4& InvMorphScale, float &InvTotalAccumulatedWeight)
{
	double TotalAccumulatedWeight = 0.0;
	double MinAccumScale[4] = { 0, 0, 0, 0 };
	double MaxAccumScale[4] = { 0, 0, 0, 0 };
	double MaxScale[4] = { 0, 0, 0, 0 };
	for (uint32 i = 0; i < MorphTargetVertexInfoBuffers.GetNumMorphs(); i++)
	{
		float AbsoluteMorphTargetWeight = FMath::Abs(MorphTargetWeights[i]);
		if (AbsoluteMorphTargetWeight > MorphTargetWeightCutoffThreshold)
		{
			TotalAccumulatedWeight += AbsoluteMorphTargetWeight;
			FVector4 MinMorphScale = MorphTargetVertexInfoBuffers.GetMinimumMorphScale(i);
			FVector4 MaxMorphScale = MorphTargetVertexInfoBuffers.GetMaximumMorphScale(i);

			for (uint32 j = 0; j < 4; j++)
			{
				MinAccumScale[j] += MorphTargetWeights[i] * MinMorphScale[j];
				MaxAccumScale[j] += MorphTargetWeights[i] * MaxMorphScale[j];

				double AbsMorphScale = FMath::Max(FMath::Abs(MinMorphScale[j]), FMath::Abs(MaxMorphScale[j]));
				double AbsAccumScale = FMath::Max(FMath::Abs(MinAccumScale[j]), FMath::Abs(MaxAccumScale[j]));
				//the maximum accumulated and the maximum local value have to fit into out int24
				MaxScale[j] = FMath::Max(MaxScale[j], FMath::Max(AbsMorphScale, AbsAccumScale));
			}
		}
	}

	const double ScaleToInt24 = 16777216.0;
	MorphScale = FVector4
	(
		ScaleToInt24 / (double)((uint64)(MaxScale[0] + 1.0)),
		ScaleToInt24 / (double)((uint64)(MaxScale[1] + 1.0)),
		ScaleToInt24 / (double)((uint64)(MaxScale[2] + 1.0)),
		ScaleToInt24 / (double)((uint64)(MaxScale[3] + 1.0))
	);

	InvMorphScale = FVector4
	(
		(double)((uint64)(MaxScale[0] + 1.0)) / ScaleToInt24,
		(double)((uint64)(MaxScale[1] + 1.0)) / ScaleToInt24,
		(double)((uint64)(MaxScale[2] + 1.0)) / ScaleToInt24,
		(double)((uint64)(MaxScale[3] + 1.0)) / ScaleToInt24
	);

	// if accumulated weight is >1.f
	// previous code was applying the weight again in GPU if less than 1, but it doesn't make sense to do so
	// so instead, we just divide by AccumulatedWeight if it's more than 1.
	// now DeltaTangentZ isn't FPackedNormal, so you can apply any value to it. 
	if (TotalAccumulatedWeight > 1.0f)
	{
		InvTotalAccumulatedWeight = (float)(1.0f / TotalAccumulatedWeight);
	}
	else
	{
		InvTotalAccumulatedWeight = 1.0f;
	}
}

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::UpdateMorphVertexBufferGPU(FRHICommandListImmediate& RHICmdList, const TArray<float>& MorphTargetWeights, const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers)
{
	SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Update);

	if (IsValidRef(MorphVertexBuffer.VertexBufferRHI))
	{
		// LOD of the skel mesh is used to find number of vertices in buffer
		FStaticLODModel& LodModel = SkelMeshResource->LODModels[LODIndex];

		MorphVertexBuffer.RecreateResourcesIfRequired(GUseGPUMorphTargets != 0);

		SCOPED_GPU_STAT(RHICmdList, Stat_GPU_MorphTargets);

		SCOPED_DRAW_EVENTF(RHICmdList, MorphUpdate,
			TEXT("MorphUpdate LodVertices=%d Threads=%d"),
			LodModel.NumVertices,
			MorphTargetVertexInfoBuffers.GetNumWorkItems());
		RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, MorphVertexBuffer.GetUAV());

		ClearUAV(RHICmdList, MorphVertexBuffer.GetUAV(), MorphVertexBuffer.GetUAVSize(), 0);

		{		
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_ApplyDelta);

			FVector4 MorphScale; 
			FVector4 InvMorphScale; 
			float InvTotalAccumulatedWeight;
			CalculateMorphDeltaBounds(MorphTargetWeights, MorphTargetVertexInfoBuffers, MorphScale, InvMorphScale, InvTotalAccumulatedWeight);
			
			TShaderMapRef<FGPUMorphUpdateCS> GPUMorphUpdateCS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
			for (uint32 i = 0; i < MorphTargetVertexInfoBuffers.GetNumMorphs(); i++)
			{
				uint32 NumMorphDeltas = MorphTargetVertexInfoBuffers.GetNumWorkItems(i);
				if (FMath::Abs(MorphTargetWeights[i]) > MorphTargetWeightCutoffThreshold && NumMorphDeltas > 0)
				{
					GPUMorphUpdateCS->SetParameters(RHICmdList, MorphScale, MorphTargetVertexInfoBuffers, MorphVertexBuffer);
					GPUMorphUpdateCS->SetOffsetAndSize(RHICmdList, MorphTargetVertexInfoBuffers.GetStartOffset(i), NumMorphDeltas, MorphTargetWeights[i]);
					GPUMorphUpdateCS->Dispatch(RHICmdList, NumMorphDeltas);
					RHICmdList.TransitionResource(EResourceTransitionAccess::ERWNoBarrier, EResourceTransitionPipeline::EComputeToCompute, MorphVertexBuffer.GetUAV());
				}
			}
			GPUMorphUpdateCS->EndAllDispatches(RHICmdList);
			RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, MorphVertexBuffer.GetUAV());

			TShaderMapRef<FGPUMorphNormalizeCS> GPUMorphNormalizeCS(GetGlobalShaderMap(ERHIFeatureLevel::SM5));
			GPUMorphNormalizeCS->Dispatch(RHICmdList, LodModel.NumVertices, InvMorphScale, InvTotalAccumulatedWeight, MorphVertexBuffer);
		}

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, MorphVertexBuffer.GetUAV());

		// set update flag
		MorphVertexBuffer.bHasBeenUpdated = true;
	}
}

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::UpdateMorphVertexBufferCPU(const TArray<FActiveMorphTarget>& ActiveMorphTargets, const TArray<float>& MorphTargetWeights)
{
	SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Update);

	if (IsValidRef(MorphVertexBuffer.VertexBufferRHI))
	{
		extern ENGINE_API bool DoRecomputeSkinTangentsOnGPU_RT();
		bool bBlendTangentsOnCPU = !DoRecomputeSkinTangentsOnGPU_RT();

		// LOD of the skel mesh is used to find number of vertices in buffer
		FStaticLODModel& LodModel = SkelMeshResource->LODModels[LODIndex];

		MorphVertexBuffer.RecreateResourcesIfRequired(GUseGPUMorphTargets != 0);

		uint32 Size = LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);

		FMorphGPUSkinVertex* Buffer = nullptr;
		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Alloc);
			Buffer = (FMorphGPUSkinVertex*)FMemory::Malloc(Size);
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Init);

			if (bBlendTangentsOnCPU)
			{
				// zero everything
				int32 vertsToAdd = static_cast<int32>(LodModel.NumVertices) - MorphAccumulatedWeightArray.Num();
				if (vertsToAdd > 0)
				{
					MorphAccumulatedWeightArray.AddUninitialized(vertsToAdd);
				}

				FMemory::Memzero(MorphAccumulatedWeightArray.GetData(), sizeof(float)*LodModel.NumVertices);
			}

			// PackedNormals will be wrong init with 0, but they'll be overwritten later
			FMemory::Memzero(&Buffer[0], sizeof(FMorphGPUSkinVertex)*LodModel.NumVertices);
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_ApplyDelta);

			// iterate over all active morph targets and accumulate their vertex deltas
			for (int32 AnimIdx = 0; AnimIdx < ActiveMorphTargets.Num(); AnimIdx++)
			{
				const FActiveMorphTarget& MorphTarget = ActiveMorphTargets[AnimIdx];
				checkSlow(MorphTarget.MorphTarget != NULL);
				checkSlow(MorphTarget.MorphTarget->HasDataForLOD(LODIndex));
				const float MorphTargetWeight = MorphTargetWeights[MorphTarget.WeightIndex];
				const float MorphAbsWeight = FMath::Abs(MorphTargetWeight);
				checkSlow(MorphAbsWeight >= MinMorphTargetBlendWeight && MorphAbsWeight <= MaxMorphTargetBlendWeight);


				// Get deltas
				int32 NumDeltas;
				FMorphTargetDelta* Deltas = MorphTarget.MorphTarget->GetMorphTargetDelta(LODIndex, NumDeltas);

				// iterate over the vertices that this lod model has changed
				for (int32 MorphVertIdx = 0; MorphVertIdx < NumDeltas; MorphVertIdx++)
				{
					const FMorphTargetDelta& MorphVertex = Deltas[MorphVertIdx];

					// @TODO FIXMELH : temp hack until we fix importing issue
					if ((MorphVertex.SourceIdx < LodModel.NumVertices))
					{
						FMorphGPUSkinVertex& DestVertex = Buffer[MorphVertex.SourceIdx];

						DestVertex.DeltaPosition += MorphVertex.PositionDelta * MorphTargetWeight;

						// todo: could be moved out of the inner loop to be more efficient
						if (bBlendTangentsOnCPU)
						{
							DestVertex.DeltaTangentZ += MorphVertex.TangentZDelta * MorphTargetWeight;
							// accumulate the weight so we can normalized it later
							MorphAccumulatedWeightArray[MorphVertex.SourceIdx] += MorphAbsWeight;
						}
					}
				} // for all vertices
			} // for all morph targets

			if (bBlendTangentsOnCPU)
			{
				// copy back all the tangent values (can't use Memcpy, since we have to pack the normals)
				for (uint32 iVertex = 0; iVertex < LodModel.NumVertices; ++iVertex)
				{
					FMorphGPUSkinVertex& DestVertex = Buffer[iVertex];
					float AccumulatedWeight = MorphAccumulatedWeightArray[iVertex];

					// if accumulated weight is >1.f
					// previous code was applying the weight again in GPU if less than 1, but it doesn't make sense to do so
					// so instead, we just divide by AccumulatedWeight if it's more than 1.
					// now DeltaTangentZ isn't FPackedNormal, so you can apply any value to it. 
					if (AccumulatedWeight > 1.f)
					{
						DestVertex.DeltaTangentZ /= AccumulatedWeight;
					}
				}
			}
		} // ApplyDelta

		// Lock the real buffer.
		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_RhiLockAndCopy);
			FMorphGPUSkinVertex* ActualBuffer = (FMorphGPUSkinVertex*)RHILockVertexBuffer(MorphVertexBuffer.VertexBufferRHI, 0, Size, RLM_WriteOnly);
			FMemory::Memcpy(ActualBuffer, Buffer, Size);
			FMemory::Free(Buffer);
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_RhiUnlock);
			// Unlock the buffer.
			RHIUnlockVertexBuffer(MorphVertexBuffer.VertexBufferRHI);
			// set update flag
			MorphVertexBuffer.bHasBeenUpdated = true;
		}
	}
}

const FVertexFactory* FSkeletalMeshObjectGPUSkin::GetSkinVertexFactory(const FSceneView* View, int32 LODIndex,int32 ChunkIdx) const
{
	checkSlow( LODs.IsValidIndex(LODIndex) );
	checkSlow( DynamicData );

	const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];
	const FSkeletalMeshObjectLOD& LOD = LODs[LODIndex];

	// cloth simulation is updated & if this ChunkIdx is for ClothVertexFactory
	if ( DynamicData->ClothingSimData.Num() > 0 
		&& LOD.GPUSkinVertexFactories.ClothVertexFactories.IsValidIndex(ChunkIdx)  
		&& LOD.GPUSkinVertexFactories.ClothVertexFactories[ChunkIdx].IsValid() )
	{
		return LOD.GPUSkinVertexFactories.ClothVertexFactories[ChunkIdx]->GetVertexFactory();
	}

	// If the GPU skinning cache was used, return the passthrough vertex factory
	if (SkinCacheEntry && FGPUSkinCache::IsEntryValid(SkinCacheEntry, ChunkIdx))
	{
		return LOD.GPUSkinVertexFactories.PassthroughVertexFactories[ChunkIdx].Get();
	}

	// use the morph enabled vertex factory if any active morphs are set
	if( DynamicData->NumWeightedActiveMorphTargets > 0 )
	{
		return LOD.GPUSkinVertexFactories.MorphVertexFactories[ChunkIdx].Get();
	}

	// use the default gpu skin vertex factory
	return LOD.GPUSkinVertexFactories.VertexFactories[ChunkIdx].Get();
}

FSkinWeightVertexBuffer* FSkeletalMeshObjectGPUSkin::GetSkinWeightVertexBuffer(int32 LODIndex) const
{
	checkSlow(LODs.IsValidIndex(LODIndex));
	return LODs[LODIndex].MeshObjectWeightBuffer;
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
template<class VertexFactoryType>
void InitGPUSkinVertexFactoryComponents(typename VertexFactoryType::FDataType* VertexFactoryData, 
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	typedef TGPUSkinVertexBase BaseVertexType;
	typedef TSkinWeightInfo<VertexFactoryType::HasExtraBoneInfluences> WeightInfoType;

	// tangents
	VertexFactoryData->TangentBasisComponents[0] = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,TangentX),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_PackedNormal);
	VertexFactoryData->TangentBasisComponents[1] = FVertexStreamComponent(
		VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(BaseVertexType,TangentZ),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_PackedNormal);
	
	// bone indices
	VertexFactoryData->BoneIndices = FVertexStreamComponent(
		VertexBuffers.SkinWeightVertexBuffer,STRUCT_OFFSET(WeightInfoType,InfluenceBones),VertexBuffers.SkinWeightVertexBuffer->GetStride(),VET_UByte4);
	// bone weights
	VertexFactoryData->BoneWeights = FVertexStreamComponent(
		VertexBuffers.SkinWeightVertexBuffer,STRUCT_OFFSET(WeightInfoType,InfluenceWeights),VertexBuffers.SkinWeightVertexBuffer->GetStride(),VET_UByte4N);

	if (VertexFactoryType::HasExtraBoneInfluences)
	{
		// Extra streams for bone indices & weights
		VertexFactoryData->ExtraBoneIndices = FVertexStreamComponent(
			VertexBuffers.SkinWeightVertexBuffer,STRUCT_OFFSET(WeightInfoType,InfluenceBones) + 4,VertexBuffers.SkinWeightVertexBuffer->GetStride(),VET_UByte4);
		VertexFactoryData->ExtraBoneWeights = FVertexStreamComponent(
			VertexBuffers.SkinWeightVertexBuffer,STRUCT_OFFSET(WeightInfoType,InfluenceWeights) + 4,VertexBuffers.SkinWeightVertexBuffer->GetStride(),VET_UByte4N);
	}

	// Add a texture coordinate for each texture coordinate set we have
	if( !VertexBuffers.VertexBufferGPUSkin->GetUseFullPrecisionUVs() )
	{
		typedef TGPUSkinVertexFloat16Uvs<MAX_TEXCOORDS> VertexType;
		VertexFactoryData->PositionComponent = FVertexStreamComponent(
			VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(VertexType,Position),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float3);

		for( uint32 UVIndex = 0; UVIndex < VertexBuffers.VertexBufferGPUSkin->GetNumTexCoords(); ++UVIndex )
		{
			VertexFactoryData->TextureCoordinates.Add(FVertexStreamComponent(
				VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(VertexType,UVs) + sizeof(FVector2DHalf) * UVIndex, VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Half2));
		}
	}
	else
	{
		typedef TGPUSkinVertexFloat32Uvs<MAX_TEXCOORDS> VertexType;
		VertexFactoryData->PositionComponent = FVertexStreamComponent(
			VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(VertexType,Position),VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float3);

		for( uint32 UVIndex = 0; UVIndex < VertexBuffers.VertexBufferGPUSkin->GetNumTexCoords(); ++UVIndex )
		{
			VertexFactoryData->TextureCoordinates.Add(FVertexStreamComponent(
				VertexBuffers.VertexBufferGPUSkin,STRUCT_OFFSET(VertexType,UVs) + sizeof(FVector2D) * UVIndex, VertexBuffers.VertexBufferGPUSkin->GetStride(),VET_Float2));
		}
	}

	// Color data may be NULL
	if( VertexBuffers.ColorVertexBuffer != NULL && 
		VertexBuffers.ColorVertexBuffer->IsInitialized() )
	{
		// Color
		VertexFactoryData->ColorComponent = FVertexStreamComponent(
			VertexBuffers.ColorVertexBuffer,0,VertexBuffers.ColorVertexBuffer->GetStride(),VET_Color);
	}
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
template<class VertexFactoryType>
void InitMorphVertexFactoryComponents(typename VertexFactoryType::FDataType* VertexFactoryData, 
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	// delta positions
	VertexFactoryData->DeltaPositionComponent = FVertexStreamComponent(
		VertexBuffers.MorphVertexBuffer,STRUCT_OFFSET(FMorphGPUSkinVertex,DeltaPosition),sizeof(FMorphGPUSkinVertex),VET_Float3);
	// delta normals
	VertexFactoryData->DeltaTangentZComponent = FVertexStreamComponent(
		VertexBuffers.MorphVertexBuffer, STRUCT_OFFSET(FMorphGPUSkinVertex, DeltaTangentZ), sizeof(FMorphGPUSkinVertex), VET_Float3);
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
template<class VertexFactoryType>
void InitAPEXClothVertexFactoryComponents(typename VertexFactoryType::FDataType* VertexFactoryData, 
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	// barycentric coord for positions
	VertexFactoryData->CoordPositionComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FMeshToMeshVertData,PositionBaryCoordsAndDist),sizeof(FMeshToMeshVertData),VET_Float4);
	// barycentric coord for normals
	VertexFactoryData->CoordNormalComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FMeshToMeshVertData,NormalBaryCoordsAndDist),sizeof(FMeshToMeshVertData),VET_Float4);
	// barycentric coord for tangents
	VertexFactoryData->CoordTangentComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FMeshToMeshVertData,TangentBaryCoordsAndDist),sizeof(FMeshToMeshVertData),VET_Float4);
	// indices for reference physics mesh vertices
	VertexFactoryData->SimulIndicesComponent = FVertexStreamComponent(
		VertexBuffers.APEXClothVertexBuffer,STRUCT_OFFSET(FMeshToMeshVertData, SourceMeshVertIndices),sizeof(FMeshToMeshVertData),VET_UShort4);
	VertexFactoryData->ClothBuffer = VertexBuffers.APEXClothVertexBuffer->GetSRV();
	VertexFactoryData->ClothIndexMapping = VertexBuffers.APEXClothVertexBuffer->GetClothIndexMapping();
}

/** 
 * Handles transferring data between game/render threads when initializing vertex factory components 
 */
template <class VertexFactoryType>
class TDynamicUpdateVertexFactoryData
{
public:
	TDynamicUpdateVertexFactoryData(
		VertexFactoryType* InVertexFactory,
		const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers)
		:	VertexFactory(InVertexFactory)
		,	VertexBuffers(InVertexBuffers)
	{}
	VertexFactoryType* VertexFactory;
	const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers VertexBuffers;
	
};

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
template <class VertexFactoryTypeBase, class VertexFactoryType>
static VertexFactoryType* CreateVertexFactory(TArray<TUniquePtr<VertexFactoryTypeBase>>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 ERHIFeatureLevel::Type FeatureLevel
						 )
{
	VertexFactoryType* VertexFactory = new VertexFactoryType(FeatureLevel);
	VertexFactories.Add(TUniquePtr<VertexFactoryTypeBase>(VertexFactory));

	// Setup the update data for enqueue
	TDynamicUpdateVertexFactoryData<VertexFactoryType> VertexUpdateData(VertexFactory,InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_RENDER_COMMAND(InitGPUSkinVertexFactory)(
		[VertexUpdateData](FRHICommandList& CmdList)
		{
			typename VertexFactoryType::FDataType Data;
			InitGPUSkinVertexFactoryComponents<VertexFactoryType>(&Data, VertexUpdateData.VertexBuffers);
			VertexUpdateData.VertexFactory->SetData(Data);
			VertexUpdateData.VertexFactory->GetShaderData().MeshOrigin = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshOrigin();
			VertexUpdateData.VertexFactory->GetShaderData().MeshExtension = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshExtension();
		}
	);

	// init rendering resource	
	BeginInitResource(VertexFactory);

	return VertexFactory;
}

template<typename VertexFactoryType>
static void CreatePassthroughVertexFactory(TArray<TUniquePtr<FGPUSkinPassthroughVertexFactory>>& PassthroughVertexFactories,
	VertexFactoryType* SourceVertexFactory)
{
	FGPUSkinPassthroughVertexFactory* NewPassthroughVertexFactory = new FGPUSkinPassthroughVertexFactory();
	PassthroughVertexFactories.Add(TUniquePtr<FGPUSkinPassthroughVertexFactory>(NewPassthroughVertexFactory));

	// update vertex factory components and sync it
	ENQUEUE_RENDER_COMMAND(InitPassthroughGPUSkinVertexFactory)(
		[NewPassthroughVertexFactory, SourceVertexFactory](FRHICommandList& RHICmdList)
		{
			SourceVertexFactory->CopyDataTypeForPassthroughFactory(NewPassthroughVertexFactory);
		}
	);

	// init rendering resource	
	BeginInitResource(NewPassthroughVertexFactory);
}

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
template <class VertexFactoryTypeBase, class VertexFactoryType>
static VertexFactoryType* CreateVertexFactoryMorph(TArray<TUniquePtr<VertexFactoryTypeBase>>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 ERHIFeatureLevel::Type FeatureLevel
						 )

{
	VertexFactoryType* VertexFactory = new VertexFactoryType(FeatureLevel);
	VertexFactories.Add(TUniquePtr<VertexFactoryTypeBase>(VertexFactory));
						
	// Setup the update data for enqueue
	TDynamicUpdateVertexFactoryData<VertexFactoryType> VertexUpdateData(VertexFactory, InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_RENDER_COMMAND(InitGPUSkinVertexFactoryMorph)(
		[VertexUpdateData](FRHICommandList& RHICmdList)
		{
			typename VertexFactoryType::FDataType Data;
			InitGPUSkinVertexFactoryComponents<VertexFactoryType>(&Data, VertexUpdateData.VertexBuffers);
			InitMorphVertexFactoryComponents<VertexFactoryType>(&Data, VertexUpdateData.VertexBuffers);
			VertexUpdateData.VertexFactory->SetData(Data);
			VertexUpdateData.VertexFactory->GetShaderData().MeshOrigin = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshOrigin();
			VertexUpdateData.VertexFactory->GetShaderData().MeshExtension = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshExtension();
		}
	);

	// init rendering resource	
	BeginInitResource(VertexFactory);

	return VertexFactory;
}

// APEX cloth

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
template <class VertexFactoryTypeBase, class VertexFactoryType>
static void CreateVertexFactoryCloth(TArray<TUniquePtr<VertexFactoryTypeBase>>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 ERHIFeatureLevel::Type FeatureLevel
						 )

{
	VertexFactoryType* VertexFactory = new VertexFactoryType(FeatureLevel);
	VertexFactories.Add(TUniquePtr<VertexFactoryTypeBase>(VertexFactory));
						
	// Setup the update data for enqueue
	TDynamicUpdateVertexFactoryData<VertexFactoryType> VertexUpdateData(VertexFactory, InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_RENDER_COMMAND(InitGPUSkinAPEXClothVertexFactory)(
		[VertexUpdateData](FRHICommandList& RHICmdList)
		{
			typename VertexFactoryType::FDataType Data;
			InitGPUSkinVertexFactoryComponents<VertexFactoryType>(&Data, VertexUpdateData.VertexBuffers);
			InitAPEXClothVertexFactoryComponents<VertexFactoryType>(&Data, VertexUpdateData.VertexBuffers);
			VertexUpdateData.VertexFactory->SetData(Data);
			VertexUpdateData.VertexFactory->GetShaderData().MeshOrigin = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshOrigin();
			VertexUpdateData.VertexFactory->GetShaderData().MeshExtension = VertexUpdateData.VertexBuffers.VertexBufferGPUSkin->GetMeshExtension();
		}
	);

	// init rendering resource	
	BeginInitResource(VertexFactory);
}

/**
 * Determine the current vertex buffers valid for the current LOD
 *
 * @param OutVertexBuffers output vertex buffers
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::GetVertexBuffers(FVertexFactoryBuffers& OutVertexBuffers, FStaticLODModel& LODModel)
{
	OutVertexBuffers.VertexBufferGPUSkin = &LODModel.VertexBufferGPUSkin;
	OutVertexBuffers.ColorVertexBuffer = MeshObjectColorBuffer;
	OutVertexBuffers.SkinWeightVertexBuffer = MeshObjectWeightBuffer;
	OutVertexBuffers.MorphVertexBuffer = &MorphVertexBuffer;
	OutVertexBuffers.APEXClothVertexBuffer = &LODModel.ClothVertexBuffer;
}

/** 
 * Init vertex factory resources for this LOD 
 *
 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
 * @param Chunks - relevant chunk information (either original or from swapped influence)
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshSection>& Sections, 
	ERHIFeatureLevel::Type InFeatureLevel)
{
	// first clear existing factories (resources assumed to have been released already)
	// then [re]create the factories

	VertexFactories.Empty(Sections.Num());
	{
		for(int32 FactoryIdx = 0; FactoryIdx < Sections.Num(); ++FactoryIdx)
		{
			if (VertexBuffers.SkinWeightVertexBuffer->HasExtraBoneInfluences())
			{
				TGPUSkinVertexFactory<true>* VertexFactory = CreateVertexFactory< FGPUBaseSkinVertexFactory, TGPUSkinVertexFactory<true> >(VertexFactories, VertexBuffers, InFeatureLevel);
				CreatePassthroughVertexFactory<TGPUSkinVertexFactory<true>>(PassthroughVertexFactories, VertexFactory);
			}
			else
			{
				TGPUSkinVertexFactory<false>* VertexFactory = CreateVertexFactory< FGPUBaseSkinVertexFactory, TGPUSkinVertexFactory<false> >(VertexFactories, VertexBuffers, InFeatureLevel);
				CreatePassthroughVertexFactory<TGPUSkinVertexFactory<false>>(PassthroughVertexFactories, VertexFactory);
			}
		}
	}
}

/** 
 * Release vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseVertexFactories()
{
	// Default factories
	for( int32 FactoryIdx=0; FactoryIdx < VertexFactories.Num(); FactoryIdx++)
	{
		BeginReleaseResource(VertexFactories[FactoryIdx].Get());
	}

	for (int32 FactoryIdx = 0; FactoryIdx < PassthroughVertexFactories.Num(); FactoryIdx++)
	{
		BeginReleaseResource(PassthroughVertexFactories[FactoryIdx].Get());
	}
}

void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitMorphVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshSection>& Sections,
	bool bInUsePerBoneMotionBlur,
	ERHIFeatureLevel::Type InFeatureLevel)
{
	// clear existing factories (resources assumed to have been released already)
	MorphVertexFactories.Empty(Sections.Num());
	for( int32 FactoryIdx=0; FactoryIdx < Sections.Num(); FactoryIdx++ )
	{
		if (VertexBuffers.SkinWeightVertexBuffer->HasExtraBoneInfluences())
		{
			CreateVertexFactoryMorph<FGPUBaseSkinVertexFactory, TGPUSkinMorphVertexFactory<true> >(MorphVertexFactories,VertexBuffers,InFeatureLevel);
		}
		else
		{
			CreateVertexFactoryMorph<FGPUBaseSkinVertexFactory, TGPUSkinMorphVertexFactory<false> >(MorphVertexFactories, VertexBuffers,InFeatureLevel);
		}
	}
}

/** 
 * Release morph vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseMorphVertexFactories()
{
	// Default morph factories
	for( int32 FactoryIdx=0; FactoryIdx < MorphVertexFactories.Num(); FactoryIdx++ )
	{
		BeginReleaseResource(MorphVertexFactories[FactoryIdx].Get());
	}
}

void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitAPEXClothVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshSection>& Sections,
	ERHIFeatureLevel::Type InFeatureLevel)
{
	// clear existing factories (resources assumed to have been released already)
	ClothVertexFactories.Empty(Sections.Num());
	for( int32 FactoryIdx=0; FactoryIdx < Sections.Num(); FactoryIdx++ )
	{
		if (Sections[FactoryIdx].HasClothingData() && InFeatureLevel >= ERHIFeatureLevel::SM4)
		{
			if (VertexBuffers.SkinWeightVertexBuffer->HasExtraBoneInfluences())
			{
				CreateVertexFactoryCloth<FGPUBaseSkinAPEXClothVertexFactory, TGPUSkinAPEXClothVertexFactory<true> >(ClothVertexFactories, VertexBuffers, InFeatureLevel);
			}
			else
			{
				CreateVertexFactoryCloth<FGPUBaseSkinAPEXClothVertexFactory, TGPUSkinAPEXClothVertexFactory<false> >(ClothVertexFactories, VertexBuffers, InFeatureLevel);
			}
		}
		else
		{
			ClothVertexFactories.Add(TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>(nullptr));
		}
	}
}

/** 
 * Release APEX cloth vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseAPEXClothVertexFactories()
{
	// Default APEX cloth factories
	for( int32 FactoryIdx=0; FactoryIdx < ClothVertexFactories.Num(); FactoryIdx++ )
	{
		TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>& ClothVertexFactory = ClothVertexFactories[FactoryIdx];
		if (ClothVertexFactory)
		{
			BeginReleaseResource(ClothVertexFactory->GetVertexFactory());
		}
	}
}

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::InitResources(const FSkelMeshObjectLODInfo& MeshLODInfo, FSkelMeshComponentLODInfo* CompLODInfo, ERHIFeatureLevel::Type InFeatureLevel)
{
	check(SkelMeshResource);
	check(SkelMeshResource->LODModels.IsValidIndex(LODIndex));

	// vertex buffer for each lod has already been created when skelmesh was loaded
	FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];
	
	// If we have a skin weight override buffer (and it's the right size) use it
	if (CompLODInfo &&
		CompLODInfo->OverrideSkinWeights &&
		CompLODInfo->OverrideSkinWeights->GetNumVertices() == LODModel.VertexBufferGPUSkin.GetNumVertices())
	{
		check(LODModel.SkinWeightVertexBuffer.HasExtraBoneInfluences() == CompLODInfo->OverrideSkinWeights->HasExtraBoneInfluences());
		MeshObjectWeightBuffer = CompLODInfo->OverrideSkinWeights;
	}
	else
	{
		MeshObjectWeightBuffer = &LODModel.SkinWeightVertexBuffer;
	}

	// If we have a vertex color override buffer (and it's the right size) use it
	if (CompLODInfo &&
		CompLODInfo->OverrideVertexColors &&
		CompLODInfo->OverrideVertexColors->GetNumVertices() == LODModel.VertexBufferGPUSkin.GetNumVertices())
	{
		MeshObjectColorBuffer = CompLODInfo->OverrideVertexColors;
	}
	else
	{
		MeshObjectColorBuffer = &LODModel.ColorVertexBuffer;
	}

	// Vertex buffers available for the LOD
	FVertexFactoryBuffers VertexBuffers;
	GetVertexBuffers(VertexBuffers, LODModel);

	// init gpu skin factories
	GPUSkinVertexFactories.InitVertexFactories(VertexBuffers,LODModel.Sections, InFeatureLevel);
	if ( LODModel.HasClothData() )
	{
		GPUSkinVertexFactories.InitAPEXClothVertexFactories(VertexBuffers,LODModel.Sections, InFeatureLevel);
	}
}

/** 
 * Release rendering resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::ReleaseResources()
{	
	// Release gpu skin vertex factories
	GPUSkinVertexFactories.ReleaseVertexFactories();

	// Release APEX cloth vertex factory
	GPUSkinVertexFactories.ReleaseAPEXClothVertexFactories();
}

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::InitMorphResources(const FSkelMeshObjectLODInfo& MeshLODInfo, bool bInUsePerBoneMotionBlur, ERHIFeatureLevel::Type InFeatureLevel)
{
	check(SkelMeshResource);
	check(SkelMeshResource->LODModels.IsValidIndex(LODIndex));

	// vertex buffer for each lod has already been created when skelmesh was loaded
	FStaticLODModel& LODModel = SkelMeshResource->LODModels[LODIndex];

	// init the delta vertex buffer for this LOD
	BeginInitResource(&MorphVertexBuffer);

	// Vertex buffers available for the LOD
	FVertexFactoryBuffers VertexBuffers;
	GetVertexBuffers(VertexBuffers,LODModel);
	// init morph skin factories
	GPUSkinVertexFactories.InitMorphVertexFactories(VertexBuffers, LODModel.Sections, bInUsePerBoneMotionBlur, InFeatureLevel);
}

/** 
* Release rendering resources for the morph stream of this LOD
*/
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::ReleaseMorphResources()
{
	// Release morph vertex factories
	GPUSkinVertexFactories.ReleaseMorphVertexFactories();
	// release the delta vertex buffer
	BeginReleaseResource(&MorphVertexBuffer);
}


TArray<FTransform>* FSkeletalMeshObjectGPUSkin::GetComponentSpaceTransforms() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(DynamicData)
	{
		return &(DynamicData->MeshComponentSpaceTransforms);
	}
	else
#endif
	{
		return NULL;
	}
}

const TArray<FMatrix>& FSkeletalMeshObjectGPUSkin::GetReferenceToLocalMatrices() const
{
	return DynamicData->ReferenceToLocal;
}

const FTwoVectors& FSkeletalMeshObjectGPUSkin::GetCustomLeftRightVectors(int32 SectionIndex) const
{
	if( DynamicData && DynamicData->CustomLeftRightVectors.IsValidIndex(SectionIndex) )
	{
		return DynamicData->CustomLeftRightVectors[SectionIndex];
	}
	else
	{
		static FTwoVectors Bad( FVector::ZeroVector, FVector(1.f,0.f,0.f) );
		return Bad;
	}
}

// @third party code - BEGIN HairWorks
FMorphVertexBuffer& FSkeletalMeshObjectGPUSkin::GetMorphVertexBuffer()
{
	// GetLOD() should be called in rendering thread to avoid crash. 
	return LODs[GetLOD()].MorphVertexBuffer;
}
// @third party code - END HairWorks

/*-----------------------------------------------------------------------------
FDynamicSkelMeshObjectDataGPUSkin
-----------------------------------------------------------------------------*/

void FDynamicSkelMeshObjectDataGPUSkin::Clear()
{
	ReferenceToLocal.Reset();
	CustomLeftRightVectors.Reset();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 
	MeshComponentSpaceTransforms.Reset();
#endif
	LODIndex = 0;
	ActiveMorphTargets.Reset();
	MorphTargetWeights.Reset();
	NumWeightedActiveMorphTargets = 0;
	ClothingSimData.Reset();
	ClothBlendWeight = 0.0f;
}


FDynamicSkelMeshObjectDataGPUSkin* FDynamicSkelMeshObjectDataGPUSkin::AllocDynamicSkelMeshObjectDataGPUSkin()
{
	return new FDynamicSkelMeshObjectDataGPUSkin;
}

void FDynamicSkelMeshObjectDataGPUSkin::FreeDynamicSkelMeshObjectDataGPUSkin(FDynamicSkelMeshObjectDataGPUSkin* Who)
{
	delete Who;
}

void FDynamicSkelMeshObjectDataGPUSkin::InitDynamicSkelMeshObjectDataGPUSkin(
	USkinnedMeshComponent* InMeshComponent,
	FSkeletalMeshResource* InSkeletalMeshResource,
	int32 InLODIndex,
	const TArray<FActiveMorphTarget>& InActiveMorphTargets,
	const TArray<float>& InMorphTargetWeights
	)
{
	LODIndex = InLODIndex;
	check(!ActiveMorphTargets.Num() && !ReferenceToLocal.Num() && !CustomLeftRightVectors.Num() && !ClothingSimData.Num() && !MorphTargetWeights.Num());

	// append instead of equals to avoid alloc
	ActiveMorphTargets.Append(InActiveMorphTargets);
	MorphTargetWeights.Append(InMorphTargetWeights);
	NumWeightedActiveMorphTargets = 0;

	// Gather any bones referenced by shadow shapes
	FSkeletalMeshSceneProxy* SkeletalMeshProxy = (FSkeletalMeshSceneProxy*)InMeshComponent->SceneProxy;
	const TArray<FBoneIndexType>* ExtraRequiredBoneIndices = SkeletalMeshProxy ? &SkeletalMeshProxy->GetSortedShadowBoneIndices() : nullptr;

	// update ReferenceToLocal
	UpdateRefToLocalMatrices( ReferenceToLocal, InMeshComponent, InSkeletalMeshResource, LODIndex, ExtraRequiredBoneIndices );
	UpdateCustomLeftRightVectors( CustomLeftRightVectors, InMeshComponent, InSkeletalMeshResource, LODIndex );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	check(!MeshComponentSpaceTransforms.Num());
	// append instead of equals to avoid alloc
	MeshComponentSpaceTransforms.Append(InMeshComponent->GetComponentSpaceTransforms());
#endif

	// find number of morphs that are currently weighted and will affect the mesh
	for( int32 MorphIdx=ActiveMorphTargets.Num()-1; MorphIdx >= 0; MorphIdx-- )
	{
		const FActiveMorphTarget& Morph = ActiveMorphTargets[MorphIdx];
		const float MorphTargetWeight = MorphTargetWeights[Morph.WeightIndex];
		const float MorphAbsWeight = FMath::Abs(MorphTargetWeight);

		if( Morph.MorphTarget != NULL && 
			MorphAbsWeight >= MinMorphTargetBlendWeight &&
			MorphAbsWeight <= MaxMorphTargetBlendWeight &&
			Morph.MorphTarget->HasDataForLOD(LODIndex) ) 
		{
			NumWeightedActiveMorphTargets++;
		}
		else
		{
			ActiveMorphTargets.RemoveAt(MorphIdx, 1, false);
		}
	}

	// Update the clothing simulation mesh positions and normals
	UpdateClothSimulationData(InMeshComponent);
}

bool FDynamicSkelMeshObjectDataGPUSkin::ActiveMorphTargetsEqual( const TArray<FActiveMorphTarget>& CompareActiveMorphTargets, const TArray<float>& CompareMorphTargetWeights)
{
	bool Result=true;
	if( CompareActiveMorphTargets.Num() == ActiveMorphTargets.Num() )
	{
		for( int32 MorphIdx=0; MorphIdx < ActiveMorphTargets.Num(); MorphIdx++ )
		{
			const FActiveMorphTarget& Morph = ActiveMorphTargets[MorphIdx];
			const FActiveMorphTarget& CompMorph = CompareActiveMorphTargets[MorphIdx];

			if( Morph.MorphTarget != CompMorph.MorphTarget ||
				FMath::Abs(MorphTargetWeights[Morph.WeightIndex] - CompareMorphTargetWeights[CompMorph.WeightIndex]) >= GMorphTargetWeightThreshold)
			{
				Result=false;
				break;
			}
		}
	}
	else
	{
		Result = false;
	}
	return Result;
}

bool FDynamicSkelMeshObjectDataGPUSkin::UpdateClothSimulationData(USkinnedMeshComponent* InMeshComponent)
{
	USkeletalMeshComponent* SimMeshComponent = Cast<USkeletalMeshComponent>(InMeshComponent);

	if (SimMeshComponent)
	{
		if(SimMeshComponent->bDisableClothSimulation)
		{
			ClothBlendWeight = 0.0f;
			ClothingSimData.Reset();
		}
		else
		{
			ClothBlendWeight = SimMeshComponent->ClothBlendWeight;
			ClothingSimData = SimMeshComponent->GetCurrentClothingData_GameThread();
		}

		return true;
	}
	return false;
}
