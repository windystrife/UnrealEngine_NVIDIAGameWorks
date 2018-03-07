// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DynamicMeshBuilder.cpp: Dynamic mesh builder implementation.
=============================================================================*/

#include "DynamicMeshBuilder.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "VertexFactory.h"
#include "MeshBatch.h"
#include "SceneManagement.h"
#include "LocalVertexFactory.h"
#include "ResourcePool.h"

class FGlobalDynamicMeshPoolPolicy
{
public:
	/** Buffers are created with a simple byte size */
	typedef uint32 CreationArguments;
	enum
	{
		NumSafeFrames = 3, /** Number of frames to leaves buffers before reclaiming/reusing */
		NumPoolBucketSizes = 16, /** Number of pool buckets */
		NumToDrainPerFrame = 100, /** Max. number of resources to cull in a single frame */
		CullAfterFramesNum = 10 /** Resources are culled if unused for more frames than this */
	};
	
	/** Get the pool bucket index from the size
	 * @param Size the number of bytes for the resource
	 * @returns The bucket index.
	 */
	uint32 GetPoolBucketIndex(uint32 Size)
	{
		unsigned long Lower = 0;
		unsigned long Upper = NumPoolBucketSizes;
		unsigned long Middle;
		
		do
		{
			Middle = ( Upper + Lower ) >> 1;
			if( Size <= BucketSizes[Middle-1] )
			{
				Upper = Middle;
			}
			else
			{
				Lower = Middle;
			}
		}
		while( Upper - Lower > 1 );
		
		check( Size <= BucketSizes[Lower] );
		check( (Lower == 0 ) || ( Size > BucketSizes[Lower-1] ) );
		
		return Lower;
	}
	
	/** Get the pool bucket size from the index
	 * @param Bucket the bucket index
	 * @returns The bucket size.
	 */
	uint32 GetPoolBucketSize(uint32 Bucket)
	{
		check(Bucket < NumPoolBucketSizes);
		return BucketSizes[Bucket];
	}
	
private:
	/** The bucket sizes */
	static uint32 BucketSizes[NumPoolBucketSizes];
};

uint32 FGlobalDynamicMeshPoolPolicy::BucketSizes[NumPoolBucketSizes] = {
	64, 128, 256, 512, 1024, 2048, 4096,
	8*1024, 16*1024, 32*1024, 64*1024, 128*1024, 256*1024,
	512*1024, 1*1024*1024, 2*1024*1024
};

#if PLATFORM_USES_ES2
typedef uint16 DynamicMeshIndexType;
#else
typedef int32 DynamicMeshIndexType;
#endif

class FGlobalDynamicMeshIndexPolicy : public FGlobalDynamicMeshPoolPolicy
{
public:
	enum
	{
		NumSafeFrames = FGlobalDynamicMeshPoolPolicy::NumSafeFrames,
		NumPoolBuckets = FGlobalDynamicMeshPoolPolicy::NumPoolBucketSizes,
		NumToDrainPerFrame = FGlobalDynamicMeshPoolPolicy::NumToDrainPerFrame,
		CullAfterFramesNum = FGlobalDynamicMeshPoolPolicy::CullAfterFramesNum
	};
	
	/** Creates the resource
	 * @param Args The buffer size in bytes.
	 * @returns A suitably sized buffer or NULL on failure.
	 */
	FIndexBufferRHIRef CreateResource(FGlobalDynamicMeshPoolPolicy::CreationArguments Args)
	{
		FGlobalDynamicMeshPoolPolicy::CreationArguments BufferSize = GetPoolBucketSize(GetPoolBucketIndex(Args));
		// The use of BUF_Static is deliberate - on OS X the buffer backing-store orphaning & reallocation will dominate execution time
		// so to avoid this we don't reuse a buffer for several frames, thereby avoiding the pipeline stall and the reallocation cost.
		FRHIResourceCreateInfo CreateInfo;
		FIndexBufferRHIRef VertexBuffer = RHICreateIndexBuffer(sizeof(DynamicMeshIndexType), BufferSize, BUF_Static, CreateInfo);
		return VertexBuffer;
	}
	
	/** Gets the arguments used to create resource
	 * @param Resource The buffer to get data for.
	 * @returns The arguments used to create the buffer.
	 */
	FGlobalDynamicMeshPoolPolicy::CreationArguments GetCreationArguments(FIndexBufferRHIRef Resource)
	{
		return (Resource->GetSize());
	}
	
	/** Frees the resource
	 * @param Resource The buffer to prepare for release from the pool permanently.
	 */
	void FreeResource(FIndexBufferRHIRef Resource)
	{
	}
};

class FGlobalDynamicMeshIndexPool : public TRenderResourcePool<FIndexBufferRHIRef, FGlobalDynamicMeshIndexPolicy, FGlobalDynamicMeshPoolPolicy::CreationArguments>
{
public:
	/** Destructor */
	virtual ~FGlobalDynamicMeshIndexPool()
	{
	}
	
public: // From FTickableObjectRenderThread
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FGlobalDynamicMeshIndexPool, STATGROUP_Tickables);
	}
};
TGlobalResource<FGlobalDynamicMeshIndexPool> GDynamicMeshIndexPool;

class FGlobalDynamicMeshVertexPolicy : public FGlobalDynamicMeshPoolPolicy
{
public:
	enum
	{
		NumSafeFrames = FGlobalDynamicMeshPoolPolicy::NumSafeFrames,
		NumPoolBuckets = FGlobalDynamicMeshPoolPolicy::NumPoolBucketSizes,
		NumToDrainPerFrame = FGlobalDynamicMeshPoolPolicy::NumToDrainPerFrame,
		CullAfterFramesNum = FGlobalDynamicMeshPoolPolicy::CullAfterFramesNum
	};
	
	/** Creates the resource
	 * @param Args The buffer size in bytes.
	 * @returns A suitably sized buffer or NULL on failure.
	 */
	FVertexBufferRHIRef CreateResource(FGlobalDynamicMeshPoolPolicy::CreationArguments Args)
	{
		FGlobalDynamicMeshPoolPolicy::CreationArguments BufferSize = GetPoolBucketSize(GetPoolBucketIndex(Args));
		FRHIResourceCreateInfo CreateInfo;
		FVertexBufferRHIRef VertexBuffer = RHICreateVertexBuffer(BufferSize, BUF_Volatile, CreateInfo);
		return VertexBuffer;
	}
	
	/** Gets the arguments used to create resource
	 * @param Resource The buffer to get data for.
	 * @returns The arguments used to create the buffer.
	 */
	FGlobalDynamicMeshPoolPolicy::CreationArguments GetCreationArguments(FVertexBufferRHIRef Resource)
	{
		return (Resource->GetSize());
	}
	
	/** Frees the resource
	 * @param Resource The buffer to prepare for release from the pool permanently.
	 */
	void FreeResource(FVertexBufferRHIRef Resource)
	{
	}
};

class FGlobalDynamicMeshVertexPool : public TRenderResourcePool<FVertexBufferRHIRef, FGlobalDynamicMeshVertexPolicy, FGlobalDynamicMeshPoolPolicy::CreationArguments>
{
public:
	/** Destructor */
	virtual ~FGlobalDynamicMeshVertexPool()
	{
	}
	
public: // From FTickableObjectRenderThread
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FGlobalDynamicMeshVertexPool, STATGROUP_Tickables);
	}
};
TGlobalResource<FGlobalDynamicMeshVertexPool> GDynamicMeshVertexPool;


/** The index buffer type used for dynamic meshes. */
class FDynamicMeshIndexBuffer : public FDynamicPrimitiveResource, public FIndexBuffer
{
public:

	TArray<DynamicMeshIndexType> Indices;

	// FRenderResource interface.
	virtual void InitRHI() override
	{
		uint32 SizeInBytes = Indices.Num() * sizeof(DynamicMeshIndexType);
		if(SizeInBytes <= FGlobalDynamicMeshIndexPolicy().GetPoolBucketSize(FGlobalDynamicMeshIndexPolicy::NumPoolBuckets - 1))
		{
			IndexBufferRHI = GDynamicMeshIndexPool.CreatePooledResource(SizeInBytes);
		}
		else
		{
			FRHIResourceCreateInfo CreateInfo;
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(DynamicMeshIndexType), SizeInBytes, BUF_Volatile, CreateInfo);
		}
		
		// Write the indices to the index buffer.
		void* Buffer = RHILockIndexBuffer(IndexBufferRHI,0,Indices.Num() * sizeof(DynamicMeshIndexType),RLM_WriteOnly);
		FMemory::Memcpy(Buffer,Indices.GetData(),Indices.Num() * sizeof(DynamicMeshIndexType));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
	
	virtual void ReleaseRHI() override
	{
		if(IndexBufferRHI->GetSize() <= FGlobalDynamicMeshIndexPolicy().GetPoolBucketSize(FGlobalDynamicMeshIndexPolicy::NumPoolBuckets - 1))
		{
			GDynamicMeshIndexPool.ReleasePooledResource(IndexBufferRHI);
			IndexBufferRHI = NULL;
		}
		FIndexBuffer::ReleaseRHI();
	}

	// FDynamicPrimitiveResource interface.
	virtual void InitPrimitiveResource() override
	{
		InitResource();
	}
	virtual void ReleasePrimitiveResource() override
	{
		ReleaseResource();
		delete this;
	}
};

/** The vertex buffer type used for dynamic meshes. */
class FDynamicMeshVertexBuffer : public FDynamicPrimitiveResource, public FVertexBuffer
{
public:

	TArray<FDynamicMeshVertex> Vertices;

	// FRenderResource interface.
	virtual void InitRHI() override
	{
		uint32 SizeInBytes = Vertices.Num() * sizeof(FDynamicMeshVertex);
		if(SizeInBytes <= FGlobalDynamicMeshIndexPolicy().GetPoolBucketSize(FGlobalDynamicMeshIndexPolicy::NumPoolBuckets - 1))
		{
			VertexBufferRHI = GDynamicMeshVertexPool.CreatePooledResource(SizeInBytes);
		}
		else
		{
			FRHIResourceCreateInfo CreateInfo;
			VertexBufferRHI = RHICreateVertexBuffer(SizeInBytes, BUF_Volatile, CreateInfo);
		}
		
		// Copy the vertex data into the vertex buffer.
		void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI,0,SizeInBytes,RLM_WriteOnly);
		FMemory::Memcpy(VertexBufferData,Vertices.GetData(),SizeInBytes);
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
	
	virtual void ReleaseRHI() override
	{
		if(VertexBufferRHI->GetSize() <= FGlobalDynamicMeshVertexPolicy().GetPoolBucketSize(FGlobalDynamicMeshVertexPolicy::NumPoolBuckets - 1))
		{
			GDynamicMeshVertexPool.ReleasePooledResource(VertexBufferRHI);
			VertexBufferRHI = NULL;
		}
		FVertexBuffer::ReleaseRHI();
	}

	// FDynamicPrimitiveResource interface.
	virtual void InitPrimitiveResource() override
	{
		InitResource();
	}
	virtual void ReleasePrimitiveResource() override
	{
		ReleaseResource();
		delete this;
	}
};

/** The vertex factory type used for dynamic meshes. */
class FDynamicMeshVertexFactory : public FDynamicPrimitiveResource, public FLocalVertexFactory
{
public:

	/** Initialization constructor. */
	FDynamicMeshVertexFactory(const FDynamicMeshVertexBuffer* VertexBuffer)
	{
		// Initialize the vertex factory's stream components.
		if(IsInRenderingThread())
		{
			FDataType TheData;
			TheData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
			TheData.TextureCoordinates.Add(
				FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
				);
			TheData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
			TheData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
			TheData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Color,VET_Color);
			SetData(TheData);
		}
		else
		{
		    ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			    InitVertexFactory,
			    FLocalVertexFactory*,VertexFactory,this,
			    const FDynamicMeshVertexBuffer*,VertexBuffer,VertexBuffer,
			    {
				    FDataType TheData;
				    TheData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
				    TheData.TextureCoordinates.Add(
					    FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
					    );
				    TheData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
				    TheData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
					TheData.ColorComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Color,VET_Color);
				    VertexFactory->SetData(TheData);
			    });
		}
	}

	// FDynamicPrimitiveResource interface.
	virtual void InitPrimitiveResource()
	{
		InitResource();
	}
	virtual void ReleasePrimitiveResource()
	{
		ReleaseResource();
		delete this;
	}
};

/** The primitive uniform buffer used for dynamic meshes. */
class FDynamicMeshPrimitiveUniformBuffer : public FDynamicPrimitiveResource, public TUniformBuffer<FPrimitiveUniformShaderParameters>
{
public:
	
	// FDynamicPrimitiveResource interface.
	virtual void InitPrimitiveResource()
	{
		InitResource();
	}
	virtual void ReleasePrimitiveResource()
	{
		ReleaseResource();
		delete this;
	}
};

FDynamicMeshBuilder::FDynamicMeshBuilder()
{
	VertexBuffer = new FDynamicMeshVertexBuffer;
	IndexBuffer = new FDynamicMeshIndexBuffer;
}

FDynamicMeshBuilder::~FDynamicMeshBuilder()
{
	//Delete the resources if they have not been already. At this point they are only valid if Draw() has never been called,
	//so the resources have not been passed to the rendering thread.  Also they do not need to be released,
	//since they are only initialized when Draw() is called.
	delete VertexBuffer;
	delete IndexBuffer;
}

int32 FDynamicMeshBuilder::AddVertex(
	const FVector& InPosition,
	const FVector2D& InTextureCoordinate,
	const FVector& InTangentX,
	const FVector& InTangentY,
	const FVector& InTangentZ,
	const FColor& InColor
	)
{
	int32 VertexIndex = VertexBuffer->Vertices.Num();
	FDynamicMeshVertex* Vertex = new(VertexBuffer->Vertices) FDynamicMeshVertex;
	Vertex->Position = InPosition;
	Vertex->TextureCoordinate = InTextureCoordinate;
	Vertex->TangentX = InTangentX;
	Vertex->TangentZ = InTangentZ;
	// store the sign of the determinant in TangentZ.W (-1=0,+1=255)
	Vertex->TangentZ.Vector.W = GetBasisDeterminantSign( InTangentX, InTangentY, InTangentZ ) < 0 ? 0 : 255;
	Vertex->Color = InColor;

	return VertexIndex;
}

/** Adds a vertex to the mesh. */
int32 FDynamicMeshBuilder::AddVertex(const FDynamicMeshVertex &InVertex)
{
	int32 VertexIndex = VertexBuffer->Vertices.Num();
	FDynamicMeshVertex* Vertex = new(VertexBuffer->Vertices) FDynamicMeshVertex(InVertex);

	return VertexIndex;
}

/** Adds a triangle to the mesh. */
void FDynamicMeshBuilder::AddTriangle(int32 V0,int32 V1,int32 V2)
{
	IndexBuffer->Indices.Add(static_cast<DynamicMeshIndexType>(V0));
	IndexBuffer->Indices.Add(static_cast<DynamicMeshIndexType>(V1));
	IndexBuffer->Indices.Add(static_cast<DynamicMeshIndexType>(V2));
}

/** Adds many vertices to the mesh. Returns start index of verts in the overall array. */
int32 FDynamicMeshBuilder::AddVertices(const TArray<FDynamicMeshVertex> &InVertices)
{
	int32 StartIndex = VertexBuffer->Vertices.Num();
	VertexBuffer->Vertices.Append(InVertices);
	return StartIndex;
}

/** Add many indices to the mesh. */
void FDynamicMeshBuilder::AddTriangles(const TArray<int32> &InIndices)
{
	if (IndexBuffer->Indices.GetTypeSize() == InIndices.GetTypeSize())
	{
		IndexBuffer->Indices.Append(InIndices);
	}
	else
	{
		for (int32 Index : InIndices)
		{
			IndexBuffer->Indices.Add(static_cast<DynamicMeshIndexType>(Index));
		}
	}
}

class FMeshBuilderOneFrameResources : public FOneFrameResource
{
public:

	FMeshBuilderOneFrameResources() :
		VertexBuffer(NULL),
		IndexBuffer(NULL),
		VertexFactory(NULL),
		PrimitiveUniformBuffer(NULL)
	{}

	FDynamicMeshVertexBuffer* VertexBuffer;
	FDynamicMeshIndexBuffer* IndexBuffer;
	FDynamicMeshVertexFactory* VertexFactory;
	FDynamicMeshPrimitiveUniformBuffer* PrimitiveUniformBuffer;

	virtual ~FMeshBuilderOneFrameResources()
	{
		VertexBuffer->ReleaseResource();
		IndexBuffer->ReleaseResource();
		VertexFactory->ReleaseResource();
		PrimitiveUniformBuffer->ReleaseResource();

		delete VertexBuffer;
		delete IndexBuffer;
		delete VertexFactory;
		delete PrimitiveUniformBuffer;
	}
};

void FDynamicMeshBuilder::GetMesh(const FMatrix& LocalToWorld, const FMaterialRenderProxy* MaterialRenderProxy, uint8 DepthPriorityGroup, bool bDisableBackfaceCulling, bool bReceivesDecals, int32 ViewIndex, FMeshElementCollector& Collector)
{
	GetMesh(LocalToWorld, MaterialRenderProxy, DepthPriorityGroup, bDisableBackfaceCulling, bReceivesDecals, true, ViewIndex, Collector, NULL);
}

void FDynamicMeshBuilder::GetMesh(const FMatrix& LocalToWorld,const FMaterialRenderProxy* MaterialRenderProxy,uint8 DepthPriorityGroup,bool bDisableBackfaceCulling, bool bReceivesDecals, bool bUseSelectionOutline, int32 ViewIndex, FMeshElementCollector& Collector, HHitProxy* HitProxy)
{
	GetMesh(LocalToWorld, MaterialRenderProxy, DepthPriorityGroup, bDisableBackfaceCulling, bReceivesDecals, true, ViewIndex, Collector, HitProxy != nullptr ? HitProxy->Id : FHitProxyId());
}

void FDynamicMeshBuilder::GetMesh(const FMatrix& LocalToWorld,const FMaterialRenderProxy* MaterialRenderProxy,uint8 DepthPriorityGroup,bool bDisableBackfaceCulling, bool bReceivesDecals, bool bUseSelectionOutline, int32 ViewIndex, FMeshElementCollector& Collector, const FHitProxyId HitProxyId )
{
	// Only draw non-empty meshes.
	if(VertexBuffer->Vertices.Num() > 0 && IndexBuffer->Indices.Num() > 0)
	{
		FMeshBuilderOneFrameResources& OneFrameResources = Collector.AllocateOneFrameResource<FMeshBuilderOneFrameResources>();

		OneFrameResources.VertexBuffer = VertexBuffer;
		OneFrameResources.IndexBuffer = IndexBuffer;

		OneFrameResources.VertexBuffer->InitResource();
		OneFrameResources.IndexBuffer->InitResource();

		OneFrameResources.VertexFactory = new FDynamicMeshVertexFactory(VertexBuffer);
		OneFrameResources.VertexFactory->InitResource();

		// Create the primitive uniform buffer.
		OneFrameResources.PrimitiveUniformBuffer = new FDynamicMeshPrimitiveUniformBuffer();
		FPrimitiveUniformShaderParameters PrimitiveParams = GetPrimitiveUniformShaderParameters(
			LocalToWorld,
			LocalToWorld.GetOrigin(),
			FBoxSphereBounds(EForceInit::ForceInit),
			FBoxSphereBounds(EForceInit::ForceInit),
			bReceivesDecals,
			false,
			false,
			false,
			false,
			false,
			GetDefaultLightingChannelMask(),
			1.0f		// LPV bias
			);


		if (IsInGameThread())
		{
			BeginSetUniformBufferContents(*OneFrameResources.PrimitiveUniformBuffer, PrimitiveParams);
		}
		else
		{
			OneFrameResources.PrimitiveUniformBuffer->SetContents(PrimitiveParams);
		}

		OneFrameResources.PrimitiveUniformBuffer->InitResource();

		// Draw the mesh.
		FMeshBatch& Mesh = Collector.AllocateMesh();
		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = IndexBuffer;
		Mesh.VertexFactory = OneFrameResources.VertexFactory;
		Mesh.MaterialRenderProxy = MaterialRenderProxy;
		BatchElement.PrimitiveUniformBufferResource = OneFrameResources.PrimitiveUniformBuffer;
		// previous l2w not used so treat as static
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = IndexBuffer->Indices.Num() / 3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = VertexBuffer->Vertices.Num() - 1;
		Mesh.ReverseCulling = LocalToWorld.Determinant() < 0.0f ? true : false;
		Mesh.bDisableBackfaceCulling = bDisableBackfaceCulling;
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = DepthPriorityGroup;
		Mesh.bUseSelectionOutline = bUseSelectionOutline;
		Mesh.BatchHitProxyId = HitProxyId;

		Collector.AddMesh(ViewIndex, Mesh);

		// Clear the resource pointers so they cannot be overwritten accidentally.
		// These resources will be released by the PDI.
		VertexBuffer = NULL;
		IndexBuffer = NULL;
	}
}

void FDynamicMeshBuilder::Draw(FPrimitiveDrawInterface* PDI,const FMatrix& LocalToWorld,const FMaterialRenderProxy* MaterialRenderProxy,uint8 DepthPriorityGroup,bool bDisableBackfaceCulling, bool bReceivesDecals, const FHitProxyId HitProxyId )
{
	// Only draw non-empty meshes.
	if(VertexBuffer->Vertices.Num() > 0 && IndexBuffer->Indices.Num() > 0)
	{
		// Register the dynamic resources with the PDI.
		PDI->RegisterDynamicResource(VertexBuffer);
		PDI->RegisterDynamicResource(IndexBuffer);

		// Create the vertex factory.
		FDynamicMeshVertexFactory* VertexFactory = new FDynamicMeshVertexFactory(VertexBuffer);
		PDI->RegisterDynamicResource(VertexFactory);

		// Create the primitive uniform buffer.
		FDynamicMeshPrimitiveUniformBuffer* PrimitiveUniformBuffer = new FDynamicMeshPrimitiveUniformBuffer();
		FPrimitiveUniformShaderParameters PrimitiveParams = GetPrimitiveUniformShaderParameters(
			LocalToWorld,
			LocalToWorld.GetOrigin(),
			FBoxSphereBounds(EForceInit::ForceInit),
			FBoxSphereBounds(EForceInit::ForceInit),
			bReceivesDecals,
			false,
			false,
			false,
			false,
			false,
			GetDefaultLightingChannelMask(),
			1.0f		// LPV bias
			);


		if (IsInGameThread())
		{
			BeginSetUniformBufferContents(*PrimitiveUniformBuffer, PrimitiveParams);
		}
		else
		{
			PrimitiveUniformBuffer->SetContents(PrimitiveParams);
		}
		PDI->RegisterDynamicResource(PrimitiveUniformBuffer);

		// Draw the mesh.
		FMeshBatch Mesh;
		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = IndexBuffer;
		Mesh.VertexFactory = VertexFactory;
		Mesh.MaterialRenderProxy = MaterialRenderProxy;
		BatchElement.PrimitiveUniformBufferResource = PrimitiveUniformBuffer;
		// previous l2w not used so treat as static
		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = IndexBuffer->Indices.Num() / 3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = VertexBuffer->Vertices.Num() - 1;
		Mesh.ReverseCulling = LocalToWorld.Determinant() < 0.0f ? true : false;
		Mesh.bDisableBackfaceCulling = bDisableBackfaceCulling;
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = DepthPriorityGroup;
		Mesh.BatchHitProxyId = HitProxyId;
		PDI->DrawMesh(Mesh);

		// Clear the resource pointers so they cannot be overwritten accidentally.
		// These resources will be released by the PDI.
		VertexBuffer = NULL;
		IndexBuffer = NULL;
	}
}
