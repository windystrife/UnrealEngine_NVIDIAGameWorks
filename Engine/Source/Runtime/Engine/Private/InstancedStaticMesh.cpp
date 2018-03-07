// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InstancedStaticMesh.cpp: Static mesh rendering code.
=============================================================================*/

#include "InstancedStaticMesh.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Components/LightComponent.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "UnrealEngine.h"
#include "AI/NavigationSystemHelpers.h"
#include "AI/Navigation/NavCollision.h"
#include "AI/NavigationOctree.h"
#include "ShaderParameterUtils.h"
#include "Misc/UObjectToken.h"
#include "PhysXPublic.h"
#include "PhysicsEngine/PhysXSupport.h"
#include "PhysicsSerializer.h"
#include "PhysicsEngine/BodySetup.h"
#include "GameFramework/WorldSettings.h"
#include "ComponentRecreateRenderStateContext.h"
#include "SceneManagement.h"

const int32 InstancedStaticMeshMaxTexCoord = 8;

IMPLEMENT_HIT_PROXY(HInstancedStaticMeshInstance, HHitProxy);

TAutoConsoleVariable<int32> CVarMinLOD(
	TEXT("foliage.MinLOD"),
	-1,
	TEXT("Used to discard the top LODs for performance evaluation. -1: Disable all effects of this cvar."));

static TAutoConsoleVariable<int32> CVarASyncInstaneBufferConversion(
	TEXT("foliage.ASyncInstaneBufferConversion"),
	1,
	TEXT("If this is > 0, then build game instance buffering async during streaming. This is not thought to be a long term solution to this problem."));

/** InstancedStaticMeshInstance hit proxy */
void HInstancedStaticMeshInstance::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Component);
}

FStaticMeshInstanceBuffer::FStaticMeshInstanceBuffer(ERHIFeatureLevel::Type InFeatureLevel, bool InIsDynamic, bool InRequireCPUAccess)
	: InstanceData(NULL)
	, Stride(0)
	, NumInstances(0)
	, IsDynamic(InIsDynamic)
	, RequireCPUAccess(InRequireCPUAccess)
{
	SetFeatureLevel(InFeatureLevel);
}

FStaticMeshInstanceBuffer::~FStaticMeshInstanceBuffer()
{
	CleanUp();
}

/** Delete existing resources */
void FStaticMeshInstanceBuffer::CleanUp()
{
	if (InstanceData)
	{
		delete InstanceData;
		InstanceData = NULL;
	}
	NumInstances = 0;
}


void FStaticMeshInstanceBuffer::SetupCPUAccess(UInstancedStaticMeshComponent* InComponent)
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));

	const bool bNeedsCPUAccess = (InComponent->CastShadow && InComponent->bAffectDistanceFieldLighting 
		// Distance field algorithms need access to instance data on the CPU
		&& (CVar->GetValueOnAnyThread(true) != 0 || (InComponent->GetStaticMesh() && InComponent->GetStaticMesh()->bGenerateMeshDistanceField)))
		// If the buffer is created dynamically it mean we want to be able to do partial data update, so data should be accessible CPU side (i.e not trashed after creation of RHI resource)
		|| IsDynamic
		|| RequireCPUAccess;

	InstanceData->SetAllowCPUAccess(InstanceData->GetAllowCPUAccess() || bNeedsCPUAccess);
}

void FStaticMeshInstanceBuffer::UpdateInstanceData(UInstancedStaticMeshComponent* InComponent, const TArray<TRefCountPtr<HHitProxy> >& InHitProxies, int32 UpdateInstanceStartingIndex, int32 UpdateInstanceIndexCount)
{
	if (UpdateInstanceIndexCount == 0)
	{
		return;
	}

	bool bUpdateAllInstances = UpdateInstanceIndexCount == InComponent->PerInstanceSMData.Num();
	bool bUseRemapTable = InComponent->InstanceReorderTable.Num() > 0;

	int32 NumRenderInstances = InComponent->PerInstanceSMData.Num() - InComponent->RemovedInstances.Num();

	// Allocate the vertex data storage type.
	InstanceData->AllocateInstances(NumRenderInstances, false);

	NumInstances = InstanceData->NumInstances();

	const FMeshMapBuildData* MeshMapBuildData = NULL;

	if (InComponent->LODData.Num() > 0)
	{
		MeshMapBuildData = InComponent->GetMeshMapBuildData(InComponent->LODData[0]);
	}

	for (int32 InstanceIndex = UpdateInstanceStartingIndex; InstanceIndex < UpdateInstanceStartingIndex + UpdateInstanceIndexCount; ++InstanceIndex)
	{
		if (InComponent->PerInstanceSMData.IsValidIndex(InstanceIndex))
		{
			const FInstancedStaticMeshInstanceData& Instance = InComponent->PerInstanceSMData[InstanceIndex];
			int32 DestInstanceIndex = InstanceIndex;

			if (bUseRemapTable)
			{
				DestInstanceIndex = InComponent->InstanceReorderTable.IsValidIndex(InstanceIndex) ? InComponent->InstanceReorderTable[InstanceIndex] : INDEX_NONE;
			}

			if (DestInstanceIndex == INDEX_NONE)
			{
				DestInstanceIndex = InstanceData->GetNextAvailableInstanceIndex();
				
				if (DestInstanceIndex != INDEX_NONE)
				{
					InComponent->InstanceReorderTable.Add(DestInstanceIndex);
				}
			}
			else
			{
				if (InComponent->RemovedInstances.Contains(DestInstanceIndex))
				{
					continue;
				}
			}

			if (DestInstanceIndex != INDEX_NONE && InstanceData->IsValidIndex(DestInstanceIndex))
			{
				FVector2D LightmapUVBias = Instance.LightmapUVBias_DEPRECATED;
				FVector2D ShadowmapUVBias = Instance.ShadowmapUVBias_DEPRECATED;

				if (MeshMapBuildData != nullptr && MeshMapBuildData->PerInstanceLightmapData.IsValidIndex(InstanceIndex))
				{
					LightmapUVBias = MeshMapBuildData->PerInstanceLightmapData[InstanceIndex].LightmapUVBias;
					ShadowmapUVBias = MeshMapBuildData->PerInstanceLightmapData[InstanceIndex].ShadowmapUVBias;
				}

				InstanceData->SetInstance(DestInstanceIndex, Instance.Transform, RandomStream.GetFraction(), LightmapUVBias, ShadowmapUVBias);

				#if WITH_EDITOR
					if (GIsEditor)
					{
						// Record if the instance is selected
						FColor HitProxyColor(ForceInit);
						bool bSelected = InComponent->SelectedInstances.IsValidIndex(InstanceIndex) && InComponent->SelectedInstances[InstanceIndex];

						if (InHitProxies.IsValidIndex(InstanceIndex))
						{
							HitProxyColor = InHitProxies[InstanceIndex]->Id.GetColor();
						}

						InstanceData->SetInstanceEditorData(DestInstanceIndex, HitProxyColor, bSelected);
					}
				#endif

				if (!bUpdateAllInstances)
				{
					//TODO: to uncomment when RHI interface support proper vertex buffer partial update lock/unlock
				//	InComponent->NeedUpdatingInstanceIndexList.Add(DestInstanceIndex);
				}
			}
		}
		else if (InstanceData->IsValidIndex(InstanceIndex))
		{
			InstanceData->NullifyInstance(InstanceIndex);
		}
	}

	// Hide any removed instances
	for (int32 Index = 0; Index < InComponent->RemovedInstances.Num(); Index++)
	{
		int32 RenderIndex = InComponent->RemovedInstances[Index];

		if (InstanceData->IsValidIndex(RenderIndex))
		{
			InstanceData->NullifyInstance(RenderIndex);
		}
	}

	InComponent->RemovedInstances.Reset();
}

void FStaticMeshInstanceBuffer::Init(UInstancedStaticMeshComponent* InComponent, const TArray<TRefCountPtr<HHitProxy> >& InHitProxies, bool InitializeBufferFromData)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FStaticMeshInstanceBuffer_Init);
	
	double StartTime = FPlatformTime::Seconds();

	// Allocate the vertex data storage type.
	AllocateData();

	SetupCPUAccess(InComponent);

	// Setup our random number generator such that random values are generated consistently for any
	// given instance index between reattaches
	check(InComponent->InstancingRandomSeed != 0)
	RandomStream.Initialize(InComponent->InstancingRandomSeed);

	if (InitializeBufferFromData)
	{
		UpdateInstanceData(InComponent, InHitProxies, 0, InComponent->PerInstanceSMData.Num());
	}

	float ThisTime = (StartTime - FPlatformTime::Seconds()) * 1000.0f;
	if (ThisTime > 30.0f)
	{
		UE_LOG(LogStaticMesh, Display, TEXT("Took %6.2fms to set up instance buffer for %d instances for component %s."), ThisTime, InComponent->PerInstanceSMData.Num(), *InComponent->GetFullName());
	}
}

void FStaticMeshInstanceBuffer::InitFromPreallocatedData(UInstancedStaticMeshComponent* InComponent, FStaticMeshInstanceData& Other, bool InRequireCPUAccess)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FStaticMeshInstanceBuffer_InitFromPreallocatedData);

	// Setup our random number generator such that random values are generated consistently for any
	// given instance index between reattaches
	RandomStream.Initialize(InComponent->InstancingRandomSeed);

	const uint32 NewNumInstances = Other.NumInstances();
	AllocateData(Other);
	NumInstances = NewNumInstances;
	RequireCPUAccess = InRequireCPUAccess;
	SetupCPUAccess(InComponent);
}

/**
 * Specialized assignment operator, only used when importing LOD's.  
 */
void FStaticMeshInstanceBuffer::operator=(const FStaticMeshInstanceBuffer &Other)
{
	checkf(0, TEXT("Unexpected assignment call"));
}

void FStaticMeshInstanceBuffer::InitRHI()
{
	check(InstanceData);

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FStaticMeshInstanceBuffer_InitRHI);

	FResourceArrayInterface* ResourceArray = InstanceData->GetResourceArray();
	if(ResourceArray->GetResourceDataSize() > 0)
	{
		// TODO: possibility over allocated the vertex buffer when we support partial update for when working in the editor
		FRHIResourceCreateInfo CreateInfo(ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer(ResourceArray->GetResourceDataSize(), IsDynamic ? BUF_Dynamic : BUF_Static, CreateInfo);
	}
}

void FStaticMeshInstanceBuffer::UpdateRHIVertexBuffer(const TSet<int32>& InIndexList)
{
	check(IsInRenderingThread());

	if (InstanceData->NumInstances() == 0)
	{
		return;
	}

	//TODO: to uncomment when RHI interface support proper vertex buffer partial update lock/unlock
//	if (InIndexList.Num() == 0 || InIndexList.Num() == InstanceData->NumInstances())
	{
		UpdateRHIVertexBuffer(0, InstanceData->NumInstances(), InstanceData->GetResourceSize(1));
	}
	/*else
	{
		for (const int32& Index : InIndexList)
		{
			UpdateRHIVertexBuffer(Index, 1, InstanceData->GetResourceSize(1));
		}
	}
	*/
}

void FStaticMeshInstanceBuffer::UpdateRHIVertexBuffer(int32 StartingIndex, uint32 InstanceCount, uint32 InstanceSize)
{
	check(IsInRenderingThread());

	if (!IsInitialized())
	{
		return;
	}

	if (IsDynamic)
	{
		uint32 UpdateSize = InstanceCount * InstanceSize;
		uint32 UpdateOffset = StartingIndex * InstanceSize;

		if (UpdateOffset + UpdateSize <= VertexBufferRHI->GetSize()) // we can only update the buffer
		{
			void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, UpdateOffset, UpdateSize, RLM_WriteOnly);
			FResourceArrayInterface* ResourceArray = InstanceData->GetResourceArray();

			FMemory::Memcpy(VertexBufferData, (uint8*)ResourceArray->GetResourceData() + UpdateOffset, UpdateSize);
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
		else // not enough space, so must recreate the RHI resource with proper size
		{
			UpdateRHI();
		}
	}
	else // in non dynamic mode we have to recreate the RHI from scratch
	{
		UpdateRHI();
	}
}

void FStaticMeshInstanceBuffer::AllocateData()
{
	// Clear any old VertexData before allocating.
	CleanUp();

	check(HasValidFeatureLevel());
	
	const bool bInstanced = GRHISupportsInstancing;
	const bool bNeedsCPUAccess = !bInstanced;
	const bool bSupportsVertexHalfFloat = GVertexElementTypeSupport.IsSupported(VET_Half2);
	InstanceData = new FStaticMeshInstanceData(bNeedsCPUAccess, bSupportsVertexHalfFloat);
	// Calculate the vertex stride.
	Stride = InstanceData->GetStride();
}

void FStaticMeshInstanceBuffer::AllocateData(FStaticMeshInstanceData& Other)
{
	AllocateData();
	Other.SetAllowCPUAccess(InstanceData->GetAllowCPUAccess());
	FMemory::Memswap(&Other, InstanceData, sizeof(FStaticMeshInstanceData));
}




/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
bool FInstancedStaticMeshVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithInstancedStaticMeshes() || Material->IsSpecialEngineMaterial()) 
			&& FLocalVertexFactory::ShouldCache(Platform, Material, ShaderType);
}


/**
 * Copy the data from another vertex factory
 * @param Other - factory to copy from
 */
void FInstancedStaticMeshVertexFactory::Copy(const FInstancedStaticMeshVertexFactory& Other)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FInstancedStaticMeshVertexFactoryCopyData,
		FInstancedStaticMeshVertexFactory*,VertexFactory,this,
		const FDataType*,DataCopy,&Other.Data,
	{
		VertexFactory->Data = *DataCopy;
	});
	BeginUpdateResourceRHI(this);
}

void FInstancedStaticMeshVertexFactory::InitRHI()
{
	check(HasValidFeatureLevel());
	const bool bInstanced = GRHISupportsInstancing;

#if !ALLOW_DITHERED_LOD_FOR_INSTANCED_STATIC_MESHES // position only shaders cannot work with dithered LOD
	// If the vertex buffer containing position is not the same vertex buffer containing the rest of the data,
	// then initialize PositionStream and PositionDeclaration.
	if(Data.PositionComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer)
	{
		FVertexDeclarationElementList PositionOnlyStreamElements;
		PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.PositionComponent,0));

		if (bInstanced)
		{
			// toss in the instanced location stream
			PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.InstanceOriginComponent,8));
			PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.InstanceTransformComponent[0],9));
			PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.InstanceTransformComponent[1],10));
			PositionOnlyStreamElements.Add(AccessPositionStreamComponent(Data.InstanceTransformComponent[2],11));
		}
		InitPositionDeclaration(PositionOnlyStreamElements);
	}
#endif

	FVertexDeclarationElementList Elements;
	if(Data.PositionComponent.VertexBuffer != NULL)
	{
		Elements.Add(AccessStreamComponent(Data.PositionComponent,0));
	}

	// only tangent,normal are used by the stream. the binormal is derived in the shader
	uint8 TangentBasisAttributes[2] = { 1, 2 };
	for(int32 AxisIndex = 0;AxisIndex < 2;AxisIndex++)
	{
		if(Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
		{
			Elements.Add(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex],TangentBasisAttributes[AxisIndex]));
		}
	}

	if(Data.ColorComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.ColorComponent,3));
	}
	else
	{
		//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
		Elements.Add(AccessStreamComponent(NullColorComponent,3));
	}

	if(Data.TextureCoordinates.Num())
	{
		const int32 BaseTexCoordAttribute = 4;
		for(int32 CoordinateIndex = 0;CoordinateIndex < Data.TextureCoordinates.Num();CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				Data.TextureCoordinates[CoordinateIndex],
				BaseTexCoordAttribute + CoordinateIndex
				));
		}

		for(int32 CoordinateIndex = Data.TextureCoordinates.Num(); CoordinateIndex < (InstancedStaticMeshMaxTexCoord + 1) / 2; CoordinateIndex++)
		{
			Elements.Add(AccessStreamComponent(
				Data.TextureCoordinates[Data.TextureCoordinates.Num() - 1],
				BaseTexCoordAttribute + CoordinateIndex
				));
		}
	}

	if(Data.LightMapCoordinateComponent.VertexBuffer)
	{
		Elements.Add(AccessStreamComponent(Data.LightMapCoordinateComponent,15));
	}
	else if(Data.TextureCoordinates.Num())
	{
		Elements.Add(AccessStreamComponent(Data.TextureCoordinates[0],15));
	}

	// toss in the instanced location stream
	if (bInstanced)
	{
		Elements.Add(AccessStreamComponent(Data.InstanceOriginComponent,8));
		Elements.Add(AccessStreamComponent(Data.InstanceTransformComponent[0],9));
		Elements.Add(AccessStreamComponent(Data.InstanceTransformComponent[1],10));
		Elements.Add(AccessStreamComponent(Data.InstanceTransformComponent[2],11));
		Elements.Add(AccessStreamComponent(Data.InstanceLightmapAndShadowMapUVBiasComponent,12));
	}

	// we don't need per-vertex shadow or lightmap rendering
	InitDeclaration(Elements);
}


FVertexFactoryShaderParameters* FInstancedStaticMeshVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FInstancedStaticMeshVertexFactoryShaderParameters() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FInstancedStaticMeshVertexFactory,"/Engine/Private/LocalVertexFactory.ush",true,true,true,true,true);
IMPLEMENT_VERTEX_FACTORY_TYPE(FEmulatedInstancedStaticMeshVertexFactory,"/Engine/Private/LocalVertexFactory.ush",true,true,true,true,true);

void FInstancedStaticMeshRenderData::InitStaticMeshVertexFactories(
		TIndirectArray<FInstancedStaticMeshVertexFactory>* VertexFactories,
		FInstancedStaticMeshRenderData* InstancedRenderData,
		UStaticMesh* Parent)
{
	const bool bInstanced = GRHISupportsInstancing;

	for( int32 LODIndex=0;LODIndex<VertexFactories->Num(); LODIndex++ )
	{
		const FStaticMeshLODResources* RenderData = &InstancedRenderData->LODModels[LODIndex];
						
		FInstancedStaticMeshVertexFactory::FDataType Data;
		Data.PositionComponent = FVertexStreamComponent(
			&RenderData->PositionVertexBuffer,
			STRUCT_OFFSET(FPositionVertex,Position),
			RenderData->PositionVertexBuffer.GetStride(),
			VET_Float3
			);

		uint32 TangentXOffset = 0;
		uint32 TangetnZOffset = 0;
		uint32 UVsBaseOffset = 0;

		SELECT_STATIC_MESH_VERTEX_TYPE(
			RenderData->VertexBuffer.GetUseHighPrecisionTangentBasis(),
			RenderData->VertexBuffer.GetUseFullPrecisionUVs(),
			RenderData->VertexBuffer.GetNumTexCoords(),
			{
				TangentXOffset = STRUCT_OFFSET(VertexType, TangentX);
				TangetnZOffset = STRUCT_OFFSET(VertexType, TangentZ);
				UVsBaseOffset = STRUCT_OFFSET(VertexType, UVs);
			});

		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			&RenderData->VertexBuffer,
			TangentXOffset,
			RenderData->VertexBuffer.GetStride(),
			RenderData->VertexBuffer.GetUseHighPrecisionTangentBasis() ?
				TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::VertexElementType : 
				TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::VertexElementType
			);

		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			&RenderData->VertexBuffer,
			TangetnZOffset,
			RenderData->VertexBuffer.GetStride(),
			RenderData->VertexBuffer.GetUseHighPrecisionTangentBasis() ?
				TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::HighPrecision>::VertexElementType : 
				TStaticMeshVertexTangentTypeSelector<EStaticMeshVertexTangentBasisType::Default>::VertexElementType
			);

		if( RenderData->ColorVertexBuffer.GetNumVertices() > 0 )
		{
			Data.ColorComponent = FVertexStreamComponent(
				&RenderData->ColorVertexBuffer,
				0,	// Struct offset to color
				RenderData->ColorVertexBuffer.GetStride(),
				VET_Color
				);
		}

		Data.TextureCoordinates.Empty();

		uint32 UVSizeInBytes = RenderData->VertexBuffer.GetUseFullPrecisionUVs() ?
			sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::HighPrecision>::UVsTypeT) : sizeof(TStaticMeshVertexUVsTypeSelector<EStaticMeshVertexUVType::Default>::UVsTypeT);

		EVertexElementType UVDoubleWideVertexElementType = RenderData->VertexBuffer.GetUseFullPrecisionUVs() ?
			VET_Float4 : VET_Half4;

		EVertexElementType UVVertexElementType = RenderData->VertexBuffer.GetUseFullPrecisionUVs() ?
			VET_Float2 : VET_Half2;

		// Only bind InstancedStaticMeshMaxTexCoord, even if the mesh has more.
		int32 NumTexCoords = FMath::Min<int32>((int32)RenderData->VertexBuffer.GetNumTexCoords(), InstancedStaticMeshMaxTexCoord);

		int32 UVIndex;
		for (UVIndex = 0; UVIndex < NumTexCoords - 1; UVIndex += 2)
		{
			Data.TextureCoordinates.Add(FVertexStreamComponent(
				&RenderData->VertexBuffer,
				UVsBaseOffset + UVSizeInBytes * UVIndex,
				RenderData->VertexBuffer.GetStride(),
				UVDoubleWideVertexElementType
				));
		}
		// possible last UV channel if we have an odd number
		if( UVIndex < NumTexCoords )
		{
			Data.TextureCoordinates.Add(FVertexStreamComponent(
				&RenderData->VertexBuffer,
				UVsBaseOffset + UVSizeInBytes * UVIndex,
				RenderData->VertexBuffer.GetStride(),
				UVVertexElementType
				));
		}

		if (Parent->LightMapCoordinateIndex >= 0 && Parent->LightMapCoordinateIndex < NumTexCoords)
		{
			Data.LightMapCoordinateComponent = FVertexStreamComponent(
				&RenderData->VertexBuffer,
				UVsBaseOffset + UVSizeInBytes * Parent->LightMapCoordinateIndex,
				RenderData->VertexBuffer.GetStride(),
				UVVertexElementType
				);
		}

		if (bInstanced && InstancedRenderData->PerInstanceRenderData.IsValid())
		{
			const FStaticMeshInstanceBuffer& InstanceBuffer = InstancedRenderData->PerInstanceRenderData->InstanceBuffer;
			const bool bSupportsVertexHalfFloat = GVertexElementTypeSupport.IsSupported(VET_Half2);
			
			if (bSupportsVertexHalfFloat)
			{
				Data.InstanceOriginComponent = FVertexStreamComponent(
					&InstanceBuffer,
					STRUCT_OFFSET(FInstanceStream16,InstanceOrigin), 
					InstanceBuffer.GetStride(),
					VET_Float4,
					true
					);
				Data.InstanceTransformComponent[0] = FVertexStreamComponent(
					&InstanceBuffer,
					STRUCT_OFFSET(FInstanceStream16,InstanceTransform1), 
					InstanceBuffer.GetStride(),
					VET_Half4,
					true
					);
				Data.InstanceTransformComponent[1] = FVertexStreamComponent(
					&InstanceBuffer,
					STRUCT_OFFSET(FInstanceStream16,InstanceTransform2), 
					InstanceBuffer.GetStride(),
					VET_Half4,
					true
					);
				Data.InstanceTransformComponent[2] = FVertexStreamComponent(
					&InstanceBuffer,
					STRUCT_OFFSET(FInstanceStream16,InstanceTransform3), 
					InstanceBuffer.GetStride(),
					VET_Half4,
					true
					);
				Data.InstanceLightmapAndShadowMapUVBiasComponent = FVertexStreamComponent(
					&InstanceBuffer,
					STRUCT_OFFSET(FInstanceStream16,InstanceLightmapAndShadowMapUVBias), 
					InstanceBuffer.GetStride(),
					VET_Short4N,
					true
					);
			}
			else
			{
				Data.InstanceOriginComponent = FVertexStreamComponent(
					&InstanceBuffer,
					STRUCT_OFFSET(FInstanceStream32,InstanceOrigin), 
					InstanceBuffer.GetStride(),
					VET_Float4,
					true
					);
				Data.InstanceTransformComponent[0] = FVertexStreamComponent(
					&InstanceBuffer,
					STRUCT_OFFSET(FInstanceStream32,InstanceTransform1), 
					InstanceBuffer.GetStride(),
					VET_Float4,
					true
					);
				Data.InstanceTransformComponent[1] = FVertexStreamComponent(
					&InstanceBuffer,
					STRUCT_OFFSET(FInstanceStream32,InstanceTransform2), 
					InstanceBuffer.GetStride(),
					VET_Float4,
					true
					);
				Data.InstanceTransformComponent[2] = FVertexStreamComponent(
					&InstanceBuffer,
					STRUCT_OFFSET(FInstanceStream32,InstanceTransform3), 
					InstanceBuffer.GetStride(),
					VET_Float4,
					true
					);
				Data.InstanceLightmapAndShadowMapUVBiasComponent = FVertexStreamComponent(
					&InstanceBuffer,
					STRUCT_OFFSET(FInstanceStream32,InstanceLightmapAndShadowMapUVBias), 
					InstanceBuffer.GetStride(),
					VET_Short4N,
					true
					);
			}
		}

		// Assign to the vertex factory for this LOD.
		FInstancedStaticMeshVertexFactory& VertexFactory = (*VertexFactories)[LODIndex];
		VertexFactory.SetData(Data);
	}
}


void FInstancedStaticMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_InstancedStaticMeshSceneProxy_GetMeshElements);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bSelectionRenderEnabled = GIsEditor && ViewFamily.EngineShowFlags.Selection;

	// If the first pass rendered selected instances only, we need to render the deselected instances in a second pass
	const int32 NumSelectionGroups = (bSelectionRenderEnabled && bHasSelectedInstances) ? 2 : 1;

	const FInstancingUserData* PassUserData[2] =
	{
		bHasSelectedInstances && bSelectionRenderEnabled ? &UserData_SelectedInstances : &UserData_AllInstances,
		&UserData_DeselectedInstances
	};

	bool BatchRenderSelection[2] = 
	{
		bSelectionRenderEnabled && IsSelected(),
		false
	};

	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			for (int32 SelectionGroupIndex = 0; SelectionGroupIndex < NumSelectionGroups; SelectionGroupIndex++)
			{
				const int32 LODIndex = GetLOD(View);
				const FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[LODIndex];

				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
				{
					const int32 NumBatches = GetNumMeshBatches();

					for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
					{
						FMeshBatch& MeshElement = Collector.AllocateMesh();

						if (GetMeshElement(LODIndex, BatchIndex, SectionIndex, GetDepthPriorityGroup(View), BatchRenderSelection[SelectionGroupIndex], IsHovered(), true, MeshElement))
						{
							//@todo-rco this is only supporting selection on the first element
							MeshElement.Elements[0].UserData = PassUserData[SelectionGroupIndex];
							MeshElement.Elements[0].bUserDataIsColorVertexBuffer = false;
							MeshElement.bCanApplyViewModeOverrides = true;
							MeshElement.bUseSelectionOutline = BatchRenderSelection[SelectionGroupIndex];
							MeshElement.bUseWireframeSelectionColoring = BatchRenderSelection[SelectionGroupIndex];

							if (View->bRenderFirstInstanceOnly)
							{
								for (int32 ElementIndex = 0; ElementIndex < MeshElement.Elements.Num(); ElementIndex++)
								{
									MeshElement.Elements[ElementIndex].NumInstances = FMath::Min<uint32>(MeshElement.Elements[ElementIndex].NumInstances, 1);
								}
							}

							Collector.AddMesh(ViewIndex, MeshElement);
							INC_DWORD_STAT_BY(STAT_StaticMeshTriangles, MeshElement.GetNumPrimitives());
						}
					}
				}
			}
		}
	}
#endif
}

int32 FInstancedStaticMeshSceneProxy::GetNumMeshBatches() const
{
	const bool bInstanced = GRHISupportsInstancing;

	if (bInstanced)
	{
		return 1;
	}
	else
	{
		const uint32 NumInstances = InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetNumInstances();
		const uint32 MaxInstancesPerBatch = FInstancedStaticMeshVertexFactory::NumBitsForVisibilityMask();
		const uint32 NumBatches = FMath::DivideAndRoundUp(NumInstances, MaxInstancesPerBatch);
		return NumBatches;
	}
}

void FInstancedStaticMeshSceneProxy::SetupInstancedMeshBatch(int32 LODIndex, int32 BatchIndex, FMeshBatch& OutMeshBatch) const
{
	const bool bInstanced = GRHISupportsInstancing;
	OutMeshBatch.VertexFactory = &InstancedRenderData.VertexFactories[LODIndex];
	const uint32 NumInstances = InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetNumInstances();
	FMeshBatchElement& BatchElement0 = OutMeshBatch.Elements[0];
	BatchElement0.UserData = (void*)&UserData_AllInstances;
	BatchElement0.bUserDataIsColorVertexBuffer = false;
	BatchElement0.InstancedLODIndex = LODIndex;
	BatchElement0.UserIndex = 0;
	BatchElement0.bIsInstancedMesh = bInstanced;

	if (bInstanced)
	{
		BatchElement0.NumInstances = NumInstances;
	}
	else
	{
		const uint32 MaxInstancesPerBatch = FInstancedStaticMeshVertexFactory::NumBitsForVisibilityMask();
		const uint32 NumBatches = FMath::DivideAndRoundUp(NumInstances, MaxInstancesPerBatch);
		uint32 InstanceIndex = BatchIndex * MaxInstancesPerBatch;
		uint32 NumInstancesThisBatch = FMath::Min(NumInstances - InstanceIndex, MaxInstancesPerBatch);
				
		if (NumInstancesThisBatch > 0)
		{
			OutMeshBatch.Elements.Reserve(NumInstancesThisBatch);
						
			// BatchElement0 is already inside the array; but Reserve() might have shifted it
			OutMeshBatch.Elements[0].UserIndex = InstanceIndex;
			--NumInstancesThisBatch;
			++InstanceIndex;

			// Add remaining BatchElements 1..n-1
			while (NumInstancesThisBatch > 0)
			{
				auto* NewBatchElement = new(OutMeshBatch.Elements) FMeshBatchElement();
				*NewBatchElement = BatchElement0;
				NewBatchElement->UserIndex = InstanceIndex;
				++InstanceIndex;
				--NumInstancesThisBatch;
			}
		}
	}
}

bool FInstancedStaticMeshSceneProxy::GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const
{
	if (LODIndex < InstancedRenderData.VertexFactories.Num() && FStaticMeshSceneProxy::GetShadowMeshElement(LODIndex, BatchIndex, InDepthPriorityGroup, OutMeshBatch, bDitheredLODTransition))
	{
		SetupInstancedMeshBatch(LODIndex, BatchIndex, OutMeshBatch);
		return true;
	}
	return false;
}

/** Sets up a FMeshBatch for a specific LOD and element. */
bool FInstancedStaticMeshSceneProxy::GetMeshElement(int32 LODIndex, int32 BatchIndex, int32 ElementIndex, uint8 InDepthPriorityGroup, bool bUseSelectedMaterial, bool bUseHoveredMaterial, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const
{
	if (LODIndex < InstancedRenderData.VertexFactories.Num() && FStaticMeshSceneProxy::GetMeshElement(LODIndex, BatchIndex, ElementIndex, InDepthPriorityGroup, bUseSelectedMaterial, bUseHoveredMaterial, bAllowPreCulledIndices, OutMeshBatch))
	{
		SetupInstancedMeshBatch(LODIndex, BatchIndex, OutMeshBatch);
		return true;
	}
	return false;
};

/** Sets up a wireframe FMeshBatch for a specific LOD. */
bool FInstancedStaticMeshSceneProxy::GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const
{
	if (LODIndex < InstancedRenderData.VertexFactories.Num() && FStaticMeshSceneProxy::GetWireframeMeshElement(LODIndex, BatchIndex, WireframeRenderProxy, InDepthPriorityGroup, bAllowPreCulledIndices, OutMeshBatch))
	{
		SetupInstancedMeshBatch(LODIndex, BatchIndex, OutMeshBatch);
		return true;
	}
	return false;
}

void FInstancedStaticMeshSceneProxy::GetDistancefieldAtlasData(FBox& LocalVolumeBounds, FVector2D& OutDistanceMinMax, FIntVector& OutBlockMin, FIntVector& OutBlockSize, bool& bOutBuiltAsIfTwoSided, bool& bMeshWasPlane, float& SelfShadowBias, TArray<FMatrix>& ObjectLocalToWorldTransforms) const
{
	FStaticMeshSceneProxy::GetDistancefieldAtlasData(LocalVolumeBounds, OutDistanceMinMax, OutBlockMin, OutBlockSize, bOutBuiltAsIfTwoSided, bMeshWasPlane, SelfShadowBias, ObjectLocalToWorldTransforms);

	ObjectLocalToWorldTransforms.Reset();

	const uint32 NumInstances = InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetNumInstances();
	for (uint32 InstanceIndex = 0; InstanceIndex < NumInstances; InstanceIndex++)
	{
		FMatrix InstanceToLocal;
		InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetInstanceTransform(InstanceIndex, InstanceToLocal);	
		InstanceToLocal.M[3][3] = 1.0f;

		ObjectLocalToWorldTransforms.Add(InstanceToLocal * GetLocalToWorld());
	}
}

void FInstancedStaticMeshSceneProxy::GetDistanceFieldInstanceInfo(int32& NumInstances, float& BoundsSurfaceArea) const
{
	NumInstances = DistanceFieldData ? InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetNumInstances() : 0;

	if (NumInstances > 0)
	{
		FMatrix InstanceToLocal;
		const int32 InstanceIndex = 0;
		InstancedRenderData.PerInstanceRenderData->InstanceBuffer.GetInstanceTransform(InstanceIndex, InstanceToLocal);
		InstanceToLocal.M[3][3] = 1.0f;

		const FMatrix InstanceTransform = InstanceToLocal * GetLocalToWorld();
		const FVector AxisScales = InstanceTransform.GetScaleVector();
		const FVector BoxDimensions = RenderData->Bounds.BoxExtent * AxisScales * 2;

		BoundsSurfaceArea = 2 * BoxDimensions.X * BoxDimensions.Y
			+ 2 * BoxDimensions.Z * BoxDimensions.Y
			+ 2 * BoxDimensions.X * BoxDimensions.Z;
	}
}

/*-----------------------------------------------------------------------------
	UInstancedStaticMeshComponent
-----------------------------------------------------------------------------*/

UInstancedStaticMeshComponent::UInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, UseDynamicInstanceBuffer(false)
	, KeepInstanceBufferCPUAccess(true)
	, bPerInstanceRenderDataWasPrebuilt(false)
{
	Mobility = EComponentMobility::Movable;
	BodyInstance.bSimulatePhysics = false;

	PhysicsSerializer = ObjectInitializer.CreateDefaultSubobject<UPhysicsSerializer>(this, TEXT("PhysicsSerializer"));
	bDisallowMeshPaintPerInstance = true;
}

UInstancedStaticMeshComponent::~UInstancedStaticMeshComponent()
{
	ReleasePerInstanceRenderData();
	FlushAsyncBuildInstanceBufferTask();
}

void UInstancedStaticMeshComponent::FlushAsyncBuildInstanceBufferTask()
{
	if (AsyncBuildInstanceBufferTask != nullptr)
	{
		AsyncBuildInstanceBufferTask->EnsureCompletion();
		delete AsyncBuildInstanceBufferTask;
		AsyncBuildInstanceBufferTask = nullptr;
	}
}


#if WITH_EDITOR
/** Helper class used to preserve lighting/selection state across blueprint reinstancing */
class FInstancedStaticMeshComponentInstanceData : public FSceneComponentInstanceData
{
public:
	
	FInstancedStaticMeshComponentInstanceData(const UInstancedStaticMeshComponent& InComponent)
		: FSceneComponentInstanceData(&InComponent)
		, StaticMesh(InComponent.GetStaticMesh())
	{
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<UInstancedStaticMeshComponent>(Component)->ApplyComponentInstanceData(this);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		FSceneComponentInstanceData::AddReferencedObjects(Collector);

		Collector.AddReferencedObject(StaticMesh);
	}

	/** Used to store lightmap data during RerunConstructionScripts */
	struct FLightMapInstanceData
	{
		/** Transform of component */
		FTransform Transform;

		/** guid from LODData */
		TArray<FGuid> MapBuildDataIds;
	};

public:
	/** Mesh being used by component */
	UStaticMesh* StaticMesh;

	// Static lighting info
	FLightMapInstanceData CachedStaticLighting;
	TArray<FInstancedStaticMeshInstanceData> PerInstanceSMData;

	/** The cached selected instances */
	TBitArray<> SelectedInstances;
};
#endif

FActorComponentInstanceData* UInstancedStaticMeshComponent::GetComponentInstanceData() const
{
#if WITH_EDITOR
	FActorComponentInstanceData* InstanceData = nullptr;
	FInstancedStaticMeshComponentInstanceData* StaticMeshInstanceData = nullptr;
		InstanceData = StaticMeshInstanceData = new FInstancedStaticMeshComponentInstanceData(*this);	

		// Fill in info (copied from UStaticMeshComponent::GetComponentInstanceData)
		StaticMeshInstanceData->CachedStaticLighting.Transform = GetComponentTransform();

		for (const FStaticMeshComponentLODInfo& LODDataEntry : LODData)
		{
		StaticMeshInstanceData->CachedStaticLighting.MapBuildDataIds.Add(LODDataEntry.MapBuildDataId);
		}

		// Back up per-instance lightmap/shadowmap info
		StaticMeshInstanceData->PerInstanceSMData = PerInstanceSMData;

	// Back up instance selection
		StaticMeshInstanceData->SelectedInstances = SelectedInstances;

	return InstanceData;
#else
	return nullptr;
#endif
}

void UInstancedStaticMeshComponent::ApplyComponentInstanceData(FInstancedStaticMeshComponentInstanceData* InstancedMeshData)
{
#if WITH_EDITOR
	check(InstancedMeshData);

	if (GetStaticMesh() != InstancedMeshData->StaticMesh)
	{
		return;
	}

	bool bMatch = false;

	// Check for any instance having moved as that would invalidate static lighting
	if (PerInstanceSMData.Num() == InstancedMeshData->PerInstanceSMData.Num() &&
		InstancedMeshData->CachedStaticLighting.Transform.Equals(GetComponentTransform()))
	{
		bMatch = true;

		for (int32 InstanceIndex = 0; InstanceIndex < PerInstanceSMData.Num(); ++InstanceIndex)
		{
			if (PerInstanceSMData[InstanceIndex].Transform != InstancedMeshData->PerInstanceSMData[InstanceIndex].Transform)
			{
				bMatch = false;
				break;
			}
		}
	}

	// Restore static lighting if appropriate
	if (bMatch)
	{
		const int32 NumLODLightMaps = InstancedMeshData->CachedStaticLighting.MapBuildDataIds.Num();
		SetLODDataCount(NumLODLightMaps, NumLODLightMaps);

		for (int32 i = 0; i < NumLODLightMaps; ++i)
		{
			LODData[i].MapBuildDataId = InstancedMeshData->CachedStaticLighting.MapBuildDataIds[i];
		}

		PerInstanceSMData = InstancedMeshData->PerInstanceSMData;
	}

	SelectedInstances = InstancedMeshData->SelectedInstances;

	// Force recreation of the render data
	ReleasePerInstanceRenderData();
	InitPerInstanceRenderData(true);
	MarkRenderStateDirty();
#endif
}

FPrimitiveSceneProxy* UInstancedStaticMeshComponent::CreateSceneProxy()
{
	ProxySize = 0;
	FlushAsyncBuildInstanceBufferTask();

	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid = 
		// make sure we have instances
		PerInstanceSMData.Num() > 0 &&
		// make sure we have an actual staticmesh
		GetStaticMesh() &&
		GetStaticMesh()->HasValidRenderData() &&
		// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// @todo: Level error or something to let LDs know this
		1;//GetStaticMesh()->LODModels(0).Elements.Num() == 1;

	if(bMeshIsValid)
	{
		// If we don't have a random seed for this instanced static mesh component yet, then go ahead and
		// generate one now.  This will be saved with the static mesh component and used for future generation
		// of random numbers for this component's instances. (Used by the PerInstanceRandom material expression)
		while( InstancingRandomSeed == 0 )
		{
			InstancingRandomSeed = FMath::Rand();
		}

		const bool bSupportsVertexHalfFloat = GVertexElementTypeSupport.IsSupported(VET_Half2);
		ProxySize = FStaticMeshInstanceData::GetResourceSize(PerInstanceRenderData->InstanceBuffer.GetNumInstances(), bSupportsVertexHalfFloat);
		return ::new FInstancedStaticMeshSceneProxy(this, GetWorld()->FeatureLevel);
	}
	else
	{
		return NULL;
	}
}

void UInstancedStaticMeshComponent::InitInstanceBody(int32 InstanceIdx, FBodyInstance* InstanceBodyInstance)
{
	if (!GetStaticMesh())
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("Unabled to create a body instance for %s in Actor %s. No StaticMesh set."), *GetName(), GetOwner() ? *GetOwner()->GetName() : TEXT("?"));
		return;
	}

	check(InstanceIdx < PerInstanceSMData.Num());
	check(InstanceIdx < InstanceBodies.Num());
	check(InstanceBodyInstance);

	UBodySetup* BodySetup = GetBodySetup();
	check(BodySetup);

	// Get transform of the instance
	FTransform InstanceTransform = FTransform(PerInstanceSMData[InstanceIdx].Transform) * GetComponentTransform();
	
	InstanceBodyInstance->CopyBodyInstancePropertiesFrom(&BodyInstance);
	InstanceBodyInstance->InstanceBodyIndex = InstanceIdx; // Set body index 

	// make sure we never enable bSimulatePhysics for ISMComps
	InstanceBodyInstance->bSimulatePhysics = false;

#if WITH_PHYSX
	// Create physics body instance.
	InstanceBodyInstance->bAutoWeld = false;	//We don't support this for instanced meshes.
	InstanceBodyInstance->InitBody(BodySetup, InstanceTransform, this, GetWorld()->GetPhysicsScene(), nullptr);
#endif //WITH_PHYSX
}

void UInstancedStaticMeshComponent::CreateAllInstanceBodies()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UInstancedStaticMeshComponent_CreateAllInstanceBodies);

	const int32 NumBodies = PerInstanceSMData.Num();
	check(InstanceBodies.Num() == 0);

	if (UBodySetup* BodySetup = GetBodySetup())
	{
		FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();

		InstanceBodies.SetNumUninitialized(NumBodies);

		// Sanitized array does not contain any nulls
		TArray<FBodyInstance*> InstanceBodiesSanitized;
		InstanceBodiesSanitized.Reserve(NumBodies);

		TArray<FTransform> Transforms;
	    Transforms.Reserve(NumBodies);
	    for (int32 i = 0; i < NumBodies; ++i)
	    {
			const FTransform InstanceTM = FTransform(PerInstanceSMData[i].Transform) * GetComponentTransform();
			if (InstanceTM.GetScale3D().IsNearlyZero())
			{
				InstanceBodies[i] = nullptr;
			}
			else
			{
				FBodyInstance* Instance = new FBodyInstance;

				InstanceBodiesSanitized.Add(Instance);
				InstanceBodies[i] = Instance;
				Instance->CopyBodyInstancePropertiesFrom(&BodyInstance);
				Instance->InstanceBodyIndex = i; // Set body index 
				Instance->bAutoWeld = false;

				// make sure we never enable bSimulatePhysics for ISMComps
				Instance->bSimulatePhysics = false;

				if (Mobility == EComponentMobility::Movable)
				{
					Instance->InitBody(BodySetup, InstanceTM, this, PhysScene, nullptr );
				}
				else
				{
					Transforms.Add(InstanceTM);
#if WITH_PHYSX
					Instance->RigidActorSyncId = i + 1;

					if (GetWorld()->GetPhysicsScene()->HasAsyncScene())
					{
						Instance->RigidActorAsyncId = Instance->RigidActorSyncId + NumBodies;
					}
#endif
				}
			}
	    }

		if (InstanceBodiesSanitized.Num() > 0 && Mobility != EComponentMobility::Movable)
		{
			TArray<UBodySetup*> BodySetups;
			TArray<UPhysicalMaterial*> PhysicalMaterials;

			BodySetups.Add(BodySetup);
			TWeakObjectPtr<UPrimitiveComponent> WeakSelfPtr(this);
			FBodyInstance::GetComplexPhysicalMaterials(&BodyInstance, WeakSelfPtr, PhysicalMaterials);
			PhysicalMaterials.Add(FBodyInstance::GetSimplePhysicalMaterial(&BodyInstance, WeakSelfPtr, TWeakObjectPtr<UBodySetup>(BodySetup)));

			PhysicsSerializer->CreatePhysicsData(BodySetups, PhysicalMaterials);
			FBodyInstance::InitStaticBodies(InstanceBodiesSanitized, Transforms, BodySetup, this, GetWorld()->GetPhysicsScene(), PhysicsSerializer);

			// Serialize physics data for fast path cooking
			PhysicsSerializer->SerializePhysics(InstanceBodiesSanitized, BodySetups, PhysicalMaterials);
		}
	}
	else
	{
		// In case we get into some bad state where the BodySetup is invalid but bPhysicsStateCreated is true,
		// issue a warning and add nullptrs to InstanceBodies.
		UE_LOG(LogStaticMesh, Warning, TEXT("Instance Static Mesh Component unable to create InstanceBodies!"));
		InstanceBodies.AddZeroed(NumBodies);
	}
}

void UInstancedStaticMeshComponent::ClearAllInstanceBodies()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UInstancedStaticMeshComponent_ClearAllInstanceBodies);
	for (int32 i = 0; i < InstanceBodies.Num(); i++)
	{
		if (InstanceBodies[i])
		{
			InstanceBodies[i]->TermBody();
			delete InstanceBodies[i];
		}
	}

	InstanceBodies.Empty();
}


void UInstancedStaticMeshComponent::OnCreatePhysicsState()
{
	check(InstanceBodies.Num() == 0);

	FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();

	if (!PhysScene)
	{
		return;
	}

	// Create all the bodies.
	CreateAllInstanceBodies();

	USceneComponent::OnCreatePhysicsState();
}

void UInstancedStaticMeshComponent::OnDestroyPhysicsState()
{
	int32 PSceneIndex = INDEX_NONE;
	for(const FBodyInstance* BI : InstanceBodies)
	{
		if(BI)
		{
			if(BI->SceneIndexSync)
			{
				PSceneIndex = BI->SceneIndexSync;
				break;
			}
			else if(BI->SceneIndexAsync)
			{
				PSceneIndex = BI->SceneIndexAsync;
				break;
			}
		}
	}

	USceneComponent::OnDestroyPhysicsState();

	// Release all physics representations
	ClearAllInstanceBodies();
}

bool UInstancedStaticMeshComponent::CanEditSimulatePhysics()
{
	// if instancedstaticmeshcomponent, we will never allow it
	return false;
}

FBoxSphereBounds UInstancedStaticMeshComponent::CalcBounds(const FTransform& BoundTransform) const
{
	if(GetStaticMesh() && PerInstanceSMData.Num() > 0)
	{
		FMatrix BoundTransformMatrix = BoundTransform.ToMatrixWithScale();

		FBoxSphereBounds RenderBounds = GetStaticMesh()->GetBounds();
		FBoxSphereBounds NewBounds = RenderBounds.TransformBy(PerInstanceSMData[0].Transform * BoundTransformMatrix);

		for (int32 InstanceIndex = 1; InstanceIndex < PerInstanceSMData.Num(); InstanceIndex++)
		{
			NewBounds = NewBounds + RenderBounds.TransformBy(PerInstanceSMData[InstanceIndex].Transform * BoundTransformMatrix);
		}

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(BoundTransform.GetLocation(), FVector::ZeroVector, 0.f);
	}
}

#if WITH_EDITOR
void UInstancedStaticMeshComponent::GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo, const TArray<ULightComponent*>& InRelevantLights, const FLightingBuildOptions& Options)
{
	if (HasValidSettingsForStaticLighting(false))
	{
		// create static lighting for LOD 0
		int32 LightMapWidth = 0;
		int32 LightMapHeight = 0;
		GetLightMapResolution(LightMapWidth, LightMapHeight);

		bool bFit = false;
		bool bReduced = false;
		while (1)
		{
			const int32 OneLessThanMaximumSupportedResolution = 1 << (GMaxTextureMipCount - 2);

			const int32 MaxInstancesInMaxSizeLightmap = (OneLessThanMaximumSupportedResolution / LightMapWidth) * ((OneLessThanMaximumSupportedResolution / 2) / LightMapHeight);
			if (PerInstanceSMData.Num() > MaxInstancesInMaxSizeLightmap)
			{
				if (LightMapWidth < 4 || LightMapHeight < 4)
				{
					break;
				}
				LightMapWidth /= 2;
				LightMapHeight /= 2;
				bReduced = true;
			}
			else
			{
				bFit = true;
				break;
			}
		}

		if (!bFit)
		{
			FMessageLog("LightingResults").Message(EMessageSeverity::Error)
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(NSLOCTEXT("InstancedStaticMesh", "FailedStaticLightingWarning", "The total lightmap size for this InstancedStaticMeshComponent is too big no matter how much we reduce the per-instance size, the number of mesh instances in this component must be reduced")));
			return;
		}
		if (bReduced)
		{
			FMessageLog("LightingResults").Message(EMessageSeverity::Warning)
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(NSLOCTEXT("InstancedStaticMesh", "ReducedStaticLightingWarning", "The total lightmap size for this InstancedStaticMeshComponent was too big and it was automatically reduced. Consider reducing the component's lightmap resolution or number of mesh instances in this component")));
		}

		const int32 LightMapSize = GetWorld()->GetWorldSettings()->PackedLightAndShadowMapTextureSize;
		const int32 MaxInstancesInDefaultSizeLightmap = (LightMapSize / LightMapWidth) * ((LightMapSize / 2) / LightMapHeight);
		if (PerInstanceSMData.Num() > MaxInstancesInDefaultSizeLightmap)
		{
			FMessageLog("LightingResults").Message(EMessageSeverity::Warning)
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(NSLOCTEXT("InstancedStaticMesh", "LargeStaticLightingWarning", "The total lightmap size for this InstancedStaticMeshComponent is large, consider reducing the component's lightmap resolution or number of mesh instances in this component")));
		}

		// TODO: Support separate static lighting in LODs for instanced meshes.

		if (!GetStaticMesh()->CanLODsShareStaticLighting())
		{
			//TODO: Detect if the UVs for all sub-LODs overlap the base LOD UVs and omit this warning if they do.
			FMessageLog("LightingResults").Message(EMessageSeverity::Warning)
				->AddToken(FUObjectToken::Create(this))
				->AddToken(FTextToken::Create(NSLOCTEXT("InstancedStaticMesh", "UniqueStaticLightingForLODWarning", "Instanced meshes don't yet support unique static lighting for each LOD. Lighting on LOD 1+ may be incorrect unless lightmap UVs are the same for all LODs.")));
		}

		// Force sharing LOD 0 lightmaps for now.
		int32 NumLODs = 1;

		CachedMappings.Reset(PerInstanceSMData.Num() * NumLODs);
		CachedMappings.AddZeroed(PerInstanceSMData.Num() * NumLODs);

		NumPendingLightmaps = 0;

		for (int32 LODIndex = 0; LODIndex < NumLODs; LODIndex++)
		{
			const FStaticMeshLODResources& LODRenderData = GetStaticMesh()->RenderData->LODResources[LODIndex];

			for (int32 InstanceIndex = 0; InstanceIndex < PerInstanceSMData.Num(); InstanceIndex++)
			{
				auto* StaticLightingMesh = new FStaticLightingMesh_InstancedStaticMesh(this, LODIndex, InstanceIndex, InRelevantLights);
				OutPrimitiveInfo.Meshes.Add(StaticLightingMesh);

				auto* InstancedMapping = new FStaticLightingTextureMapping_InstancedStaticMesh(this, LODIndex, InstanceIndex, StaticLightingMesh, LightMapWidth, LightMapHeight, GetStaticMesh()->LightMapCoordinateIndex, true);
				OutPrimitiveInfo.Mappings.Add(InstancedMapping);

				CachedMappings[LODIndex * PerInstanceSMData.Num() + InstanceIndex].Mapping = InstancedMapping;
				NumPendingLightmaps++;
			}

			// Shrink LOD texture lightmaps by half for each LOD level (minimum 4x4 px)
			LightMapWidth  = FMath::Max(LightMapWidth  / 2, 4);
			LightMapHeight = FMath::Max(LightMapHeight / 2, 4);
		}
	}
}

void UInstancedStaticMeshComponent::ApplyLightMapping(FStaticLightingTextureMapping_InstancedStaticMesh* InMapping, ULevel* LightingScenario)
{
	NumPendingLightmaps--;

	if (NumPendingLightmaps == 0)
	{
		// Calculate the range of each coefficient in this light-map and repack the data to have the same scale factor and bias across all instances
		// TODO: Per instance scale?

		// generate the final lightmaps for all the mappings for this component
		TArray<TUniquePtr<FQuantizedLightmapData>> AllQuantizedData;
		for (auto& MappingInfo : CachedMappings)
		{
			FStaticLightingTextureMapping_InstancedStaticMesh* Mapping = MappingInfo.Mapping;
			AllQuantizedData.Add(MoveTemp(Mapping->QuantizedData));
		}

		bool bNeedsShadowMap = false;
		TArray<TMap<ULightComponent*, TUniquePtr<FShadowMapData2D>>> AllShadowMapData;
		for (auto& MappingInfo : CachedMappings)
		{
			FStaticLightingTextureMapping_InstancedStaticMesh* Mapping = MappingInfo.Mapping;
			bNeedsShadowMap = bNeedsShadowMap || (Mapping->ShadowMapData.Num() > 0);
			AllShadowMapData.Add(MoveTemp(Mapping->ShadowMapData));
		}

		UStaticMesh* ResolvedMesh = GetStaticMesh();
		if (LODData.Num() != ResolvedMesh->GetNumLODs())
		{
			MarkPackageDirty();
		}

		// Ensure LODData has enough entries in it, free not required.
		SetLODDataCount(ResolvedMesh->GetNumLODs(), ResolvedMesh->GetNumLODs());

		ULevel* StorageLevel = LightingScenario ? LightingScenario : GetOwner()->GetLevel();
		UMapBuildDataRegistry* Registry = StorageLevel->GetOrCreateMapBuildData();
		FMeshMapBuildData& MeshBuildData = Registry->AllocateMeshBuildData(LODData[0].MapBuildDataId, true);

		MeshBuildData.PerInstanceLightmapData.Empty(AllQuantizedData.Num());
		MeshBuildData.PerInstanceLightmapData.AddZeroed(AllQuantizedData.Num());

		// Create a light-map for the primitive.
		const ELightMapPaddingType PaddingType = GAllowLightmapPadding ? LMPT_NormalPadding : LMPT_NoPadding;
		TRefCountPtr<FLightMap2D> NewLightMap = FLightMap2D::AllocateInstancedLightMap(Registry, this, MoveTemp(AllQuantizedData), Registry, LODData[0].MapBuildDataId, Bounds, PaddingType, LMF_Streamed);

		// Create a shadow-map for the primitive.
		TRefCountPtr<FShadowMap2D> NewShadowMap = bNeedsShadowMap ? FShadowMap2D::AllocateInstancedShadowMap(Registry, this, MoveTemp(AllShadowMapData), Registry, LODData[0].MapBuildDataId, Bounds, PaddingType, SMF_Streamed) : nullptr;

		MeshBuildData.LightMap = NewLightMap;
		MeshBuildData.ShadowMap = NewShadowMap;

		// Build the list of statically irrelevant lights.
		// TODO: This should be stored per LOD.
		TSet<FGuid> RelevantLights;
		TSet<FGuid> PossiblyIrrelevantLights;
		for (auto& MappingInfo : CachedMappings)
		{
			for (const ULightComponent* Light : MappingInfo.Mapping->Mesh->RelevantLights)
			{
				// Check if the light is stored in the light-map.
				const bool bIsInLightMap = MeshBuildData.LightMap && MeshBuildData.LightMap->LightGuids.Contains(Light->LightGuid);

				// Check if the light is stored in the shadow-map.
				const bool bIsInShadowMap = MeshBuildData.ShadowMap && MeshBuildData.ShadowMap->LightGuids.Contains(Light->LightGuid);

				// If the light isn't already relevant to another mapping, add it to the potentially irrelevant list
				if (!bIsInLightMap && !bIsInShadowMap && !RelevantLights.Contains(Light->LightGuid))
				{
					PossiblyIrrelevantLights.Add(Light->LightGuid);
				}

				// Light is relevant
				if (bIsInLightMap || bIsInShadowMap)
				{
					RelevantLights.Add(Light->LightGuid);
					PossiblyIrrelevantLights.Remove(Light->LightGuid);
				}
			}
		}

		MeshBuildData.IrrelevantLights = PossiblyIrrelevantLights.Array();

		PerInstanceRenderData->UpdateAllInstanceData(this);
	}
}
#endif

void UInstancedStaticMeshComponent::ReleasePerInstanceRenderData()
{
	if (PerInstanceRenderData.IsValid() && !bPerInstanceRenderDataWasPrebuilt)
	{
		typedef TSharedPtr<FPerInstanceRenderData, ESPMode::ThreadSafe> FPerInstanceRenderDataPtr;

		PerInstanceRenderData->HitProxies.Empty();

		// Make shared pointer object on the heap
		FPerInstanceRenderDataPtr* CleanupRenderDataPtr = new FPerInstanceRenderDataPtr(PerInstanceRenderData);
		PerInstanceRenderData.Reset();

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FReleasePerInstanceRenderData,
			FPerInstanceRenderDataPtr*, InCleanupRenderDataPtr, CleanupRenderDataPtr,
		{
			// Destroy the shared pointer object we allocated on the heap.
			// Resource will either be released here or by scene proxy on the render thread, whoever gets executed last
			delete InCleanupRenderDataPtr;
		});
	} //-V773
}

void UInstancedStaticMeshComponent::PropagateLightingScenarioChange()
{
	FComponentRecreateRenderStateContext Context(this);

	if (PerInstanceRenderData.IsValid())
	{
		PerInstanceRenderData->UpdateInstanceData(this, 0, PerInstanceSMData.Num());
	}
}

void UInstancedStaticMeshComponent::GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const
{
	Super::GetLightAndShadowMapMemoryUsage(LightMapMemoryUsage, ShadowMapMemoryUsage);

	int32 NumInstances = PerInstanceSMData.Num();

	// Scale lighting demo by number of instances
	LightMapMemoryUsage *= NumInstances;
	ShadowMapMemoryUsage *= NumInstances;
}


void UInstancedStaticMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	PerInstanceSMData.BulkSerialize(Ar);

#if WITH_EDITOR
	if( Ar.IsTransacting() )
	{
		Ar << SelectedInstances;
	}
#endif
}

int32 UInstancedStaticMeshComponent::AddInstanceInternal(int32 InstanceIndex, FInstancedStaticMeshInstanceData* InNewInstanceData, const FTransform& InstanceTransform)
{
	FInstancedStaticMeshInstanceData* NewInstanceData = InNewInstanceData;

	if (NewInstanceData == nullptr)
	{
		NewInstanceData = new(PerInstanceSMData) FInstancedStaticMeshInstanceData();
	}

	SetupNewInstanceData(*NewInstanceData, InstanceIndex, InstanceTransform);

#if WITH_EDITOR
	if (SelectedInstances.Num())
	{
		SelectedInstances.Add(false);
	}
#endif

	int32 UpdateInstanceCount = 1;

	if (!InstanceReorderTable.Contains(InstanceIndex))
	{
		InstanceReorderTable.Add(InstanceIndex);
	}
	else
	{
		InstanceReorderTable.Insert(InstanceIndex, InstanceIndex);

		for (int32 i = InstanceIndex + 1; i < InstanceReorderTable.Num(); ++i)
		{
			InstanceReorderTable[i] = i;
		}

		UpdateInstanceCount = InstanceReorderTable.Num() - InstanceIndex;
	}

	if (PerInstanceRenderData.IsValid())
	{
		PerInstanceRenderData->UpdateInstanceData(this, InstanceIndex, UpdateInstanceCount);
	}

	PartialNavigationUpdate(InstanceIndex);

	MarkRenderStateDirty();

	return InstanceIndex;
}

int32 UInstancedStaticMeshComponent::AddInstance(const FTransform& InstanceTransform)
{
	if (PerInstanceSMData.Num() > 0 && PerInstanceRenderData.IsValid() && PerInstanceRenderData->InstanceBuffer.GetCurrentNumInstances() == 0)
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("Trying to change instance buffer for component %s, but we have no CPU copy. Set KeepInstanceBufferCPUAccess to true to keep access at the cost of memory."), *GetPathName());
		return INDEX_NONE;
	}

	return AddInstanceInternal(PerInstanceSMData.Num(), nullptr, InstanceTransform);
}

int32 UInstancedStaticMeshComponent::AddInstanceWorldSpace(const FTransform& WorldTransform)
 {
	// Transform from world space to local space
	FTransform RelativeTM = WorldTransform.GetRelativeTransform(GetComponentTransform());
	return AddInstance(RelativeTM);
}

bool UInstancedStaticMeshComponent::RemoveInstanceInternal(int32 InstanceIndex, bool ReorderInstances, bool InstanceAlreadyRemoved)
{
	if (PerInstanceSMData.Num() > 0 && PerInstanceRenderData.IsValid() && PerInstanceRenderData->InstanceBuffer.GetCurrentNumInstances() == 0)
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("Trying to change instance buffer for component %s, but we have no CPU copy. Set KeepInstanceBufferCPUAccess to true to keep access at the cost of memory."), *GetPathName());
		return false;
	}

	// Request navigation update
	PartialNavigationUpdate(InstanceIndex);

	// Save the render index
	int32 RemovedRenderIndex = InstanceIndex;

	if (InstanceReorderTable.Num() > 0 && InstanceReorderTable.IsValidIndex(InstanceIndex))
	{
		RemovedRenderIndex = InstanceReorderTable[InstanceIndex];
	}

	if (RemovedRenderIndex != INDEX_NONE)
	{
		RemovedInstances.Add(RemovedRenderIndex);
	}

	if (PerInstanceRenderData.IsValid())
	{
		PerInstanceRenderData->RemoveInstanceData(this, InstanceIndex);
	}

	if (InstanceReorderTable.IsValidIndex(InstanceIndex))
	{
		InstanceReorderTable.RemoveAt(InstanceIndex);
	}

	if (ReorderInstances)
	{
		for (int32 i = InstanceIndex; i < InstanceReorderTable.Num(); ++i)
		{
			InstanceReorderTable[i] = i;
		}

		if (PerInstanceRenderData.IsValid())
		{
			PerInstanceRenderData->UpdateInstanceData(this, InstanceIndex, PerInstanceRenderData->InstanceBuffer.GetNumInstances() - InstanceIndex);
		}
	}

	// remove instance
	if (!InstanceAlreadyRemoved && PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		PerInstanceSMData.RemoveAt(InstanceIndex);
	}

#if WITH_EDITOR
	// remove selection flag if array is filled in
	if (SelectedInstances.IsValidIndex(InstanceIndex))
	{
		SelectedInstances.RemoveAt(InstanceIndex);
	}
#endif

	// update the physics state
	if (bPhysicsStateCreated)
	{
		// TODO: it may be possible to instead just update the BodyInstanceIndex for all bodies after the removed instance. 
		ClearAllInstanceBodies();
		CreateAllInstanceBodies();
	}

	RemovedInstances.Reset();

	// Indicate we need to update render state to reflect changes
	MarkRenderStateDirty();

	return true;
}

bool UInstancedStaticMeshComponent::RemoveInstance(int32 InstanceIndex)
{
	return RemoveInstanceInternal(InstanceIndex, false, false);
}

bool UInstancedStaticMeshComponent::GetInstanceTransform(int32 InstanceIndex, FTransform& OutInstanceTransform, bool bWorldSpace) const
{
	if (!PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	const FInstancedStaticMeshInstanceData& InstanceData = PerInstanceSMData[InstanceIndex];

	OutInstanceTransform = FTransform(InstanceData.Transform);
	if (bWorldSpace)
	{
		OutInstanceTransform = OutInstanceTransform * GetComponentTransform();
	}

	return true;
}

void UInstancedStaticMeshComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	// We are handling the physics move below, so don't handle it at higher levels
	Super::OnUpdateTransform(UpdateTransformFlags | EUpdateTransformFlags::SkipPhysicsUpdate, Teleport);

	const bool bTeleport = TeleportEnumToFlag(Teleport);

	// Always send new transform to physics
	if (bPhysicsStateCreated && !(EUpdateTransformFlags::SkipPhysicsUpdate & UpdateTransformFlags))
	{
		for (int32 i = 0; i < PerInstanceSMData.Num(); i++)
		{
			const FTransform InstanceTransform(PerInstanceSMData[i].Transform);
			UpdateInstanceTransform(i, InstanceTransform * GetComponentTransform(), /* bWorldSpace= */true, /* bMarkRenderStateDirty= */false, bTeleport);
		}
	}
}

bool UInstancedStaticMeshComponent::UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	if (!PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	if (PerInstanceSMData.Num() > 0 && PerInstanceRenderData.IsValid() && PerInstanceRenderData->InstanceBuffer.GetCurrentNumInstances() == 0)
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("Trying to change instance buffer for component %s, but we have no CPU copy. Set KeepInstanceBufferCPUAccess to true to keep access at the cost of memory."), *GetPathName());
		return false;
	}

	Modify();

	FInstancedStaticMeshInstanceData& InstanceData = PerInstanceSMData[InstanceIndex];

    // TODO: Computing LocalTransform is useless when we're updating the world location for the entire mesh.
	// Should find some way around this for performance.
    
	// Render data uses local transform of the instance
	FTransform LocalTransform = bWorldSpace ? NewInstanceTransform.GetRelativeTransform(GetComponentTransform()) : NewInstanceTransform;
	InstanceData.Transform = LocalTransform.ToMatrixWithScale();

	if (bPhysicsStateCreated)
	{
		// Physics uses world transform of the instance
		FTransform WorldTransform = bWorldSpace ? NewInstanceTransform : (LocalTransform * GetComponentTransform());
		FBodyInstance*& InstanceBodyInstance = InstanceBodies[InstanceIndex];
#if WITH_PHYSX
		if (NewInstanceTransform.GetScale3D().IsNearlyZero())
		{
			if (InstanceBodyInstance)
			{
				// delete BodyInstance
				InstanceBodyInstance->TermBody();
				delete InstanceBodyInstance;
				InstanceBodyInstance = nullptr;
			}
		}
		else
		{
			if (InstanceBodyInstance)
			{
				// Update existing BodyInstance
				InstanceBodyInstance->SetBodyTransform(WorldTransform, TeleportFlagToEnum(bTeleport));
				InstanceBodyInstance->UpdateBodyScale(WorldTransform.GetScale3D());
			}
			else
			{
				// create new BodyInstance
				InstanceBodyInstance = new FBodyInstance();
				InitInstanceBody(InstanceIndex, InstanceBodyInstance);
			}

		}
#endif //WITH_PHYSX
	}

	// Request navigation update
	PartialNavigationUpdate(InstanceIndex);

	if (PerInstanceRenderData.IsValid())
	{
		PerInstanceRenderData->UpdateInstanceData(this, InstanceIndex);
	}

	if (bMarkRenderStateDirty)
	{
		MarkRenderStateDirty();
	}

	return true;
}

TArray<int32> UInstancedStaticMeshComponent::GetInstancesOverlappingSphere(const FVector& Center, float Radius, bool bSphereInWorldSpace) const
{
	TArray<int32> Result;

	if (UStaticMesh* Mesh = GetStaticMesh())
	{
		FSphere Sphere(Center, Radius);
		if (bSphereInWorldSpace)
		{
			Sphere = Sphere.TransformBy(GetComponentTransform().Inverse());
		}

		const float StaticMeshBoundsRadius = Mesh->GetBounds().SphereRadius;

		for (int32 Index = 0; Index < PerInstanceSMData.Num(); Index++)
		{
			const FMatrix& Matrix = PerInstanceSMData[Index].Transform;
			const FSphere InstanceSphere(Matrix.GetOrigin(), StaticMeshBoundsRadius * Matrix.GetScaleVector().GetMax());

			if (Sphere.Intersects(InstanceSphere))
			{
				Result.Add(Index);
			}
		}
	}

	return Result;
}

TArray<int32> UInstancedStaticMeshComponent::GetInstancesOverlappingBox(const FBox& InBox, bool bBoxInWorldSpace) const
{
	TArray<int32> Result;

	if (UStaticMesh* Mesh = GetStaticMesh())
	{
		FBox Box(InBox);
		if (bBoxInWorldSpace)
		{
			Box = Box.TransformBy(GetComponentTransform().Inverse());
		}

		const FVector StaticMeshBoundsExtent = Mesh->GetBounds().BoxExtent;

		for (int32 Index = 0; Index < PerInstanceSMData.Num(); Index++)
		{
			const FMatrix& Matrix = PerInstanceSMData[Index].Transform;
			FBox InstanceBox(Matrix.GetOrigin() - StaticMeshBoundsExtent, Matrix.GetOrigin() + StaticMeshBoundsExtent);

			if (Box.Intersect(InstanceBox))
			{
				Result.Add(Index);
			}
		}
	}

	return Result;
}

bool UInstancedStaticMeshComponent::ShouldCreatePhysicsState() const
{
	return IsRegistered() && !IsBeingDestroyed() && GetStaticMesh() && (bAlwaysCreatePhysicsState || IsCollisionEnabled());
}

float UInstancedStaticMeshComponent::GetTextureStreamingTransformScale() const
{
	// By default if there are no per instance data, use a scale of 1.
	// This is required because some derived class use the instancing system without filling the per instance data. (like landscape grass)
	// In those cases, we assume the instance are spreaded across the bounds with a scale of 1.
	float TransformScale = 1.f; 

	if (PerInstanceSMData.Num() > 0)
	{
		TransformScale = Super::GetTextureStreamingTransformScale();

		float WeightedAxisScaleSum = 0;
		float WeightSum = 0;

		for (int32 InstanceIndex = 0; InstanceIndex < PerInstanceSMData.Num(); InstanceIndex++)
		{
			const float AxisScale = PerInstanceSMData[InstanceIndex].Transform.GetMaximumAxisScale();
			const float Weight = AxisScale; // The weight is the axis scale since we want to weight by surface coverage.
			WeightedAxisScaleSum += AxisScale * Weight;
			WeightSum += Weight;
		}

		if (WeightSum > SMALL_NUMBER)
		{
			TransformScale *= WeightedAxisScaleSum / WeightSum;
		}
	}
	return TransformScale;
}

bool UInstancedStaticMeshComponent::GetMaterialStreamingData(int32 MaterialIndex, FPrimitiveMaterialInfo& MaterialData) const
{
	// Same thing as StaticMesh but we take the full bounds to cover the instances.
	if (GetStaticMesh())
	{
		MaterialData.Material = GetMaterial(MaterialIndex);
		MaterialData.UVChannelData = GetStaticMesh()->GetUVChannelData(MaterialIndex);
		MaterialData.PackedRelativeBox = PackedRelativeBox_Identity;
	}
	return MaterialData.IsValid();
}

bool UInstancedStaticMeshComponent::BuildTextureStreamingData(ETextureStreamingBuildType BuildType, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TSet<FGuid>& DependentResources)
{
#if WITH_EDITORONLY_DATA // Only rebuild the data in editor 
	if (GetInstanceCount() > 0)
	{
		return Super::BuildTextureStreamingData(BuildType, QualityLevel, FeatureLevel, DependentResources);
	}
#endif
	return true;
}

void UInstancedStaticMeshComponent::GetStreamingTextureInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	// Don't only look the instance count but also if the bound is valid, as derived classes might not set PerInstanceSMData.
	if (GetInstanceCount() > 0 || Bounds.SphereRadius > 0)
	{
		return Super::GetStreamingTextureInfo(LevelContext, OutStreamingTextures);
	}
}

void UInstancedStaticMeshComponent::ClearInstances()
{
	if (bPerInstanceRenderDataWasPrebuilt)
	{
		return;
	}

	// Clear all the per-instance data
	PerInstanceSMData.Empty();
	InstanceReorderTable.Empty();
	RemovedInstances.Empty();

	ProxySize = 0;

	// Release any physics representations
	ClearAllInstanceBodies();

	// Indicate we need to update render state to reflect changes
	bPerInstanceRenderDataWasPrebuilt = false;
	ReleasePerInstanceRenderData();
	InitPerInstanceRenderData(false);
	MarkRenderStateDirty();

	UNavigationSystem::UpdateComponentInNavOctree(*this);
}

int32 UInstancedStaticMeshComponent::GetInstanceCount() const
{
	return PerInstanceSMData.Num();
}

void UInstancedStaticMeshComponent::SetCullDistances(int32 StartCullDistance, int32 EndCullDistance)
{
	InstanceStartCullDistance = StartCullDistance;
	InstanceEndCullDistance = EndCullDistance;
	MarkRenderStateDirty();
}

void UInstancedStaticMeshComponent::SetupNewInstanceData(FInstancedStaticMeshInstanceData& InOutNewInstanceData, int32 InInstanceIndex, const FTransform& InInstanceTransform)
{
	InOutNewInstanceData.Transform = InInstanceTransform.ToMatrixWithScale();
	InOutNewInstanceData.LightmapUVBias_DEPRECATED = FVector2D( -1.0f, -1.0f );
	InOutNewInstanceData.ShadowmapUVBias_DEPRECATED = FVector2D( -1.0f, -1.0f );

	if (bPhysicsStateCreated)
	{
		if (InInstanceTransform.GetScale3D().IsNearlyZero())
		{
			InstanceBodies.Insert(nullptr, InInstanceIndex);
		}
		else
		{
			FBodyInstance* NewBodyInstance = new FBodyInstance();
			int32 BodyIndex = InstanceBodies.Insert(NewBodyInstance, InInstanceIndex);
			check(InInstanceIndex == BodyIndex);
			InitInstanceBody(BodyIndex, NewBodyInstance);
		}
	}
}

void UInstancedStaticMeshComponent::InitPerInstanceRenderData(bool InitializeFromCurrentData, FStaticMeshInstanceData* InSharedInstanceBufferData)
{
	while (InstancingRandomSeed == 0)
	{
		InstancingRandomSeed = FMath::Rand();
	}

	UWorld* World = GetWorld();

	ERHIFeatureLevel::Type FeatureLevel = World != nullptr ? World->FeatureLevel : GMaxRHIFeatureLevel;

	if (!PerInstanceRenderData.IsValid())
	{
		bool IsDynamic = UseDynamicInstanceBuffer;

#if WITH_EDITOR
		if (!FApp::IsGame())
		{
			// In Editor mode we're always dynamic
			IsDynamic = true;
		}
#endif

		bPerInstanceRenderDataWasPrebuilt = false;

		if (InSharedInstanceBufferData != nullptr)
		{
			PerInstanceRenderData = MakeShareable(new FPerInstanceRenderData(this, *InSharedInstanceBufferData, FeatureLevel, IsDynamic, KeepInstanceBufferCPUAccess));
			bPerInstanceRenderDataWasPrebuilt = true;
		}
		else
		{
			PerInstanceRenderData = MakeShareable(new FPerInstanceRenderData(this, FeatureLevel, IsDynamic, KeepInstanceBufferCPUAccess, InitializeFromCurrentData));
		}
	}
}

void UInstancedStaticMeshComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	if (FApp::CanEverRender() && !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		InitPerInstanceRenderData(PerInstanceSMData.Num() > 0);
	}
}

void UInstancedStaticMeshComponent::PostLoad()
{
	Super::PostLoad();

	if (FApp::CanEverRender() && !HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject))
	{
		InitPerInstanceRenderData(false);

		if (PerInstanceSMData.Num() > 0 && PerInstanceRenderData->InstanceBuffer.GetNumInstances() == 0) // only load the data if it's not already loaded
		{
			UWorld* World = GetWorld();

			// Force update all the Render Data
			if (CVarASyncInstaneBufferConversion.GetValueOnGameThread() > 0 && World != nullptr && World->IsGameWorld())
			{
				World->AsyncPreRegisterLevelStreamingTasks.Increment();

				AsyncBuildInstanceBufferTask = new FAsyncTask<FAsyncBuildInstanceBuffer>(this, World);
				AsyncBuildInstanceBufferTask->StartBackgroundTask();
			}
			else
			{
				PerInstanceRenderData->UpdateAllInstanceData(this);
			}
		}
	}
}

void FAsyncBuildInstanceBuffer::DoWork()
{
	LLM_SCOPE(ELLMTag::StaticMesh);

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FoliageAsyncBufferUpdate);

	check(Component->PerInstanceRenderData.IsValid());
	Component->PerInstanceRenderData->UpdateAllInstanceData(Component);
	
	check(World);
	check(World->AsyncPreRegisterLevelStreamingTasks.GetValue() > 0);
	FPlatformMisc::MemoryBarrier();
	World->AsyncPreRegisterLevelStreamingTasks.Decrement();
}

void UInstancedStaticMeshComponent::PartialNavigationUpdate(int32 InstanceIdx)
{
	// Just update everything
	UNavigationSystem::UpdateComponentInNavOctree(*this);
}

bool UInstancedStaticMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	if (GetStaticMesh() && GetStaticMesh()->NavCollision)
	{
		UNavCollision* NavCollision = GetStaticMesh()->NavCollision;
		if (GetStaticMesh()->NavCollision->bIsDynamicObstacle)
		{
			return false;
		}
		
		if (NavCollision->bHasConvexGeometry)
		{
			GeomExport.ExportCustomMesh(NavCollision->ConvexCollision.VertexBuffer.GetData(), NavCollision->ConvexCollision.VertexBuffer.Num(),
				NavCollision->ConvexCollision.IndexBuffer.GetData(), NavCollision->ConvexCollision.IndexBuffer.Num(), FTransform::Identity);

			GeomExport.ExportCustomMesh(NavCollision->TriMeshCollision.VertexBuffer.GetData(), NavCollision->TriMeshCollision.VertexBuffer.Num(),
				NavCollision->TriMeshCollision.IndexBuffer.GetData(), NavCollision->TriMeshCollision.IndexBuffer.Num(), FTransform::Identity);
		}
		else
		{
			UBodySetup* BodySetup = GetStaticMesh()->BodySetup;
			if (BodySetup)
			{
				GeomExport.ExportRigidBodySetup(*BodySetup, FTransform::Identity);
			}
		}

		// Hook per instance transform delegate
		GeomExport.SetNavDataPerInstanceTransformDelegate(FNavDataPerInstanceTransformDelegate::CreateUObject(this, &UInstancedStaticMeshComponent::GetNavigationPerInstanceTransforms));
	}

	// we don't want "regular" collision export for this component
	return false;
}

void UInstancedStaticMeshComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	if (GetStaticMesh() && GetStaticMesh()->NavCollision)
	{
		UNavCollision* NavCollision = GetStaticMesh()->NavCollision;
		if (NavCollision->bIsDynamicObstacle)
		{
			NavCollision->GetNavigationModifier(Data.Modifiers, FTransform::Identity);

			// Hook per instance transform delegate
			Data.NavDataPerInstanceTransformDelegate = FNavDataPerInstanceTransformDelegate::CreateUObject(this, &UInstancedStaticMeshComponent::GetNavigationPerInstanceTransforms);
		}
	}
}

void UInstancedStaticMeshComponent::GetNavigationPerInstanceTransforms(const FBox& AreaBox, TArray<FTransform>& InstanceData) const
{
	for (const auto& InstancedData : PerInstanceSMData)
	{
		//TODO: Is it worth doing per instance bounds check here ?
		const FTransform InstanceToComponent(InstancedData.Transform);
		if (!InstanceToComponent.GetScale3D().IsZero())
		{
			InstanceData.Add(InstanceToComponent*GetComponentTransform());
		}
	}
}

void UInstancedStaticMeshComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	// proxy stuff
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(ProxySize); 

	// component stuff
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(InstanceBodies.GetAllocatedSize());
	for (int32 i=0; i < InstanceBodies.Num(); ++i)
	{
		if (InstanceBodies[i] != NULL && InstanceBodies[i]->IsValidBodyInstance())
		{
			InstanceBodies[i]->GetBodyInstanceResourceSizeEx(CumulativeResourceSize);
		}
	}
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(InstanceReorderTable.GetAllocatedSize());
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(RemovedInstances.GetAllocatedSize());

#if WITH_EDITOR
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(SelectedInstances.GetAllocatedSize());
#endif
}

void UInstancedStaticMeshComponent::BeginDestroy()
{
	ReleasePerInstanceRenderData();
	bPerInstanceRenderDataWasPrebuilt = false;
	Super::BeginDestroy();
}

void UInstancedStaticMeshComponent::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject) && bDuplicateForPIE)
	{
		InitPerInstanceRenderData(true);		
	}
}

#if WITH_EDITOR
void UInstancedStaticMeshComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.Property != NULL)
	{
		// Only permit editing archetype or instance if instance was changed by an archetype
		if (PropertyChangedEvent.Property->GetFName() == "PerInstanceSMData" && (HasAnyFlags(RF_ArchetypeObject|RF_ClassDefaultObject) ||  PropertyChangedEvent.HasArchetypeInstanceChanged(this)))
		{
			if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd
				|| PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
			{
				int32 AddedAtIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.Property->GetFName().ToString());
				check(AddedAtIndex != INDEX_NONE);

				AddInstanceInternal(AddedAtIndex, &PerInstanceSMData[AddedAtIndex], FTransform::Identity);

				// added via the property editor, so we will want to interactively work with instances
				bHasPerInstanceHitProxies = true;
			}
			else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove)
			{
				int32 RemovedAtIndex = PropertyChangedEvent.GetArrayIndex(PropertyChangedEvent.Property->GetFName().ToString());
				check(RemovedAtIndex != INDEX_NONE);

				RemoveInstanceInternal(RemovedAtIndex, true, true);
			}
			else if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayClear)
			{
				ClearInstances();
			}

			MarkRenderStateDirty();
		}
		else if (PropertyChangedEvent.Property->GetFName() == "Transform")
		{
			if (PerInstanceRenderData.IsValid())
			{
				PerInstanceRenderData->UpdateAllInstanceData(this);
			}

			PartialNavigationUpdate(-1);

			MarkRenderStateDirty();
		}
		else if (PropertyChangedEvent.Property->GetFName() == "UseDynamicInstanceBuffer"
			|| PropertyChangedEvent.Property->GetFName() == "KeepInstanceBufferCPUAccess")
		{
			// Force a full refresh of the instance buffer
			ReleasePerInstanceRenderData();
			InitPerInstanceRenderData(true);
			MarkRenderStateDirty();
		}
	}
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

void UInstancedStaticMeshComponent::PostEditUndo()
{
	Super::PostEditUndo();

	UNavigationSystem::UpdateComponentInNavOctree(*this);
}
#endif

bool UInstancedStaticMeshComponent::IsInstanceSelected(int32 InInstanceIndex) const
{
#if WITH_EDITOR
	if(SelectedInstances.IsValidIndex(InInstanceIndex))
	{
		return SelectedInstances[InInstanceIndex];
	}
#endif

	return false;
}

void UInstancedStaticMeshComponent::SelectInstance(bool bInSelected, int32 InInstanceIndex, int32 InInstanceCount)
{
#if WITH_EDITOR
	if (InInstanceCount > 0)
	{
		if (PerInstanceSMData.Num() != SelectedInstances.Num())
		{
			SelectedInstances.Init(false, PerInstanceSMData.Num());
		}

		check(SelectedInstances.IsValidIndex(InInstanceIndex));
		check(SelectedInstances.IsValidIndex(InInstanceIndex + (InInstanceCount - 1)));

		for (int32 InstanceIndex = InInstanceIndex; InstanceIndex < InInstanceIndex + InInstanceCount; InstanceIndex++)
		{
			SelectedInstances[InstanceIndex] = bInSelected;
		}

		PerInstanceRenderData->UpdateInstanceData(this, InInstanceIndex, InInstanceCount);
	}
#endif
}

void UInstancedStaticMeshComponent::ClearInstanceSelection()
{
#if WITH_EDITOR
	int32 InstanceCount = SelectedInstances.Num();
	SelectedInstances.Empty();

	PerInstanceRenderData->UpdateInstanceData(this, 0, InstanceCount);
#endif
}

static TAutoConsoleVariable<int32> CVarCullAllInVertexShader(
	TEXT("foliage.CullAllInVertexShader"),
	0,
	TEXT("Debugging, if this is greater than 0, cull all instances in the vertex shader."));

void FInstancedStaticMeshVertexFactoryShaderParameters::SetMesh( FRHICommandList& RHICmdList, FShader* VertexShader,const class FVertexFactory* VertexFactory,const class FSceneView& View,const struct FMeshBatchElement& BatchElement,uint32 DataFlags ) const 
{
	FLocalVertexFactoryShaderParameters::SetMesh(RHICmdList, VertexShader, VertexFactory, View, BatchElement, DataFlags);

	FRHIVertexShader* VS = VertexShader->GetVertexShader();
	const FInstancingUserData* InstancingUserData = (const FInstancingUserData*)BatchElement.UserData;
	if( InstancingWorldViewOriginOneParameter.IsBound() )
	{
		FVector4 InstancingViewZCompareZero(MIN_flt, MIN_flt, MAX_flt, 1.0f);
		FVector4 InstancingViewZCompareOne(MIN_flt, MIN_flt, MAX_flt, 0.0f);
		FVector4 InstancingViewZConstant(ForceInit);
		FVector4 InstancingWorldViewOriginZero(ForceInit);
		FVector4 InstancingWorldViewOriginOne(ForceInit);
		InstancingWorldViewOriginOne.W = 1.0f;
		if (InstancingUserData && BatchElement.InstancedLODRange)
		{
			int32 FirstLOD = InstancingUserData->MinLOD;

			int32 DebugMin = FMath::Min(CVarMinLOD.GetValueOnRenderThread(), InstancingUserData->MeshRenderData->LODResources.Num() - 1);
			if (DebugMin >= 0)
			{
				FirstLOD = FMath::Max(FirstLOD, DebugMin);
			}


			float SphereRadius = InstancingUserData->MeshRenderData->Bounds.SphereRadius;
			float MinSize = View.ViewMatrices.IsPerspectiveProjection() ? CVarFoliageMinimumScreenSize.GetValueOnRenderThread() : 0.0f;
			float LODScale = CVarFoliageLODDistanceScale.GetValueOnRenderThread();
			float LODRandom = CVarRandomLODRange.GetValueOnRenderThread();
			float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;

			if (BatchElement.InstancedLODIndex)
			{
				InstancingViewZConstant.X = -1.0f;
			}
			else
			{
				// this is the first LOD, so we don't have a fade-in region
				InstancingViewZConstant.X = 0.0f;
			}
			InstancingViewZConstant.Y = 0.0f;
			InstancingViewZConstant.Z = 1.0f;

			// now we subtract off the lower segments, since they will be incorporated 
			InstancingViewZConstant.Y -= InstancingViewZConstant.X;
			InstancingViewZConstant.Z -= InstancingViewZConstant.X + InstancingViewZConstant.Y;
			//not using W InstancingViewZConstant.W -= InstancingViewZConstant.X + InstancingViewZConstant.Y + InstancingViewZConstant.Z;

			for (int32 SampleIndex = 0; SampleIndex < 2; SampleIndex++)
			{
				FVector4& InstancingViewZCompare(SampleIndex ? InstancingViewZCompareOne : InstancingViewZCompareZero);
				float Fac = View.GetTemporalLODDistanceFactor(SampleIndex) * SphereRadius * LODScale;

				float FinalCull = MAX_flt;
				if (MinSize > 0.0)
				{
					FinalCull = ComputeBoundsDrawDistance(MinSize, SphereRadius, View.ViewMatrices.GetProjectionMatrix()) * LODScale;
				}
				if (InstancingUserData->EndCullDistance > 0.0f)
				{
					FinalCull = FMath::Min(FinalCull, InstancingUserData->EndCullDistance * MaxDrawDistanceScale);
				}
				FinalCull *= MaxDrawDistanceScale;

				InstancingViewZCompare.Z = FinalCull;
				if (BatchElement.InstancedLODIndex < InstancingUserData->MeshRenderData->LODResources.Num() - 1)
				{
					float NextCut = ComputeBoundsDrawDistance(InstancingUserData->MeshRenderData->ScreenSize[BatchElement.InstancedLODIndex + 1], SphereRadius, View.ViewMatrices.GetProjectionMatrix()) * LODScale;
					InstancingViewZCompare.Z = FMath::Min(NextCut, FinalCull);
				}

				InstancingViewZCompare.X = MIN_flt;
				if (BatchElement.InstancedLODIndex > FirstLOD)
				{
					float CurCut = ComputeBoundsDrawDistance(InstancingUserData->MeshRenderData->ScreenSize[BatchElement.InstancedLODIndex], SphereRadius, View.ViewMatrices.GetProjectionMatrix()) * LODScale;
					if (CurCut < FinalCull)
					{
						InstancingViewZCompare.Y = CurCut;
					}
					else
					{
						// this LOD is completely removed by one of the other two factors
						InstancingViewZCompare.Y = MIN_flt;
						InstancingViewZCompare.Z = MIN_flt;
					}
				}
				else
				{
					// this is the first LOD, so we don't have a fade-in region
					InstancingViewZCompare.Y = MIN_flt;
				}
			}


			InstancingWorldViewOriginZero = View.GetTemporalLODOrigin(0);
			InstancingWorldViewOriginOne = View.GetTemporalLODOrigin(1);

			float Alpha = View.GetTemporalLODTransition();
			InstancingWorldViewOriginZero.W = 1.0f - Alpha;
			InstancingWorldViewOriginOne.W = Alpha;

			InstancingViewZCompareZero.W = LODRandom;
		}
		SetShaderValue(RHICmdList, VS, InstancingViewZCompareZeroParameter, InstancingViewZCompareZero );
		SetShaderValue(RHICmdList, VS, InstancingViewZCompareOneParameter, InstancingViewZCompareOne );
		SetShaderValue(RHICmdList, VS, InstancingViewZConstantParameter, InstancingViewZConstant );
		SetShaderValue(RHICmdList, VS, InstancingWorldViewOriginZeroParameter, InstancingWorldViewOriginZero );
		SetShaderValue(RHICmdList, VS, InstancingWorldViewOriginOneParameter, InstancingWorldViewOriginOne );
	}
	if( InstancingFadeOutParamsParameter.IsBound() )
	{
		FVector4 InstancingFadeOutParams(MAX_flt,0.f,1.f,1.f);
		if (InstancingUserData)
		{
			InstancingFadeOutParams.X = InstancingUserData->StartCullDistance;
			if( InstancingUserData->EndCullDistance > 0 )
			{
				if( InstancingUserData->EndCullDistance > InstancingUserData->StartCullDistance )
				{
					InstancingFadeOutParams.Y = 1.f / (float)(InstancingUserData->EndCullDistance - InstancingUserData->StartCullDistance);
				}
				else
				{
					InstancingFadeOutParams.Y = 1.f;
				}
			}
			else
			{
				InstancingFadeOutParams.Y = 0.f;
			}
			if (CVarCullAllInVertexShader.GetValueOnRenderThread() > 0)
			{
				InstancingFadeOutParams.Z = 0.0f;
				InstancingFadeOutParams.W = 0.0f;
			}
			else
			{
			InstancingFadeOutParams.Z = InstancingUserData->bRenderSelected ? 1.f : 0.f;
			InstancingFadeOutParams.W = InstancingUserData->bRenderUnselected ? 1.f : 0.f;
		}
		}
		SetShaderValue(RHICmdList, VS, InstancingFadeOutParamsParameter, InstancingFadeOutParams );
	}

	auto ShaderPlatform = GetFeatureLevelShaderPlatform(View.GetFeatureLevel());
	const bool bInstanced = GRHISupportsInstancing;
	if (!bInstanced)
	{
		if (CPUInstanceOrigin.IsBound())
		{
			const float ShortScale = 1.0f / 32767.0f;
			auto* InstancingData = (const FInstancingUserData*)BatchElement.UserData;
			check(InstancingData);

			FVector4 InstanceTransform[3];
			FVector4 InstanceLightmapAndShadowMapUVBias;
			FVector4 InstanceOrigin;
			InstancingData->RenderData->PerInstanceRenderData->InstanceBuffer.GetInstanceShaderValues(BatchElement.UserIndex, InstanceTransform, InstanceLightmapAndShadowMapUVBias, InstanceOrigin);

			SetShaderValue(RHICmdList, VS, CPUInstanceOrigin, InstanceOrigin);
			SetShaderValueArray(RHICmdList, VS, CPUInstanceTransform, InstanceTransform, 3);
			SetShaderValue(RHICmdList, VS, CPUInstanceLightmapAndShadowMapBias, InstanceLightmapAndShadowMapUVBias); 
		}
	}
}
