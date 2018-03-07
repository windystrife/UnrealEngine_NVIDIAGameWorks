// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved. 

#include "CustomMeshComponent.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Engine/CollisionProfile.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"

/** Vertex Buffer */
class FCustomMeshVertexBuffer : public FVertexBuffer 
{
public:
	TArray<FDynamicMeshVertex> Vertices;

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		void* VertexBufferData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Vertices.Num() * sizeof(FDynamicMeshVertex), BUF_Static, CreateInfo, VertexBufferData);

		// Copy the vertex data into the vertex buffer.		
		FMemory::Memcpy(VertexBufferData,Vertices.GetData(),Vertices.Num() * sizeof(FDynamicMeshVertex));
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}

};

/** Index Buffer */
class FCustomMeshIndexBuffer : public FIndexBuffer 
{
public:
	TArray<int32> Indices;

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32),Indices.Num() * sizeof(int32),BUF_Static, CreateInfo, Buffer);

		// Write the indices to the index buffer.		
		FMemory::Memcpy(Buffer,Indices.GetData(),Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};

/** Vertex Factory */
class FCustomMeshVertexFactory : public FLocalVertexFactory
{
public:

	FCustomMeshVertexFactory()
	{}

	/** Init function that should only be called on render thread. */
	void Init_RenderThread(const FCustomMeshVertexBuffer* VertexBuffer)
	{
		check(IsInRenderingThread());

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

	/** Initialization */
	void Init(const FCustomMeshVertexBuffer* VertexBuffer)
	{
		if (IsInRenderingThread())
		{
			Init_RenderThread(VertexBuffer);
		}
		else
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				InitCustomMeshVertexFactory,
				FCustomMeshVertexFactory*, VertexFactory, this,
				const FCustomMeshVertexBuffer*, VertexBuffer, VertexBuffer,
				{
				VertexFactory->Init_RenderThread(VertexBuffer);
			});
		}	
	}
};

/** Scene proxy */
class FCustomMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

	FCustomMeshSceneProxy(UCustomMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		const FColor VertexColor(255,255,255);

		const int32 NumTris = Component->CustomMeshTris.Num();
		VertexBuffer.Vertices.AddUninitialized(NumTris * 3);
		IndexBuffer.Indices.AddUninitialized(NumTris * 3);
		// Add each triangle to the vertex/index buffer
		for(int32 TriIdx = 0; TriIdx < NumTris; TriIdx++)
		{
			FCustomMeshTriangle& Tri = Component->CustomMeshTris[TriIdx];

			const FVector Edge01 = (Tri.Vertex1 - Tri.Vertex0);
			const FVector Edge02 = (Tri.Vertex2 - Tri.Vertex0);

			const FVector TangentX = Edge01.GetSafeNormal();
			const FVector TangentZ = (Edge02 ^ Edge01).GetSafeNormal();
			const FVector TangentY = (TangentX ^ TangentZ).GetSafeNormal();

			FDynamicMeshVertex Vert;
			
			Vert.Color = VertexColor;
			Vert.SetTangents(TangentX, TangentY, TangentZ);

			Vert.Position = Tri.Vertex0;
			VertexBuffer.Vertices[TriIdx * 3 + 0] = Vert;
			IndexBuffer.Indices[TriIdx * 3 + 0] = TriIdx * 3 + 0;

			Vert.Position = Tri.Vertex1;
			VertexBuffer.Vertices[TriIdx * 3 + 1] = Vert;
			IndexBuffer.Indices[TriIdx * 3 + 1] = TriIdx * 3 + 1;

			Vert.Position = Tri.Vertex2;
			VertexBuffer.Vertices[TriIdx * 3 + 2] = Vert;
			IndexBuffer.Indices[TriIdx * 3 + 2] = TriIdx * 3 + 2;
		}

		// Init vertex factory
		VertexFactory.Init(&VertexBuffer);

		// Enqueue initialization of render resource
		BeginInitResource(&VertexBuffer);
		BeginInitResource(&IndexBuffer);
		BeginInitResource(&VertexFactory);

		// Grab material
		Material = Component->GetMaterial(0);
		if(Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}

	virtual ~FCustomMeshSceneProxy()
	{
		VertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_CustomMeshSceneProxy_GetDynamicMeshElements );

		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : NULL,
			FLinearColor(0, 0.5f, 1.f)
			);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

		FMaterialRenderProxy* MaterialProxy = NULL;
		if(bWireframe)
		{
			MaterialProxy = WireframeMaterialInstance;
		}
		else
		{
			MaterialProxy = Material->GetRenderProxy(IsSelected());
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				// Draw the mesh.
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialProxy;
				BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = VertexBuffer.Vertices.Num() - 1;
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }

	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:

	UMaterialInterface* Material;
	FCustomMeshVertexBuffer VertexBuffer;
	FCustomMeshIndexBuffer IndexBuffer;
	FCustomMeshVertexFactory VertexFactory;

	FMaterialRelevance MaterialRelevance;
};

//////////////////////////////////////////////////////////////////////////

UCustomMeshComponent::UCustomMeshComponent( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
}

bool UCustomMeshComponent::SetCustomMeshTriangles(const TArray<FCustomMeshTriangle>& Triangles)
{
	CustomMeshTris = Triangles;

	// Need to recreate scene proxy to send it over
	MarkRenderStateDirty();
	UpdateBounds();

	return true;
}

void UCustomMeshComponent::AddCustomMeshTriangles(const TArray<FCustomMeshTriangle>& Triangles)
{
	CustomMeshTris.Append(Triangles);

	// Need to recreate scene proxy to send it over
	MarkRenderStateDirty();
	UpdateBounds();
}

void  UCustomMeshComponent::ClearCustomMeshTriangles()
{
	CustomMeshTris.Reset();

	// Need to recreate scene proxy to send it over
	MarkRenderStateDirty();
	UpdateBounds();
}


FPrimitiveSceneProxy* UCustomMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if(CustomMeshTris.Num() > 0)
	{
		Proxy = new FCustomMeshSceneProxy(this);
	}
	return Proxy;
}

int32 UCustomMeshComponent::GetNumMaterials() const
{
	return 1;
}


FBoxSphereBounds UCustomMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(ForceInit);

	// Bounds are tighter if the box is generated from pre-transformed vertices.
	for (int32 Index = 0; Index < CustomMeshTris.Num(); ++Index)
	{
		BoundingBox += LocalToWorld.TransformPosition(CustomMeshTris[Index].Vertex0);
		BoundingBox += LocalToWorld.TransformPosition(CustomMeshTris[Index].Vertex1);
		BoundingBox += LocalToWorld.TransformPosition(CustomMeshTris[Index].Vertex2);
	}

	FBoxSphereBounds NewBounds;
	NewBounds.BoxExtent = BoundingBox.GetExtent();
	NewBounds.Origin = BoundingBox.GetCenter();
	NewBounds.SphereRadius = NewBounds.BoxExtent.Size();

	return NewBounds;
}

