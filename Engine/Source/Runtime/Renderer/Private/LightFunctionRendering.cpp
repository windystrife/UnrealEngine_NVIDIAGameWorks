// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightFunctionRendering.cpp: Implementation for rendering light functions.
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "EngineGlobals.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "MaterialShaderType.h"
#include "SceneRenderTargetParameters.h"
#include "MaterialShader.h"
#include "ShadowRendering.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "LightRendering.h"
#include "PipelineStateCache.h"
// @third party code - BEGIN HairWorks
#include "HairWorksRenderer.h"
// @third party code - END HairWorks

/**
 * A vertex shader for projecting a light function onto the scene.
 */
class FLightFunctionVS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FLightFunctionVS,Material);
public:

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsLightFunction' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsLightFunction() && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FLightFunctionVS( )	{ }
	FLightFunctionVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		StencilingGeometryParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FLightSceneInfo* LightSceneInfo )
	{
		FMaterialShader::SetViewParameters(RHICmdList, GetVertexShader(), View, View.ViewUniformBuffer);
		
		// Light functions are projected using a bounding sphere.
		// Calculate transform for bounding stencil sphere.
		FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();
		if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
		{
			LightBounds.Center = View.ViewMatrices.GetViewOrigin();
		}

		FVector4 StencilingSpherePosAndScale;
		StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(StencilingSpherePosAndScale, LightBounds, View.ViewMatrices.GetPreViewTranslation());
		StencilingGeometryParameters.Set(RHICmdList, this, StencilingSpherePosAndScale);
	}

	// Begin FShader interface
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << StencilingGeometryParameters;
		return bShaderHasOutdatedParameters;
	}
	//  End FShader interface 

private:
	FStencilingGeometryShaderParameters StencilingGeometryParameters;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FLightFunctionVS,TEXT("/Engine/Private/LightFunctionVertexShader.usf"),TEXT("Main"),SF_Vertex);

/**
 * A pixel shader for projecting a light function onto the scene.
 */
class FLightFunctionPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FLightFunctionPS,Material);
public:

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsLightFunction' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsLightFunction() && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FLightFunctionPS() {}
	FLightFunctionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		SvPositionToLight.Bind(Initializer.ParameterMap,TEXT("SvPositionToLight"));
		LightFunctionParameters.Bind(Initializer.ParameterMap);
		LightFunctionParameters2.Bind(Initializer.ParameterMap,TEXT("LightFunctionParameters2"));
		DeferredParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FLightSceneInfo* LightSceneInfo, const FMaterialRenderProxy* MaterialProxy, bool bRenderingPreviewShadowIndicator, float ShadowFadeFraction )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, *MaterialProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, true, ESceneRenderTargetsMode::SetTextures);

		// Set the transform from screen space to light space.
		if ( SvPositionToLight.IsBound() )
		{
			const FVector Scale = LightSceneInfo->Proxy->GetLightFunctionScale();
			// Switch x and z so that z of the user specified scale affects the distance along the light direction
			const FVector InverseScale = FVector( 1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X );
			const FMatrix WorldToLight = LightSceneInfo->Proxy->GetWorldToLight() * FScaleMatrix(FVector(InverseScale));	

			FVector2D InvViewSize = FVector2D(1.0f / View.ViewRect.Width(), 1.0f / View.ViewRect.Height());

			// setup a matrix to transform float4(SvPosition.xyz,1) directly to Light (quality, performance as we don't need to convert or use interpolator)

			//	new_xy = (xy - ViewRectMin.xy) * ViewSizeAndInvSize.zw * float2(2,-2) + float2(-1, 1);

			//  transformed into one MAD:  new_xy = xy * ViewSizeAndInvSize.zw * float2(2,-2)      +       (-ViewRectMin.xy) * ViewSizeAndInvSize.zw * float2(2,-2) + float2(-1, 1);

			float Mx = 2.0f * InvViewSize.X;
			float My = -2.0f * InvViewSize.Y;
			float Ax = -1.0f - 2.0f * View.ViewRect.Min.X * InvViewSize.X;
			float Ay = 1.0f + 2.0f * View.ViewRect.Min.Y * InvViewSize.Y;

			// todo: we could use InvTranslatedViewProjectionMatrix and TranslatedWorldToLight for better quality
			const FMatrix SvPositionToLightValue = 
				FMatrix(
					FPlane(Mx,  0,   0,  0),
					FPlane( 0, My,   0,  0),
					FPlane( 0,  0,   1,  0),
					FPlane(Ax, Ay,   0,  1)
				) * View.ViewMatrices.GetInvViewProjectionMatrix() * WorldToLight;

			SetShaderValue(RHICmdList, ShaderRHI, SvPositionToLight, SvPositionToLightValue );
		}

		LightFunctionParameters.Set(RHICmdList, ShaderRHI, LightSceneInfo, ShadowFadeFraction);

		SetShaderValue(RHICmdList, ShaderRHI, LightFunctionParameters2, FVector(
			LightSceneInfo->Proxy->GetLightFunctionFadeDistance(), 
			LightSceneInfo->Proxy->GetLightFunctionDisabledBrightness(),
			bRenderingPreviewShadowIndicator ? 1.0f : 0.0f));

		DeferredParameters.Set(RHICmdList, ShaderRHI, View, MD_LightFunction);

		auto DeferredLightParameter = GetUniformBufferParameter<FDeferredLightUniformStruct>();

		if (DeferredLightParameter.IsBound())
		{
			SetDeferredLightParameters(RHICmdList, ShaderRHI, DeferredLightParameter, LightSceneInfo, View);
		}
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << SvPositionToLight;
		Ar << LightFunctionParameters;
		Ar << LightFunctionParameters2;
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter SvPositionToLight;
	FLightFunctionSharedParameters LightFunctionParameters;
	FShaderParameter LightFunctionParameters2;
	FDeferredPixelShaderParameters DeferredParameters;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FLightFunctionPS,TEXT("/Engine/Private/LightFunctionPixelShader.usf"),TEXT("Main"),SF_Pixel);

/** Returns a fade fraction for a light function and a given view based on the appropriate fade settings. */
static float GetLightFunctionFadeFraction(const FViewInfo& View, FSphere LightBounds)
{
	extern float CalculateShadowFadeAlpha(const float MaxUnclampedResolution, const uint32 ShadowFadeResolution, const uint32 MinShadowResolution);

	// Override the global settings with the light's settings if the light has them specified
	static auto CVarMinShadowResolution = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Shadow.MinResolution"));
	static auto CVarShadowFadeResolution = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Shadow.FadeResolution"));

	const uint32 MinShadowResolution  = FMath::Max<int32>(0, CVarMinShadowResolution->GetValueOnRenderThread());
	const uint32 ShadowFadeResolution = FMath::Max<int32>(0, CVarShadowFadeResolution->GetValueOnRenderThread());

	// Project the bounds onto the view
	const FVector4 ScreenPosition = View.WorldToScreen(LightBounds.Center);

	int32 SizeX = View.ViewRect.Width();
	int32 SizeY = View.ViewRect.Height();
	const float ScreenRadius = FMath::Max(
		SizeX / 2.0f * View.ViewMatrices.GetProjectionMatrix().M[0][0],
		SizeY / 2.0f * View.ViewMatrices.GetProjectionMatrix().M[1][1]) *
		LightBounds.W /
		FMath::Max(ScreenPosition.W, 1.0f);

	static auto CVarShadowTexelsPerPixel = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.TexelsPerPixel"));
	const float UnclampedResolution = ScreenRadius * CVarShadowTexelsPerPixel->GetValueOnRenderThread();
	const float ResolutionFadeAlpha = CalculateShadowFadeAlpha(UnclampedResolution, ShadowFadeResolution, MinShadowResolution);
	return ResolutionFadeAlpha;
}

/**
* Used by RenderLights to figure out if light functions need to be rendered to the attenuation buffer.
*
* @param LightSceneInfo Represents the current light
* @return true if anything got rendered
*/
bool FDeferredShadingSceneRenderer::CheckForLightFunction( const FLightSceneInfo* LightSceneInfo ) const
{
	// NOTE: The extra check is necessary because there could be something wrong with the material.
	if( LightSceneInfo->Proxy->GetLightFunctionMaterial() && 
		LightSceneInfo->Proxy->GetLightFunctionMaterial()->GetMaterial(Scene->GetFeatureLevel())->IsLightFunction())
	{
		FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();
		for (int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
			{
				LightBounds.Center = View.ViewMatrices.GetViewOrigin();
			}

			if(View.VisibleLightInfos[LightSceneInfo->Id].bInViewFrustum
				// Only draw the light function if it hasn't completely faded out
				&& GetLightFunctionFadeFraction(View, LightBounds) > 1.0f / 256.0f)
			{
				return true;
			}
		}
	}
	return false;
}

/**
 * Used by RenderLights to render a light function to the attenuation buffer.
 *
 * @param LightSceneInfo Represents the current light
 */
bool FDeferredShadingSceneRenderer::RenderLightFunction(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture, bool bLightAttenuationCleared, bool bProjectingForForwardShading)
{
	if (ViewFamily.EngineShowFlags.LightFunctions)
	{
		return RenderLightFunctionForMaterial(RHICmdList, LightSceneInfo, ScreenShadowMaskTexture, LightSceneInfo->Proxy->GetLightFunctionMaterial(), bLightAttenuationCleared, bProjectingForForwardShading, false);
	}
	
	return false;
}

bool FDeferredShadingSceneRenderer::RenderPreviewShadowsIndicator(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, IPooledRenderTarget* ScreenShadowMaskTexture, bool bLightAttenuationCleared)
{
	if (GEngine->PreviewShadowsIndicatorMaterial)
	{
		return RenderLightFunctionForMaterial(RHICmdList, LightSceneInfo, ScreenShadowMaskTexture, GEngine->PreviewShadowsIndicatorMaterial->GetRenderProxy(false), bLightAttenuationCleared, false, true);
	}

	return false;
}

bool FDeferredShadingSceneRenderer::RenderLightFunctionForMaterial(
	FRHICommandListImmediate& RHICmdList, 
	const FLightSceneInfo* LightSceneInfo, 
	IPooledRenderTarget* ScreenShadowMaskTexture,
	const FMaterialRenderProxy* MaterialProxy, 
	bool bLightAttenuationCleared, 
	bool bProjectingForForwardShading, 
	bool bRenderingPreviewShadowsIndicator)
{
	bool bRenderedLightFunction = false;

	check(ScreenShadowMaskTexture);
	if (MaterialProxy && MaterialProxy->GetMaterial(Scene->GetFeatureLevel())->IsLightFunction() )
	{
		// @third party code - BEGIN HairWorks
		bool bHairPass = false;

RenderForHair:
		if(bHairPass)
		{
			FSceneRenderTargets::Get(RHICmdList).SceneDepthZ.Swap(HairWorksRenderer::HairRenderTargets->HairDepthZForShadow);
			ScreenShadowMaskTexture = HairWorksRenderer::HairRenderTargets->LightAttenuation;
		}
		// @third party code - END HairWorks

		SetRenderTarget(RHICmdList, ScreenShadowMaskTexture->GetRenderTargetItem().TargetableTexture, FSceneRenderTargets::Get(RHICmdList).GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);

		bRenderedLightFunction = true;

		const FMaterial* Material = MaterialProxy->GetMaterial(Scene->GetFeatureLevel());
		SCOPED_DRAW_EVENTF(RHICmdList, LightFunction, TEXT("LightFunction Material=%s"), *Material->GetFriendlyName());

		const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();
		FLightFunctionVS* VertexShader = MaterialShaderMap->GetShader<FLightFunctionVS>();
		FLightFunctionPS* PixelShader = MaterialShaderMap->GetShader<FLightFunctionPS>();

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader->GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader->GetPixelShader();

		FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

		// Render to the light attenuation buffer for all views.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			const FViewInfo& View = Views[ViewIndex];

			if (View.VisibleLightInfos[LightSceneInfo->Id].bInViewFrustum)
			{
				if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
				{
					LightBounds.Center = View.ViewMatrices.GetViewOrigin();
				}

				const float FadeAlpha = GetLightFunctionFadeFraction(View, LightBounds);
				// Don't draw the light function if it has completely faded out
				if (FadeAlpha < 1.0f / 256.0f)
				{
					if( !bLightAttenuationCleared )
					{
						LightSceneInfo->Proxy->SetScissorRect(RHICmdList, View);
						FSceneRenderTargets::Get(RHICmdList).BeginRenderingLightAttenuation(RHICmdList, true);
					}
				}
				else
				{
					// @third party code - BEGIN HairWorks
					if(bHairPass && !HairWorksRenderer::IsLightAffectHair(*LightSceneInfo, View))
						continue;
					// @third party code - END HairWorks

					// Set the device viewport for the view.
					RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

					// Set the states to modulate the light function with the render target.
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false,CF_Always>::GetRHI();
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					if( bLightAttenuationCleared )
					{
						if (bRenderingPreviewShadowsIndicator)
						{
							GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA,BO_Max,BF_One,BF_One,BO_Max,BF_One,BF_One>::GetRHI();
						}
						else
						{
							FProjectedShadowInfo::SetBlendStateForProjection(
								GraphicsPSOInit,
								LightSceneInfo->GetDynamicShadowMapChannel(), 
								false,
								false,
								bProjectingForForwardShading, 
								false);
						}
					}
					else
					{
						GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA>::GetRHI();
					}

					if (((FVector)View.ViewMatrices.GetViewOrigin() - LightBounds.Center).SizeSquared() < FMath::Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f))
					{
						// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light function geometry
						GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
					}
					else
					{
						// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light function geometry
						GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false,CF_DepthNearOrEqual>::GetRHI();
						GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
					}

					// Set the light's scissor rectangle.
					LightSceneInfo->Proxy->SetScissorRect(RHICmdList, View);

					// Render a bounding light sphere.
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
					VertexShader->SetParameters(RHICmdList, View, LightSceneInfo);
					PixelShader->SetParameters(RHICmdList, View, LightSceneInfo, MaterialProxy, bRenderingPreviewShadowsIndicator, FadeAlpha);

					// Project the light function using a sphere around the light
					//@todo - could use a cone for spotlights
					StencilingGeometry::DrawSphere(RHICmdList);
				}
			}
		}

		// Restore states.
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

		// @third party code - BEGIN HairWorks
		if(bHairPass)
		{
			FSceneRenderTargets::Get(RHICmdList).SceneDepthZ.Swap(HairWorksRenderer::HairRenderTargets->HairDepthZForShadow);
		}

		if(!bHairPass && HairWorksRenderer::ViewsHasHair(Views))
		{
			bHairPass = true;
			goto RenderForHair;
		}
		// @third party code - END HairWorks
	}
	return bRenderedLightFunction;
}
