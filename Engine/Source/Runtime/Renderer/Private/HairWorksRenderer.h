// @third party code - BEGIN HairWorks
#pragma once

namespace HairWorksRenderer
{
	void StepSimulation(FRHICommandList& RHICmdList, const float CurrentWorldTime, const float DeltaWorldTime);
	void SetupViews(TArray<FViewInfo>& Views);
	bool ViewsHasHair(const TArray<FViewInfo>& Views);
	void AllocRenderTargets(FRHICommandList& RHICmdList, const FIntPoint& Size);
	void RenderBasePass(FRHICommandList& RHICmdList, TArray<FViewInfo>& Views);
	void RenderShadow(FRHICommandList& RHICmdList, const FProjectedShadowInfo& Shadow,const FProjectedShadowInfo::PrimitiveArrayType& SubjectPrimitives, const FViewInfo& View);
	void RenderVelocities(FRHICommandList& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT);
	void RenderVisualization(FRHICommandList& RHICmdList, const FViewInfo& View);
	void RenderHitProxies(FRHICommandList& RHICmdList, const TArray<FViewInfo>& Views);
	void RenderCustomStencil(FRHICommandList& RHICmdList, const FViewInfo& View);
	void RenderSelectionOutline(FRHICommandList& RHICmdList, const FViewInfo& View);
	void BeginRenderingSceneColor(FRHICommandList& RHICmdList);	// Add a render target for hair
	void BlendLightingColor(FRHICommandList& RHICmdList);
	bool IsLightAffectHair(const FLightSceneInfo& LightSceneInfo, const FViewInfo& View);

	static const int HairInstanceMaterialArraySize = 128;

	BEGIN_UNIFORM_BUFFER_STRUCT(FHairInstanceDataShaderUniform, )	// We would use array of structure instead of array of raw float in future.
		DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4, Spec0_SpecPower0_Spec1_SpecPower1, [HairInstanceMaterialArraySize])
		DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4, Spec1Offset_DiffuseBlend_ReceiveShadows_ShadowSigma, [HairInstanceMaterialArraySize])
		DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4, GlintStrength_LightingChannelMask, [HairInstanceMaterialArraySize])
	END_UNIFORM_BUFFER_STRUCT(FHairInstanceDataShaderUniform)

	class FRenderTargets
	{
	public:
		TRefCountPtr<IPooledRenderTarget> GBufferA;
		TRefCountPtr<IPooledRenderTarget> GBufferB;
		TRefCountPtr<IPooledRenderTarget> GBufferC;
		TRefCountPtr<IPooledRenderTarget> HairDepthZ;
		TRefCountPtr<IPooledRenderTarget> HairDepthZForShadow;
		TRefCountPtr<FRHIShaderResourceView> StencilSRV;
		TRefCountPtr<IPooledRenderTarget> LightAttenuation;
		TRefCountPtr<IPooledRenderTarget> VelocityBuffer;
		TRefCountPtr<IPooledRenderTarget> PrecomputedLight;
		TRefCountPtr<IPooledRenderTarget> AccumulatedColor;

		TUniformBufferRef<FHairInstanceDataShaderUniform> HairInstanceDataShaderUniform;
	};

	extern TSharedRef<FRenderTargets> HairRenderTargets;

	class FDeferredShadingParameters
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap);

		template<typename TRHICmdList, typename ShaderRHIParamRef>
		void SetParameters(TRHICmdList& RHICmdList, ShaderRHIParamRef& ShaderRHI, const FShader& Shader, bool bHairDeferredRendering)const;

		friend FArchive& operator<<(FArchive& Ar, FDeferredShadingParameters& P);

	private:
		FShaderParameter HairDeferredRendering;
		FShaderResourceParameter HairNearestDepthTexture;
		FShaderResourceParameter HairLightAttenuationTexture;
		FShaderResourceParameter HairGBufferATextureMS;
		FShaderResourceParameter HairGBufferBTextureMS;
		FShaderResourceParameter HairGBufferCTextureMS;
		FShaderResourceParameter HairPrecomputeLightTextureMS;
		FShaderResourceParameter HairDepthTextureMS;
		FShaderResourceParameter HairStencilTextureMS;
	};

	template<typename TRHICmdList, typename ShaderRHIParamRef>
	void FDeferredShadingParameters::SetParameters(TRHICmdList& RHICmdList, ShaderRHIParamRef& ShaderRHI, const FShader& Shader, bool bHairDeferredRendering) const
	{
		SetShaderValue(RHICmdList, ShaderRHI, HairDeferredRendering, bHairDeferredRendering);
		if(!bHairDeferredRendering)
			return;

		auto BindTexture = [&](const FShaderResourceParameter& Parameter, TRefCountPtr<IPooledRenderTarget>& Texture)
		{
			if(!Texture)
				return;

			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				Parameter,
				Texture->GetRenderTargetItem().TargetableTexture
			);
		};

		BindTexture(HairNearestDepthTexture, HairRenderTargets->HairDepthZForShadow);

		auto HairLightAttenuationTextureRHIRef = GWhiteTexture->TextureRHI;
		if(HairRenderTargets->LightAttenuation != nullptr)
		{
			HairLightAttenuationTextureRHIRef = HairRenderTargets->LightAttenuation->GetRenderTargetItem().TargetableTexture;
		}

		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			HairLightAttenuationTexture,
			HairLightAttenuationTextureRHIRef
		);
		BindTexture(HairGBufferATextureMS, HairRenderTargets->GBufferA);
		BindTexture(HairGBufferBTextureMS, HairRenderTargets->GBufferB);
		BindTexture(HairGBufferCTextureMS, HairRenderTargets->GBufferC);
		BindTexture(HairPrecomputeLightTextureMS, HairRenderTargets->PrecomputedLight);
		BindTexture(HairDepthTextureMS, HairRenderTargets->HairDepthZ);
		SetSRVParameter(
			RHICmdList,
			ShaderRHI,
			HairStencilTextureMS,
			HairRenderTargets->StencilSRV
		);

		SetUniformBufferParameter(RHICmdList, ShaderRHI, Shader.GetUniformBufferParameter<FHairInstanceDataShaderUniform>(), HairRenderTargets->HairInstanceDataShaderUniform);
	}
}
// @third party code - END HairWorks
