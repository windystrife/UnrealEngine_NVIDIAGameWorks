// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved. 

#include "PhysicsEngine/FlexRopeComponent.h"
#include "PhysicsEngine/FlexContainerInstance.h"

#include "PhysXSupport.h"
#include "DynamicMeshBuilder.h"


/** Vertex Buffer */
class FFlexRopeVertexBuffer : public FVertexBuffer 
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		VertexBufferRHI = RHICreateVertexBuffer(NumVerts * sizeof(FDynamicMeshVertex), BUF_Dynamic, CreateInfo);
	}

	int32 NumVerts;
};

/** Index Buffer */
class FFlexRopeIndexBuffer : public FIndexBuffer 
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32), NumIndices * sizeof(int32), BUF_Dynamic, CreateInfo);
	}

	int32 NumIndices;
};

/** Vertex Factory */
class FFlexRopeVertexFactory : public FLocalVertexFactory
{
public:

	FFlexRopeVertexFactory()
	{}


	/** Initialization */
	void Init(const FFlexRopeVertexBuffer* VertexBuffer)
	{
		if(IsInRenderingThread())
		{
			// Initialize the vertex factory's stream components.
			FDataType NewData;
			NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
			NewData.TextureCoordinates.Add(
				FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
				);
			NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
			NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
			SetData(NewData);
		}
		else
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				InitFlexRopeVertexFactory,
				FFlexRopeVertexFactory*,VertexFactory,this,
				const FFlexRopeVertexBuffer*,VertexBuffer,VertexBuffer,
			{
				// Initialize the vertex factory's stream components.
				FDataType NewData;
				NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
				NewData.TextureCoordinates.Add(
					FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
					);
				NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
				NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
				VertexFactory->SetData(NewData);
			});
		}
	}
};

/** Dynamic data sent to render thread */
struct FFlexRopeDynamicData
{
	/** Array of points */
	TArray<FVector> FlexRopePoints;
};

//////////////////////////////////////////////////////////////////////////
// FFlexRopeSceneProxy

class FFlexRopeSceneProxy : public FPrimitiveSceneProxy
{
public:

	FFlexRopeSceneProxy(UFlexRopeComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, Material(NULL)
		, DynamicData(NULL)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		, NumSegments(Component->NumSegments)
		, Width(Component->Width)
		, NumSides(Component->NumSides)
		, TileMaterial(Component->TileMaterial)
	{
		VertexBuffer.NumVerts = GetRequiredVertexCount();
		IndexBuffer.NumIndices = GetRequiredIndexCount();

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

	virtual ~FFlexRopeSceneProxy()
	{
		VertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();

		if(DynamicData != NULL)
		{
			delete DynamicData;
		}
	}

	int32 GetRequiredVertexCount() const
	{
		return (NumSegments + 1) * (NumSides + 1);
	}

	int32 GetRequiredIndexCount() const
	{
		return (NumSegments * NumSides * 2) * 3;
	}

	int32 GetVertIndex(int32 AlongIdx, int32 AroundIdx) const
	{
		return (AlongIdx * (NumSides+1)) + AroundIdx;
	}

	void BuildRopeMesh(const TArray<FVector>& InPoints, TArray<FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices)
	{
		const FColor VertexColor(255,255,255);
		const int32 NumPoints = InPoints.Num();
		const int32 SegmentCount = NumPoints-1;

		// Build vertices

		// We double up the first and last vert of the ring, because the UVs are different
		int32 NumRingVerts = NumSides+1;

		if (NumPoints < 2)
			return;

		FVector BasisX = (InPoints[1] - InPoints[0]).GetSafeNormal();
		FVector BasisY, BasisZ;
		BasisX.FindBestAxisVectors(BasisY, BasisZ);

		// For each point along spline..
		for(int32 PointIdx=0; PointIdx<NumPoints; PointIdx++)
		{
			const float AlongFrac = (float)PointIdx/(float)SegmentCount; // Distance along FlexRope

			// Find direction of FlexRope at this point, by averaging previous and next points
			const int32 PrevIndex = FMath::Max(0, PointIdx-1);
			const int32 NextIndex = FMath::Min(PointIdx+1, NumPoints-1);
			const FVector ForwardDir = (InPoints[NextIndex] - InPoints[PrevIndex]).GetSafeNormal();

			FVector RotationAxis = (BasisX ^ ForwardDir).GetSafeNormal();
			float CosTheta = ForwardDir | BasisX;

			// Use a parallel transport frame to create a smooth basis along the rope
			if (FMath::Abs(CosTheta - 1.0f) > KINDA_SMALL_NUMBER)
			{
				BasisX = ForwardDir;
			
				// TODO: trigonometric functions totally unnecessary here
				float Theta = FMath::RadiansToDegrees(FMath::Acos(CosTheta));
				
				BasisY = BasisY.RotateAngleAxis(Theta, RotationAxis);
				BasisZ = BasisZ.RotateAngleAxis(Theta, RotationAxis);
			}

			// Generate a ring of verts
			for(int32 VertIdx = 0; VertIdx<NumRingVerts; VertIdx++)
			{
				const float AroundFrac = float(VertIdx)/float(NumSides);
				// Find angle around the ring
				const float RadAngle = 2.f * PI * AroundFrac;
				// Find direction from center of FlexRope to this vertex
				const FVector OutDir = (FMath::Cos(RadAngle) * BasisY) + (FMath::Sin(RadAngle) * BasisZ);

				FDynamicMeshVertex Vert;
				Vert.Position = InPoints[PointIdx] + (OutDir * 0.5f * Width);
				Vert.TextureCoordinate = FVector2D(AlongFrac * TileMaterial, AroundFrac);
				Vert.Color = VertexColor;
				Vert.SetTangents(ForwardDir, OutDir ^ ForwardDir, OutDir);
				OutVertices.Add(Vert);
			}
		}

		// Build triangles
		for(int32 SegIdx=0; SegIdx<SegmentCount; SegIdx++)
		{
			for(int32 SideIdx=0; SideIdx<NumSides; SideIdx++)
			{
				int32 TL = GetVertIndex(SegIdx, SideIdx);
				int32 BL = GetVertIndex(SegIdx, SideIdx+1);
				int32 TR = GetVertIndex(SegIdx+1, SideIdx);
				int32 BR = GetVertIndex(SegIdx+1, SideIdx+1);

				OutIndices.Add(TL);
				OutIndices.Add(BL);
				OutIndices.Add(TR);

				OutIndices.Add(TR);
				OutIndices.Add(BL);
				OutIndices.Add(BR);
			}
		}
	}

	/** Called on render thread to assign new dynamic data */
	void SetDynamicData_RenderThread(FFlexRopeDynamicData* NewDynamicData)
	{
		check(IsInRenderingThread());

		// Free existing data if present
		if(DynamicData)
		{
			delete DynamicData;
			DynamicData = NULL;
		}
		DynamicData = NewDynamicData;

		// Build mesh from FlexRope points
		TArray<FDynamicMeshVertex> Vertices;
		TArray<int32> Indices;
		BuildRopeMesh(NewDynamicData->FlexRopePoints, Vertices, Indices);

		check(Vertices.Num() == GetRequiredVertexCount());
		check(Indices.Num() == GetRequiredIndexCount());

		void* VertexBufferData = RHILockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, Vertices.Num() * sizeof(FDynamicMeshVertex), RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData, &Vertices[0], Vertices.Num() * sizeof(FDynamicMeshVertex));
		RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);

		void* IndexBufferData = RHILockIndexBuffer(IndexBuffer.IndexBufferRHI, 0, Indices.Num() * sizeof(int32), RLM_WriteOnly);
		FMemory::Memcpy(IndexBufferData, &Indices[0], Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBuffer.IndexBufferRHI);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_FlexRopeSceneProxy_GetDynamicMeshElements );

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
				BatchElement.NumPrimitives = GetRequiredIndexCount()/3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = GetRequiredVertexCount();
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				// Render bounds
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
#endif
			}
		}
	}

	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View)
	{
		QUICK_SCOPE_CYCLE_COUNTER( STAT_FlexRopeSceneProxy_DrawDynamicElements );

		const bool bWireframe = AllowDebugViewmodes() && View->Family->EngineShowFlags.Wireframe;

		FColoredMaterialRenderProxy WireframeMaterialInstance(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : NULL,
			FLinearColor(0, 0.5f, 1.f)
			);

		FMaterialRenderProxy* MaterialProxy = NULL;
		if(bWireframe)
		{
			MaterialProxy = &WireframeMaterialInstance;
		}
		else
		{
			MaterialProxy = Material->GetRenderProxy(IsSelected());
		}

		// Draw the mesh.
		FMeshBatch Mesh;
		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = &IndexBuffer;
		Mesh.bWireframe = bWireframe;
		Mesh.VertexFactory = &VertexFactory;
		Mesh.MaterialRenderProxy = MaterialProxy;
		BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, UseEditorDepthTest());
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = GetRequiredIndexCount()/3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = GetRequiredVertexCount();
		Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		PDI->DrawMesh(Mesh);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Render bounds
		RenderBounds(PDI, View->Family->EngineShowFlags, GetBounds(), IsSelected());
#endif
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual uint32 GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }

	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:

	UMaterialInterface* Material;

	FFlexRopeVertexBuffer VertexBuffer;
	FFlexRopeIndexBuffer IndexBuffer;
	FFlexRopeVertexFactory VertexFactory;

	FFlexRopeDynamicData* DynamicData;

	FMaterialRelevance MaterialRelevance;

	int32 NumSegments;

	float Width;

	int32 NumSides;

	float TileMaterial;
};


static int32 CalcNumSegmentsNeeded(float Length, float Radius)
{
	const float ParticleOverlap = 1.6f; // causes the particles to overlap by 60% of their radius 
	const float SafeRadius = FMath::Max(Radius, 0.01f);
	int32 NumSegments = static_cast<int32>(ParticleOverlap * Length / SafeRadius);
	NumSegments = FMath::Min(NumSegments, 2000);
	NumSegments = FMath::Max(NumSegments, 1);
	return NumSegments;
}

//////////////////////////////////////////////////////////////////////////

UFlexRopeComponent::UFlexRopeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;

	Length = 100.f;
	Width = 10.f;
	NumSegments = 10;
	AutoComputeSegments = true;
	if (ContainerTemplate)
	{
		NumSegments = CalcNumSegmentsNeeded(Length, ContainerTemplate->Radius);
	}

	NumSides = 4;
	EndLocation = FVector(100.0f, 0.0f, 0.0f);
	AttachToRigids = true;	
	StretchStiffness = 1.0f;
	BendStiffness = 0.5f;
	TetherStiffness = 0.0f;
	TileMaterial = 1.f;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	Asset = NULL;
}

void UFlexRopeComponent::UpdateSceneProxy(FFlexRopeSceneProxy* Proxy)
{
	// Allocate FlexRope dynamic data
	FFlexRopeDynamicData* DynamicData = new FFlexRopeDynamicData;

	// Transform current positions from particles into component-space array
	int32 NumPoints = NumSegments + 1;
	DynamicData->FlexRopePoints.AddUninitialized(NumPoints);
	for (int32 PointIdx = 0; PointIdx<NumPoints; PointIdx++)
	{
		DynamicData->FlexRopePoints[PointIdx] = GetComponentTransform().InverseTransformPosition(FVector(Particles[PointIdx]));
	}

	// Enqueue command to send to render thread
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FSendFlexRopeDynamicData,
		FFlexRopeSceneProxy*, Proxy, Proxy,
		FFlexRopeDynamicData*, DynamicData, DynamicData,
		{
			Proxy->SetDynamicData_RenderThread(DynamicData);
		});
}

FPrimitiveSceneProxy* UFlexRopeComponent::CreateSceneProxy()
{
	FFlexRopeSceneProxy* Proxy = new FFlexRopeSceneProxy(this);
	UpdateSceneProxy(Proxy);

	return Proxy;
}

int32 UFlexRopeComponent::GetNumMaterials() const
{
	return 1;
}

#if WITH_EDITOR
void UFlexRopeComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (AutoComputeSegments && (PropertyName == FName(TEXT("ContainerTemplate")) || PropertyName == FName(TEXT("AutoComputeSegments")) || PropertyName == FName(TEXT("Length"))))
	{
		if (ContainerTemplate)
		{
			NumSegments = CalcNumSegmentsNeeded(Length, ContainerTemplate->Radius);
		}
	}
}
#endif

void UFlexRopeComponent::OnRegister()
{
	Super::OnRegister();
#if WITH_FLEX

	// create initial geometry
	CreateRopeGeometry();

	// set up physics
	FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();

	if (ContainerTemplate && PhysScene && (!GIsEditor || GIsPlayInEditorWorld) && !AssetInstance)
	{
		FFlexContainerInstance* Container = PhysScene->GetFlexContainer(ContainerTemplate);
		if (Container)
		{
			ContainerInstance = Container;
			ContainerInstance->Register(this);

			Asset = new NvFlexExtAsset();
			FMemory::Memset(Asset, 0, sizeof(NvFlexExtAsset));

			// particles
			Asset->numParticles = Particles.Num();
			Asset->maxParticles = Particles.Num();

			// particles
			if (Asset->numParticles)
				Asset->particles = (float*)&Particles[0];

			// distance constraints
			Asset->numSprings = SpringCoefficients.Num();
			if (Asset->numSprings)
			{
				Asset->springIndices = (int*)&SpringIndices[0];
				Asset->springCoefficients = (float*)&SpringCoefficients[0];
				Asset->springRestLengths = (float*)&SpringLengths[0];
			}
		}
	}
#endif //WITH_FLEX
}

void UFlexRopeComponent::OnUnregister()
{
	Super::OnUnregister();

#if WITH_FLEX

	if (ContainerInstance && AssetInstance)
	{
		ContainerInstance->DestroyInstance(AssetInstance);
		AssetInstance = NULL;

		delete Asset;
	}

	if (ContainerInstance)
	{
		ContainerInstance->Unregister(this);
		ContainerInstance = NULL;
	}

#endif // WITH_FLEX

}

void UFlexRopeComponent::GetEndPositions(FVector& OutStartPosition, FVector& OutEndPosition)
{
	OutStartPosition = GetComponentLocation();
	OutEndPosition = GetComponentTransform().TransformPosition(EndLocation);
}

void UFlexRopeComponent::CreateRopeGeometry()
{
	// create rope geometry
	Particles.Reset();
	SpringIndices.Reset();
	SpringLengths.Reset();
	SpringCoefficients.Reset();

	FVector FlexRopeStart, FlexRopeEnd;
	GetEndPositions(FlexRopeStart, FlexRopeEnd);

	const int32 NumParticles = NumSegments + 1;

	const FVector Delta = FlexRopeEnd - FlexRopeStart;
	const float RestDistance = Length / NumSegments;

	for (int32 ParticleIdx = 0; ParticleIdx<NumParticles; ParticleIdx++)
	{
		const float Alpha = (float)ParticleIdx / (float)NumSegments;
		const FVector InitialPosition = FlexRopeStart + (Alpha * Delta);

		Particles.Add(FVector4(InitialPosition, 1.0f));

		// create springs between particles
		if (ParticleIdx > 0 && StretchStiffness > 0.0f)
		{
			const int P0 = ParticleIdx - 1;
			const int P1 = ParticleIdx;

			SpringIndices.Add(P0);
			SpringIndices.Add(P1);

			SpringLengths.Add(RestDistance);
			SpringCoefficients.Add(StretchStiffness);
		}

		// create bending springs (connect over three particles)
		if (ParticleIdx > 1 && BendStiffness > 0.0f)
		{
			const int P0 = ParticleIdx - 2;
			const int P1 = ParticleIdx;

			SpringIndices.Add(P0);
			SpringIndices.Add(P1);

			SpringLengths.Add(2.0f * RestDistance);
			SpringCoefficients.Add(BendStiffness);
		}

		// create "tether" constraints
		if (ParticleIdx > 0 && TetherStiffness > 0.0f)
		{
			float Dist = FVector(Particles[0] - InitialPosition).Size();

			SpringIndices.Add(0);
			SpringIndices.Add(ParticleIdx);
			SpringLengths.Add(Dist);
			SpringCoefficients.Add(-TetherStiffness);	// negate stiffness coefficient to signal to Flex that this a unilateral constraint
		}
	}
}

void UFlexRopeComponent::Synchronize()
{
#if WITH_FLEX
	if (ContainerInstance && Asset && !AssetInstance)
	{
		// try to create asset if not already created
		AssetInstance = ContainerInstance->CreateInstance(Asset, FMatrix::Identity, FVector(0.0f), ContainerInstance->GetPhase(Phase));
	}

	if (ContainerInstance && AssetInstance)
	{
		// if attach requested then generate attachment points for overlapping shapes
		if (AttachToRigids)
		{
			// clear out any previous attachments
			Attachments.SetNum(0);

			for (int ParticleIndex = 0; ParticleIndex < AssetInstance->numParticles; ++ParticleIndex)
			{
				FVector4 ParticlePos = Particles[ParticleIndex];

				// perform a point check (small sphere)
				FCollisionShape Shape;
				Shape.SetSphere(0.001f);

				// gather overlapping primitives
				TArray<FOverlapResult> Overlaps;
				GetWorld()->OverlapMultiByObjectType(Overlaps, ParticlePos, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllObjects), Shape, FCollisionQueryParams());

				// pick first non-flex actor, that has a body and is not a trigger
				UPrimitiveComponent* PrimComp = NULL;
				for (int32 OverlapIdx = 0; OverlapIdx<Overlaps.Num() && !PrimComp; ++OverlapIdx)
				{
					FOverlapResult const& O = Overlaps[OverlapIdx];

					if (!O.Component.IsValid() || O.Component.Get() == this)
						continue;

					UPrimitiveComponent* TmpPrimComp = O.Component.Get();

					if (TmpPrimComp->GetBodyInstance() == NULL)
						continue;

					ECollisionResponse Response = TmpPrimComp->GetCollisionResponseToChannel(ContainerInstance->Template->ObjectType);
					if (Response == ECollisionResponse::ECR_Ignore)
						continue;

					PrimComp = TmpPrimComp;
				}

				if (PrimComp)
				{
					FBodyInstance* Body = PrimComp->GetBodyInstance();

					if (!Body)
						continue;

					// calculate local space position of particle in component
					FTransform LocalToWorld = PrimComp->GetComponentToWorld();
					FVector LocalPos = LocalToWorld.InverseTransformPosition(ParticlePos);

					FlexParticleAttachment Attachment;
					Attachment.Primitive = PrimComp;
					Attachment.ParticleIndex = ParticleIndex;
					Attachment.OldMass = ParticlePos.W;
					Attachment.LocalPos = LocalPos;
					Attachment.ShapeIndex = 0;	// don't currently support shape indices

					Attachments.Add(Attachment);
				}
			}

			// reset attach flag
			AttachToRigids = false;
		}

		// process attachments
		for (int AttachmentIndex = 0; AttachmentIndex < Attachments.Num();)
		{
			const FlexParticleAttachment& Attachment = Attachments[AttachmentIndex];
			const UPrimitiveComponent* PrimComp = Attachment.Primitive.Get();

			// index into the simulation data, we need to modify the container's copy
			// of the data so that the new positions get sent back to the sim
			const int ParticleIndex = AssetInstance->particleIndices[Attachment.ParticleIndex];

			if (PrimComp)
			{
				// calculate world position of attached particle, and zero mass
				const FTransform PrimTransform = PrimComp->GetComponentToWorld();
				const FVector& AttachedPos = PrimTransform.TransformPosition(Attachment.LocalPos);

				ContainerInstance->Particles[ParticleIndex] = FVector4(AttachedPos, 0.0f);
				ContainerInstance->Velocities[ParticleIndex] = FVector(0.0f);

				++AttachmentIndex;
			}
			else // process detachments
			{
				ContainerInstance->Particles[ParticleIndex].W = Attachment.OldMass;
				ContainerInstance->Velocities[ParticleIndex] = FVector(0.0f);

				Attachments.RemoveAt(AttachmentIndex);
			}
		}

		// Copy simulation data back to local array
		if (ContainerInstance && AssetInstance)
		{
			for (int i = 0; i < Particles.Num(); ++i)
			{
				Particles[i] = ContainerInstance->Particles[AssetInstance->particleIndices[i]];
			}
		}
	}
#endif //WITH_FLEX
}

void UFlexRopeComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!AssetInstance)
	{
		// if we're not actively being simulated then just update the rope geometry each 
		// frame, this ensures the editor view is updated when modifying parameters
		CreateRopeGeometry();
	}

	// Need to send new data to render thread
	MarkRenderDynamicDataDirty();

	// Call this because bounds have changed
	UpdateComponentToWorld();
}

void UFlexRopeComponent::SendRenderDynamicData_Concurrent()
{
	if (SceneProxy)
	{
		UpdateSceneProxy((FFlexRopeSceneProxy*)SceneProxy);
	}
}

FBoxSphereBounds UFlexRopeComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	// Calculate bounding box of FlexRope points
	FBox RopeBox(ForceInit);
	for(int32 ParticleIdx=0; ParticleIdx<Particles.Num(); ParticleIdx++)
	{
		const FVector& Position = FVector(Particles[ParticleIdx]);
		RopeBox += Position;
	}

	// Expand by rope width
	FBoxSphereBounds NewBounds = FBoxSphereBounds(RopeBox.ExpandBy(Width));

	// Clamp bounds in case of instability
	const float MaxRadius = 1000000.0f;

	if (NewBounds .SphereRadius > MaxRadius)
		NewBounds  = FBoxSphereBounds(EForceInit::ForceInitToZero);

	return NewBounds;
}

