// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderingThread.h"
#include "ShaderParameters.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "PrimitiveViewRelevance.h"
#include "Components/SplineMeshComponent.h"
#include "StaticMeshResources.h"

//////////////////////////////////////////////////////////////////////////
// SplineMeshVertexFactory

/** A vertex factory for spline-deformed static meshes */
struct FSplineMeshVertexFactory : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FSplineMeshVertexFactory);
public:

	/** Should we cache the material's shadertype on this platform with this vertex factory? */
	static bool ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		return (Material->IsUsedWithSplineMeshes() || Material->IsSpecialEngineMaterial())
			&& FLocalVertexFactory::ShouldCache(Platform, Material, ShaderType);
	}

	/** Modify compile environment to enable spline deformation */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// We don't call this because we don't actually support speed tree wind, and this advertises support for that
		//FLocalVertexFactory::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("USE_SPLINEDEFORM"), TEXT("1"));
	}

	/** Copy the data from another vertex factory */
	void Copy(const FSplineMeshVertexFactory& Other)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FSplineMeshVertexFactoryCopyData,
			FSplineMeshVertexFactory*, VertexFactory, this,
			const FDataType*, DataCopy, &Other.Data,
			{
			VertexFactory->Data = *DataCopy;
		});
		BeginUpdateResourceRHI(this);
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
};

//////////////////////////////////////////////////////////////////////////
// FSplineMeshVertexFactoryShaderParameters

/** Factory specific params */
class FSplineMeshVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	void Bind(const FShaderParameterMap& ParameterMap) override;

	void SetMesh(FRHICommandList& RHICmdList, FShader* Shader, const FVertexFactory* VertexFactory, const FSceneView& View, const FMeshBatchElement& BatchElement, uint32 DataFlags) const override;

	void Serialize(FArchive& Ar) override
	{
		Ar << SplineStartPosParam;
		Ar << SplineStartTangentParam;
		Ar << SplineStartRollParam;
		Ar << SplineStartScaleParam;
		Ar << SplineStartOffsetParam;

		Ar << SplineEndPosParam;
		Ar << SplineEndTangentParam;
		Ar << SplineEndRollParam;
		Ar << SplineEndScaleParam;
		Ar << SplineEndOffsetParam;

		Ar << SplineUpDirParam;
		Ar << SmoothInterpRollScaleParam;

		Ar << SplineMeshMinZParam;
		Ar << SplineMeshScaleZParam;

		Ar << SplineMeshDirParam;
		Ar << SplineMeshXParam;
		Ar << SplineMeshYParam;
	}

	virtual uint32 GetSize() const override
	{
		return sizeof(*this);
	}

private:
	FShaderParameter SplineStartPosParam;
	FShaderParameter SplineStartTangentParam;
	FShaderParameter SplineStartRollParam;
	FShaderParameter SplineStartScaleParam;
	FShaderParameter SplineStartOffsetParam;

	FShaderParameter SplineEndPosParam;
	FShaderParameter SplineEndTangentParam;
	FShaderParameter SplineEndRollParam;
	FShaderParameter SplineEndScaleParam;
	FShaderParameter SplineEndOffsetParam;

	FShaderParameter SplineUpDirParam;
	FShaderParameter SmoothInterpRollScaleParam;

	FShaderParameter SplineMeshMinZParam;
	FShaderParameter SplineMeshScaleZParam;

	FShaderParameter SplineMeshDirParam;
	FShaderParameter SplineMeshXParam;
	FShaderParameter SplineMeshYParam;
};

//////////////////////////////////////////////////////////////////////////
// SplineMeshSceneProxy

/** Scene proxy for SplineMesh instance */
class FSplineMeshSceneProxy : public FStaticMeshSceneProxy
{
protected:
	struct FLODResources
	{
		/** Pointer to vertex factory object */
		FSplineMeshVertexFactory* VertexFactory;

		FLODResources(FSplineMeshVertexFactory* InVertexFactory) :
			VertexFactory(InVertexFactory)
		{
		}
	};
public:

	FSplineMeshSceneProxy(USplineMeshComponent* InComponent);

	void InitVertexFactory(USplineMeshComponent* InComponent, int32 InLODIndex, FColorVertexBuffer*);

	/** Sets up a shadow FMeshBatch for a specific LOD. */
	virtual bool GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const override;

	/** Sets up a FMeshBatch for a specific LOD and element. */
	virtual bool GetMeshElement(int32 LODIndex, int32 BatchIndex, int32 ElementIndex, uint8 InDepthPriorityGroup, bool bUseSelectedMaterial, bool bUseHoveredMaterial, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const override;

	/** Sets up a wireframe FMeshBatch for a specific LOD. */
	virtual bool GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		return FStaticMeshSceneProxy::GetViewRelevance(View);
	}

	// 	  virtual uint32 GetMemoryFootprint( void ) const { return 0; }

	/** Parameters that define the spline, used to deform mesh */
	FSplineMeshParams SplineParams;
	/** Axis (in component space) that is used to determine X axis for co-ordinates along spline */
	FVector SplineUpDir;
	/** Smoothly (cubic) interpolate the Roll and Scale params over spline. */
	bool bSmoothInterpRollScale;
	/** Chooses the forward axis for the spline mesh orientation */
	ESplineMeshAxis::Type ForwardAxis;

	/** Minimum Z value of the entire mesh */
	float SplineMeshMinZ;
	/** Range of Z values over entire mesh */
	float SplineMeshScaleZ;

protected:
	TArray<FLODResources> LODResources;
};
