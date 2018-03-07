// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightRendering.h: Light rendering declarations.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShadowRendering.h"
#include "SceneRendering.h"

template<typename ShaderRHIParamRef>
void SetSimpleDeferredLightParameters(
	FRHICommandList& RHICmdList, 
	const ShaderRHIParamRef ShaderRHI, 
	const TShaderUniformBufferParameter<FDeferredLightUniformStruct>& DeferredLightUniformBufferParameter, 
	const FSimpleLightEntry& SimpleLight,
	const FSimpleLightPerViewEntry &SimpleLightPerViewData,
	const FSceneView& View)
{
	FDeferredLightUniformStruct DeferredLightUniformsValue;
	DeferredLightUniformsValue.LightPosition = SimpleLightPerViewData.Position;
	DeferredLightUniformsValue.LightInvRadius = 1.0f / FMath::Max(SimpleLight.Radius, KINDA_SMALL_NUMBER);
	DeferredLightUniformsValue.LightColor = SimpleLight.Color;
	DeferredLightUniformsValue.LightFalloffExponent = SimpleLight.Exponent;
	DeferredLightUniformsValue.NormalizedLightDirection = FVector(1, 0, 0);
	DeferredLightUniformsValue.NormalizedLightTangent = FVector(1, 0, 0);
	DeferredLightUniformsValue.SpotAngles = FVector2D(-2, 1);
	DeferredLightUniformsValue.SourceRadius = 0.0f;
	DeferredLightUniformsValue.SoftSourceRadius = 0.0f;
	DeferredLightUniformsValue.SourceLength = 0.0f;
	DeferredLightUniformsValue.MinRoughness = 0.08f;
	DeferredLightUniformsValue.ContactShadowLength = 0.0f;
	DeferredLightUniformsValue.DistanceFadeMAD = FVector2D(0, 0);
	DeferredLightUniformsValue.ShadowMapChannelMask = FVector4(0, 0, 0, 0);
	DeferredLightUniformsValue.ShadowedBits = 0;
	DeferredLightUniformsValue.LightingChannelMask = 0;

	SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, DeferredLightUniformBufferParameter, DeferredLightUniformsValue);
}

/** Shader parameters needed to render a light function. */
class FLightFunctionSharedParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		LightFunctionParameters.Bind(ParameterMap,TEXT("LightFunctionParameters"));
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FLightSceneInfo* LightSceneInfo, float ShadowFadeFraction) const
	{
		const bool bIsSpotLight = LightSceneInfo->Proxy->GetLightType() == LightType_Spot;
		const bool bIsPointLight = LightSceneInfo->Proxy->GetLightType() == LightType_Point;
		const float TanOuterAngle = bIsSpotLight ? FMath::Tan(LightSceneInfo->Proxy->GetOuterConeAngle()) : 1.0f;

		SetShaderValue( 
			RHICmdList, 
			ShaderRHI, 
			LightFunctionParameters, 
			FVector4(TanOuterAngle, ShadowFadeFraction, bIsSpotLight ? 1.0f : 0.0f, bIsPointLight ? 1.0f : 0.0f));
	}

	/** Serializer. */ 
	friend FArchive& operator<<(FArchive& Ar,FLightFunctionSharedParameters& P)
	{
		Ar << P.LightFunctionParameters;
		return Ar;
	}

private:

	FShaderParameter LightFunctionParameters;
};

/** A vertex shader for rendering the light in a deferred pass. */
template<bool bRadialLight>
class TDeferredLightVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDeferredLightVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return bRadialLight ? IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) : true;
	}

	TDeferredLightVS()	{}
	TDeferredLightVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		StencilingGeometryParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FLightSceneInfo* LightSceneInfo)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(),View.ViewUniformBuffer);
		StencilingGeometryParameters.Set(RHICmdList, this, View, LightSceneInfo);
	}

	void SetSimpleLightParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FSphere& LightBounds)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(),View.ViewUniformBuffer);

		FVector4 StencilingSpherePosAndScale;
		StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(StencilingSpherePosAndScale, LightBounds, View.ViewMatrices.GetPreViewTranslation());
		StencilingGeometryParameters.Set(RHICmdList, this, StencilingSpherePosAndScale);
	}

	//~ Begin FShader Interface
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << StencilingGeometryParameters;
		return bShaderHasOutdatedParameters;
	}
	//~ End FShader Interface
private:
	FStencilingGeometryShaderParameters StencilingGeometryParameters;
};

extern void SetBoundingGeometryRasterizerAndDepthState(FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FSphere& LightBounds);

