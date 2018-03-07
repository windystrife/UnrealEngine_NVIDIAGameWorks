// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MRMeshComponent.h"
#include "PrimitiveSceneProxy.h"
#include "DynamicMeshBuilder.h"
#include "LocalVertexFactory.h"
#include "Containers/ResourceArray.h"
#include "SceneManagement.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "RenderingThread.h"
#include "BaseMeshReconstructorModule.h"
#include "MeshReconstructorBase.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsPublic.h"
#include "IPhysXCooking.h"
#include "PhysXCookHelper.h"
#include "Misc/RuntimeErrors.h"

/**
 * A vertex buffer with some dummy data to send down for MRMesh vertex components that we aren't feeding at the moment.
 */
class FNullVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	virtual void InitRHI() override
	{
		static const int32 NUM_ELTS = 4;

		// create a static vertex buffer
		FRHIResourceCreateInfo CreateInfo;

		void* LockedData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(NUM_ELTS*sizeof(uint32), BUF_Static | BUF_ZeroStride | BUF_ShaderResource, CreateInfo, LockedData);
		uint32* Vertices = (uint32*)LockedData;
		Vertices[0] = FColor(255, 255, 255, 255).DWColor();
		Vertices[1] = FColor(255, 255, 255, 255).DWColor();
		Vertices[2] = FColor(255, 255, 255, 255).DWColor();
		Vertices[3] = FColor(255, 255, 255, 255).DWColor();
		RHIUnlockVertexBuffer(VertexBufferRHI);
		VertexBufferSRV = RHICreateShaderResourceView(VertexBufferRHI, NUM_ELTS*sizeof(FColor), PF_R8G8B8A8);
	}

	virtual void ReleaseRHI() override
	{
		VertexBufferSRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}

	FShaderResourceViewRHIRef VertexBufferSRV;
};

class FMRMeshVertexResourceArray : public FResourceArrayInterface
{
public:
	FMRMeshVertexResourceArray(void* InData, uint32 InSize)
		: Data(InData)
		, Size(InSize)
	{
	}

	virtual const void* GetResourceData() const override { return Data; }
	virtual uint32 GetResourceDataSize() const override { return Size; }
	virtual void Discard() override { }
	virtual bool IsStatic() const override { return false; }
	virtual bool GetAllowCPUAccess() const override { return false; }
	virtual void SetAllowCPUAccess(bool bInNeedsCPUAccess) override { }

private:
	void* Data;
	uint32 Size;
};

/** Support for non-interleaved data streams. */
template<typename DataType>
class FMRMeshVertexBuffer : public FVertexBuffer
{
public:
	int32 NumVerts = 0;
	void InitRHIWith( TArray<DataType>& PerVertexData )
	{
		NumVerts = PerVertexData.Num();

		const uint32 SizeInBytes = PerVertexData.Num() * sizeof(DataType);

		FMRMeshVertexResourceArray ResourceArray(PerVertexData.GetData(), SizeInBytes);
		FRHIResourceCreateInfo CreateInfo(&ResourceArray);
		VertexBufferRHI = RHICreateVertexBuffer(SizeInBytes, BUF_Static, CreateInfo);
	}

};

class FMRMeshIndexBuffer : public FIndexBuffer
{
public:
	int32 NumIndices = 0;
	void InitRHIWith( const TArray<uint32>& Indices )
	{
		NumIndices = Indices.Num();

		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32), Indices.Num() * sizeof(int32), BUF_Static, CreateInfo, Buffer);

		// Write the indices to the index buffer.
		FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
};


struct FMRMeshProxySection;

class FMRMeshVertexFactory : public FLocalVertexFactory
{
public:

	FMRMeshVertexFactory()
	{}

	/** Init function that should only be called on render thread. */
	void Init_RenderThread(const FMRMeshProxySection& MRMeshSection);

	/** Init function that can be called on any thread, and will do the right thing (enqueue command if called on main thread) */
	void Init(const FMRMeshProxySection& MRMeshSection);
};

struct FMRMeshProxySection
{
	/** Which brick this section represents */
	FIntVector BrickId;
	/** Position buffer */
	FMRMeshVertexBuffer<FVector> PositionBuffer;
	/** Texture coordinates buffer */
	FNullVertexBuffer UVBuffer;
	/** Tangent space buffer */
	FNullVertexBuffer TangentXBuffer;
	/** Tangent space buffer */
	FNullVertexBuffer TangentZBuffer;
	/** We don't need color */
	FMRMeshVertexBuffer<FColor> ColorBuffer;
	/** Index buffer for this section */
	FMRMeshIndexBuffer IndexBuffer;
	/** Vertex factory for this section */
	FMRMeshVertexFactory VertexFactory;

	FMRMeshProxySection(FIntVector InBrickId)
		: BrickId(InBrickId)
	{
	}

	void ReleaseResources()
	{
		PositionBuffer.ReleaseResource();
		UVBuffer.ReleaseResource();
		TangentXBuffer.ReleaseResource();
		TangentZBuffer.ReleaseResource();
		ColorBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	FMRMeshProxySection(const FMRMeshVertexFactory&) = delete;
	void operator==(const FMRMeshVertexFactory&) = delete;
};


void FMRMeshVertexFactory::Init_RenderThread(const FMRMeshProxySection& MRMeshSection)
{
	check(IsInRenderingThread());

	// Initialize the vertex factory's stream components.
	FDataType NewData;
	NewData.PositionComponent = FVertexStreamComponent(&MRMeshSection.PositionBuffer, 0, sizeof(FVector), VET_Float3);
	NewData.TextureCoordinates.Add(
		FVertexStreamComponent(&MRMeshSection.UVBuffer, 0, 0/*sizeof(FVector2D)*/, VET_Float2)
	);
	NewData.TangentBasisComponents[0] = FVertexStreamComponent(&MRMeshSection.TangentXBuffer, 0, 0/*sizeof(FPackedNormal)*/, VET_PackedNormal);
	NewData.TangentBasisComponents[1] = FVertexStreamComponent(&MRMeshSection.TangentZBuffer, 0, 0/*sizeof(FPackedNormal)*/, VET_PackedNormal);
	NewData.ColorComponent = FVertexStreamComponent(&MRMeshSection.ColorBuffer, 0, sizeof(FColor), VET_Color);
	SetData(NewData);
}

void FMRMeshVertexFactory::Init(const FMRMeshProxySection& MRMeshSection)
{
	if (IsInRenderingThread())
	{
		Init_RenderThread(MRMeshSection);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitProcMeshVertexFactory,
			FMRMeshVertexFactory*, VertexFactory, this,
			const FMRMeshProxySection&, MRMeshSection, MRMeshSection,
			{
				VertexFactory->Init_RenderThread(MRMeshSection);
			});
	}
}


class FMRMeshProxy : public FPrimitiveSceneProxy
{
public:
	FMRMeshProxy(const UMRMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, MaterialToUse((InComponent->Material!=nullptr) ? InComponent->Material : UMaterial::GetDefaultMaterial(MD_Surface) )
	{
	}

	virtual ~FMRMeshProxy()
	{
		for (FMRMeshProxySection* Section : ProxySections)
		{
			if (Section != nullptr)
			{
				Section->ReleaseResources();
				delete Section;
			}
		}
	}

	void RenderThread_UploadNewSection(IMRMesh::FSendBrickDataArgs Args)
	{
		check(IsInRenderingThread() || IsInRHIThread());

		FMRMeshProxySection* NewSection = new FMRMeshProxySection(Args.BrickCoords);
		ProxySections.Add(NewSection);

		check(Args.PositionData.Num() == Args.ColorData.Num() || Args.ColorData.Num() == 0
			//&& Args.PositionData.Num() == Args.UVData.Num()
			//&& Args.PositionData.Num() == Args.TangentXData.Num()
			//&& Args.PositionData.Num() == Args.TangentZData.Num()
		);

		// POSITION BUFFER
		{
			NewSection->PositionBuffer.InitResource();
			NewSection->PositionBuffer.InitRHIWith(Args.PositionData);
		}

		// TEXTURE COORDS BUFFER
		{
			NewSection->UVBuffer.InitResource();
			//NewSection->UVBuffer.InitRHIWith(Args.UVData);
		}

		// TEXTURE COORDS BUFFER
		{
			NewSection->TangentXBuffer.InitResource();
			//NewSection->TangentXBuffer.InitRHIWith(Args.TangentXData);
		}

		// TEXTURE COORDS BUFFER
		{
			NewSection->TangentZBuffer.InitResource();
			//NewSection->TangentZBuffer.InitRHIWith(Args.TangentZData);
		}

		// NO COLOR
		{
			NewSection->ColorBuffer.InitResource();
			NewSection->ColorBuffer.InitRHIWith(Args.ColorData);
		}

		// INDEX BUFFER
		{
			NewSection->IndexBuffer.InitResource();
			NewSection->IndexBuffer.InitRHIWith(Args.Indices);
		}

		// VERTEX FACTORY
		{
			NewSection->VertexFactory.Init(*NewSection);
			NewSection->VertexFactory.InitResource();
		}
	}

	bool RenderThread_RemoveSection(FIntVector BrickCoords)
	{
		check(IsInRenderingThread() || IsInRHIThread());
		for (int32 i = 0; i < ProxySections.Num(); ++i)
		{
			if (ProxySections[i]->BrickId == BrickCoords)
			{
				ProxySections[i]->ReleaseResources();
				delete ProxySections[i];
				ProxySections.RemoveAtSwap(i);
				return true;
			}
		}
		return false;
	}

	void RenderThread_RemoveAllSections()
	{
		check(IsInRenderingThread() || IsInRHIThread());
		for (int32 i = ProxySections.Num()-1; i >=0; i--)
		{
			ProxySections[i]->ReleaseResources();
			delete ProxySections[i];
			ProxySections.RemoveAtSwap(i);
		}
	}

private:
	//~ FPrimitiveSceneProxy
	virtual uint32 GetMemoryFootprint(void) const override
	{
		return 0;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override
	{
		static const FBoxSphereBounds InfiniteBounds(FSphere(FVector::ZeroVector, HALF_WORLD_MAX));

		// Iterate over sections
		for (const FMRMeshProxySection* Section : ProxySections)
		{
			if (Section != nullptr)
			{
				const bool bIsSelected = false;
				FMaterialRenderProxy* MaterialProxy = MaterialToUse->GetRenderProxy(bIsSelected);

				// For each view..
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						const FSceneView* View = Views[ViewIndex];
						// Draw the mesh.
						FMeshBatch& Mesh = Collector.AllocateMesh();
						FMeshBatchElement& BatchElement = Mesh.Elements[0];
						BatchElement.IndexBuffer = &Section->IndexBuffer;
						Mesh.bWireframe = false;
						Mesh.VertexFactory = &Section->VertexFactory;
						Mesh.MaterialRenderProxy = MaterialProxy;
						BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), InfiniteBounds, InfiniteBounds, true, UseEditorDepthTest());
						BatchElement.FirstIndex = 0;
						BatchElement.NumPrimitives = Section->IndexBuffer.NumIndices / 3;
						BatchElement.MinVertexIndex = 0;
						BatchElement.MaxVertexIndex = Section->PositionBuffer.NumVerts - 1;
						Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
						Mesh.Type = PT_TriangleList;
						Mesh.DepthPriorityGroup = SDPG_World;
						Mesh.bCanApplyViewModeOverrides = false;
						Collector.AddMesh(ViewIndex, Mesh);
					}
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		//MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}
	//~ FPrimitiveSceneProxy

private:
	TArray<FMRMeshProxySection*> ProxySections;
	UMaterialInterface* MaterialToUse;
};


UMRMeshComponent::UMRMeshComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, MeshReconstructor(nullptr)
{

}

void UMRMeshComponent::ConnectReconstructor(UMeshReconstructorBase* Reconstructor)
{
	if (ensureAsRuntimeWarning(Reconstructor != nullptr))
	{
		if (ensure(MeshReconstructor == nullptr))
		{
			MeshReconstructor = Reconstructor;
			MeshReconstructor->ConnectMRMesh(this);
		}
	}
}

UMeshReconstructorBase* UMRMeshComponent::GetReconstructor() const
{
	return MeshReconstructor;
}

FPrimitiveSceneProxy* UMRMeshComponent::CreateSceneProxy()
{
	// The render thread owns the memory, so if this function is
	// being called, it's safe to just re-allocate.
	return new FMRMeshProxy(this);
}

void UMRMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials /*= false*/) const
{
	if (Material != nullptr)
	{
		OutMaterials.Add(Material);
	}
}

FBoxSphereBounds UMRMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds(FSphere(FVector::ZeroVector, HALF_WORLD_MAX));
}

void UMRMeshComponent::SendBrickData(IMRMesh::FSendBrickDataArgs Args, const FOnProcessingComplete& OnProcessingComplete /*= FOnProcessingComplete()*/)
{
	auto BrickDataTask = FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UMRMeshComponent::SendBrickData_Internal, Args, OnProcessingComplete);

	DECLARE_CYCLE_STAT(TEXT("UMRMeshComponent.SendBrickData"),
		STAT_UMRMeshComponent_SendBrickData,
		STATGROUP_MRMESH);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(BrickDataTask, GET_STATID(STAT_UMRMeshComponent_SendBrickData), nullptr, ENamedThreads::GameThread);

}

void UMRMeshComponent::ClearAllBrickData()
{
	auto ClearBrickDataTask = FSimpleDelegateGraphTask::FDelegate::CreateUObject(this, &UMRMeshComponent::ClearAllBrickData_Internal);

	DECLARE_CYCLE_STAT(TEXT("UMRMeshComponent.ClearAllBrickData"),
	STAT_UMRMeshComponent_ClearAllBrickData,
		STATGROUP_MRMESH);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(ClearBrickDataTask, GET_STATID(STAT_UMRMeshComponent_ClearAllBrickData), nullptr, ENamedThreads::GameThread);
}

UBodySetup* CreateBodySetupHelper(UMRMeshComponent* Outer)
{
	// The body setup in a template needs to be public since the property is Instanced and thus is the archetype of the instance meaning there is a direct reference
	UBodySetup* NewBS = NewObject<UBodySetup>(Outer, NAME_None);
	NewBS->BodySetupGuid = FGuid::NewGuid();
	NewBS->bGenerateMirroredCollision = false;
	NewBS->bHasCookedCollisionData = true;
	return NewBS;
}

void UMRMeshComponent::SendBrickData_Internal(IMRMesh::FSendBrickDataArgs Args, FOnProcessingComplete OnProcessingComplete)
{
	check(IsInGameThread());

	if (!IsPendingKill() && this->GetCollisionEnabled())
	{
		// Physics update
		UWorld* MyWorld = GetWorld();
		if ( MyWorld && MyWorld->GetPhysicsScene() )
		{
			int32 BodyIndex = BodyIds.Find(Args.BrickCoords);

			if (const bool bBrickHasData = Args.Indices.Num() > 0)
			{
				if (BodyIndex == INDEX_NONE)
				{
					BodyIds.Add(Args.BrickCoords);
					BodySetups.Add(CreateBodySetupHelper(this));
					BodyInstances.Add(new FBodyInstance());
					BodyIndex = BodyIds.Num() - 1;
				}

				UBodySetup* MyBS = BodySetups[BodyIndex];
				MyBS->bHasCookedCollisionData = true;
				MyBS->CollisionTraceFlag = CTF_UseComplexAsSimple;

				FCookBodySetupInfo CookInfo;
				// Disable mesh cleaning by passing in EPhysXMeshCookFlags::DeformableMesh
				static const EPhysXMeshCookFlags CookFlags = EPhysXMeshCookFlags::FastCook | EPhysXMeshCookFlags::DeformableMesh;
				MyBS->GetCookInfo(CookInfo, CookFlags);
				CookInfo.bCookTriMesh = true;
				CookInfo.TriMeshCookFlags = CookInfo.ConvexCookFlags = CookFlags;
				CookInfo.TriangleMeshDesc.bFlipNormals = true;
				CookInfo.TriangleMeshDesc.Vertices = Args.PositionData;
				const int NumFaces = Args.Indices.Num() / 3;
				CookInfo.TriangleMeshDesc.Indices.Reserve(Args.Indices.Num() / 3);
				for (int i = 0; i < NumFaces; ++i)
				{
					CookInfo.TriangleMeshDesc.Indices.AddUninitialized(1);
					CookInfo.TriangleMeshDesc.Indices[i].v0 = Args.Indices[3 * i + 0];
					CookInfo.TriangleMeshDesc.Indices[i].v1 = Args.Indices[3 * i + 1];
					CookInfo.TriangleMeshDesc.Indices[i].v2 = Args.Indices[3 * i + 2];
				}

				FPhysXCookHelper CookHelper(GetPhysXCookingModule());
				CookHelper.CookInfo = CookInfo;
				CookHelper.CreatePhysicsMeshes_Concurrent();

				MyBS->InvalidatePhysicsData();
				MyBS->FinishCreatingPhysicsMeshes(CookHelper.OutNonMirroredConvexMeshes, CookHelper.OutMirroredConvexMeshes, CookHelper.OutTriangleMeshes);

				FBodyInstance* MyBI = BodyInstances[BodyIndex];
				MyBI->TermBody();
				MyBI->InitBody(MyBS, FTransform::Identity, this, MyWorld->GetPhysicsScene());
			}
			else
			{
				if (BodyIndex != INDEX_NONE)
				{
					RemoveBodyInstance(BodyIndex);
				}
				else
				{
					// This brick already doesn't exist, so no work to be done.
				}
			}
		}

	}

	if (SceneProxy != nullptr && GRenderingThread != nullptr)
	{
		check(GRenderingThread != nullptr);
		check(SceneProxy != nullptr);

		// Graphics update
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			FSendBrickDataLambda,
			UMRMeshComponent*, This, this,
			IMRMesh::FSendBrickDataArgs, Args, Args,
			FOnProcessingComplete, OnProcessingComplete, OnProcessingComplete,
			{
				FMRMeshProxy* MRMeshProxy = static_cast<FMRMeshProxy*>(This->SceneProxy);
				if (MRMeshProxy)
				{
					MRMeshProxy->RenderThread_RemoveSection(Args.BrickCoords);

					if (const bool bBrickHasData = Args.Indices.Num() > 0)
					{
						MRMeshProxy->RenderThread_UploadNewSection(Args);
					}

					if (OnProcessingComplete.IsBound())
					{
						OnProcessingComplete.Execute();
					}
				}
			}
		);
	}
}

void UMRMeshComponent::RemoveBodyInstance(int32 BodyIndex)
{
	BodyIds.RemoveAtSwap(BodyIndex);
	BodySetups.RemoveAtSwap(BodyIndex);
	BodyInstances[BodyIndex]->TermBody();
	delete BodyInstances[BodyIndex];
	BodyInstances.RemoveAtSwap(BodyIndex);
}

void UMRMeshComponent::ClearAllBrickData_Internal()
{
	check(IsInGameThread());

	for (int32 i = BodyIds.Num()-1; i >= 0; i--)
	{
		RemoveBodyInstance(i);
	}

	// Graphics update
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FClearAllBricksLambda,
		UMRMeshComponent*, This, this,
		{
			FMRMeshProxy* MRMeshProxy = static_cast<FMRMeshProxy*>(This->SceneProxy);
			if (MRMeshProxy)
			{
				MRMeshProxy->RenderThread_RemoveAllSections();
			}
		}
	);
}

void UMRMeshComponent::SetMaterial(int32 ElementIndex, class UMaterialInterface* InMaterial)
{
	if (Material != InMaterial)
	{
		Material = InMaterial;
		MarkRenderStateDirty();
	}
}

void UMRMeshComponent::BeginPlay()
{
	UE_LOG(LogTemp, Log, TEXT("MRMesh: MeshReconstructor: %p"), MeshReconstructor);
	if (MeshReconstructor != nullptr)
	{
		FMRMeshConfiguration MRMeshConfig = MeshReconstructor->ConnectMRMesh(this);
	}
}

void UMRMeshComponent::BeginDestroy()
{
	if (MeshReconstructor != nullptr)
	{
		MeshReconstructor->DisconnectMRMesh();
	}
	Super::BeginDestroy();
}

