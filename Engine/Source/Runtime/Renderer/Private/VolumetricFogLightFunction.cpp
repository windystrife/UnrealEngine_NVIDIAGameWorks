// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VolumetricFogLightFunction.cpp
=============================================================================*/

#include "VolumetricFog.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "LightRendering.h"
#include "SceneFilterRendering.h"
#include "PostProcessing.h"

float GVolumetricFogLightFunctionSupersampleScale = 2.0f;
FAutoConsoleVariableRef CVarVolumetricFogLightFunctionSupersampleScale(
	TEXT("r.VolumetricFog.LightFunctionSupersampleScale"),
	GVolumetricFogLightFunctionSupersampleScale,
	TEXT("Scales the slice depth distribution."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

class FVolumetricFogLightFunctionPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FVolumetricFogLightFunctionPS,Material);
public:

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsLightFunction() && DoesPlatformSupportVolumetricFog(Platform);
	}

	FVolumetricFogLightFunctionPS() {}
	FVolumetricFogLightFunctionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		LightFunctionParameters.Bind(Initializer.ParameterMap);
		LightFunctionWorldToLight.Bind(Initializer.ParameterMap,TEXT("LightFunctionWorldToLight"));
		LightFunctionParameters2.Bind(Initializer.ParameterMap,TEXT("LightFunctionParameters2"));
		LightFunctionTexelSize.Bind(Initializer.ParameterMap,TEXT("LightFunctionTexelSize"));
		ShadowToWorld.Bind(Initializer.ParameterMap,TEXT("ShadowToWorld"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		const FLightSceneInfo* LightSceneInfo, 
		const FMaterialRenderProxy* MaterialProxy,
		FVector2D LightFunctionTexelSizeValue,
		const FMatrix& ShadowToWorldValue)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, *MaterialProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, true, ESceneRenderTargetsMode::SetTextures);

		LightFunctionParameters.Set(RHICmdList, ShaderRHI, LightSceneInfo, 1.0f);

		SetShaderValue(RHICmdList, ShaderRHI, LightFunctionParameters2, FVector(
			LightSceneInfo->Proxy->GetLightFunctionFadeDistance(), 
			LightSceneInfo->Proxy->GetLightFunctionDisabledBrightness(),
			0.0f));

		if (LightFunctionWorldToLight.IsBound())
		{
			const FVector Scale = LightSceneInfo->Proxy->GetLightFunctionScale();
			// Switch x and z so that z of the user specified scale affects the distance along the light direction
			const FVector InverseScale = FVector( 1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X );
			const FMatrix WorldToLight = LightSceneInfo->Proxy->GetWorldToLight() * FScaleMatrix(FVector(InverseScale));	

			SetShaderValue(RHICmdList, ShaderRHI, LightFunctionWorldToLight, WorldToLight);
		}

		SetShaderValue(RHICmdList, ShaderRHI, LightFunctionTexelSize, LightFunctionTexelSizeValue);
		
		SetShaderValue(RHICmdList, ShaderRHI, ShadowToWorld, ShadowToWorldValue);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << LightFunctionParameters;
		Ar << LightFunctionParameters2;
		Ar << LightFunctionWorldToLight;
		Ar << LightFunctionTexelSize;
		Ar << ShadowToWorld;
		return bShaderHasOutdatedParameters;
	}

private:
	FLightFunctionSharedParameters LightFunctionParameters;
	FShaderParameter LightFunctionParameters2;
	FShaderParameter LightFunctionWorldToLight;
	FShaderParameter LightFunctionTexelSize;
	FShaderParameter ShadowToWorld;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FVolumetricFogLightFunctionPS,TEXT("/Engine/Private/VolumetricFogLightFunction.usf"),TEXT("Main"),SF_Pixel);

void FDeferredShadingSceneRenderer::RenderLightFunctionForVolumetricFog(
	FRHICommandListImmediate& RHICmdList, 
	FViewInfo& View, 
	FIntVector VolumetricFogGridSize,
	float VolumetricFogMaxDistance,
	FMatrix& OutLightFunctionWorldToShadow,
	TRefCountPtr<IPooledRenderTarget>& OutLightFunctionTexture,
	bool& bOutUseDirectionalLightShadowing)
{
	OutLightFunctionWorldToShadow = FMatrix::Identity;
	OutLightFunctionTexture = NULL;
	bOutUseDirectionalLightShadowing = true;

	FLightSceneInfo* DirectionalLightSceneInfo = NULL;

	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		if (ViewFamily.EngineShowFlags.LightFunctions 
			&& LightSceneInfo->Proxy->GetLightType() == LightType_Directional
			&& LightSceneInfo->ShouldRenderLightViewIndependent() 
			&& LightSceneInfo->ShouldRenderLight(View))
		{
			bOutUseDirectionalLightShadowing = LightSceneInfo->Proxy->CastsVolumetricShadow();

			if (CheckForLightFunction(LightSceneInfo))
			{
				DirectionalLightSceneInfo = LightSceneInfo;
				break;
			}
		}
	}

	if (DirectionalLightSceneInfo)
	{
		FProjectedShadowInfo ProjectedShadowInfo;
		FIntPoint LightFunctionResolution;

		{
			const FVector ViewForward = View.ViewMatrices.GetOverriddenTranslatedViewMatrix().GetColumn(2);
			const FVector ViewUp = View.ViewMatrices.GetOverriddenTranslatedViewMatrix().GetColumn(1);
			const FVector ViewRight = View.ViewMatrices.GetOverriddenTranslatedViewMatrix().GetColumn(0);

			const FVector LightDirection = DirectionalLightSceneInfo->Proxy->GetDirection().GetSafeNormal();
			FVector AxisWeights;
			AxisWeights.X = FMath::Abs(LightDirection | ViewRight) * VolumetricFogGridSize.X;
			AxisWeights.Y = FMath::Abs(LightDirection | ViewUp) * VolumetricFogGridSize.Y;
			AxisWeights.Z = FMath::Abs(LightDirection | ViewForward) * VolumetricFogGridSize.Z;

			const float VolumeResolutionEstimate = FMath::Max(AxisWeights.X, FMath::Max(AxisWeights.Y, AxisWeights.Z)) * GVolumetricFogLightFunctionSupersampleScale;
			LightFunctionResolution = FIntPoint(FMath::TruncToInt(VolumeResolutionEstimate), FMath::TruncToInt(VolumeResolutionEstimate));

			// Snap the resolution to allow render target pool hits most of the time
			const int32 ResolutionSnapFactor = 32;
			LightFunctionResolution.X = FMath::DivideAndRoundUp(LightFunctionResolution.X, ResolutionSnapFactor) * ResolutionSnapFactor;
			LightFunctionResolution.Y = FMath::DivideAndRoundUp(LightFunctionResolution.Y, ResolutionSnapFactor) * ResolutionSnapFactor;

			FWholeSceneProjectedShadowInitializer ShadowInitializer;

			check(VolumetricFogMaxDistance > 0);
			FSphere Bounds = DirectionalLightSceneInfo->Proxy->GetShadowSplitBoundsDepthRange(View, View.ViewMatrices.GetViewOrigin(), 0, VolumetricFogMaxDistance, NULL);
			check(Bounds.W > 0);

			const float ShadowExtent = Bounds.W / FMath::Sqrt(3.0f);
			const FBoxSphereBounds SubjectBounds(Bounds.Center, FVector(ShadowExtent, ShadowExtent, ShadowExtent), Bounds.W);
			ShadowInitializer.PreShadowTranslation = -Bounds.Center;
			ShadowInitializer.WorldToLight = FInverseRotationMatrix(LightDirection.Rotation());
			ShadowInitializer.Scales = FVector(1.0f,1.0f / Bounds.W,1.0f / Bounds.W);
			ShadowInitializer.FaceDirection = FVector(1,0,0);
			ShadowInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector,SubjectBounds.BoxExtent,SubjectBounds.SphereRadius);
			ShadowInitializer.WAxis = FVector4(0,0,0,1);
			ShadowInitializer.MinLightW = -HALF_WORLD_MAX;
			// Reduce casting distance on a directional light
			// This is necessary to improve floating point precision in several places, especially when deriving frustum verts from InvReceiverMatrix
			ShadowInitializer.MaxDistanceToCastInLightW = HALF_WORLD_MAX / 32.0f;
			ShadowInitializer.bRayTracedDistanceField = false;
			ShadowInitializer.CascadeSettings.bFarShadowCascade = false;

			ProjectedShadowInfo.SetupWholeSceneProjection(
				DirectionalLightSceneInfo,
				&View,
				ShadowInitializer,
				LightFunctionResolution.X,
				LightFunctionResolution.Y,
				0,
				false
				);
		}

		const FMaterialRenderProxy* MaterialProxy = DirectionalLightSceneInfo->Proxy->GetLightFunctionMaterial();

		if (MaterialProxy && MaterialProxy->GetMaterial(Scene->GetFeatureLevel())->IsLightFunction())
		{
			FPooledRenderTargetDesc LightFunctionTextureDesc(FPooledRenderTargetDesc::Create2DDesc(LightFunctionResolution, PF_G8, FClearValueBinding::None, TexCreate_None, TexCreate_ShaderResource | TexCreate_RenderTargetable, false));
			LightFunctionTextureDesc.Flags |= GFastVRamConfig.VolumetricFog;
			GRenderTargetPool.FindFreeElement(RHICmdList, LightFunctionTextureDesc, OutLightFunctionTexture, TEXT("VolumetricFogLightFunction"));
			
			const FMatrix WorldToShadowValue = FTranslationMatrix(ProjectedShadowInfo.PreShadowTranslation) * ProjectedShadowInfo.SubjectAndReceiverMatrix;
			OutLightFunctionWorldToShadow = WorldToShadowValue;

			const FMaterial* Material = MaterialProxy->GetMaterial(Scene->GetFeatureLevel());
			SCOPED_DRAW_EVENTF(RHICmdList, LightFunction, TEXT("LightFunction %ux%u Material=%s"), LightFunctionResolution.X, LightFunctionResolution.Y, *Material->GetFriendlyName());

			SetRenderTarget(RHICmdList, OutLightFunctionTexture->GetRenderTargetItem().TargetableTexture, NULL, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthNop_StencilNop, true);
			RHICmdList.SetViewport(0, 0, 0.0f, LightFunctionResolution.X, LightFunctionResolution.Y, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();
			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
			FVolumetricFogLightFunctionPS* PixelShader = MaterialShaderMap->GetShader<FVolumetricFogLightFunctionPS>();

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(PixelShader);

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

			VertexShader->SetParameters(RHICmdList, View.ViewUniformBuffer);
			PixelShader->SetParameters(RHICmdList, View, DirectionalLightSceneInfo, MaterialProxy, FVector2D(1.0f / LightFunctionResolution.X, 1.0f / LightFunctionResolution.Y), WorldToShadowValue.Inverse());

			DrawRectangle( 
				RHICmdList,
				0, 0, 
				LightFunctionResolution.X, LightFunctionResolution.Y,
				0, 0, 
				LightFunctionResolution.X, LightFunctionResolution.Y,
				LightFunctionResolution,
				LightFunctionResolution,
				*VertexShader);

			RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, OutLightFunctionTexture->GetRenderTargetItem().TargetableTexture);

			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, OutLightFunctionTexture);
		}
	}
}
