/*=============================================================================
	NVVolumetricLightingRHI.h: Nvidia Volumetric Lighting Render Hardware Interface definitions.
=============================================================================*/

#pragma once

#if WITH_NVVOLUMETRICLIGHTING

DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("VolumetricLighting BeginAccumulation"), Stat_GPU_BeginAccumulation, STATGROUP_GPU, RHI_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("VolumetricLighting RenderVolume"), Stat_GPU_RenderVolume, STATGROUP_GPU, RHI_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("VolumetricLighting EndAccumulation"), Stat_GPU_EndAccumulation, STATGROUP_GPU, RHI_API);
DECLARE_FLOAT_COUNTER_STAT_EXTERN(TEXT("VolumetricLighting ApplyLighting"), Stat_GPU_ApplyLighting, STATGROUP_GPU, RHI_API);

/** The interface which is implemented by Nvidia Volumetric Lighting RHI. */
class RHI_API FNVVolumetricLightingRHI
{
public:
	FNVVolumetricLightingRHI();

	/** Declare a virtual destructor. */
	~FNVVolumetricLightingRHI() {}

	/** Initializes the RHI;*/
	bool Init();

	/** Shutdown the RHI; */
	void Shutdown();

	void UpdateContext(const NvVl::ContextDesc& InContextDesc);
	void ReleaseContext();
	void UpdateRendering(bool Enabled);

	// SeparateTranslucency
	void SetSeparateTranslucencyPostprocessDesc(const NvVl::PostprocessDesc& InPostprocessDesc);
	NvVl::PostprocessDesc* GetSeparateTranslucencyPostprocessDesc();

	// 
	bool IsMSAAEnabled() const { return ContextDesc.eInternalSampleMode == NvVl::MultisampleMode::MSAA2 || ContextDesc.eInternalSampleMode == NvVl::MultisampleMode::MSAA4; }
	bool IsTemporalFilterEnabled() const { return ContextDesc.eFilterMode == NvVl::FilterMode::TEMPORAL; }
	bool IsRendering() const { return bEnableRendering; }
	
	void BeginAccumulation(FTextureRHIParamRef SceneDepthTextureRHI, const TArray<NvVl::ViewerDesc>& ViewerDescs, const NvVl::MediumDesc& MediumDesc, NvVl::DebugFlags DebugFlags);
	void RenderVolume(const TArray<FTextureRHIParamRef>& ShadowMapTextures, const NvVl::ShadowMapDesc& ShadowMapDesc, const NvVl::LightDesc& LightDesc, const NvVl::VolumeDesc& VolumeDesc);
	void EndAccumulation();
	void ApplyLighting(FTextureRHIParamRef SceneColorSurfaceRHI, const NvVl::PostprocessDesc& PostprocessDesc);
private:

	bool bSupportedRHI;
	bool bEnableRendering;
	bool bEnableSeparateTranslucency;

	NvVl::ContextDesc		ContextDesc;
	NvVl::PlatformDesc		PlatformDesc;
	NvVl::Context			Context;
	NvVl::PostprocessDesc	SeparateTranslucencyPostprocessDesc;

	NvVl::PlatformRenderCtx	RenderCtx;

	NvVl::PlatformShaderResource SceneDepthSRV;
};

/** A global pointer to Nvidia Volumetric Lighting RHI implementation. */
extern RHI_API FNVVolumetricLightingRHI* GNVVolumetricLightingRHI;

extern RHI_API FNVVolumetricLightingRHI* CreateNVVolumetricLightingRHI();

#endif