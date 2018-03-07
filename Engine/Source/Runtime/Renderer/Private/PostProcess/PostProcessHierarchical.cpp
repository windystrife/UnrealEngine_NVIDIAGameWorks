// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessHierarchical.cpp: Post processing Screen Space Reflections implementation.
=============================================================================*/

#include "PostProcess/PostProcessHierarchical.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "PipelineStateCache.h"

template< uint32 Stage >
class TPostProcessBuildHCBPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TPostProcessBuildHCBPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("STAGE"), Stage );
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_FloatRGBA);
	}

	TPostProcessBuildHCBPS() {}

public:
	FShaderParameter				InvSize;
	FShaderParameter				InputUvFactorAndOffset;
	FShaderParameter				InputUvBundaries;
	FPostProcessPassParameters		PostprocessParameter;
	FShaderResourceParameter		ColorMip;
	FShaderResourceParameter		ColorMipSampler;

	TPostProcessBuildHCBPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		InvSize.Bind( Initializer.ParameterMap, TEXT("InvSize") );
		InputUvFactorAndOffset.Bind( Initializer.ParameterMap, TEXT("InputUvFactorAndOffset") );
		InputUvBundaries.Bind( Initializer.ParameterMap, TEXT("InputUvBundaries") );
		PostprocessParameter.Bind( Initializer.ParameterMap );
		ColorMip.Bind( Initializer.ParameterMap, TEXT("ColorMip") );
		ColorMipSampler.Bind( Initializer.ParameterMap, TEXT("ColorMipSampler") );
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		
		const FIntPoint GBufferSize = SceneContext.GetBufferSizeXY();
		const FVector InvSizeValue( 1.0f / float(GBufferSize.X), 1.0f / float(GBufferSize.Y), 0.0 );
		const FVector4 InputUvFactorAndOffsetValue (
			float(2 * Context.View.HZBMipmap0Size.X) / float(GBufferSize.X),
			float(2 * Context.View.HZBMipmap0Size.Y) / float(GBufferSize.Y),
			float(Context.View.ViewRect.Min.X) / float(GBufferSize.X),
			float(Context.View.ViewRect.Min.Y) / float(GBufferSize.Y)
			);
		const FVector4 InputUvBundariesValue (
			float(Context.View.ViewRect.Min.X) / float(GBufferSize.X) + 0.5f * InvSizeValue.X,
			float(Context.View.ViewRect.Min.Y) / float(GBufferSize.Y) + 0.5f * InvSizeValue.Y,
			float(Context.View.ViewRect.Max.X) / float(GBufferSize.X) - 0.5f * InvSizeValue.X,
			float(Context.View.ViewRect.Max.Y) / float(GBufferSize.Y) - 0.5f * InvSizeValue.Y
			);
		SetShaderValue(RHICmdList, ShaderRHI, InvSize, InvSizeValue );
		SetShaderValue(RHICmdList, ShaderRHI, InputUvFactorAndOffset, InputUvFactorAndOffsetValue );
		SetShaderValue(RHICmdList, ShaderRHI, InputUvBundaries, InputUvBundariesValue );
		
		PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FIntPoint& Size, FShaderResourceViewRHIParamRef ShaderResourceView )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, View.ViewUniformBuffer);
		
		const FVector2D InvSizeValue( 1.0f / Size.X, 1.0f / Size.Y );
		const FVector4 InputUvBundariesValue (
			0.0f,
			0.0f,
			float(View.ViewRect.Size().X) / float(2 * View.HZBMipmap0Size.X) - 0.5f * InvSizeValue.X,
			float(View.ViewRect.Size().Y) / float(2 * View.HZBMipmap0Size.Y) - 0.5f * InvSizeValue.Y
			);
		SetShaderValue(RHICmdList, ShaderRHI, InvSize, InvSizeValue );
		SetShaderValue(RHICmdList, ShaderRHI, InputUvBundaries, InputUvBundariesValue );

		SetSRVParameter(RHICmdList, ShaderRHI, ColorMip, ShaderResourceView );
		SetSamplerParameter(RHICmdList, ShaderRHI, ColorMipSampler, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI() );
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InvSize;
		Ar << InputUvFactorAndOffset;
		Ar << InputUvBundaries;
		Ar << PostprocessParameter;
		Ar << ColorMip;
		Ar << ColorMipSampler;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(template<>,TPostProcessBuildHCBPS<0>,TEXT("/Engine/Private/PostProcessHierarchical.usf"),TEXT("BuildHCB"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TPostProcessBuildHCBPS<1>,TEXT("/Engine/Private/PostProcessHierarchical.usf"),TEXT("BuildHCB"),SF_Pixel);

static void HierarchycalSizeAndMips(const FIntPoint& BufferSize, uint32 Mip0Downsample, uint32& OutNumMips, FIntPoint& OutHierachicalSize)
{
	const uint32 NumMipsX = FPlatformMath::CeilLogTwo(BufferSize.X) - Mip0Downsample;
	const uint32 NumMipsY = FPlatformMath::CeilLogTwo(BufferSize.Y) - Mip0Downsample;

	OutNumMips = FMath::Max(NumMipsX, NumMipsY);
	OutHierachicalSize = FIntPoint(1 << NumMipsX, 1 << NumMipsY);
}

void FRCPassPostProcessBuildHCB::Process(FRenderingCompositePassContext& Context)
{
	FIntPoint HCBSize;
	uint32 HCBMipCount;
	HierarchycalSizeAndMips(FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY(), 1, HCBMipCount, HCBSize);
	
	auto& RHICmdList = Context.RHICmdList;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const FViewInfo& View = Context.View;
	const auto FeatureLevel = Context.GetFeatureLevel();

	SCOPED_DRAW_EVENT(RHICmdList, BuildHCB);
		
	FSceneRenderTargetItem HCBRenderTarget = PassOutputs[0].RequestSurface(Context);
	const FVector2D HCBUsedPercentage (
		float(View.ViewRect.Size().X) / float(2 * HCBSize.X),
		float(View.ViewRect.Size().Y) / float(2 * HCBSize.Y)
		);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

	{ // mip 0
		TShaderMapRef< FPostProcessVS > VertexShader(Context.GetShaderMap());
		TShaderMapRef< TPostProcessBuildHCBPS<0> > PixelShader(Context.GetShaderMap());
		
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetRenderTarget(RHICmdList, HCBRenderTarget.TargetableTexture, 0, NULL);
		Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);
		
		VertexShader->SetParameters(Context);
		PixelShader->SetParameters(Context.RHICmdList, Context);
			
			
#if 0
		const FIntPoint DstDrawSize (
			FMath::Min(FMath::CeilToInt(HCBUsedPercentage.X * float(HCBSize.X)) + 1, HCBSize.X),
			FMath::Min(FMath::CeilToInt(HCBUsedPercentage.Y * float(HCBSize.Y)) + 1, HCBSize.Y)
			);

		RHICmdList.SetViewport(0, 0, 0.0f, DstDrawSize.X, DstDrawSize.Y, 1.0f);
		
		DrawPostProcessPass(
			RHICmdList,
			0, 0,
			HCBSize.X, HCBSize.Y,
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			DstDrawSize,
			SceneContext.GetBufferSizeXY(),
			*VertexShader,
			View.StereoPass,
			Context.HasHmdMesh(),
			EDRF_UseTriangleOptimization);
#else
		RHICmdList.SetViewport(0, 0, 0.0f, HCBSize.X, HCBSize.Y, 1.0f);
			
		DrawPostProcessPass(
			RHICmdList,
			0, 0,
			HCBSize.X, HCBSize.Y,
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			HCBSize,
			SceneContext.GetBufferSizeXY(),
			*VertexShader,
			View.StereoPass,
			Context.HasHmdMesh(),
			EDRF_UseTriangleOptimization);
#endif

		RHICmdList.CopyToResolveTarget(HCBRenderTarget.TargetableTexture, HCBRenderTarget.ShaderResourceTexture, false, FResolveParams(FResolveRect(), CubeFace_PosX, 0));
	}
		
	FIntPoint SrcSize = HCBSize;
	FIntPoint DstSize = SrcSize / 2;
		
	// Downsampling...
	for( uint8 MipIndex = 1; MipIndex < HCBMipCount; MipIndex++ )
	{
		DstSize.X = FMath::Max(DstSize.X, 1);
		DstSize.Y = FMath::Max(DstSize.Y, 1);

		SetRenderTarget(RHICmdList, HCBRenderTarget.TargetableTexture, MipIndex, NULL);
		Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		TShaderMapRef< FPostProcessVS >	VertexShader(Context.GetShaderMap());
		TShaderMapRef< TPostProcessBuildHCBPS<1> >	PixelShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		PixelShader->SetParameters(RHICmdList, View, SrcSize, HCBRenderTarget.MipSRVs[ MipIndex - 1 ] );
			
#if 0
		const FIntPoint DstDrawSize (
			FMath::Min(FMath::CeilToInt(HCBUsedPercentage.X * float(DstSize.X)) + 1, DstSize.X),
			FMath::Min(FMath::CeilToInt(HCBUsedPercentage.Y * float(DstSize.Y)) + 1, DstSize.Y)
			);

		RHICmdList.SetViewport(0, 0, 0.0f, DstDrawSize.X, DstDrawSize.Y, 1.0f);

		DrawPostProcessPass(
			RHICmdList,
			0, 0,
			DstSize.X, DstSize.Y,
			0, 0,
			SrcSize.X, SrcSize.Y,
			DstDrawSize,
			SrcSize,
			*VertexShader,
			View.StereoPass,
			Context.HasHmdMesh(),
			EDRF_UseTriangleOptimization);
#else
		RHICmdList.SetViewport(0, 0, 0.0f, DstSize.X, DstSize.Y, 1.0f);

		DrawPostProcessPass(
			RHICmdList,
			0, 0,
			DstSize.X, DstSize.Y,
			0, 0,
			SrcSize.X, SrcSize.Y,
			DstSize,
			SrcSize,
			*VertexShader,
			View.StereoPass,
			Context.HasHmdMesh(),
			EDRF_UseTriangleOptimization);
#endif

		RHICmdList.CopyToResolveTarget(HCBRenderTarget.TargetableTexture, HCBRenderTarget.ShaderResourceTexture, false, FResolveParams(FResolveRect(), CubeFace_PosX, MipIndex));

		SrcSize /= 2;
		DstSize /= 2;
	}
}

FPooledRenderTargetDesc FRCPassPostProcessBuildHCB::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FIntPoint HCBSize;
	uint32 HCBMipCount;
	HierarchycalSizeAndMips(FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY(), 1, HCBMipCount, HCBSize);
	FPooledRenderTargetDesc Ret(FPooledRenderTargetDesc::Create2DDesc(HCBSize, PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_NoFastClear, false, HCBMipCount));
	
	Ret.DebugName = TEXT("HCB");

	return Ret;
}
