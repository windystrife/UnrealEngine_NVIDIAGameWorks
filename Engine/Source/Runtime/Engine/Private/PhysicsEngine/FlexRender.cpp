// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "FlexRender.h"

#include "PhysicsEngine/FlexComponent.h"

#if WITH_FLEX

#if STATS
DECLARE_CYCLE_STAT(TEXT("Skin Mesh Time (CPU)"), STAT_Flex_RenderMeshTime, STATGROUP_Flex);
#endif


/* CPU Skinning */

struct FFlexVertex
{
	FVector Position;
	FPackedNormal TangentZ;
};


void FFlexVertexBuffer::Init(int32 Count, int32 MaxCount)
{
	MaxVerts = MaxCount;
	NumVerts = Count;

	BeginInitResource(this);
}

void FFlexVertexBuffer::InitRHI()
{
	// Create the vertex buffer.
	FRHIResourceCreateInfo CreateInfo;
	VertexBufferRHI = RHICreateVertexBuffer( MaxVerts*sizeof(FFlexVertex), BUF_AnyDynamic, CreateInfo);
}

int32 FFlexVertexBuffer::CopyVertex(int32 Index)
{
	check(NumVerts < MaxVerts);


	// todo: copy other information

	return NumVerts++;
}

// attribute buffer 
void FFlexAttributeBuffer::Init(const FStaticMeshVertexBuffer& Vertices, const FColorVertexBuffer& Colors, int32 MaxCount)
{
	Attributes.SetNum(MaxCount);

	for (uint32 i=0; i < Vertices.GetNumVertices(); ++i)
	{
		// only copy the first UV set
		if (Vertices.GetNumTexCoords())
		{
			Attributes[i].TextureCoordinate = Vertices.GetVertexUV(i, 0);
		}
		else
		{
			Attributes[i].TextureCoordinate = FVector2D(0.0f, 0.0f);
		}

		if (Colors.GetNumVertices())
		{
			Attributes[i].Color = Colors.VertexColor(i);
		}
		else
		{
			Attributes[i].Color = FColor(255,255,255,255);
		}
	}
	   
 	MaxVerts = int(MaxCount);
	NumVerts = int(Vertices.GetNumVertices());

	BeginInitResource(this);
}


void FFlexAttributeBuffer::InitRHI()
{
	// Create the vertex buffer.
	FRHIResourceCreateInfo CreateInfo;
	VertexBufferRHI = RHICreateVertexBuffer( MaxVerts*sizeof(FFlexVertexAttribute), BUF_AnyDynamic, CreateInfo);

	Update();
}

void FFlexAttributeBuffer::Update()
{
	FFlexVertexAttribute* RESTRICT AttributeData = (FFlexVertexAttribute*)RHILockVertexBuffer(VertexBufferRHI, 0, NumVerts*sizeof(FFlexVertexAttribute), RLM_WriteOnly);

	FMemory::Memcpy(AttributeData, &Attributes[0], Attributes.Num() * sizeof(FFlexVertexAttribute));
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

int32 FFlexAttributeBuffer::CopyVertex(int32 Index, float Alpha)
{
	check(NumVerts < MaxVerts);

	const int32 NewIndex = NumVerts++;

	// todo: copy other information
	Attributes[NewIndex] = Attributes[Index];
	
	// set vertex alpha to user value
	Attributes[NewIndex].Color.A = Alpha*255;
	Attributes[Index].Color.A = Alpha*255;

	return NewIndex;
}

// Overrides local vertex factory with CPU skinned deformation
FFlexCPUVertexFactory::FFlexCPUVertexFactory(
	const FLocalVertexFactory& Base, 
	int NumVerts, 
	int MaxVerts, 
	const int* ParticleMap, 
	const FRawStaticIndexBuffer& Indices, 
	const FStaticMeshVertexBuffer& Vertices, 
	const FColorVertexBuffer& Colors)
{
	VertexBuffer.Init(NumVerts, MaxVerts);
	
	// if tearing
	AttributeBuffer.Init(Vertices, Colors, MaxVerts);

	// copy index buffer for tearing
	IndexBuffer.Init(Indices);

	VertexToParticleMap.Insert(ParticleMap, NumVerts, 0);
	VertexToParticleMap.SetNum(MaxVerts);

	// have to first initialize our RHI and then recreate it from the static mesh
	BeginInitResource(this);

	// copy vertex factory from LOD0 of staticmesh
	Copy(Base);

	// update position and normal components to point to our vertex buffer
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		InitFlexVertexFactory,
		FFlexCPUVertexFactory*, Factory, this,
		const FFlexVertexBuffer*, VertexBuffer, &VertexBuffer,
		const FFlexAttributeBuffer*, AttributeBuffer, &AttributeBuffer,
	{
		Factory->Data.PositionComponent = FVertexStreamComponent(
			VertexBuffer,	
			STRUCT_OFFSET(FFlexVertex,Position),
			sizeof(FFlexVertex),
			VET_Float3
			);
			
		/*
		// temp disable other streams
		Factory->Data.TextureCoordinates.Reset();
		Factory->Data.LightMapCoordinateComponent =  FVertexStreamComponent();
		Factory->Data.ColorComponent = FVertexStreamComponent();
		Factory->Data.TangentBasisComponents[0] = FVertexStreamComponent();
		*/

		// re-point attribute streams
		Factory->Data.TextureCoordinates.Reset();
		Factory->Data.TextureCoordinates.Add(FVertexStreamComponent(
			AttributeBuffer,
			STRUCT_OFFSET(FFlexVertexAttribute, TextureCoordinate),
			sizeof(FFlexVertexAttribute),
			VET_Float2
			));

		Factory->Data.ColorComponent = FVertexStreamComponent(
			AttributeBuffer,
			STRUCT_OFFSET(FFlexVertexAttribute, Color),
			sizeof(FFlexVertexAttribute),
			VET_Color);

		
		/*
		Data->TangentBasisComponents[0] = FVertexStreamComponent(
			VertexBuffer,
			STRUCT_OFFSET(FFlexVertex,TangentX),
			sizeof(FFlexVertex),
			VET_Float3
			);
		*/

		Factory->Data.TangentBasisComponents[1] = FVertexStreamComponent(
			VertexBuffer,
			STRUCT_OFFSET(FFlexVertex,TangentZ),
			sizeof(FFlexVertex),
			VET_PackedNormal
			);

		Factory->UpdateRHI();
	});
}

FFlexCPUVertexFactory::~FFlexCPUVertexFactory()
{
	VertexBuffer.ReleaseResource();
	AttributeBuffer.ReleaseResource();
	IndexBuffer.ReleaseResource();

	ReleaseResource();
}

void FFlexCPUVertexFactory::OverrideMeshElement(FMeshBatchElement& Element)
{
	Element.IndexBuffer = &IndexBuffer;
	Element.MaxVertexIndex = VertexBuffer.NumVerts;
	Element.MinVertexIndex = 0;
}

void FFlexCPUVertexFactory::SkinCloth(const FVector4* SimulatedPositions, const FVector* SimulatedNormals)
{
	SCOPE_CYCLE_COUNTER(STAT_Flex_RenderMeshTime);

	FFlexVertex* RESTRICT Vertex = (FFlexVertex*)RHILockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.NumVerts*sizeof(FFlexVertex), RLM_WriteOnly);

	if (SimulatedPositions && SimulatedNormals)
	{
		// update both positions and normals
		for (int i=0; i < VertexBuffer.NumVerts; ++i)
		{
			const int particleIndex = VertexToParticleMap[i];

			Vertex[i].Position = FVector(SimulatedPositions[particleIndex]);
			
			// convert normal to packed format
			FPackedNormal Normal = -FVector(SimulatedNormals[particleIndex]);
			Normal.Vector.W = 255;
			Vertex[i].TangentZ = Normal;
		}
	}

	RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);		
}

void FFlexCPUVertexFactory::TearCloth(const NvFlexExtTearingMeshEdit* Edits, int32 NumEdits, float Alpha)
{	
	// resize vertex buffers if necessary
	if (VertexBuffer.MaxVerts < VertexBuffer.NumVerts + NumEdits)
	{
		const int NewSize = FMath::Max(VertexBuffer.MaxVerts*2, VertexBuffer.NumVerts + NumEdits);

		VertexBuffer.MaxVerts = NewSize;
		VertexBuffer.UpdateRHI();

		AttributeBuffer.MaxVerts = NewSize;
		AttributeBuffer.Attributes.SetNum(NewSize);
		AttributeBuffer.UpdateRHI();

		VertexToParticleMap.SetNum(NewSize);
	}

	// apply mesh edits, this code assumes an edit means the vertex was also duplicated
	// so always creates a new vertex which is bound to the new particle index
	for (int32 i=0; i < NumEdits; ++i)
	{
		const int OldVertex = IndexBuffer.Indices[Edits[i].triIndex];
		
		// copy vertex and add reference to new particle
		const int NewVertex = VertexBuffer.CopyVertex(OldVertex);
		const int NewAttribute = AttributeBuffer.CopyVertex(OldVertex, Alpha);

		check(NewVertex == NewAttribute);

		// update vertex to point to new particle
		VertexToParticleMap[NewVertex] = Edits[i].newParticleIndex;

		// update triangle to point to new vertex
		IndexBuffer.Indices[Edits[i].triIndex] = NewVertex;
	}

	AttributeBuffer.Update();
	IndexBuffer.Update();
}

void FFlexCPUVertexFactory::SkinSoft(const FPositionVertexBuffer& Positions, const FStaticMeshVertexBuffer& Vertices, const FFlexShapeTransform* Transforms, const FVector* RestPoses, const int16* ClusterIndices, const float* ClusterWeights, int NumClusters)
{
	SCOPE_CYCLE_COUNTER(STAT_Flex_RenderMeshTime);

	const int NumVertices = Vertices.GetNumVertices();

	TArray<FFlexVertex> SkinnedVertices;
	SkinnedVertices.SetNum(NumVertices);

	for (int VertexIndex=0; VertexIndex < NumVertices; ++VertexIndex)
	{
		FVector SoftPos(0.0f);
		FVector SoftNormal(0.0f);
		FVector SoftTangent(0.0f);

		for (int w=0; w < 4; ++ w)
		{
			const int Cluster = ClusterIndices[VertexIndex*4 + w];
			const float Weight = ClusterWeights[VertexIndex*4 + w];

			if (Cluster > -1)
			{
				const FFlexShapeTransform Transform = Transforms[Cluster];

				FVector LocalPos = Positions.VertexPosition(VertexIndex) - RestPoses[Cluster];
				FVector LocalNormal = Vertices.VertexTangentZ(VertexIndex);
				//FVector LocalTangent = Vertices.VertexTangentX(VertexIndex);
			
				SoftPos += (Transform.Rotation.RotateVector(LocalPos) + Transform.Translation)*Weight;
				SoftNormal += (Transform.Rotation.RotateVector(LocalNormal))*Weight;
				//SoftTangent += Rotation.RotateVector(LocalTangent)*Weight;
			}
		}
			
		// position
		SkinnedVertices[VertexIndex].Position = SoftPos;

		// normal
		FPackedNormal Normal = SoftNormal;
		Normal.Vector.W = 255;
		SkinnedVertices[VertexIndex].TangentZ = Normal;

		// tangent
		//FPackedNormal Tangent = -SoftTangent;
		//Tangent.Vector.W = 255;
		//SkinnedTangents[VertexIndex] = Tangent;
	}

	FFlexVertex* RESTRICT Vertex = (FFlexVertex*)RHILockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, NumVertices*sizeof(FFlexVertex), RLM_WriteOnly);
	FMemory::Memcpy(Vertex, &SkinnedVertices[0], sizeof(FFlexVertex)*NumVertices);
	RHIUnlockVertexBuffer(VertexBuffer.VertexBufferRHI);		
}

/* GPU Skinning */

class FFlexMeshVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	virtual void Bind(const FShaderParameterMap& ParameterMap) override;
	virtual void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override;
	void Serialize(FArchive& Ar) override;
	virtual uint32 GetSize() const override;

private:

	FShaderResourceParameter ClusterTranslationsParameter;
	FShaderResourceParameter ClusterRotationsParameter;
};

FVertexFactoryShaderParameters* FFlexGPUVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FFlexMeshVertexFactoryShaderParameters() : NULL;
}

FFlexGPUVertexFactory::FFlexGPUVertexFactory(const FLocalVertexFactory& Base, const FVertexBuffer* ClusterWeightsVertexBuffer, const FVertexBuffer* ClusterIndicesVertexBuffer)
{
	// set our streams
	FlexData.ClusterWeights = FVertexStreamComponent(ClusterWeightsVertexBuffer, 0, sizeof(float)*4, VET_Float4);
	FlexData.ClusterIndices = FVertexStreamComponent(ClusterIndicesVertexBuffer, 0,	sizeof(int16)*4, VET_Short4);

	MaxClusters = 0;

	// have to first initialize our RHI and then recreate it from the static mesh
	BeginInitResource(this);

	// copy vertex factory from LOD0 of staticmesh
	Copy(Base);
}

FFlexGPUVertexFactory::~FFlexGPUVertexFactory()
{
	ReleaseResource();
}

void FFlexGPUVertexFactory::AddVertexElements(FDataType& InData, FVertexDeclarationElementList& Elements)
{
	FLocalVertexFactory::AddVertexElements(InData, Elements);

	// Add Flex elements
	Elements.Add(AccessStreamComponent(FlexData.ClusterIndices, 8));
	Elements.Add(AccessStreamComponent(FlexData.ClusterWeights, 9));
	// And Flex elements
}

void FFlexGPUVertexFactory::AddVertexPositionElements(FDataType& InData, FVertexDeclarationElementList& Elements)
{
	FLocalVertexFactory::AddVertexElements(InData, Elements);

	// Add Flex elements
	Elements.Add(AccessStreamComponent(FlexData.ClusterIndices, 8));
	Elements.Add(AccessStreamComponent(FlexData.ClusterWeights, 9));
	// And Flex elements
}

void FFlexGPUVertexFactory::InitDynamicRHI()
{
	if (MaxClusters > 0)
	{
		ClusterTranslations.Initialize(sizeof(FVector4), MaxClusters, PF_A32B32G32R32F, BUF_AnyDynamic);
		ClusterRotations.Initialize(sizeof(FVector4), MaxClusters, PF_A32B32G32R32F, BUF_AnyDynamic);			
	}
}

void FFlexGPUVertexFactory::ReleaseDynamicRHI()
{
	if (ClusterTranslations.NumBytes > 0)
	{
		ClusterTranslations.Release();
		ClusterRotations.Release();
	}
}

void FFlexGPUVertexFactory::AllocateFor(int32 InMaxClusters)
{
	if (InMaxClusters > MaxClusters)
	{
		MaxClusters = InMaxClusters;

		if (!IsInitialized())
		{
			InitResource();
		}
		else
		{
			UpdateRHI();
		}
	}
}

void FFlexGPUVertexFactory::SkinCloth(const FVector4* SimulatedPositions, const FVector* SimulatedNormals)
{
	// todo: implement
	check(0);
}

// for GPU skinning this method just uploads the necessary data to the skinning buffers
void FFlexGPUVertexFactory::SkinSoft(const FPositionVertexBuffer& Positions, const FStaticMeshVertexBuffer& Vertices, const FFlexShapeTransform* Transforms, const FVector* RestPoses, const int16* ClusterIndices, const float* ClusterWeights, int NumClusters)
{
	SCOPE_CYCLE_COUNTER(STAT_Flex_RenderMeshTime);
	
	AllocateFor(NumClusters);

	if(NumClusters)
	{
		// remove rest pose translation now, rest pose rotation is always the identity so we can send those directly (below)
		FVector4* TranslationData = (FVector4*)RHILockVertexBuffer(ClusterTranslations.Buffer, 0, NumClusters*sizeof(FVector4), RLM_WriteOnly);			
			
		for (int i=0; i < NumClusters; ++i)
			TranslationData[i] = FVector4(Transforms[i].Translation - Transforms[i].Rotation*RestPoses[i], 0.0f);
			
		RHIUnlockVertexBuffer(ClusterTranslations.Buffer);

		// rotations
		FQuat* RotationData = (FQuat*)RHILockVertexBuffer(ClusterRotations.Buffer, 0, NumClusters*sizeof(FQuat), RLM_WriteOnly);
			
		for (int i=0; i < NumClusters; ++i)
			RotationData[i] = Transforms[i].Rotation;

		RHIUnlockVertexBuffer(ClusterRotations.Buffer);
	}
}

// factory shader parameter implementation methods

void FFlexMeshVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap) 
{
	ClusterTranslationsParameter.Bind(ParameterMap, TEXT("ClusterTranslations"), SPF_Optional);
	ClusterRotationsParameter.Bind(ParameterMap, TEXT("ClusterRotations"), SPF_Optional);
}

void FFlexMeshVertexFactoryShaderParameters::SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const 
{
	if (Shader->GetVertexShader())
	{
		FFlexGPUVertexFactory* Factory = (FFlexGPUVertexFactory*)(VertexFactory);

		if(ClusterTranslationsParameter.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(Shader->GetVertexShader(), ClusterTranslationsParameter.GetBaseIndex(), Factory->ClusterTranslations.SRV);
		}

		if(ClusterRotationsParameter.IsBound())
		{
			RHICmdList.SetShaderResourceViewParameter(Shader->GetVertexShader(), ClusterRotationsParameter.GetBaseIndex(), Factory->ClusterRotations.SRV);
		}
	}
}

void FFlexMeshVertexFactoryShaderParameters::Serialize(FArchive& Ar) 
{
	Ar << ClusterTranslationsParameter;
	Ar << ClusterRotationsParameter;
}

uint32 FFlexMeshVertexFactoryShaderParameters::GetSize() const 
{
	return sizeof(*this);
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FFlexGPUVertexFactory, "/Engine/Private/LocalVertexFactory.ush", true, true, true, true, true);


/** Scene proxy */
FFlexMeshSceneProxy::FFlexMeshSceneProxy(UStaticMeshComponent* Component)
	: FStaticMeshSceneProxy(Component, false)
{
	FlexComponent = Cast<UFlexComponent>(Component);
	
	const FStaticMeshLODResources& LOD = Component->GetStaticMesh()->RenderData->LODResources[0];
	const UFlexAssetSoft* SoftAsset = Cast<UFlexAssetSoft>(FlexComponent->GetStaticMesh()->FlexAsset);
	const UFlexAssetCloth* ClothAsset = Cast<UFlexAssetCloth>(FlexComponent->GetStaticMesh()->FlexAsset);

	ERHIFeatureLevel::Type FeatureLevel = Component->GetWorld()->FeatureLevel;

	if (FeatureLevel >= ERHIFeatureLevel::SM4 && SoftAsset)
	{
		// Ensure top LOD has a compatible material 
		FLODInfo& ProxyLODInfo = LODs[0];
		for (int i=0; i < ProxyLODInfo.Sections.Num(); ++i)
		{
			if (!ProxyLODInfo.Sections[i].Material->CheckMaterialUsage_Concurrent(MATUSAGE_FlexMeshes))
				ProxyLODInfo.Sections[i].Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		// use GPU skinning for SM4 and soft assets only
		VertexFactory = new FFlexGPUVertexFactory(LOD.VertexFactory, &SoftAsset->WeightsVertexBuffer, &SoftAsset->IndicesVertexBuffer);
	}
	else
	{
		// use CPU skinning for everything else
		VertexFactory = new FFlexCPUVertexFactory(
								LOD.VertexFactory, 
								LOD.VertexBuffer.GetNumVertices(),
								LOD.GetNumTriangles()*3, 
								&FlexComponent->GetStaticMesh()->FlexAsset->VertexToParticleMap[0],
								LOD.IndexBuffer,
								LOD.VertexBuffer,
								LOD.ColorVertexBuffer);
	}
	LastFrame = 0;	
}

FFlexMeshSceneProxy::~FFlexMeshSceneProxy()
{
	check(IsInRenderingThread());
	delete VertexFactory;
}

int FFlexMeshSceneProxy::GetLastVisibleFrame()
{
	// called by the game thread to determine whether to disable simulation
	return LastFrame;
}

FPrimitiveViewRelevance FFlexMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Relevance = FStaticMeshSceneProxy::GetViewRelevance(View);
	Relevance.bDynamicRelevance = true;
	Relevance.bStaticRelevance = false;

	return Relevance;
}
	
void FFlexMeshSceneProxy::UpdateClothTransforms()
{	
	// unsafe: update vertex buffers here by grabbing data directly from simulation container 
	// this won't be necessary when skinning is done on the GPU
	VertexFactory->SkinCloth(
		&FlexComponent->SimPositions[0],
		&FlexComponent->SimNormals[0]);
}

void FFlexMeshSceneProxy::UpdateClothMesh(const NvFlexExtTearingMeshEdit* Edits, int32 NumEdits, float Alpha)
{
	VertexFactory->TearCloth(Edits, NumEdits, Alpha);

	delete[] Edits;
}

void FFlexMeshSceneProxy::UpdateSoftTransforms(const FFlexShapeTransform* NewTransforms, int32 NumShapes)
{
	// delete old transforms
	const UFlexAssetSoft* SoftAsset = Cast<UFlexAssetSoft>(FlexComponent->GetStaticMesh()->FlexAsset);

	const FPositionVertexBuffer& Positions = FlexComponent->GetStaticMesh()->RenderData->LODResources[0].PositionVertexBuffer;
	const FStaticMeshVertexBuffer& Vertices = FlexComponent->GetStaticMesh()->RenderData->LODResources[0].VertexBuffer;			

	// only used for CPU skinning
	const int16* ClusterIndices = &SoftAsset->IndicesVertexBuffer.Vertices[0];
	const float* ClusterWeights = &SoftAsset->WeightsVertexBuffer.Vertices[0];
	const FVector* RestPoses = &SoftAsset->ShapeCenters[0];

	VertexFactory->SkinSoft(Positions, Vertices, NewTransforms, RestPoses, ClusterIndices, ClusterWeights, NumShapes);
}

void FFlexMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const 
{
	// store last renderer frame (used for LOD), is the const_cast valid here?
	const_cast<FFlexMeshSceneProxy*>(this)->LastFrame = GFrameNumber;

	FStaticMeshSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
}

bool FFlexMeshSceneProxy::GetMeshElement(int32 LODIndex, int32 BatchIndex, int32 ElementIndex, uint8 InDepthPriorityGroup, bool bUseSelectedMaterial, bool bUseHoveredMaterial, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const
{
	bool Ret = FStaticMeshSceneProxy::GetMeshElement(LODIndex, BatchIndex, ElementIndex, InDepthPriorityGroup, bUseSelectedMaterial, bUseHoveredMaterial, bAllowPreCulledIndices, OutMeshBatch);

	// override top LOD with our simulated vertex factory
	if (LODIndex == 0)
	{
		OutMeshBatch.VertexFactory = VertexFactory;

		VertexFactory->OverrideMeshElement(OutMeshBatch.Elements[0]);
	}
	
	return Ret;
}

bool FFlexMeshSceneProxy::GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const
{
	bool Ret = FStaticMeshSceneProxy::GetWireframeMeshElement(LODIndex, BatchIndex, WireframeRenderProxy, InDepthPriorityGroup, bAllowPreCulledIndices, OutMeshBatch);

	// override top LOD with our simulated vertex factory
	if (LODIndex == 0)
	{
		OutMeshBatch.VertexFactory = VertexFactory;

		VertexFactory->OverrideMeshElement(OutMeshBatch.Elements[0]);
	}
	
	return Ret;

}


bool FFlexMeshSceneProxy::GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const
{
	return false;
}

#endif // WITH_FLEX
