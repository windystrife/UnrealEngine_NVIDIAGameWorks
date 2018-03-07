
/*=============================================================================
	NVVolumetricLightingRHI.cpp: Nvidia Volumetric Lighting rendering implementation.
=============================================================================*/

#if WITH_NVVOLUMETRICLIGHTING
#include "NVVolumetricLightingRHI.h"
#include "RHI.h"

void* NvVlModuleHandle = nullptr;

DEFINE_STAT(Stat_GPU_BeginAccumulation);
DEFINE_STAT(Stat_GPU_RenderVolume);
DEFINE_STAT(Stat_GPU_EndAccumulation);
DEFINE_STAT(Stat_GPU_ApplyLighting);

FNVVolumetricLightingRHI* GNVVolumetricLightingRHI = nullptr;

static TAutoConsoleVariable<int32> CVarNvVl(
	TEXT("r.NvVl"),
	1,
	TEXT("0 to disable volumetric lighting feature. Restart required"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

FNVVolumetricLightingRHI* CreateNVVolumetricLightingRHI()
{
	if (CVarNvVl.GetValueOnGameThread() == false)
	{
		return nullptr;
	}

	return new FNVVolumetricLightingRHI();
}

FNVVolumetricLightingRHI::FNVVolumetricLightingRHI()
	: Context(nullptr)
	, RenderCtx(nullptr)
	, SceneDepthSRV(nullptr)
	, bSupportedRHI(false)
	, bEnableRendering(false)
	, bEnableSeparateTranslucency(false)
{
}

bool FNVVolumetricLightingRHI::Init()
{
	FString NVVolumetricLightingBinariesPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/GameWorks/NvVolumetricLighting/");
#if PLATFORM_64BITS
#if UE_BUILD_DEBUG && !defined(NDEBUG)
	NvVlModuleHandle = FPlatformProcess::GetDllHandle(*(NVVolumetricLightingBinariesPath + "NvVolumetricLighting.win64.D.dll"));
#else
	NvVlModuleHandle = FPlatformProcess::GetDllHandle(*(NVVolumetricLightingBinariesPath + "NvVolumetricLighting.win64.dll"));
#endif
#endif
	check(NvVlModuleHandle);

	if (NvVlModuleHandle == nullptr)
	{
		UE_LOG(LogRHI, Fatal, TEXT("Failed to load module '%s'."), *NVVolumetricLightingBinariesPath);
		return false;
	}
	else
	{
		NvVl::OpenLibrary();

		FMemory::Memzero(ContextDesc);
		ContextDesc.bReversedZ = true;

		check(GDynamicRHI);
		bSupportedRHI = GDynamicRHI->GetPlatformDesc(PlatformDesc);
		GDynamicRHI->GetPlatformRenderCtx(RenderCtx);
	}

	return true;	
}

void FNVVolumetricLightingRHI::ReleaseContext()
{
	if (Context)
	{
		NvVl::ReleaseContext(Context);
		Context = nullptr;
	}
}

void FNVVolumetricLightingRHI::Shutdown()
{
	ReleaseContext();

	NvVl::CloseLibrary();

	FPlatformProcess::FreeDllHandle(NvVlModuleHandle);
	NvVlModuleHandle = nullptr;
}

void FNVVolumetricLightingRHI::UpdateContext(const NvVl::ContextDesc& InContextDesc)
{
	if (Context == nullptr || FMemory::Memcmp(&InContextDesc, &ContextDesc, sizeof(NvVl::ContextDesc)) != 0)
	{
		ContextDesc = InContextDesc;

		ReleaseContext();

		NvVl::Status NvVlStatus = NvVl::CreateContext(Context, &PlatformDesc, &ContextDesc);
		check(NvVlStatus == NvVl::Status::OK);
	}
}

void FNVVolumetricLightingRHI::BeginAccumulation(FTextureRHIParamRef SceneDepthTextureRHI, const TArray<NvVl::ViewerDesc>& ViewerDescs, const NvVl::MediumDesc& MediumDesc, NvVl::DebugFlags DebugFlags)
{
	GDynamicRHI->GetPlatformShaderResource(SceneDepthTextureRHI, SceneDepthSRV);
	const NvVl::ViewerDesc* ppViewerDesc[2] = { nullptr };
	for(int32 i = 0; i < ViewerDescs.Num(); i++)
	{
		ppViewerDesc[i] = &(ViewerDescs[i]);
	}
	NvVl::Status Status = NvVl::BeginAccumulation(Context, RenderCtx, SceneDepthSRV, ppViewerDesc, &MediumDesc, DebugFlags);
	check(Status == NvVl::Status::OK);
	GDynamicRHI->ClearStateCache();
}

void FNVVolumetricLightingRHI::RenderVolume(const TArray<FTextureRHIParamRef>& ShadowMapTextures, const NvVl::ShadowMapDesc& ShadowMapDesc, const NvVl::LightDesc& LightDesc, const NvVl::VolumeDesc& VolumeDesc)
{
	NvVl::PlatformShaderResource ShadowMapSRVs[NvVl::MAX_SHADOWMAP_ELEMENTS] = {
		NvVl::PlatformShaderResource(nullptr),
		NvVl::PlatformShaderResource(nullptr),
		NvVl::PlatformShaderResource(nullptr),
		NvVl::PlatformShaderResource(nullptr),
	};

	for (int Index = 0; Index < NvVl::MAX_SHADOWMAP_ELEMENTS; Index++)
	{
		if (Index < ShadowMapTextures.Num() && ShadowMapTextures[Index])
		{
			GDynamicRHI->GetPlatformShaderResource(ShadowMapTextures[Index], ShadowMapSRVs[Index]);
		}
	}
	
	NvVl::Status Status = NvVl::RenderVolume(Context, RenderCtx, ShadowMapSRVs, &ShadowMapDesc, &LightDesc, &VolumeDesc);
	check(Status == NvVl::Status::OK);
	GDynamicRHI->ClearStateCache();
}

void FNVVolumetricLightingRHI::EndAccumulation()
{
	NvVl::Status Status = NvVl::EndAccumulation(Context, RenderCtx);
	check(Status == NvVl::Status::OK);
}

void FNVVolumetricLightingRHI::ApplyLighting(FTextureRHIParamRef SceneColorSurfaceRHI, const NvVl::PostprocessDesc& PostprocessDesc)
{
	NvVl::PlatformRenderTarget SceneRTV(nullptr);
	GDynamicRHI->GetPlatformRenderTarget(SceneColorSurfaceRHI, SceneRTV);
	NvVl::Status Status = NvVl::ApplyLighting(Context, RenderCtx, SceneRTV, SceneDepthSRV, &PostprocessDesc);
	check(Status == NvVl::Status::OK);
	GDynamicRHI->ClearStateCache();
}

void FNVVolumetricLightingRHI::SetSeparateTranslucencyPostprocessDesc(const NvVl::PostprocessDesc& InPostprocessDesc)
{
	bEnableSeparateTranslucency = true;
	SeparateTranslucencyPostprocessDesc = InPostprocessDesc;
}

NvVl::PostprocessDesc* FNVVolumetricLightingRHI::GetSeparateTranslucencyPostprocessDesc()
{
	if (bEnableSeparateTranslucency)
	{
		return &SeparateTranslucencyPostprocessDesc;
	}
	else
	{
		return nullptr;
	}
}

void FNVVolumetricLightingRHI::UpdateRendering(bool Enabled)
{
	bEnableSeparateTranslucency = false;
	bEnableRendering = bSupportedRHI && Enabled;
}
#endif