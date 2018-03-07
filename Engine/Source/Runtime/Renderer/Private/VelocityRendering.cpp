// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VelocityRendering.cpp: Velocity rendering implementation.
=============================================================================*/

#include "VelocityRendering.h"
#include "SceneUtils.h"
#include "Materials/Material.h"
#include "PostProcess/SceneRenderTargets.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "SceneRendering.h"
#include "StaticMeshDrawList.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "PostProcess/ScreenSpaceReflections.h"
// @third party code - BEGIN HairWorks
#include "HairWorksRenderer.h"
// @third party code - END HairWorks

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarBasePassOutputsVelocity(
	TEXT("r.BasePassOutputsVelocity"),
	0,
	TEXT("Enables rendering WPO velocities on the base pass.\n") \
	TEXT(" 0: Renders in a separate pass/rendertarget, all movable static meshes + dynamic.\n") \
	TEXT(" 1: Renders during the regular base pass adding an extra GBuffer, but allowing motion blur on materials with Time-based WPO."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarParallelVelocity(
	TEXT("r.ParallelVelocity"),
	1,  
	TEXT("Toggles parallel velocity rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarRHICmdVelocityPassDeferredContexts(
	TEXT("r.RHICmdVelocityPassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize velocity pass command list execution."));

RENDERER_API TAutoConsoleVariable<int32> CVarAllowMotionBlurInVR(
	TEXT("vr.AllowMotionBlurInVR"),
	0,
	TEXT("For projects with motion blur enabled, this allows motion blur to be enabled even while in VR."));

DECLARE_FLOAT_COUNTER_STAT(TEXT("Render Velocities"), Stat_GPU_RenderVelocities, STATGROUP_GPU);

bool IsParallelVelocity()
{
	return GRHICommandList.UseParallelAlgorithms() && CVarParallelVelocity.GetValueOnRenderThread();
}

//=============================================================================
/** Encapsulates the Velocity vertex shader. */
class FVelocityVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVelocityVS,MeshMaterial);

public:

	void SetParameters(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FMaterialRenderProxy* MaterialRenderProxy, const FViewInfo& View, const bool bIsInstancedStereo)
	{
		if (IsInstancedStereoParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), IsInstancedStereoParameter, bIsInstancedStereo);
		}

		if (InstancedEyeIndexParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), InstancedEyeIndexParameter, 0);
		}

		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneRenderTargetsMode::DontSet);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FMeshBatch& Mesh, int32 BatchElementIndex, const FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& View, const FPrimitiveSceneProxy* Proxy, const FMatrix& InPreviousLocalToWorld)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(), VertexFactory, View, Proxy, Mesh.Elements[BatchElementIndex], DrawRenderState);

		SetShaderValue(RHICmdList, GetVertexShader(), PreviousLocalToWorld, InPreviousLocalToWorld);
	}

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex)
	{
		if (EyeIndex > 0 && InstancedEyeIndexParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), InstancedEyeIndexParameter, EyeIndex);
		}
	}

	bool SupportsVelocity() const
	{
		return PreviousLocalToWorld.IsBound() || 
			GPUSkinCachePreviousBuffer.IsBound() || 
			PrevTransformBuffer.IsBound() || 
			(PrevTransform0.IsBound() && PrevTransform1.IsBound() && PrevTransform2.IsBound());
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//Only compile the velocity shaders for the default material or if it's masked,
		return ((Material->IsSpecialEngineMaterial() || !Material->WritesEveryPixel() 
			//or if the material is opaque and two-sided,
			|| (Material->IsTwoSided() && !IsTranslucentBlendMode(Material->GetBlendMode()))
			// or if the material modifies meshes
			|| Material->MaterialMayModifyMeshPosition()))
			&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) 
			&& !FVelocityRendering::OutputsOnlyToGBuffer(VertexFactoryType->SupportsStaticLighting());
	}

protected:
	FVelocityVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		PreviousLocalToWorld.Bind(Initializer.ParameterMap,TEXT("PreviousLocalToWorld"));
		GPUSkinCachePreviousBuffer.Bind(Initializer.ParameterMap, TEXT("GPUSkinCachePreviousBuffer"));
		PrevTransform0.Bind(Initializer.ParameterMap, TEXT("PrevTransform0"));
		PrevTransform1.Bind(Initializer.ParameterMap, TEXT("PrevTransform1"));
		PrevTransform2.Bind(Initializer.ParameterMap, TEXT("PrevTransform2"));
		PrevTransformBuffer.Bind(Initializer.ParameterMap, TEXT("PrevTransformBuffer"));
		InstancedEyeIndexParameter.Bind(Initializer.ParameterMap, TEXT("InstancedEyeIndex"));
		IsInstancedStereoParameter.Bind(Initializer.ParameterMap, TEXT("bIsInstancedStereo"));
	}

	FVelocityVS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);

		Ar << PreviousLocalToWorld;
		Ar << GPUSkinCachePreviousBuffer;
		Ar << PrevTransform0;
		Ar << PrevTransform1;
		Ar << PrevTransform2;
		Ar << PrevTransformBuffer;
		Ar << InstancedEyeIndexParameter;
		Ar << IsInstancedStereoParameter;

		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter PreviousLocalToWorld;
	FShaderResourceParameter GPUSkinCachePreviousBuffer;
	FShaderParameter PrevTransform0;
	FShaderParameter PrevTransform1;
	FShaderParameter PrevTransform2;
	FShaderResourceParameter PrevTransformBuffer;
	FShaderParameter InstancedEyeIndexParameter;
	FShaderParameter IsInstancedStereoParameter;
};


//=============================================================================
/** Encapsulates the Velocity hull shader. */
class FVelocityHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(FVelocityHS,MeshMaterial);

protected:
	FVelocityHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseHS(Initializer)
	{}

	FVelocityHS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType) &&
				FVelocityVS::ShouldCache(Platform, Material, VertexFactoryType); // same rules as VS
	}
};

//=============================================================================
/** Encapsulates the Velocity domain shader. */
class FVelocityDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FVelocityDS,MeshMaterial);

public:

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy,const FViewInfo& View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, (FDomainShaderRHIParamRef)GetDomainShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneRenderTargetsMode::DontSet);
	}

protected:
	FVelocityDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseDS(Initializer)
	{}

	FVelocityDS() {}

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType) &&
				FVelocityVS::ShouldCache(Platform, Material, VertexFactoryType); // same rules as VS
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityVS,TEXT("/Engine/Private/VelocityShader.usf"),TEXT("MainVertexShader"),SF_Vertex); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityHS,TEXT("/Engine/Private/VelocityShader.usf"),TEXT("MainHull"),SF_Hull); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityDS,TEXT("/Engine/Private/VelocityShader.usf"),TEXT("MainDomain"),SF_Domain);

//=============================================================================
/** Encapsulates the Velocity pixel shader. */
class FVelocityPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVelocityPS,MeshMaterial);
public:
	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//Only compile the velocity shaders for the default material or if it's masked,
		return ((Material->IsSpecialEngineMaterial() || !Material->WritesEveryPixel() 
			//or if the material is opaque and two-sided,
			|| (Material->IsTwoSided() && !IsTranslucentBlendMode(Material->GetBlendMode()))
			// or if the material modifies meshes
			|| Material->MaterialMayModifyMeshPosition()))
			&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && 
			!FVelocityRendering::OutputsOnlyToGBuffer(VertexFactoryType->SupportsStaticLighting());
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_G16R16);
	}

	FVelocityPS() {}

	FVelocityPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FViewInfo& View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneRenderTargetsMode::DontSet);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FMeshBatch& Mesh,int32 BatchElementIndex,const FDrawingPolicyRenderState& DrawRenderState,const FViewInfo& View, const FPrimitiveSceneProxy* Proxy)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FMeshMaterialShader::SetMesh(RHICmdList, ShaderRHI, VertexFactory, View, Proxy, Mesh.Elements[BatchElementIndex], DrawRenderState);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityPS,TEXT("/Engine/Private/VelocityShader.usf"),TEXT("MainPixelShader"),SF_Pixel);

IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(VelocityPipeline, FVelocityVS, FVelocityPS, true);

//=============================================================================
/** FVelocityDrawingPolicy - Policy to wrap FMeshDrawingPolicy with new shaders. */

FVelocityDrawingPolicy::FVelocityDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
	ERHIFeatureLevel::Type InFeatureLevel
	)
	:	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,InOverrideSettings)	
{
	const FMaterialShaderMap* MaterialShaderIndex = InMaterialResource.GetRenderingThreadShaderMap();
	const FMeshMaterialShaderMap* MeshShaderIndex = MaterialShaderIndex->GetMeshShaderMap(InVertexFactory->GetType());

	ShaderPipeline = nullptr;
	HullShader = nullptr;
	DomainShader = nullptr;
	VertexShader = nullptr;
	PixelShader = nullptr;

	const EMaterialTessellationMode MaterialTessellationMode = InMaterialResource.GetTessellationMode();
	if (RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& MaterialTessellationMode != MTM_NoTessellation)
	{
		bool HasHullShader = MeshShaderIndex->HasShader(&FVelocityHS::StaticType);
		bool HasDomainShader = MeshShaderIndex->HasShader(&FVelocityDS::StaticType);

		HullShader = HasHullShader ? MeshShaderIndex->GetShader<FVelocityHS>() : NULL;
		DomainShader = HasDomainShader ? MeshShaderIndex->GetShader<FVelocityDS>() : NULL;
	}
	else
	{
		static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShaderPipelines"));
		ShaderPipeline = (CVar && CVar->GetValueOnAnyThread()) ? MeshShaderIndex->GetShaderPipeline(&VelocityPipeline) : nullptr;
		if (ShaderPipeline)
		{
			VertexShader = ShaderPipeline->GetShader<FVelocityVS>();
			PixelShader = ShaderPipeline->GetShader<FVelocityPS>();
			check(VertexShader && PixelShader);
		}
	}

	if (!VertexShader)
	{
		check(!PixelShader);
		bool bHasVertexShader = MeshShaderIndex->HasShader(&FVelocityVS::StaticType);
		bool bHasPixelShader = MeshShaderIndex->HasShader(&FVelocityPS::StaticType);

		check((bHasVertexShader && bHasPixelShader) || (!bHasVertexShader && !bHasPixelShader));
		VertexShader = bHasVertexShader ? MeshShaderIndex->GetShader<FVelocityVS>() : nullptr;
		PixelShader = bHasPixelShader ? MeshShaderIndex->GetShader<FVelocityPS>() : nullptr;
	}
}

bool FVelocityDrawingPolicy::SupportsVelocity() const
{
	if (VertexShader && PixelShader && VertexShader->SupportsVelocity() && GPixelFormats[PF_G16R16].Supported)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void FVelocityDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* SceneView, FVelocityDrawingPolicy::ContextDataType PolicyContext) const
{
	// NOTE: Assuming this cast is always safe!
	FViewInfo* View = (FViewInfo*)SceneView;

	VertexShader->SetParameters(RHICmdList, VertexFactory, MaterialRenderProxy, *View, PolicyContext.bIsInstancedStereo);
	PixelShader->SetParameters(RHICmdList, VertexFactory, MaterialRenderProxy, *View);

	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
		DomainShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
	}

	// Set the shared mesh resources.
	FMeshDrawingPolicy::SetSharedState(RHICmdList, DrawRenderState, View, PolicyContext);
}

void FVelocityDrawingPolicy::SetMeshRenderState(
	FRHICommandList& RHICmdList, 
	const FSceneView& SceneView,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMeshBatch& Mesh,
	int32 BatchElementIndex,
	const FDrawingPolicyRenderState& DrawRenderState,
	const ElementDataType& ElementData,
	const ContextDataType PolicyContext
	) const
{
	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
	FMatrix PreviousLocalToWorld;
	
	FViewInfo& View = (FViewInfo&)SceneView;	// NOTE: Assuming this cast is always safe!

	// hack
	FScene* Scene = (FScene*)&PrimitiveSceneProxy->GetScene();

	// previous transform can be stored in the scene for each primitive
	if(Scene->MotionBlurInfoData.GetPrimitiveMotionBlurInfo(PrimitiveSceneProxy->GetPrimitiveSceneInfo(), PreviousLocalToWorld))
	{
		VertexShader->SetMesh(RHICmdList, VertexFactory, Mesh, BatchElementIndex, DrawRenderState, View, PrimitiveSceneProxy, PreviousLocalToWorld);
	}	
	else
	{
		const FMatrix& LocalToWorld = PrimitiveSceneProxy->GetLocalToWorld();		// different implementation from UE4
		VertexShader->SetMesh(RHICmdList, VertexFactory, Mesh, BatchElementIndex, DrawRenderState, View, PrimitiveSceneProxy, LocalToWorld);
	}

	if (HullShader && DomainShader)
	{
		DomainShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, Mesh.Elements[BatchElementIndex], DrawRenderState);
		HullShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, Mesh.Elements[BatchElementIndex], DrawRenderState);
	}

	PixelShader->SetMesh(RHICmdList, VertexFactory, Mesh, BatchElementIndex, DrawRenderState, View, PrimitiveSceneProxy);	
}

void FVelocityDrawingPolicy::SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const {
	VertexShader->SetInstancedEyeIndex(RHICmdList, EyeIndex);
}

bool FVelocityDrawingPolicy::HasVelocity(const FViewInfo& View, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	checkSlow(IsInParallelRenderingThread());
	check(PrimitiveSceneInfo->Proxy);

	// No velocity if motionblur is off, or if it's a non-moving object (treat as background in that case)
	if (View.bCameraCut || !PrimitiveSceneInfo->Proxy->IsMovable())
	{
		return false;
	}

	if (PrimitiveSceneInfo->Proxy->AlwaysHasVelocity())
	{
		return true;
	}

	// check if the primitive has moved
	{
		FMatrix PreviousLocalToWorld;

		// hack
		FScene* Scene = PrimitiveSceneInfo->Scene;

		if (Scene->MotionBlurInfoData.GetPrimitiveMotionBlurInfo(PrimitiveSceneInfo, PreviousLocalToWorld))
		{
			const FMatrix& LocalToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();

			// Hasn't moved (treat as background by not rendering any special velocities)?
			if (LocalToWorld.Equals(PreviousLocalToWorld, 0.0001f))
			{
				return false;
			}
		}
		else 
		{
			return false;
		}
	}

	return true;
}

bool FVelocityDrawingPolicy::HasVelocityOnBasePass(const FViewInfo& View,const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, bool& bOutHasTransform, FMatrix& OutTransform)
{
	checkSlow(IsInParallelRenderingThread());
	// No velocity if motionblur is off, or if it's a non-moving object (treat as background in that case)
	if (View.bCameraCut)
	{
		return false;
	}

	// hack
	FScene* Scene = PrimitiveSceneInfo->Scene;
	if (Scene->MotionBlurInfoData.GetPrimitiveMotionBlurInfo(PrimitiveSceneInfo, OutTransform))
	{
		bOutHasTransform = true;
/*
		const FMatrix& LocalToWorld = PrimitiveSceneProxy->GetLocalToWorld();
		// Hasn't moved (treat as background by not rendering any special velocities)?
		if (LocalToWorld.Equals(OutTransform, 0.0001f))
		{
			return false;
		}
*/
		return true;
	}

	bOutHasTransform = false;
	if (PrimitiveSceneProxy->IsMovable())
	{
		return true;
	}

	//@todo-rco: Optimize finding WPO!
	auto* Material = Mesh.MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel());
	return Material->MaterialModifiesMeshPosition_RenderThread() && Material->OutputsVelocityOnBasePass();
}

FBoundShaderStateInput FVelocityDrawingPolicy::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
{
	return FBoundShaderStateInput(
		FMeshDrawingPolicy::GetVertexDeclaration(), 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		PixelShader->GetPixelShader(),
		FGeometryShaderRHIRef());
}

int32 Compare(const FVelocityDrawingPolicy& A,const FVelocityDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(HullShader);
	COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	return 0;
}


//=============================================================================
/** Policy to wrap FMeshDrawingPolicy with new shaders. */

void FVelocityDrawingPolicyFactory::AddStaticMesh(FScene* Scene, FStaticMesh* StaticMesh)
{
	const auto FeatureLevel = Scene->GetFeatureLevel();
	const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh->MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial(FeatureLevel);

	// When selective outputs are enable, only primitive with no static lighting output velocity in GBuffer.
	const bool bVelocityInGBuffer = FVelocityRendering::OutputsToGBuffer() && (!UseSelectiveBasePassOutputs() || !StaticMesh->PrimitiveSceneInfo->Proxy->HasStaticLighting());

	// Velocity only needs to be directly rendered for movable meshes
	if (StaticMesh->PrimitiveSceneInfo->Proxy->IsMovable() && !bVelocityInGBuffer)
	{
	    EBlendMode BlendMode = Material->GetBlendMode();
	    if(BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
	    {
			if (Material->WritesEveryPixel() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition_RenderThread())
		    {
			    // Default material doesn't handle masked or mesh-mod, and doesn't have the correct bIsTwoSided setting.
			    MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
		    }

			FVelocityDrawingPolicy DrawingPolicy(StaticMesh->VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(FeatureLevel), ComputeMeshOverrideSettings(*StaticMesh), FeatureLevel);

			if (DrawingPolicy.SupportsVelocity())
			{
				// Add the static mesh to the depth-only draw list.
				Scene->VelocityDrawList.AddMesh(StaticMesh, FVelocityDrawingPolicy::ElementDataType(), DrawingPolicy, FeatureLevel);
			}
	    }
	}
}

bool FVelocityDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	bool bPreFog,
	const FDrawingPolicyRenderState& DrawRenderState,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId, 
	const bool bIsInstancedStereo
	)
{
	// Only draw opaque materials in the depth pass.
	const auto FeatureLevel = View.GetFeatureLevel();
	const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial(FeatureLevel);
	EBlendMode BlendMode = Material->GetBlendMode();

	if ((BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked) && ShouldIncludeDomainInMeshPass(Material->GetMaterialDomain()))
	{
		// This should be enforced at a higher level
		//@todo - figure out why this is failing and re-enable
		//check(FVelocityDrawingPolicy::HasVelocity(View, PrimitiveSceneInfo));
		if (Material->WritesEveryPixel() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition_RenderThread())
		{
			// Default material doesn't handle masked, and doesn't have the correct bIsTwoSided setting.
			MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
		}
		FVelocityDrawingPolicy DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(FeatureLevel), ComputeMeshOverrideSettings(Mesh), FeatureLevel);
		if(DrawingPolicy.SupportsVelocity())
		{			
			FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
			DrawRenderStateLocal.SetDitheredLODTransitionAlpha(Mesh.DitheredLODTransitionAlpha);
			DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
			CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
			DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, FVelocityDrawingPolicy::ContextDataType(bIsInstancedStereo));
			for (int32 BatchElementIndex = 0, BatchElementCount = Mesh.Elements.Num(); BatchElementIndex < BatchElementCount; ++BatchElementIndex)
			{
				// We draw instanced static meshes twice when rendering with instanced stereo. Once for each eye.
				const bool bIsInstancedMesh = Mesh.Elements[BatchElementIndex].bIsInstancedMesh;
				const uint32 InstancedStereoDrawCount = (bIsInstancedStereo && bIsInstancedMesh) ? 2 : 1;
				for (uint32 DrawCountIter = 0; DrawCountIter < InstancedStereoDrawCount; ++DrawCountIter)
				{
					DrawingPolicy.SetInstancedEyeIndex(RHICmdList, DrawCountIter);

					TDrawEvent<FRHICommandList> MeshEvent;
					BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

					DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderStateLocal, FMeshDrawingPolicy::ElementDataType(), FVelocityDrawingPolicy::ContextDataType());
					DrawingPolicy.DrawMesh(RHICmdList, Mesh, BatchElementIndex, bIsInstancedStereo);
				}
			}
			return true;
		}
	}

	return false;
}


int32 GetMotionBlurQualityFromCVar()
{
	int32 MotionBlurQuality;

	static const auto ICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MotionBlurQuality"));
	MotionBlurQuality = FMath::Clamp(ICVar->GetValueOnRenderThread(), 0, 4);

	return MotionBlurQuality;
}

bool IsMotionBlurEnabled(const FViewInfo& View)
{
	if (View.GetFeatureLevel() < ERHIFeatureLevel::SM5)
	{
		return false; 
	}

	int32 MotionBlurQuality = GetMotionBlurQualityFromCVar();

	return View.Family->EngineShowFlags.PostProcessing
		&& View.Family->EngineShowFlags.MotionBlur
		&& View.FinalPostProcessSettings.MotionBlurAmount > 0.001f
		&& View.FinalPostProcessSettings.MotionBlurMax > 0.001f
		&& View.Family->bRealtimeUpdate
		&& MotionBlurQuality > 0
		&& (CVarAllowMotionBlurInVR->GetInt() != 0 || !(View.Family->Views.Num() > 1));
}

void FDeferredShadingSceneRenderer::RenderDynamicVelocitiesMeshElementsInner(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, int32 FirstIndex, int32 LastIndex)
{
	FVelocityDrawingPolicyFactory::ContextType Context(DDM_AllOccluders, false);

	for (int32 MeshBatchIndex = FirstIndex; MeshBatchIndex <= LastIndex; MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

		if (MeshBatchAndRelevance.GetHasOpaqueOrMaskedMaterial()
			&& MeshBatchAndRelevance.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->ShouldRenderVelocity(View))
		{
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			FVelocityDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, true, DrawRenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId, View.IsInstancedStereoPass());
		}
	}
}

class FRenderVelocityDynamicThreadTask : public FRenderTask
{
	FDeferredShadingSceneRenderer& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	FDrawingPolicyRenderState DrawRenderState;
	int32 FirstIndex;
	int32 LastIndex;

public:

	FRenderVelocityDynamicThreadTask(
		FDeferredShadingSceneRenderer& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		const FDrawingPolicyRenderState& InDrawRenderState,
		int32 InFirstIndex, int32 InLastIndex
		)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, DrawRenderState(InDrawRenderState)
		, FirstIndex(InFirstIndex)
		, LastIndex(InLastIndex)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderVelocityDynamicThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		ThisRenderer.RenderDynamicVelocitiesMeshElementsInner(RHICmdList, View, DrawRenderState, FirstIndex, LastIndex);
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

static void BeginVelocityRendering(FRHICommandList& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT, bool bPerformClear)
{
	FTextureRHIRef VelocityTexture = VelocityRT->GetRenderTargetItem().TargetableTexture;
	FTexture2DRHIRef DepthTexture = FSceneRenderTargets::Get(RHICmdList).GetSceneDepthTexture();	
	if (bPerformClear)
	{
		// now make the FRHISetRenderTargetsInfo that encapsulates all of the info
		FRHIRenderTargetView ColorView(VelocityTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);
		FRHIDepthRenderTargetView DepthView(DepthTexture, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction, FExclusiveDepthStencil::DepthRead_StencilWrite);

		FRHISetRenderTargetsInfo Info(1, &ColorView, DepthView);		

		// Clear the velocity buffer (0.0f means "use static background velocity").
		RHICmdList.SetRenderTargetsAndClear(Info);		
	}
	else
	{
		SetRenderTarget(RHICmdList, VelocityTexture, DepthTexture, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

		// some platforms need the clear color when rendertargets transition to SRVs.  We propagate here to allow parallel rendering to always
		// have the proper mapping when the RT is transitioned.
		RHICmdList.BindClearMRTValues(true, false, false);
	}
}

static void SetVelocitiesState(FRHICommandList& RHICmdList, const FViewInfo& View, FDrawingPolicyRenderState& DrawRenderState, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	const FIntPoint BufferSize = FSceneRenderTargets::Get(RHICmdList).GetBufferSizeXY();
	const FIntPoint VelocityBufferSize = BufferSize;		// full resolution so we can reuse the existing full res z buffer

	if (!View.IsInstancedStereoPass())
	{
		const uint32 MinX = View.ViewRect.Min.X * VelocityBufferSize.X / BufferSize.X;
		const uint32 MinY = View.ViewRect.Min.Y * VelocityBufferSize.Y / BufferSize.Y;
		const uint32 MaxX = View.ViewRect.Max.X * VelocityBufferSize.X / BufferSize.X;
		const uint32 MaxY = View.ViewRect.Max.Y * VelocityBufferSize.Y / BufferSize.Y;
		RHICmdList.SetViewport(MinX, MinY, 0.0f, MaxX, MaxY, 1.0f);
	}
	else
	{
		if (View.bIsMultiViewEnabled)
		{
			const uint32 LeftMinX = View.Family->Views[0]->ViewRect.Min.X;
			const uint32 LeftMaxX = View.Family->Views[0]->ViewRect.Max.X;
			const uint32 RightMinX = View.Family->Views[1]->ViewRect.Min.X;
			const uint32 RightMaxX = View.Family->Views[1]->ViewRect.Max.X;
			
			const uint32 LeftMaxY = View.Family->Views[0]->ViewRect.Max.Y;
			const uint32 RightMaxY = View.Family->Views[1]->ViewRect.Max.Y;
			
			RHICmdList.SetStereoViewport(LeftMinX, RightMinX, 0, 0, 0.0f, LeftMaxX, RightMaxX, LeftMaxY, RightMaxY, 1.0f);
		}
		else
		{
			const uint32 MaxX = View.Family->InstancedStereoWidth * VelocityBufferSize.X / BufferSize.X;
			const uint32 MaxY = View.ViewRect.Max.Y * VelocityBufferSize.Y / BufferSize.Y;
			RHICmdList.SetViewport(0, 0, 0.0f, MaxX, MaxY, 1.0f);
		}
	}

	DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	//TODO Where does this state go?
	//RHICmdList.SetRasterizerState(GetStaticRasterizerState<true>(FM_Solid, CM_CW));
}

DECLARE_CYCLE_STAT(TEXT("Velocity"), STAT_CLP_Velocity, STATGROUP_ParallelCommandListMarkers);


class FVelocityPassParallelCommandListSet : public FParallelCommandListSet
{
	TRefCountPtr<IPooledRenderTarget>& VelocityRT;
public:
	FVelocityPassParallelCommandListSet(const FViewInfo& InView, FRHICommandListImmediate& InParentCmdList, bool bInParallelExecute, bool bInCreateSceneContext, TRefCountPtr<IPooledRenderTarget>& InVelocityRT)
		: FParallelCommandListSet(GET_STATID(STAT_CLP_Velocity), InView, InParentCmdList, bInParallelExecute, bInCreateSceneContext)
		, VelocityRT(InVelocityRT)
	{
		SetStateOnCommandList(ParentCmdList);
	}

	virtual ~FVelocityPassParallelCommandListSet()
	{
		Dispatch();
	}	

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		FParallelCommandListSet::SetStateOnCommandList(CmdList);
		BeginVelocityRendering(CmdList, VelocityRT, false);
		SetVelocitiesState(CmdList, View, DrawRenderState, VelocityRT);
	}
};

static TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasksVelocityPass(
	TEXT("r.RHICmdFlushRenderThreadTasksVelocityPass"),
	0,
	TEXT("Wait for completion of parallel render thread tasks at the end of the velocity pass.  A more granular version of r.RHICmdFlushRenderThreadTasks. If either r.RHICmdFlushRenderThreadTasks or r.RHICmdFlushRenderThreadTasksVelocityPass is > 0 we will flush."));

void FDeferredShadingSceneRenderer::RenderVelocitiesInnerParallel(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	// parallel version
	FScopedCommandListWaitForTasks Flusher(CVarRHICmdFlushRenderThreadTasksVelocityPass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0, RHICmdList);

	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		if (View.ShouldRenderView())
		{
			FVelocityPassParallelCommandListSet ParallelCommandListSet(View,
				RHICmdList,
				CVarRHICmdVelocityPassDeferredContexts.GetValueOnRenderThread() > 0,
				CVarRHICmdFlushRenderThreadTasksVelocityPass.GetValueOnRenderThread() == 0 && CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() == 0,
				VelocityRT);

			if (!View.IsInstancedStereoPass())
			{
				Scene->VelocityDrawList.DrawVisibleParallel(View.StaticMeshVelocityMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);
			}
			else
			{
				const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshVelocityMap, Views[1].StaticMeshVelocityMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);
				Scene->VelocityDrawList.DrawVisibleParallelInstancedStereo(StereoView, ParallelCommandListSet);
			}
			
			int32 NumPrims = View.DynamicMeshElements.Num();
			int32 EffectiveThreads = FMath::Min<int32>(FMath::DivideAndRoundUp(NumPrims, ParallelCommandListSet.MinDrawsPerCommandList), ParallelCommandListSet.Width);

			int32 Start = 0;
			if (EffectiveThreads)
			{
				check(IsInRenderingThread());

				int32 NumPer = NumPrims / EffectiveThreads;
				int32 Extra = NumPrims - NumPer * EffectiveThreads;


				for (int32 ThreadIndex = 0; ThreadIndex < EffectiveThreads; ThreadIndex++)
				{
					int32 Last = Start + (NumPer - 1) + (ThreadIndex < Extra);
					check(Last >= Start);

					FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

					FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderVelocityDynamicThreadTask>::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::RenderThread)
						.ConstructAndDispatchWhenReady(*this, *CmdList, View, ParallelCommandListSet.DrawRenderState, Start, Last);

					ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);

					Start = Last + 1;
				}
			} 
		}
	}
}

void FDeferredShadingSceneRenderer::RenderVelocitiesInner(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		FDrawingPolicyRenderState DrawRenderState(View);

		if (View.ShouldRenderView())
		{
			SetVelocitiesState(RHICmdList, View, DrawRenderState, VelocityRT);

			// Draw velocities for movable static meshes.
			if (!View.IsInstancedStereoPass())
			{
				Scene->VelocityDrawList.DrawVisible(RHICmdList, View, DrawRenderState, View.StaticMeshVelocityMap, View.StaticMeshBatchVisibility);
			}
			else
			{
				const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshVelocityMap, Views[1].StaticMeshVelocityMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);
				Scene->VelocityDrawList.DrawVisibleInstancedStereo(RHICmdList, StereoView, DrawRenderState);
			}

			RenderDynamicVelocitiesMeshElementsInner(RHICmdList, View, DrawRenderState, 0, View.DynamicMeshElements.Num() - 1);
		}
	}
}

bool FDeferredShadingSceneRenderer::ShouldRenderVelocities() const
{
	if (!GPixelFormats[PF_G16R16].Supported)
	{
		return false;
	}

	bool bNeedsVelocity = false;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		bool bTemporalAA = (View.AntiAliasingMethod == AAM_TemporalAA) && !View.bCameraCut;
		bool bMotionBlur = IsMotionBlurEnabled(View);
		bool bDistanceFieldAO = ShouldPrepareForDistanceFieldAO();

		bool bSSRTemporal = IsSSRTemporalPassRequired(View);

		bNeedsVelocity |= bMotionBlur || bTemporalAA || bDistanceFieldAO || bSSRTemporal;
	}

	return bNeedsVelocity;
}

void FDeferredShadingSceneRenderer::RenderVelocities(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_RenderVelocities, FColor::Emerald);

	check(FeatureLevel >= ERHIFeatureLevel::SM4);
	SCOPE_CYCLE_COUNTER(STAT_RenderVelocities);

	if (!ShouldRenderVelocities())
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, RenderVelocities);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_RenderVelocities);

	FPooledRenderTargetDesc Desc = FVelocityRendering::GetRenderTargetDesc();
	GRenderTargetPool.FindFreeElement(RHICmdList, Desc, VelocityRT, TEXT("Velocity"));

	{
		static const auto MotionBlurDebugVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MotionBlurDebug"));

		if(MotionBlurDebugVar->GetValueOnRenderThread())
		{
			UE_LOG(LogEngine, Log, TEXT("r.MotionBlurDebug: FrameNumber=%d Pause=%d"), ViewFamily.FrameNumber, ViewFamily.bWorldIsPaused ? 1 : 0);
		}
	}

	{
		if (FVelocityRendering::OutputsToGBuffer() && UseSelectiveBasePassOutputs())
		{
			// In this case, basepass also outputs some of the velocities, so append is already started, and don't clear the buffer.
			BeginVelocityRendering(RHICmdList, VelocityRT, false);
		}
		else
		{
			BeginVelocityRendering(RHICmdList, VelocityRT, true);
		}

		if (IsParallelVelocity())
		{
			RenderVelocitiesInnerParallel(RHICmdList, VelocityRT);
		}
		else
		{
			RenderVelocitiesInner(RHICmdList, VelocityRT);
		}

		// @third party code - BEGIN HairWorks
		// Draw hair velocities
		if(HairWorksRenderer::ViewsHasHair(Views))
			HairWorksRenderer::RenderVelocities(RHICmdList, VelocityRT);
		// @third party code - END HairWorks

		RHICmdList.CopyToResolveTarget(VelocityRT->GetRenderTargetItem().TargetableTexture, VelocityRT->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
	}

	// to be able to observe results with VisualizeTexture
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, VelocityRT);
}

FPooledRenderTargetDesc FVelocityRendering::GetRenderTargetDesc()
{
	const FIntPoint BufferSize = FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY();
	const FIntPoint VelocityBufferSize = BufferSize;		// full resolution so we can reuse the existing full res z buffer
	return FPooledRenderTargetDesc(FPooledRenderTargetDesc::Create2DDesc(VelocityBufferSize, PF_G16R16, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable, false));
}

bool FVelocityRendering::OutputsToGBuffer()
{
	return CVarBasePassOutputsVelocity.GetValueOnAnyThread() == 1;
}

bool FVelocityRendering::OutputsOnlyToGBuffer(bool bSupportsStaticLighting)
{
	// With selective outputs, only primitive that have static lighting are rendered in the velocity pass.
	// If the vertex factory does not support static lighting, then it must be rendered in the velocity pass.
	return CVarBasePassOutputsVelocity.GetValueOnAnyThread() == 1 &&
		   (!UseSelectiveBasePassOutputs() || !bSupportsStaticLighting);
}
