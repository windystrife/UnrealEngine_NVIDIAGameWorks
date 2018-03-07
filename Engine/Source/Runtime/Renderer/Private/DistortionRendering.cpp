// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistortionRendering.cpp: Distortion rendering implementation.
=============================================================================*/

#include "DistortionRendering.h"
#include "HitProxies.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/RenderTargetPool.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "DrawingPolicy.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "SceneRendering.h"
#include "DynamicPrimitiveDrawing.h"
#include "PostProcess/RenderingCompositionGraph.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"
#include "Materials/Material.h"
#include "PipelineStateCache.h"
#include "ScenePrivate.h"

DECLARE_FLOAT_COUNTER_STAT(TEXT("Distortion"), Stat_GPU_Distortion, STATGROUP_GPU);

const uint8 kStencilMaskBit = STENCIL_SANDBOX_MASK;

static TAutoConsoleVariable<int32> CVarDisableDistortion(
														 TEXT("r.DisableDistortion"),
														 0,
														 TEXT("Prevents distortion effects from rendering.  Saves a full-screen framebuffer's worth of memory."),
														 ECVF_Default);

/**
* A pixel shader for rendering the full screen refraction pass
*/
template <bool UseMSAA>
class TDistortionApplyScreenPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistortionApplyScreenPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{ 
		return !UseMSAA || IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	TDistortionApplyScreenPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		if (UseMSAA)
		{
			DistortionTexture.Bind(Initializer.ParameterMap, TEXT("DistortionMSAATexture"));
			SceneColorTexture.Bind(Initializer.ParameterMap, TEXT("SceneColorMSAATexture"));
		}
		else
		{
			DistortionTexture.Bind(Initializer.ParameterMap, TEXT("DistortionTexture"));
			SceneColorTexture.Bind(Initializer.ParameterMap, TEXT("SceneColorTexture"));
		}
		SceneColorRect.Bind(Initializer.ParameterMap, TEXT("SceneColorRect"));
		DistortionTextureSampler.Bind(Initializer.ParameterMap,TEXT("DistortionTextureSampler"));
		SceneColorTextureSampler.Bind(Initializer.ParameterMap,TEXT("SceneColorTextureSampler"));
	}
	TDistortionApplyScreenPS() {}

	void SetParameters(const FRenderingCompositePassContext& Context, const FViewInfo& View, IPooledRenderTarget& DistortionRT)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		FTextureRHIParamRef DistortionTextureValue = DistortionRT.GetRenderTargetItem().TargetableTexture;
		FTextureRHIParamRef SceneColorTextureValue = SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture;

		// Here we use SF_Point as in fullscreen the pixels are 1:1 mapped.
		SetTextureParameter(
			Context.RHICmdList,
			ShaderRHI,
			DistortionTexture,
			DistortionTextureSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistortionTextureValue
			);

		SetTextureParameter(
			Context.RHICmdList,
			ShaderRHI,
			SceneColorTexture,
			SceneColorTextureSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			SceneColorTextureValue
			);

		FIntPoint SceneBufferSize = SceneContext.GetBufferSizeXY();
		FIntRect ViewportRect = Context.GetViewport();
		FVector4 SceneColorRectValue = FVector4((float)ViewportRect.Min.X/SceneBufferSize.X,
												(float)ViewportRect.Min.Y/SceneBufferSize.Y,
												(float)ViewportRect.Max.X/SceneBufferSize.X,
												(float)ViewportRect.Max.Y/SceneBufferSize.Y);
		SetShaderValue(Context.RHICmdList, ShaderRHI, SceneColorRect, SceneColorRectValue);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DistortionTexture << DistortionTextureSampler << SceneColorTexture << SceneColorTextureSampler << SceneColorRect;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/DistortApplyScreenPS.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Main");
	}

private:
	FShaderResourceParameter DistortionTexture;
	FShaderResourceParameter DistortionTextureSampler;
	FShaderResourceParameter SceneColorTexture;
	FShaderResourceParameter SceneColorTextureSampler;
	FShaderParameter SceneColorRect;
	FShaderParameter DistortionParams;

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_MSAA"), UseMSAA ? 1 : 0);
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) \
	typedef TDistortionApplyScreenPS<A> TDistortionApplyScreenPS##A; \
	IMPLEMENT_SHADER_TYPE2(TDistortionApplyScreenPS##A,SF_Pixel)
VARIATION1(false);
VARIATION1(true);
#undef VARIATION1

/**
* A pixel shader that applies the distorted image to the scene
*/
template <bool UseMSAA>
class TDistortionMergePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDistortionMergePS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !UseMSAA || IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	TDistortionMergePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		if (UseMSAA)
		{
			SceneColorTexture.Bind(Initializer.ParameterMap, TEXT("SceneColorMSAATexture"));
		}
		else
		{
			SceneColorTexture.Bind(Initializer.ParameterMap, TEXT("SceneColorTexture"));
		}
		SceneColorTextureSampler.Bind(Initializer.ParameterMap,TEXT("SceneColorTextureSampler"));
	}
	TDistortionMergePS() {}

	void SetParameters(const FRenderingCompositePassContext& Context, const FViewInfo& View, const FTextureRHIParamRef& PassTexture)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, View.ViewUniformBuffer);

		SetTextureParameter(
			Context.RHICmdList,
			ShaderRHI,
			SceneColorTexture,
			SceneColorTextureSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			PassTexture
			);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneColorTexture << SceneColorTextureSampler;
		return bShaderHasOutdatedParameters;
	}
	
	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/DistortApplyScreenPS.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("Merge");
	}

private:
	FShaderResourceParameter SceneColorTexture;
	FShaderResourceParameter SceneColorTextureSampler;

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_MSAA"), UseMSAA ? 1 : 0);
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A) \
	typedef TDistortionMergePS<A> TDistortionMergePS##A; \
	IMPLEMENT_SHADER_TYPE2(TDistortionMergePS##A,SF_Pixel)
VARIATION1(false);
VARIATION1(true);
#undef VARIATION1

/**
* Policy for drawing distortion mesh accumulated offsets
*/
class FDistortMeshAccumulatePolicy
{	
public:
	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material && IsTranslucentBlendMode(Material->GetBlendMode()) && Material->IsDistorted();
	}
};

/**
* A vertex shader for rendering distortion meshes
*/
template<class DistortMeshPolicy>
class TDistortionMeshVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(TDistortionMeshVS,MeshMaterial);

protected:

	TDistortionMeshVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
	}

	TDistortionMeshVS()
	{
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return DistortMeshPolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

public:
	
	void SetParameters(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View->GetFeatureLevel()), *View, View->ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}
};


/**
 * A hull shader for rendering distortion meshes
 */
template<class DistortMeshPolicy>
class TDistortionMeshHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(TDistortionMeshHS,MeshMaterial);

protected:

	TDistortionMeshHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseHS(Initializer)
	{}

	TDistortionMeshHS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType)
			&& DistortMeshPolicy::ShouldCache(Platform, Material, VertexFactoryType);
	}
};

/**
 * A domain shader for rendering distortion meshes
 */
template<class DistortMeshPolicy>
class TDistortionMeshDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(TDistortionMeshDS,MeshMaterial);

protected:

	TDistortionMeshDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseDS(Initializer)
	{}

	TDistortionMeshDS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType)
			&& DistortMeshPolicy::ShouldCache(Platform, Material, VertexFactoryType);
	}
};


IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshVS<FDistortMeshAccumulatePolicy>,TEXT("/Engine/Private/DistortAccumulateVS.usf"),TEXT("Main"),SF_Vertex); 
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshHS<FDistortMeshAccumulatePolicy>,TEXT("/Engine/Private/DistortAccumulateVS.usf"),TEXT("MainHull"),SF_Hull); 
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshDS<FDistortMeshAccumulatePolicy>,TEXT("/Engine/Private/DistortAccumulateVS.usf"),TEXT("MainDomain"),SF_Domain);


/**
* A pixel shader to render distortion meshes
*/
template<class DistortMeshPolicy>
class TDistortionMeshPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(TDistortionMeshPS,MeshMaterial);

public:
	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return DistortMeshPolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

	TDistortionMeshPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
//later?		MaterialParameters.Bind(Initializer.ParameterMap);
		DistortionParams.Bind(Initializer.ParameterMap,TEXT("DistortionParams"));
	}

	TDistortionMeshPS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);

		float Ratio = View.UnscaledViewRect.Width() / (float)View.UnscaledViewRect.Height();
		float Params[4];
		Params[0] = View.ViewMatrices.GetProjectionMatrix().M[0][0];
		Params[1] = Ratio;
		Params[2] = (float)View.UnscaledViewRect.Width();
		Params[3] = (float)View.UnscaledViewRect.Height();

		SetShaderValue(RHICmdList, GetPixelShader(), DistortionParams, Params);

	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << DistortionParams;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter DistortionParams;
};

//** distortion accumulate pixel shader type implementation */
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshPS<FDistortMeshAccumulatePolicy>,TEXT("/Engine/Private/DistortAccumulatePS.usf"),TEXT("Main"),SF_Pixel);

/*-----------------------------------------------------------------------------
TDistortionMeshDrawingPolicy
-----------------------------------------------------------------------------*/

/**
* Distortion mesh drawing policy
*/
template<class DistortMeshPolicy>
class TDistortionMeshDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** context type */
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	/**
	* Constructor
	* @param InIndexBuffer - index buffer for rendering
	* @param InVertexFactory - vertex factory for rendering
	* @param InMaterialRenderProxy - material instance for rendering
	* @param bInOverrideWithShaderComplexity - whether to override with shader complexity
	*/
	TDistortionMeshDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& MaterialResouce,
		bool bInitializeOffsets,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		EDebugViewShaderMode InDebugViewShaderMode,
		ERHIFeatureLevel::Type InFeatureLevel
		);

	// FMeshDrawingPolicy interface.

	/**
	* Match two draw policies
	* @param Other - draw policy to compare
	* @return true if the draw policies are a match
	*/
	FDrawingPolicyMatchResult Matches(const TDistortionMeshDrawingPolicy& Other) const;

	/**
	* Executes the draw commands which can be shared between any meshes using this drawer.
	* @param CI - The command interface to execute the draw commands on.
	* @param View - The view of the scene being drawn.
	*/
	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View, const ContextDataType PolicyContext) const;
	
	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const;

	/**
	* Sets the render states for drawing a mesh.
	* @param PrimitiveSceneProxy - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
	* @param Mesh - mesh element with data needed for rendering
	* @param ElementData - context specific data for mesh rendering
	*/
	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		const FDrawingPolicyRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const;

private:
	/** vertex shader based on policy type */
	TDistortionMeshVS<DistortMeshPolicy>* VertexShader;

	TDistortionMeshHS<DistortMeshPolicy>* HullShader;
	TDistortionMeshDS<DistortMeshPolicy>* DomainShader;

	/** whether we are initializing offsets or accumulating them */
	bool bInitializeOffsets;
	/** pixel shader based on policy type */
	TDistortionMeshPS<DistortMeshPolicy>* DistortPixelShader;
	/** pixel shader used to initialize offsets */
//later	FShaderComplexityAccumulatePixelShader* InitializePixelShader;
};

/**
* Constructor
* @param InIndexBuffer - index buffer for rendering
* @param InVertexFactory - vertex factory for rendering
* @param InMaterialRenderProxy - material instance for rendering
*/
template<class DistortMeshPolicy> 
TDistortionMeshDrawingPolicy<DistortMeshPolicy>::TDistortionMeshDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	bool bInInitializeOffsets,
	const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
	EDebugViewShaderMode InDebugViewShaderMode,
	ERHIFeatureLevel::Type InFeatureLevel
	)
:	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,InOverrideSettings,InDebugViewShaderMode)
,	bInitializeOffsets(bInInitializeOffsets)
{
	HullShader = NULL;
	DomainShader = NULL;

	const EMaterialTessellationMode MaterialTessellationMode = MaterialResource->GetTessellationMode();
	if (RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& MaterialTessellationMode != MTM_NoTessellation)
	{
		HullShader = InMaterialResource.GetShader<TDistortionMeshHS<DistortMeshPolicy>>(VertexFactory->GetType());
		DomainShader = InMaterialResource.GetShader<TDistortionMeshDS<DistortMeshPolicy>>(VertexFactory->GetType());
	}

	VertexShader = InMaterialResource.GetShader<TDistortionMeshVS<DistortMeshPolicy> >(InVertexFactory->GetType());

	if (bInitializeOffsets)
	{
//later		InitializePixelShader = GetGlobalShaderMap(View.ShaderMap)->GetShader<FShaderComplexityAccumulatePixelShader>();
		DistortPixelShader = NULL;
	}
	else
	{
		DistortPixelShader = InMaterialResource.GetShader<TDistortionMeshPS<DistortMeshPolicy> >(InVertexFactory->GetType());
//later		InitializePixelShader = NULL;
	}
}

/**
* Match two draw policies
* @param Other - draw policy to compare
* @return true if the draw policies are a match
*/
template<class DistortMeshPolicy>
FDrawingPolicyMatchResult TDistortionMeshDrawingPolicy<DistortMeshPolicy>::Matches(
	const TDistortionMeshDrawingPolicy& Other
	) const
{
	DRAWING_POLICY_MATCH_BEGIN
		DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
		DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
		DRAWING_POLICY_MATCH(HullShader == Other.HullShader) &&
		DRAWING_POLICY_MATCH(DomainShader == Other.DomainShader) &&
		DRAWING_POLICY_MATCH(bInitializeOffsets == Other.bInitializeOffsets) &&
		DRAWING_POLICY_MATCH(DistortPixelShader == Other.DistortPixelShader); //&&
	//later	InitializePixelShader == Other.InitializePixelShader;
	DRAWING_POLICY_MATCH_END
}

/**
* Executes the draw commands which can be shared between any meshes using this drawer.
* @param CI - The command interface to execute the draw commands on.
* @param View - The view of the scene being drawn.
*/
template<class DistortMeshPolicy>
void TDistortionMeshDrawingPolicy<DistortMeshPolicy>::SetSharedState(
	FRHICommandList& RHICmdList, 
	const FDrawingPolicyRenderState& DrawRenderState,
	const FSceneView* View,
	const ContextDataType PolicyContext
	) const
{
	// Set shared mesh resources
	FMeshDrawingPolicy::SetSharedState(RHICmdList, DrawRenderState, View, PolicyContext);

	// Set the translucent shader parameters for the material instance
	VertexShader->SetParameters(RHICmdList, VertexFactory,MaterialRenderProxy,View);

	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		DomainShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
	}

	if (UseDebugViewPS())
	{
		check(!bInitializeOffsets);
//later		TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityPixelShader(GetGlobalShaderMap(View->ShaderMap));
		//don't add any vertex complexity
//later		ShaderComplexityPixelShader->SetParameters(0, DistortPixelShader->GetNumInstructions());
	}
	if (bInitializeOffsets)
	{
//later			InitializePixelShader->SetParameters(0, 0);
	}
	else
	{
		DistortPixelShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
	}
}

/** 
* Create bound shader state using the vertex decl from the mesh draw policy
* as well as the shaders needed to draw the mesh
* @return new bound shader state object
*/
template<class DistortMeshPolicy>
FBoundShaderStateInput TDistortionMeshDrawingPolicy<DistortMeshPolicy>::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
{
	FPixelShaderRHIParamRef PixelShaderRHIRef = NULL;

	if (UseDebugViewPS())
	{
		check(!bInitializeOffsets);
//later		TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityAccumulatePixelShader(GetGlobalShaderMap(InFeatureLevel));
//later		PixelShaderRHIRef = ShaderComplexityAccumulatePixelShader->GetPixelShader();
	}

	if (bInitializeOffsets)
	{
//later		PixelShaderRHIRef = InitializePixelShader->GetPixelShader();
	}
	else
	{
		PixelShaderRHIRef = DistortPixelShader->GetPixelShader();
	}

	return FBoundShaderStateInput(
		FMeshDrawingPolicy::GetVertexDeclaration(), 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		PixelShaderRHIRef,
		FGeometryShaderRHIRef());

}

/**
* Sets the render states for drawing a mesh.
* @param PrimitiveSceneProxy - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
* @param Mesh - mesh element with data needed for rendering
* @param ElementData - context specific data for mesh rendering
*/
template<class DistortMeshPolicy>
void TDistortionMeshDrawingPolicy<DistortMeshPolicy>::SetMeshRenderState(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMeshBatch& Mesh,
	int32 BatchElementIndex,
	const FDrawingPolicyRenderState& DrawRenderState,
	const ElementDataType& ElementData,
	const ContextDataType PolicyContext
	) const
{

	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

	// Set transforms
	VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);

	if(HullShader && DomainShader)
	{
		HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
		DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	}
	

	// Don't set pixel shader constants if we are overriding with the shader complexity pixel shader
	if (!bInitializeOffsets && !UseDebugViewPS())
	{
		DistortPixelShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	}
}

/*-----------------------------------------------------------------------------
TDistortionMeshDrawingPolicyFactory
-----------------------------------------------------------------------------*/

/**
* Distortion mesh draw policy factory. 
* Creates the policies needed for rendering a mesh based on its material
*/
template<class DistortMeshPolicy>
class TDistortionMeshDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = false };
	typedef bool ContextType;

	/**
	* Render a dynamic mesh using a distortion mesh draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

	/**
	* Render a dynamic mesh using a distortion mesh draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawStaticMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo* View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		uint64 BatchElementMask,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};

/**
* Render a dynamic mesh using a distortion mesh draw policy 
* @return true if the mesh rendered
*/
template<class DistortMeshPolicy>
bool TDistortionMeshDrawingPolicyFactory<DistortMeshPolicy>::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	ContextType bInitializeOffsets,
	const FMeshBatch& Mesh,
	bool bPreFog,
	const FDrawingPolicyRenderState& DrawRenderState,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	const auto FeatureLevel = View.GetFeatureLevel();
	bool bDistorted = Mesh.MaterialRenderProxy && Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel)->IsDistorted() && ShouldIncludeDomainInMeshPass(Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel)->GetMaterialDomain());

	//reconstruct bBackface from the View
	const bool bBackFace = XOR(View.bReverseCulling, !!(DrawRenderState.GetViewOverrideFlags() & EDrawingPolicyOverrideFlags::ReverseCullMode));
	if(bDistorted && !bBackFace)
	{
		// draw dynamic mesh element using distortion mesh policy
		TDistortionMeshDrawingPolicy<DistortMeshPolicy> DrawingPolicy(
			Mesh.VertexFactory,
			Mesh.MaterialRenderProxy,
			*Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel),
			bInitializeOffsets,
			ComputeMeshOverrideSettings(Mesh),
			View.Family->GetDebugViewShaderMode(),
			FeatureLevel
			);

		FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
		DrawRenderStateLocal.SetDitheredLODTransitionAlpha(Mesh.DitheredLODTransitionAlpha);
		DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
		CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ContextDataType());
		
		for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
		{
			TDrawEvent<FRHICommandList> MeshEvent;
			BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

			DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex, DrawRenderStateLocal,typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ElementDataType(), typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ContextDataType());
			DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
		}

		return true;
	}
	else
	{
		return false;
	}
}

/**
* Render a dynamic mesh using a distortion mesh draw policy 
* @return true if the mesh rendered
*/
template<class DistortMeshPolicy>
bool TDistortionMeshDrawingPolicyFactory<DistortMeshPolicy>::DrawStaticMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo* View,
	ContextType bInitializeOffsets,
	const FStaticMesh& StaticMesh,
	uint64 BatchElementMask,
	const FDrawingPolicyRenderState& DrawRenderState,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	const auto FeatureLevel = View->GetFeatureLevel();
	bool bDistorted = StaticMesh.MaterialRenderProxy && StaticMesh.MaterialRenderProxy->GetMaterial(FeatureLevel)->IsDistorted();
	
	const bool bBackFace = XOR(View->bReverseCulling, !!(DrawRenderState.GetViewOverrideFlags() & EDrawingPolicyOverrideFlags::ReverseCullMode));
	if(bDistorted && !bBackFace)
	{
		// draw static mesh element using distortion mesh policy
		TDistortionMeshDrawingPolicy<DistortMeshPolicy> DrawingPolicy(
			StaticMesh.VertexFactory,
			StaticMesh.MaterialRenderProxy,
			*StaticMesh.MaterialRenderProxy->GetMaterial(FeatureLevel),
			bInitializeOffsets,
			ComputeMeshOverrideSettings(StaticMesh),
			View->Family->GetDebugViewShaderMode(),
			FeatureLevel
			);

		FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
		DrawingPolicy.ApplyDitheredLODTransitionState(DrawRenderStateLocal, *View, StaticMesh, false);
		DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, *View);
		CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View->GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, View, typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ContextDataType());
		int32 BatchElementIndex = 0;
		do
		{
			if(BatchElementMask & 1)
			{
				TDrawEvent<FRHICommandList> MeshEvent;
				BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, StaticMesh, MeshEvent);


				DrawingPolicy.SetMeshRenderState(RHICmdList, *View,PrimitiveSceneProxy,StaticMesh,BatchElementIndex,DrawRenderStateLocal,
					typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ElementDataType(),
					typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ContextDataType()
					);
				DrawingPolicy.DrawMesh(RHICmdList, StaticMesh, BatchElementIndex);
			}
			BatchElementMask >>= 1;
			BatchElementIndex++;
		} while(BatchElementMask);

		return true;
	}
	else
	{
		return false;
	}
}

/*-----------------------------------------------------------------------------
	FDistortionPrimSet
-----------------------------------------------------------------------------*/

bool FDistortionPrimSet::DrawAccumulatedOffsets(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, bool bInitializeOffsets)
{
	bool bDirty = false;

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDistortionPrimSet_DrawAccumulatedOffsets);

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDistortionPrimSet_DrawAccumulatedOffsets_View);
		// Draw the view's elements with the distortion drawing policy.
		bDirty |= DrawViewElements<TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy> >(
			RHICmdList,
			View,
			DrawRenderState,
			bInitializeOffsets,
			0,	// DPG Index?
			false // Distortion is rendered post fog.
			);
	}

	if( Prims.Num() )
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDistortionPrimSet_DrawAccumulatedOffsets_Prims);
		
		// Draw scene prims
		for( int32 PrimIdx = 0; PrimIdx < Prims.Num(); PrimIdx++ )
		{
			FPrimitiveSceneProxy* PrimitiveSceneProxy = Prims[PrimIdx];

#if WITH_FLEX
			if (PrimitiveSceneProxy->IsFlexFluidSurface())
				continue;
#endif

			const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetIndex()];
			FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();

			TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy>::ContextType Context(bInitializeOffsets);

			// Note: As for distortion rendering the order doesn't matter we actually could iterate View.DynamicMeshElements without this indirection
			{

				// range in View.DynamicMeshElements[]
				FInt32Range range = View.GetDynamicMeshElementRange(PrimitiveSceneInfo->GetIndex());

				for (int32 MeshBatchIndex = range.GetLowerBoundValue(); MeshBatchIndex < range.GetUpperBoundValue(); MeshBatchIndex++)
				{
					const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

					checkSlow(MeshBatchAndRelevance.PrimitiveSceneProxy == PrimitiveSceneProxy);

					const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
					bDirty |= TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy>::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, DrawRenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
				}
			}

			// Render static scene prim
			if( ViewRelevance.bStaticRelevance )
			{
				// Render static meshes from static scene prim
				for( int32 StaticMeshIdx = 0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++ )
				{
					FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];
					if( View.StaticMeshVisibilityMap[StaticMesh.Id]
						// Only render static mesh elements using translucent materials
						&& StaticMesh.IsTranslucent(View.GetFeatureLevel()) )
					{
						bDirty |= TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy>::DrawStaticMesh(
							RHICmdList, 
							&View,
							bInitializeOffsets,
							StaticMesh,
							StaticMesh.bRequiresPerElementVisibility ? View.StaticMeshBatchVisibility[StaticMesh.BatchVisibilityId] : ((1ull << StaticMesh.Elements.Num()) - 1),
							DrawRenderState,
							PrimitiveSceneProxy,
							StaticMesh.BatchHitProxyId
							);
					}
				}
			}
		}
	}
	return bDirty;
}

int32 FSceneRenderer::GetRefractionQuality(const FSceneViewFamily& ViewFamily)
{
	static const auto ICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RefractionQuality"));
	
	int32 Value = 0;
	
	if(ViewFamily.EngineShowFlags.Refraction)
	{
		Value = ICVar->GetValueOnRenderThread();
	}
	
	return Value;
}

template <bool UseMSAA>
static void DrawDistortionApplyScreenPass(FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext, FViewInfo& View, IPooledRenderTarget& DistortionRT) {
	TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
	TShaderMapRef<TDistortionApplyScreenPS<UseMSAA>> PixelShader(View.ShaderMap);

	FRenderingCompositePassContext Context(RHICmdList, View);

	Context.SetViewportAndCallRHI(View.ViewRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// test against stencil mask
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
		false, CF_Always,
		true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		kStencilMaskBit, kStencilMaskBit>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	RHICmdList.SetStencilRef(kStencilMaskBit);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context, View, DistortionRT);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Size(),
		SceneContext.GetBufferSizeXY(),
		*VertexShader,
		EDRF_UseTriangleOptimization);
}

template <bool UseMSAA>
static void DrawDistortionMergePass(FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext, FViewInfo& View, const FTextureRHIParamRef& PassTexture) {
	TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
	TShaderMapRef<TDistortionMergePS<UseMSAA>> PixelShader(View.ShaderMap);

	FRenderingCompositePassContext Context(RHICmdList, View);

	Context.SetViewportAndCallRHI(View.ViewRect);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// test against stencil mask and clear it
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
		false, CF_Always,
		true, CF_Equal, SO_Keep, SO_Keep, SO_Zero,
		false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
		kStencilMaskBit, kStencilMaskBit>::GetRHI();

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
	RHICmdList.SetStencilRef(kStencilMaskBit);

	VertexShader->SetParameters(Context);
	PixelShader->SetParameters(Context, View, PassTexture);

	DrawRectangle(
		RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Size(),
		SceneContext.GetBufferSizeXY(),
		*VertexShader,
		EDRF_UseTriangleOptimization);
}

/** 
 * Renders the scene's distortion 
 */
void FSceneRenderer::RenderDistortion(FRHICommandListImmediate& RHICmdList)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion);
	SCOPED_DRAW_EVENT(RHICmdList, Distortion);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_Distortion);

	// do we need to render the distortion pass?
	bool bRender=false;
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		if( View.DistortionPrimSet.NumPrims() > 0)
		{
			bRender=true;
			break;
		}
	}

	bool bDirty = false;

	TRefCountPtr<IPooledRenderTarget> DistortionRT;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	uint32 MSAACount = SceneContext.SceneDepthZ->GetDesc().NumSamples;
	
	// Use stencil mask to optimize cases with lower screen coverage.
	// Note: This adds an extra pass which is actually slower as distortion tends towards full-screen.
	//       It could be worth testing object screen bounds then reverting to a target flip and single pass.

	// Render accumulated distortion offsets
	if( bRender)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_Render);
		SCOPED_DRAW_EVENT(RHICmdList, DistortionAccum);

		// Create a texture to store the resolved light attenuation values, and a render-targetable surface to hold the unresolved light attenuation values.
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_B8G8R8A8, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable, false));
			Desc.Flags |= GFastVRamConfig.Distortion;
			Desc.NumSamples = MSAACount;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DistortionRT, TEXT("Distortion"));

			// use RGBA8 light target for accumulating distortion offsets	
			// R = positive X offset
			// G = positive Y offset
			// B = negative X offset
			// A = negative Y offset
		}

		// DistortionRT==0 should never happen but better we don't crash
		if(DistortionRT)
		{
			FRHIRenderTargetView ColorView( DistortionRT->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore );
			FRHIDepthRenderTargetView DepthView( SceneContext.GetSceneDepthSurface(), ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction, FExclusiveDepthStencil::DepthRead_StencilWrite );
			FRHISetRenderTargetsInfo Info( 1, &ColorView, DepthView );		

			RHICmdList.SetRenderTargetsAndClear(Info);

			for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

				FViewInfo& View = Views[ViewIndex];
				// viewport to match view size
				RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);	

				FDrawingPolicyRenderState DrawRenderState(View);

				// test against depth and write stencil mask
				DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
					false, CF_DepthNearOrEqual,
					true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					kStencilMaskBit, kStencilMaskBit>::GetRHI());
				DrawRenderState.SetStencilRef(kStencilMaskBit);

				// additive blending of offsets (or complexity if the shader complexity viewmode is enabled)
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());

				// draw only distortion meshes to accumulate their offsets
				bDirty |= View.DistortionPrimSet.DrawAccumulatedOffsets(RHICmdList, View, DrawRenderState, false);
			}

			if (bDirty)
			{
				// Ideally we skip the EliminateFastClear since we don't need pixels with no stencil set to be cleared
				RHICmdList.TransitionResource( EResourceTransitionAccess::EReadable, DistortionRT->GetRenderTargetItem().TargetableTexture );
				// to be able to observe results with VisualizeTexture
				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, DistortionRT);
			}
		}
	}

	if(bDirty)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_Post);
		SCOPED_DRAW_EVENT(RHICmdList, DistortionApply);

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture);
		
		TRefCountPtr<IPooledRenderTarget> NewSceneColor;
		FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, NewSceneColor, TEXT("DistortedSceneColor"));
		const FSceneRenderTargetItem& DestRenderTarget = NewSceneColor->GetRenderTargetItem();

		// Apply distortion and store off-screen
		SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilRead);

		for(int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_PostView1);
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			if (MSAACount == 1)
			{
				DrawDistortionApplyScreenPass<false>(RHICmdList, SceneContext, View, *DistortionRT);
			}
			else
			{
				DrawDistortionApplyScreenPass<true>(RHICmdList, SceneContext, View, *DistortionRT);
			}
		}

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, DestRenderTarget.TargetableTexture);
		SetRenderTarget(RHICmdList, SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);

		for(int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_PostView2);
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			if (MSAACount == 1)
			{
				DrawDistortionMergePass<false>(RHICmdList, SceneContext, View, DestRenderTarget.TargetableTexture);
			}
			else
			{
				DrawDistortionMergePass<true>(RHICmdList, SceneContext, View, DestRenderTarget.TargetableTexture);
			}
		}
	}
}

void FSceneRenderer::RenderDistortionES2(FRHICommandListImmediate& RHICmdList)
{
	// We need access to HDR scene color
#ifndef UE4_HTML5_TARGET_WEBGL2
	if (!IsMobileHDR() || IsMobileHDRMosaic())
	{
		return;
	}
#endif

	// do we need to render the distortion pass?
	bool bRender=false;
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		if( View.DistortionPrimSet.NumPrims() > 0)
		{
			bRender=true;
			break;
		}
	}
	
	static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DisableDistortion"));
	int32 DisableDistortion = CVar->GetInt();
	
	if (bRender && !DisableDistortion)
	{
		// Apply distortion
		SCOPED_DRAW_EVENT(RHICmdList, Distortion);
		
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		RHICmdList.CopyToResolveTarget(SceneContext.GetSceneColorSurface(), SceneContext.GetSceneColorTexture(), true, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));

		TRefCountPtr<IPooledRenderTarget> SceneColorDistorted;
		FPooledRenderTargetDesc Desc = SceneContext.GetSceneColor()->GetDesc();
		Desc.Flags &= ~(TexCreate_FastVRAM | TexCreate_Transient);
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneColorDistorted, TEXT("SceneColorDistorted"));
		const FSceneRenderTargetItem& DistortedRenderTarget = SceneColorDistorted->GetRenderTargetItem();

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		SetRenderTarget(RHICmdList, DistortedRenderTarget.TargetableTexture, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EClearColorExistingDepth, FExclusiveDepthStencil::DepthRead_StencilNop);
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		// Copy scene color to a new render target
		for(int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			// useful when we move this into the compositing graph
			FRenderingCompositePassContext Context(RHICmdList, View);

			// Set the view family's render target/viewport.
			Context.SetViewportAndCallRHI(View.ViewRect);				

			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				
			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
			TShaderMapRef<TDistortionMergePS<false>> PixelShader(View.ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			VertexShader->SetParameters(Context);
			PixelShader->SetParameters(Context, View, SceneContext.GetSceneColor()->GetRenderTargetItem().ShaderResourceTexture);

			DrawRectangle(
				RHICmdList,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y, 
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Size(),
				SceneContext.GetBufferSizeXY(),
				*VertexShader,
				EDRF_UseTriangleOptimization);
		}

		// Distort scene color in place
		for(int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			// useful when we move this into the compositing graph
			FRenderingCompositePassContext Context(RHICmdList, View);

			// Set the view family's render target/viewport.
			Context.SetViewportAndCallRHI(View.ViewRect);

			FDrawingPolicyRenderState DrawRenderState(View);
			// test against depth
			DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

			// draw only distortion meshes
			View.DistortionPrimSet.DrawAccumulatedOffsets(RHICmdList, View, DrawRenderState, false);
		}
	
		// Set distorted scene color as main
		SceneContext.SetSceneColor(SceneColorDistorted);
	}
}
