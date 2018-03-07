// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraRenderer.h"
#include "ParticleResources.h"
#include "NiagaraSpriteVertexFactory.h"
#include "NiagaraDataSet.h"
#include "NiagaraStats.h"

DECLARE_CYCLE_STAT(TEXT("Generate Sprite Vertex Data"), STAT_NiagaraGenSpriteVertexData, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("Render Sprites"), STAT_NiagaraRenderSprites, STATGROUP_Niagara);

DECLARE_CYCLE_STAT(TEXT("Genereate GPU Buffers"), STAT_NiagaraGenGpuBuffers, STATGROUP_Niagara);


/**
* Per-particle data sent to the GPU.
*/
struct FNiagaraSpriteVertex
{
	/** The position of the particle. */
	FVector Position;
	/** The relative time of the particle. */
	float RelativeTime;
	/** The previous position of the particle. */
	FVector	OldPosition;
	/** Value that remains constant over the lifetime of a particle. */
	float ParticleId;
	/** The size of the particle. */
	FVector2D Size;
	/** The rotation of the particle. */
	float Rotation;
	/** The sub-image index for the particle. */
	float SubImageIndex;
	/** The color of the particle. */
	FLinearColor Color;
	/* Custom Alignment vector*/
	FVector CustomAlignmentVector;
	/* Custom Facing vector*/
	FVector CustomFacingVector;
};


struct FNiagaraDynamicDataSprites : public FNiagaraDynamicDataBase
{
	TArray<FNiagaraSpriteVertex> VertexData;
	TArray<FParticleVertexDynamicParameter> MaterialParameterVertexData;
	const FNiagaraDataSet *DataSet;
	//uint32 ComponentBufferSize;
	int32 PositionDataOffset;
	int32 VelocityDataOffset;
	int32 SizeDataOffset;
	int32 RotationDataOffset;
	int32 SubimageDataOffset;
	int32 ColorDataOffset;
	int32 FacingOffset;
	int32 AlignmentOffset;
	//int32 NumInstances;
	bool bCustomAlignmentAvailable;
};



/* Mesh collector classes */
class FNiagaraMeshCollectorResourcesSprite : public FOneFrameResource
{
public:
	FNiagaraSpriteVertexFactory VertexFactory;
	FNiagaraSpriteUniformBufferRef UniformBuffer;

	virtual ~FNiagaraMeshCollectorResourcesSprite()
	{
		VertexFactory.ReleaseResource();
	}
};



NiagaraRendererSprites::NiagaraRendererSprites(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *InProps) :
	NiagaraRenderer()
{
	//check(InProps);
	VertexFactory = new FNiagaraSpriteVertexFactory(NVFT_Sprite, FeatureLevel);
	Properties = Cast<UNiagaraSpriteRendererProperties>(InProps);
}


void NiagaraRendererSprites::ReleaseRenderThreadResources()
{
	VertexFactory->ReleaseResource();
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

void NiagaraRendererSprites::CreateRenderThreadResources()
{
	VertexFactory->SetNumVertsInInstanceBuffer(4);
	VertexFactory->InitResource();
}

void NiagaraRendererSprites::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRender);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraRenderSprites);

	SimpleTimer MeshElementsTimer;

	//check(DynamicDataRender)

	FNiagaraDynamicDataSprites *DynamicDataSprites = static_cast<FNiagaraDynamicDataSprites*>(DynamicDataRender);
	if (!DynamicDataSprites || DynamicDataSprites->DataSet->PrevDataRender().GetNumInstances()==0 || nullptr == Properties)
	{
		return;
	}

	int32 NumInstances = DynamicDataSprites->DataSet->PrevDataRender().GetNumInstances();

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());

	int32 SizeInBytes = DynamicDataSprites->VertexData.GetTypeSize() * DynamicDataSprites->VertexData.Num();
	FGlobalDynamicVertexBuffer::FAllocation LocalDynamicVertexMaterialParamsAllocation;

	if (DynamicDataSprites->MaterialParameterVertexData.Num() > 0)
	{
		int32 MatParamSizeInBytes = DynamicDataSprites->MaterialParameterVertexData.GetTypeSize() * DynamicDataSprites->MaterialParameterVertexData.Num();
		LocalDynamicVertexMaterialParamsAllocation = FGlobalDynamicVertexBuffer::Get().Allocate(MatParamSizeInBytes);

		if (LocalDynamicVertexMaterialParamsAllocation.IsValid())
		{
			// Copy the extra material vertex data over.
			FMemory::Memcpy(LocalDynamicVertexMaterialParamsAllocation.Buffer, DynamicDataSprites->MaterialParameterVertexData.GetData(), MatParamSizeInBytes);
		}
	}

	// Update the primitive uniform buffer if needed.
	if (!WorldSpacePrimitiveUniformBuffer.IsInitialized())
	{
		FPrimitiveUniformShaderParameters PrimitiveUniformShaderParameters = GetPrimitiveUniformShaderParameters(
			FMatrix::Identity,
			SceneProxy->GetActorPosition(),
			SceneProxy->GetBounds(),
			SceneProxy->GetLocalBounds(),
			SceneProxy->ReceivesDecals(),
			false,
			false,
			SceneProxy->UseSingleSampleShadowFromStationaryLights(),
			SceneProxy->GetScene().HasPrecomputedVolumetricLightmap_RenderThread(),
			SceneProxy->UseEditorDepthTest(),
			SceneProxy->GetLightingChannelMask()
			);
		WorldSpacePrimitiveUniformBuffer.SetContents(PrimitiveUniformShaderParameters);
		WorldSpacePrimitiveUniformBuffer.InitResource();
	}


	// Compute the per-view uniform buffers.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			FGlobalDynamicVertexBuffer::FAllocation LocalDynamicVertexAllocation = FGlobalDynamicVertexBuffer::Get().Allocate(SizeInBytes);
			//if (LocalDynamicVertexAllocation.IsValid())
			{
				const FSceneView* View = Views[ViewIndex];
				FMatrix TransMat;
				/*
				if (Properties->SortMode == ENiagaraSortMode::SortViewDepth)
				{
					TransMat = View->ViewMatrices.GetViewProjectionMatrix();

					// view depth sorting
					DynamicDataSprites->VertexData.Sort(
						[&TransMat](const FNiagaraSpriteVertex& A, const FNiagaraSpriteVertex& B) {
						float W1 = TransMat.TransformPosition(A.Position).W;
						float W2 = TransMat.TransformPosition(B.Position).W;
						return (W1 > W2);
					});
				}
				else if (Properties->SortMode == ENiagaraSortMode::SortViewDistance)
				{
					FVector ViewLoc = View->ViewLocation;
					// dist sorting
					DynamicDataSprites->VertexData.Sort(
						[&ViewLoc](const FNiagaraSpriteVertex& A, const FNiagaraSpriteVertex& B) {
						float D1 = (ViewLoc - A.Position).SizeSquared();
						float D2 = (ViewLoc - B.Position).SizeSquared();
						return (D1 > D2);
					});
				}

				// Copy the vertex data over.
				*/
				//FMemory::Memcpy(LocalDynamicVertexAllocation.Buffer, DynamicDataSprites->VertexData.GetData(), SizeInBytes);

				FNiagaraMeshCollectorResourcesSprite& CollectorResources = Collector.AllocateOneFrameResource<FNiagaraMeshCollectorResourcesSprite>();
				FNiagaraSpriteUniformParameters PerViewUniformParameters;// = UniformParameters;
				PerViewUniformParameters.RotationBias = 0.0f;
				PerViewUniformParameters.RotationScale = 1.0f;
				PerViewUniformParameters.TangentSelector = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
				PerViewUniformParameters.InvDeltaSeconds = 30.0f;
				PerViewUniformParameters.NormalsType = 0.0f;
				PerViewUniformParameters.NormalsSphereCenter = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
				PerViewUniformParameters.NormalsCylinderUnitDirection = FVector4(0.0f, 0.0f, 1.0f, 0.0f);
				PerViewUniformParameters.PivotOffset = FVector2D(-0.5f, -0.5f);
				PerViewUniformParameters.MacroUVParameters = FVector4(0.0f, 0.0f, 1.0f, 1.0f);
				PerViewUniformParameters.CameraFacingBlend = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
				PerViewUniformParameters.RemoveHMDRoll = 0.0f;
				PerViewUniformParameters.CustomFacingVectorMask = Properties->CustomFacingVectorMask;
				PerViewUniformParameters.SubImageSize = FVector4(Properties->SubImageSize.X, Properties->SubImageSize.Y, 1.0f / Properties->SubImageSize.X, 1.0f / Properties->SubImageSize.Y);
				PerViewUniformParameters.PositionDataOffset = DynamicDataSprites->PositionDataOffset;
				PerViewUniformParameters.VelocityDataOffset = DynamicDataSprites->VelocityDataOffset;
				PerViewUniformParameters.RotationDataOffset = DynamicDataSprites->RotationDataOffset;
				PerViewUniformParameters.SizeDataOffset = DynamicDataSprites->SizeDataOffset;
				PerViewUniformParameters.ColorDataOffset = DynamicDataSprites->ColorDataOffset;
				PerViewUniformParameters.SubimageDataOffset = DynamicDataSprites->SubimageDataOffset;
				PerViewUniformParameters.FacingOffset = DynamicDataSprites->FacingOffset;
				PerViewUniformParameters.AlignmentOffset = DynamicDataSprites->AlignmentOffset;
				if (Properties->Alignment == ENiagaraSpriteAlignment::VelocityAligned)
				{
					// velocity aligned
					PerViewUniformParameters.RotationScale = 0.0f;
					PerViewUniformParameters.TangentSelector = FVector4(0.0f, 1.0f, 0.0f, 0.0f);
				}

				// Collector.AllocateOneFrameResource uses default ctor, initialize the vertex factory

				// use custom alignment if the data is available and if it's set in the props
				bool bUseCustomAlignment = DynamicDataSprites->bCustomAlignmentAvailable && Properties->Alignment == ENiagaraSpriteAlignment::CustomAlignment;
				bool bUseVectorAlignment = Properties->Alignment != ENiagaraSpriteAlignment::Unaligned;
				
				CollectorResources.VertexFactory.SetParticleData(DynamicDataSprites->DataSet);
				CollectorResources.VertexFactory.SetCustomAlignment(bUseCustomAlignment);
				CollectorResources.VertexFactory.SetVectorAligned(bUseVectorAlignment);
				CollectorResources.VertexFactory.SetCameraPlaneFacing(Properties->FacingMode == ENiagaraSpriteFacingMode::FaceCameraPlane);

				CollectorResources.VertexFactory.SetFeatureLevel(ViewFamily.GetFeatureLevel());
				CollectorResources.VertexFactory.SetParticleFactoryType(NVFT_Sprite);


				CollectorResources.UniformBuffer = FNiagaraSpriteUniformBufferRef::CreateUniformBufferImmediate(PerViewUniformParameters, UniformBuffer_SingleFrame);

				CollectorResources.VertexFactory.SetNumVertsInInstanceBuffer(4);
				CollectorResources.VertexFactory.InitResource();
				CollectorResources.VertexFactory.SetSpriteUniformBuffer(CollectorResources.UniformBuffer);

				CollectorResources.VertexFactory.SetInstanceBuffer(
					/*LocalDynamicVertexAllocation.VertexBuffer*/nullptr,
					LocalDynamicVertexAllocation.VertexOffset,
					sizeof(FNiagaraSpriteVertex),
					true
					);


				/*if (DynamicDataSprites->MaterialParameterVertexData.Num() > 0 && LocalDynamicVertexMaterialParamsAllocation.IsValid())
				{
					CollectorResources.VertexFactory.SetDynamicParameterBuffer(
						LocalDynamicVertexMaterialParamsAllocation.VertexBuffer,
						LocalDynamicVertexMaterialParamsAllocation.VertexOffset,
						sizeof(FParticleVertexDynamicParameter),
						true);
				}
				else*/
				{
					CollectorResources.VertexFactory.SetDynamicParameterBuffer(NULL, 0, 0, true);
				}

				FMeshBatch& MeshBatch = Collector.AllocateMesh();
				MeshBatch.VertexFactory = &CollectorResources.VertexFactory;
				MeshBatch.CastShadow = SceneProxy->CastsDynamicShadow();
				MeshBatch.bUseAsOccluder = false;
				MeshBatch.ReverseCulling = SceneProxy->IsLocalToWorldDeterminantNegative();
				MeshBatch.Type = PT_TriangleList;
				MeshBatch.DepthPriorityGroup = SceneProxy->GetDepthPriorityGroup(View);
				MeshBatch.bCanApplyViewModeOverrides = true;
				MeshBatch.bUseWireframeSelectionColoring = SceneProxy->IsSelected();

				if (bIsWireframe)
				{
					MeshBatch.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(SceneProxy->IsSelected(), SceneProxy->IsHovered());
				}
				else
				{
					MeshBatch.MaterialRenderProxy = MaterialRenderProxy;
				}

				FMeshBatchElement& MeshElement = MeshBatch.Elements[0];
				MeshElement.IndexBuffer = &GParticleIndexBuffer;
				MeshElement.FirstIndex = 0;
				MeshElement.NumPrimitives = 2;
				MeshElement.NumInstances = FMath::Max(0, NumInstances);	//->VertexData.Num();
				MeshElement.MinVertexIndex = 0;
				MeshElement.MaxVertexIndex = 0;// MeshElement.NumInstances * 4 - 1;
				MeshElement.PrimitiveUniformBufferResource = &WorldSpacePrimitiveUniformBuffer;
				
				
				Collector.AddMesh(ViewIndex, MeshBatch);
				
			}
		}
	}

	CPUTimeMS += MeshElementsTimer.GetElapsedMilliseconds();
}

bool NiagaraRendererSprites::SetMaterialUsage()
{
	//Causes deadlock :S Need to look at / rework the setting of materials and render modules.
	return Material && Material->CheckMaterialUsage_Concurrent(MATUSAGE_NiagaraSprites);
}

/** Update render data buffer from attributes */
FNiagaraDynamicDataBase *NiagaraRendererSprites::GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target)
{
	FNiagaraDynamicDataSprites *DynamicData = nullptr;

	if (Data.PrevData().GetNumInstances() > 0)
	{
		SimpleTimer VertexDataTimer;

		SCOPE_CYCLE_COUNTER(STAT_NiagaraGenSpriteVertexData);

		const FNiagaraVariableLayoutInfo* PositionLayout = Data.GetVariableLayout(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		const FNiagaraVariableLayoutInfo* VelocityLayout = Data.GetVariableLayout(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
		const FNiagaraVariableLayoutInfo* RotationLayout = Data.GetVariableLayout(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("SpriteRotation")));
		const FNiagaraVariableLayoutInfo* SizeLayout = Data.GetVariableLayout(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("SpriteSize")));
		const FNiagaraVariableLayoutInfo* ColorLayout = Data.GetVariableLayout(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));

		if (!bEnabled || !PositionLayout || !VelocityLayout || !RotationLayout || !SizeLayout || !ColorLayout)
		{
			return nullptr;
		}


		// required attributes //TODO: This does not need to be done every frame. Losing not insignificant time getting the layout ptrs.
		DynamicData = new FNiagaraDynamicDataSprites;
		DynamicData->PositionDataOffset = PositionLayout->FloatComponentStart;
		DynamicData->VelocityDataOffset = VelocityLayout->FloatComponentStart;
		DynamicData->RotationDataOffset = RotationLayout->FloatComponentStart;
		DynamicData->SizeDataOffset = SizeLayout->FloatComponentStart;
		DynamicData->ColorDataOffset = ColorLayout->FloatComponentStart;

		// optional attributes; we pass -1 as the offset so the VF can branch
		int32 IntDummy;
		Data.GetVariableComponentOffsets(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Facing")), DynamicData->FacingOffset, IntDummy);
		Data.GetVariableComponentOffsets(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Alignment")), DynamicData->AlignmentOffset, IntDummy);
		Data.GetVariableComponentOffsets(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("SubImageIndex")), DynamicData->SubimageDataOffset, IntDummy);


		// if we're CPU simulating, need to init the GPU buffers for the vertex factory
			//This is a killer for perf. Assume its all the GPU buffer allocation.
			//Need to allocate big shared buffer and allocate ranges out of those.
		SCOPE_CYCLE_COUNTER(STAT_NiagaraGenGpuBuffers);
		if (Target == ENiagaraSimTarget::CPUSim)
		{
			Data.ValidateBufferIndices();
			Data.InitGPUFromCPU();
		}

		DynamicData->DataSet = &Data;
		CPUTimeMS = VertexDataTimer.GetElapsedMilliseconds();
	}

	return DynamicData;  // for VF that can fetch from particle data directly
}



void NiagaraRendererSprites::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	check(IsInRenderingThread());

	if (DynamicDataRender)
	{
		delete static_cast<FNiagaraDynamicDataSprites*>(DynamicDataRender);
		DynamicDataRender = NULL;
	}
	DynamicDataRender = NewDynamicData;
}

int NiagaraRendererSprites::GetDynamicDataSize()
{
	uint32 Size = sizeof(FNiagaraDynamicDataSprites);
	if (DynamicDataRender)
	{
		Size += (static_cast<FNiagaraDynamicDataSprites*>(DynamicDataRender))->VertexData.GetAllocatedSize();
	}

	return Size;
}

bool NiagaraRendererSprites::HasDynamicData()
{
//	return DynamicDataRender && static_cast<FNiagaraDynamicDataSprites*>(DynamicDataRender)->VertexData.Num() > 0;
	return DynamicDataRender!=nullptr;// && static_cast<FNiagaraDynamicDataSprites*>(DynamicDataRender)->NumInstances > 0;
}

#if WITH_EDITORONLY_DATA

const TArray<FNiagaraVariable>& NiagaraRendererSprites::GetRequiredAttributes()
{
	return Properties->GetRequiredAttributes();
}

const TArray<FNiagaraVariable>& NiagaraRendererSprites::GetOptionalAttributes()
{
	return Properties->GetOptionalAttributes();
}

#endif