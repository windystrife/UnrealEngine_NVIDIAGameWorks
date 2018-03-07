// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheSceneProxy.h"
#include "MaterialShared.h"
#include "SceneManagement.h"
#include "EngineGlobals.h"
#include "Materials/Material.h"
#include "Engine/Engine.h"
#include "GeometryCacheComponent.h"
#include "GeometryCacheMeshData.h"

FGeometryCacheSceneProxy::FGeometryCacheSceneProxy(UGeometryCacheComponent* Component) : FPrimitiveSceneProxy(Component)
, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
	// Copy each section
	const int32 NumSections = Component->TrackSections.Num();
	Sections.AddZeroed(NumSections);
	for (int SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
	{
		FTrackRenderData& SrcSection = Component->TrackSections[SectionIdx];
		
		if (SrcSection.MeshData->Indices.Num() > 0)
		{
			FGeomCacheTrackProxy* NewSection = new FGeomCacheTrackProxy();

			NewSection->WorldMatrix = SrcSection.WorldMatrix;
			FGeometryCacheMeshData* MeshData = NewSection->MeshData = SrcSection.MeshData;

			// Copy data from vertex buffer
			const int32 NumVerts = MeshData->Vertices.Num();

			// Allocate verts
			NewSection->VertexBuffer.Vertices.Empty(NumVerts);
			// Copy verts
			NewSection->VertexBuffer.Vertices.Append(MeshData->Vertices);

			// Copy index buffer
			NewSection->IndexBuffer.Indices = SrcSection.MeshData->Indices;

			// Init vertex factory
			NewSection->VertexFactory.Init(&NewSection->VertexBuffer);

			// Enqueue initialization of render resource
			BeginInitResource(&NewSection->VertexBuffer);
			BeginInitResource(&NewSection->IndexBuffer);
			BeginInitResource(&NewSection->VertexFactory);

			// Grab materials
			for (FGeometryCacheMeshBatchInfo& BatchInfo : MeshData->BatchesInfo)
			{
				UMaterialInterface* Material = Component->GetMaterial(BatchInfo.MaterialIndex);
				if (Material == nullptr)
				{
					Material = UMaterial::GetDefaultMaterial(MD_Surface);
				}

				NewSection->Materials.Push(Material);
			}			

			// Save ref to new section
			Sections[SectionIdx] = NewSection;
		}
	}
}

FGeometryCacheSceneProxy::~FGeometryCacheSceneProxy()
{
	for (FGeomCacheTrackProxy* Section : Sections)
	{
		if (Section != nullptr)
		{
			Section->VertexBuffer.ReleaseResource();
			Section->IndexBuffer.ReleaseResource();
			Section->VertexFactory.ReleaseResource();
			delete Section;
		}
	}
	Sections.Empty();
}

void FGeometryCacheSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_GeometryCacheSceneProxy_GetMeshElements);

	// Set up wireframe material (if needed)
	const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

	FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;
	if (bWireframe)
	{
		WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : nullptr,
			FLinearColor(0, 0.5f, 1.f)
			);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
	}
	
	// Iterate over sections	
	for (const FGeomCacheTrackProxy* TrackProxy : Sections )
	{
		// Render out stored TrackProxy's
		if (TrackProxy != nullptr)
		{
			INC_DWORD_STAT_BY(STAT_GeometryCacheSceneProxy_MeshBatchCount, TrackProxy->MeshData->BatchesInfo.Num());

			int32 BatchIndex = 0;
			for (FGeometryCacheMeshBatchInfo& BatchInfo : TrackProxy->MeshData->BatchesInfo)
			{
				FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : TrackProxy->Materials[BatchIndex]->GetRenderProxy(IsSelected());

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						const FSceneView* View = Views[ViewIndex];
						// Draw the mesh.
						FMeshBatch& Mesh = Collector.AllocateMesh();
						FMeshBatchElement& BatchElement = Mesh.Elements[0];
						BatchElement.IndexBuffer = &TrackProxy->IndexBuffer;
						Mesh.bWireframe = bWireframe;
						Mesh.VertexFactory = &TrackProxy->VertexFactory;
						Mesh.MaterialRenderProxy = MaterialProxy;
						BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(TrackProxy->WorldMatrix * GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
						BatchElement.FirstIndex = BatchInfo.StartIndex;
						BatchElement.NumPrimitives = BatchInfo.NumTriangles;
						BatchElement.MinVertexIndex = 0;
						BatchElement.MaxVertexIndex = TrackProxy->VertexBuffer.Vertices.Num() - 1;
						Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
						Mesh.Type = PT_TriangleList;
						Mesh.DepthPriorityGroup = SDPG_World;
						Mesh.bCanApplyViewModeOverrides = false;
						Collector.AddMesh(ViewIndex, Mesh);

						INC_DWORD_STAT_BY(STAT_GeometryCacheSceneProxy_TriangleCount, BatchElement.NumPrimitives);
					}
				}

				++BatchIndex;
			}			
		}
		
	}

	// Draw bounds
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			// Render bounds
			RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
		}
	}
#endif
}

FPrimitiveViewRelevance FGeometryCacheSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}

bool FGeometryCacheSceneProxy::CanBeOccluded() const
{
	return !MaterialRelevance.bDisableDepthTest;
}

uint32 FGeometryCacheSceneProxy::GetMemoryFootprint(void) const
{
	return(sizeof(*this) + GetAllocatedSize());
}

uint32 FGeometryCacheSceneProxy::GetAllocatedSize(void) const
{
	return(FPrimitiveSceneProxy::GetAllocatedSize());
}

void FGeometryCacheSceneProxy::UpdateSectionWorldMatrix(const int32 SectionIndex, const FMatrix& WorldMatrix)
{
	check(SectionIndex < Sections.Num() && "Section Index out of range");
	Sections[SectionIndex]->WorldMatrix = WorldMatrix;
}


void FGeometryCacheSceneProxy::UpdateSectionVertexBuffer(const int32 SectionIndex, FGeometryCacheMeshData* MeshData)
{
	check(SectionIndex < Sections.Num() && "Section Index out of range");
	check(IsInRenderingThread());

	Sections[SectionIndex]->MeshData = MeshData;

	const bool bRecreate = Sections[SectionIndex]->VertexBuffer.Vertices.Num() != Sections[SectionIndex]->MeshData->Vertices.Num();
	Sections[SectionIndex]->VertexBuffer.Vertices.Empty(Sections[SectionIndex]->MeshData->Vertices.Num());
	Sections[SectionIndex]->VertexBuffer.Vertices.Append(Sections[SectionIndex]->MeshData->Vertices);

	if (bRecreate)
	{
		Sections[SectionIndex]->VertexBuffer.InitRHI();
	}
	else
	{
		Sections[SectionIndex]->VertexBuffer.UpdateRHI();
	}
}

void FGeometryCacheSceneProxy::UpdateSectionIndexBuffer(const int32 SectionIndex, const TArray<uint32>& Indices)
{
	check(SectionIndex < Sections.Num() && "Section Index out of range");
	check(IsInRenderingThread());

	const bool bRecreate = Sections[SectionIndex]->IndexBuffer.Indices.Num() != Indices.Num();
	
	Sections[SectionIndex]->IndexBuffer.Indices.Empty(Indices.Num());
	Sections[SectionIndex]->IndexBuffer.Indices.Append(Indices);
	if (bRecreate)
	{
		Sections[SectionIndex]->IndexBuffer.InitRHI();
	}
	else
	{
		Sections[SectionIndex]->IndexBuffer.UpdateRHI();
	}	
}

void FGeometryCacheSceneProxy::ClearSections()
{
	Sections.Empty();
}

FGeomCacheVertexFactory::FGeomCacheVertexFactory()
{

}

void FGeomCacheVertexFactory::Init_RenderThread(const FGeomCacheVertexBuffer* VertexBuffer)
{
	check(IsInRenderingThread());

	// Initialize the vertex factory's stream components.
	FDataType NewData;
	NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Position, VET_Float3);
	NewData.TextureCoordinates.Add(
		FVertexStreamComponent(VertexBuffer, STRUCT_OFFSET(FDynamicMeshVertex, TextureCoordinate), sizeof(FDynamicMeshVertex), VET_Float2)
		);
	NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentX, VET_PackedNormal);
	NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, TangentZ, VET_PackedNormal);
	NewData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer, FDynamicMeshVertex, Color, VET_Color);
	SetData(NewData);
}

void FGeomCacheVertexFactory::Init(const FGeomCacheVertexBuffer* VertexBuffer)
{
	if (IsInRenderingThread())
	{
		Init_RenderThread(VertexBuffer);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitGeomCacheVertexFactory,
			FGeomCacheVertexFactory*, VertexFactory, this,
			const FGeomCacheVertexBuffer*, VertexBuffer, VertexBuffer,
			{
			VertexFactory->Init_RenderThread(VertexBuffer);
		});
	}
}

void FGeomCacheIndexBuffer::InitRHI()
{
	FRHIResourceCreateInfo CreateInfo;
	void* Buffer = nullptr;
	IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(uint32), Indices.Num() * sizeof(uint32), BUF_Static, CreateInfo, Buffer);

	// Write the indices to the index buffer.	

	// Write the indices to the index buffer.
	FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(uint32));
	RHIUnlockIndexBuffer(IndexBufferRHI);
}

void FGeomCacheIndexBuffer::UpdateRHI()
{
	// Copy the index data into the index buffer.
	void* Buffer = RHILockIndexBuffer(IndexBufferRHI, 0, Indices.Num() * sizeof(uint32), RLM_WriteOnly);
	FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(uint32));
	RHIUnlockIndexBuffer(IndexBufferRHI);
}

void FGeomCacheVertexBuffer::InitRHI()
{
	const uint32 SizeInBytes = Vertices.Num() * sizeof(FDynamicMeshVertex);

	FGeomCacheVertexResourceArray ResourceArray(Vertices.GetData(), SizeInBytes);
	FRHIResourceCreateInfo CreateInfo(&ResourceArray);
	VertexBufferRHI = RHICreateVertexBuffer(SizeInBytes, BUF_Static, CreateInfo);
}


void FGeomCacheVertexBuffer::UpdateRHI()
{
	// Copy the vertex data into the vertex buffer.
	void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI, 0, Vertices.Num() * sizeof(FDynamicMeshVertex), RLM_WriteOnly);
	FMemory::Memcpy(VertexBufferData, Vertices.GetData(), Vertices.Num() * sizeof(FDynamicMeshVertex));
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

FGeomCacheVertexResourceArray::FGeomCacheVertexResourceArray(void* InData, uint32 InSize) : Data(InData)
, Size(InSize)
{

}

const void* FGeomCacheVertexResourceArray::GetResourceData() const
{
	return Data;
}

uint32 FGeomCacheVertexResourceArray::GetResourceDataSize() const
{
	return Size;
}

void FGeomCacheVertexResourceArray::Discard()
{

}

bool FGeomCacheVertexResourceArray::IsStatic() const
{
	return false;
}

bool FGeomCacheVertexResourceArray::GetAllowCPUAccess() const
{
	return false;
}

void FGeomCacheVertexResourceArray::SetAllowCPUAccess(bool bInNeedsCPUAccess)
{

}

