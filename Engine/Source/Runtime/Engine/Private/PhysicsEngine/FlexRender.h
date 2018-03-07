#pragma once

#include "PhysXSupport.h"
#include "StaticMeshResources.h"

#if WITH_FLEX

// if true GPU skinning will be used for soft bodies on SM4+ devices
#define USE_FLEX_GPU_SKINNING 1	

struct FFlexShapeTransform
{
	FVector Translation;
	FQuat Rotation;
};

class FFlexVertexBuffer : public FVertexBuffer
{
public:

	int32 NumVerts;
	int32 MaxVerts;

	void Init(int32 Count, int32 MaxCount);

	virtual ~FFlexVertexBuffer() {}
	virtual void InitRHI() override;

	int32 CopyVertex(int32 Index);
};

struct FFlexVertexAttribute
{
	FVector2D TextureCoordinate;
	FColor Color;
};


class FFlexAttributeBuffer : public FVertexBuffer
{
public:

	int32 NumVerts;
	int32 MaxVerts;

	void Init(const FStaticMeshVertexBuffer& Vertices, const FColorVertexBuffer& Colors, int32 MaxCount);

	virtual ~FFlexAttributeBuffer() {}
	virtual void InitRHI() override;

	int32 CopyVertex(int32 Index, float Alpha);
	void Update();

	TArray<FFlexVertexAttribute> Attributes;
};


class FFlexIndexBuffer : public FIndexBuffer 
{
public:

	void Init(const FRawStaticIndexBuffer& StaticMeshIndices)
	{
		StaticMeshIndices.GetCopy(Indices);

		BeginInitResource(this);
	}

	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32), Indices.Num() * sizeof(int32), BUF_Dynamic, CreateInfo);

		Update();
	}

	void Update()
	{
		void* IndexBufferData = RHILockIndexBuffer(IndexBufferRHI, 0, Indices.Num() * sizeof(int32), RLM_WriteOnly);
		FMemory::Memcpy(IndexBufferData, &Indices[0], Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}

	TArray<uint32> Indices;
};

// Flex vertex factories override the local vertex factory and modify the position stream
class FFlexVertexFactory : public FLocalVertexFactory
{
public:

	virtual void SkinSoft(const FPositionVertexBuffer& Positions, const FStaticMeshVertexBuffer& Vertices, const FFlexShapeTransform* Transforms, const FVector* RestPoses, const int16* ClusterIndices, const float* ClusterWeights, int NumClusters) = 0;
	virtual void SkinCloth(const FVector4* SimulatedPositions, const FVector* SimulatedNormals) = 0;
	virtual void TearCloth(const NvFlexExtTearingMeshEdit* Edits, int32 NumEdits, float Alpha) = 0;

	virtual void OverrideMeshElement(FMeshBatchElement& element) {};
};

// Overrides local vertex factory with CPU skinned deformation
class FFlexCPUVertexFactory : public FFlexVertexFactory
{
public:
	
	FFlexCPUVertexFactory(const FLocalVertexFactory& Base, int NumVerts, int MaxVerts, const int* ParticleMap, const FRawStaticIndexBuffer& Indices, const FStaticMeshVertexBuffer& Vertices, const FColorVertexBuffer& Colors);
	virtual ~FFlexCPUVertexFactory();

	virtual void SkinCloth(const FVector4* SimulatedPositions, const FVector* SimulatedNormals) override;
	virtual void SkinSoft(const FPositionVertexBuffer& Positions, const FStaticMeshVertexBuffer& Vertices, const FFlexShapeTransform* Transforms, const FVector* RestPoses, const int16* ClusterIndices, const float* ClusterWeights, int NumClusters) override;

	virtual void TearCloth(const NvFlexExtTearingMeshEdit* Edits, int32 NumEdits, float Alpha) override;

	virtual void OverrideMeshElement(FMeshBatchElement& element);

	// Stores CPU skinned positions and normals to override default static mesh stream
	FFlexVertexBuffer VertexBuffer;
	FFlexAttributeBuffer AttributeBuffer;

	// Index buffer copy for tearing to override static mesh topology
	FFlexIndexBuffer IndexBuffer;

	TArray<int> VertexToParticleMap;

};

class FFlexGPUVertexFactory : public FFlexVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FFlexVertexFactory);

public:
	
	struct FlexDataType
	{
		/** Skinning weights for clusters */
		FVertexStreamComponent ClusterWeights;

		/** Skinning indices for clusters */
		FVertexStreamComponent ClusterIndices;
	};

	/** Should we cache the material's shadertype on this platform with this vertex factory? */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		return  IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) &&
				(Material->IsUsedWithFlexMeshes() || Material->IsSpecialEngineMaterial()) && 
				FLocalVertexFactory::ShouldCache(Platform, Material, ShaderType);
	}

	/** Modify compile environment to enable flex cluster deformation */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FLocalVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_FLEX_DEFORM"), TEXT("1"));
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
	
public:

	FFlexGPUVertexFactory(const FLocalVertexFactory& Base, const FVertexBuffer* ClusterWeightsVertexBuffer, const FVertexBuffer* ClusterIndicesVertexBuffer);
	virtual ~FFlexGPUVertexFactory();

	virtual void AddVertexElements(FDataType& InData, FVertexDeclarationElementList& Elements) override;
	virtual void AddVertexPositionElements(FDataType& Data, FVertexDeclarationElementList& Elements) override;

	virtual void InitDynamicRHI();
	virtual void ReleaseDynamicRHI();

	void AllocateFor(int32 InMaxClusters);

	// FFlexVertexFactory methods
	virtual void SkinCloth(const FVector4* SimulatedPositions, const FVector* SimulatedNormals) override;
	virtual void SkinSoft(const FPositionVertexBuffer& Positions, const FStaticMeshVertexBuffer& Vertices, const FFlexShapeTransform* Transforms, const FVector* RestPoses, const int16* ClusterIndices, const float* ClusterWeights, int NumClusters) override;
	virtual void TearCloth(const NvFlexExtTearingMeshEdit* Edits, int32 NumEdits, float Alpha) override {}

	int MaxClusters;

	FReadBuffer ClusterTranslations;
	FReadBuffer ClusterRotations;

protected:

	FlexDataType FlexData;
};

/** Scene proxy, overrides default static mesh behavior */
class FFlexMeshSceneProxy : public FStaticMeshSceneProxy
{
public:

	FFlexMeshSceneProxy(UStaticMeshComponent* Component);
	virtual ~FFlexMeshSceneProxy();

	int GetLastVisibleFrame();

	void UpdateClothTransforms();
	void UpdateClothMesh(const NvFlexExtTearingMeshEdit* Edits, int32 NumEdits, float Alpha);

	void UpdateSoftTransforms(const FFlexShapeTransform* Transforms, int32 NumShapes);

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	//virtual void PreRenderView(const FSceneViewFamily* ViewFamily, const uint32 VisibilityMap, int32 FrameNumber) override;
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const override;
		 
	virtual bool GetMeshElement(
		int32 LODIndex, 
		int32 BatchIndex, 
		int32 ElementIndex, 
		uint8 InDepthPriorityGroup, 
		bool bUseSelectedMaterial, 
		bool bUseHoveredMaterial,
		bool bAllowPreCulledIndices,
		FMeshBatch& OutMeshBatch) const;

	virtual bool GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const;
	virtual bool GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const;

	FFlexVertexFactory* VertexFactory;

	UFlexComponent* FlexComponent;
	uint32 LastFrame;
};

#endif // WITH FLEX