// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
FlexFluidSurfaceSceneProxy.h:
=============================================================================*/

/*=============================================================================
FFlexFluidSurfaceTextures, Encapsulates the textures used for scene rendering. TODO, move to some better place
=============================================================================*/
struct FFlexFluidSurfaceTextures
{
public:
	FFlexFluidSurfaceTextures() : BufferSize(0, 0)	{}

	~FFlexFluidSurfaceTextures() {}

	// Current buffer size
	FIntPoint BufferSize;

	// Intermediate results
	TRefCountPtr<IPooledRenderTarget> Depth;
	TRefCountPtr<IPooledRenderTarget> DepthStencil;
	TRefCountPtr<IPooledRenderTarget> Thickness;
	TRefCountPtr<IPooledRenderTarget> DownSampledSceneDepth;
	TRefCountPtr<IPooledRenderTarget> SmoothDepth;
	TRefCountPtr<IPooledRenderTarget> UpSampledDepth;
};

/*=============================================================================
FFlexFluidSurfaceSceneProxy
=============================================================================*/

struct FSurfaceParticleMesh
{
	const FParticleSystemSceneProxy* PSysSceneProxy;
	const FDynamicEmitterDataBase* DynamicEmitterData;
	const FMeshBatch* Mesh;
};

struct FFlexFluidSurfaceProperties
{
	float SmoothingRadius;
	int32 MaxRadialSamples;
	float DepthEdgeFalloff;
	float ThicknessParticleScale;
	float DepthParticleScale;
	float TexResScale;
	bool ReceiveShadows;
	UMaterialInterface* Material;
};

class FFlexFluidSurfaceSceneProxy : public FPrimitiveSceneProxy
{
public:

	FFlexFluidSurfaceSceneProxy(UFlexFluidSurfaceComponent* Component);

	virtual ~FFlexFluidSurfaceSceneProxy();

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI, const FSceneView* View) {}

	virtual void CreateRenderThreadResources() override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const;

	virtual uint32 GetMemoryFootprint(void) const { return(sizeof(*this) + GetAllocatedSize()); }

	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

	void SetEmitterDynamicData_RenderThread(class FParticleSystemSceneProxy* PSysSceneProxy,
		struct FDynamicEmitterDataBase* DynamicEmitterData);

	void SetDynamicData_RenderThread(FFlexFluidSurfaceProperties SurfaceProperties);

public:

	// resources managed by game thread
	TArray<struct FDynamicEmitterDataBase*> DynamicEmitterDataArray;
	TArray<class FParticleSystemSceneProxy*> ParticleSystemSceneProxyArray;
	UMaterialInterface* SurfaceMaterial;
	float SmoothingRadius;
	int32 MaxRadialSamples;
	float DepthEdgeFalloff;
	float ThicknessParticleScale;
	float DepthParticleScale;
	float TexResScale;

	// resources managed in render thread
	TArray<FSurfaceParticleMesh> VisibleParticleMeshes;
	FMeshBatch* MeshBatch;
	FFlexFluidSurfaceTextures* Textures;

private:
	class FFlexFluidSurfaceVertexFactory* VertexFactory;
};