// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHI.cpp: Metal device RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "RenderUtils.h"
#if PLATFORM_IOS
#include "IOSAppDelegate.h"
#elif PLATFORM_MAC
#include "MacApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#endif
#include "ShaderCache.h"
#include "MetalProfiler.h"
#include "GenericPlatformDriver.h"

DEFINE_LOG_CATEGORY(LogMetal)

bool GMetalSupportsHeaps = false;
bool GMetalSupportsIndirectArgumentBuffers = false;
bool GMetalSupportsTileShaders = false;
bool GMetalSupportsStoreActionOptions = false;
bool GMetalSupportsDepthClipMode = false;
bool GMetalCommandBufferHasStartEndTimeAPI = false;

static void ValidateTargetedRHIFeatureLevelExists(EShaderPlatform Platform)
{
	bool bSupportsShaderPlatform = false;
#if PLATFORM_MAC
	TArray<FString> TargetedShaderFormats;
	GConfig->GetArray(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);
	
	for (FString Name : TargetedShaderFormats)
	{
		FName ShaderFormatName(*Name);
		if (ShaderFormatToLegacyShaderPlatform(ShaderFormatName) == Platform)
		{
			bSupportsShaderPlatform = true;
			break;
		}
	}
#else
	if (Platform == SP_METAL)
	{
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsShaderPlatform, GEngineIni);
	}
	else if (Platform == SP_METAL_MRT)
	{
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsShaderPlatform, GEngineIni);
	}
#endif
	
	if (!bSupportsShaderPlatform && !WITH_EDITOR)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ShaderPlatform"), FText::FromString(LegacyShaderPlatformToShaderFormat(Platform).ToString()));
		FText LocalizedMsg = FText::Format(NSLOCTEXT("MetalRHI", "ShaderPlatformUnavailable","Shader platform: {ShaderPlatform} was not cooked! Please enable this shader platform in the project's target settings."),Args);
		
		FText Title = NSLOCTEXT("MetalRHI", "ShaderPlatformUnavailableTitle","Shader Platform Unavailable");
		FMessageDialog::Open(EAppMsgType::Ok, LocalizedMsg, &Title);
		FPlatformMisc::RequestExit(true);
		
		UE_LOG(LogMetal, Fatal, TEXT("Shader platform: %s was not cooked! Please enable this shader platform in the project's target settings."), *LegacyShaderPlatformToShaderFormat(Platform).ToString());
	}
}

bool FMetalDynamicRHIModule::IsSupported()
{
	return true;
}

FDynamicRHI* FMetalDynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)
{
	return new FMetalDynamicRHI(RequestedFeatureLevel);
}

IMPLEMENT_MODULE(FMetalDynamicRHIModule, MetalRHI);

FMetalDynamicRHI::FMetalDynamicRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)
: ImmediateContext(nullptr, FMetalDeviceContext::CreateDeviceContext())
, AsyncComputeContext(nullptr)
{
	@autoreleasepool {
	// This should be called once at the start 
	check( IsInGameThread() );
	check( !GIsThreadedRendering );
	
	// @todo Zebra This is now supported on all GPUs in Mac Metal, but not on iOS.
	// we cannot render to a volume texture without geometry shader support
	GSupportsVolumeTextureRendering = false;
	
	// Metal always needs a render target to render with fragment shaders!
	// GRHIRequiresRenderTargetForPixelShaderUAVs = true;

	//@todo-rco: Query name from API
	GRHIAdapterName = TEXT("Metal");
	GRHIVendorId = 1; // non-zero to avoid asserts
	
	bool const bRequestedFeatureLevel = (RequestedFeatureLevel != ERHIFeatureLevel::Num);
	bool bSupportsPointLights = false;
	bool bSupportsRHIThread = false;
	
#if PLATFORM_IOS
	// get the device to ask about capabilities
	id<MTLDevice> Device = [IOSAppDelegate GetDelegate].IOSView->MetalDevice;
	// A8 can use 256 bits of MRTs
#if PLATFORM_TVOS
	bool bCanUseWideMRTs = true;
	bool bCanUseASTC = true;
#else
	bool bCanUseWideMRTs = [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1];
	bool bCanUseASTC = [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1] && !FParse::Param(FCommandLine::Get(),TEXT("noastc"));
#endif

    bool bProjectSupportsMRTs = false;
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bProjectSupportsMRTs, GEngineIni);
	
	bool const bRequestedMetalMRT = ((RequestedFeatureLevel >= ERHIFeatureLevel::SM4) || (!bRequestedFeatureLevel && FParse::Param(FCommandLine::Get(),TEXT("metalmrt"))));
	
    // only allow GBuffers, etc on A8s (A7s are just not going to cut it)
    if (bProjectSupportsMRTs && bCanUseWideMRTs && bRequestedMetalMRT)
    {
		ValidateTargetedRHIFeatureLevelExists(SP_METAL_MRT);
		
        GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
        GMaxRHIShaderPlatform = SP_METAL_MRT;
		
		bSupportsRHIThread = FParse::Param(FCommandLine::Get(),TEXT("rhithread"));
    }
    else
	{
		if (bRequestedMetalMRT)
		{
			UE_LOG(LogMetal, Warning, TEXT("Metal MRT support requires an iOS or tvOS device with an A8 processor or later. Falling back to Metal ES 3.1."));
		}
		
		ValidateTargetedRHIFeatureLevelExists(SP_METAL);
		
        GMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;
        GMaxRHIShaderPlatform = SP_METAL;
	}
		
	FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
		
	MemoryStats.DedicatedVideoMemory = 0;
	MemoryStats.TotalGraphicsMemory = Stats.AvailablePhysical;
	MemoryStats.DedicatedSystemMemory = 0;
	MemoryStats.SharedSystemMemory = Stats.AvailablePhysical;
	
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_METAL;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_METAL;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4) ? GMaxRHIShaderPlatform : SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4) ? GMaxRHIShaderPlatform : SP_NumPlatforms;

#else // @todo zebra
    // get the device to ask about capabilities?
    id<MTLDevice> Device = ImmediateContext.Context->GetDevice();
	uint32 DeviceIndex = ((FMetalDeviceContext*)ImmediateContext.Context)->GetDeviceIndex();
	
	TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = FPlatformMisc::GetGPUDescriptors();
	check(DeviceIndex < GPUs.Num());
	FMacPlatformMisc::FGPUDescriptor const& GPUDesc = GPUs[DeviceIndex];
	
    // A8 can use 256 bits of MRTs
    bool bCanUseWideMRTs = true;
    bool bCanUseASTC = false;
	bool bSupportsD24S8 = false;
	bool bSupportsD16 = false;
	
	GRHIAdapterName = FString(Device.name);
	
	// However they don't all support other features depending on the version of the OS.
	bool bSupportsTiledReflections = false;
	bool bSupportsDistanceFields = false;
	
	// Default is SM5 on:
	// 10.11.6 for AMD/Nvidia
	// 10.12.2+ for AMD/Nvidia
	// 10.12.4+ for Intel
	bool bSupportsSM5 = ((FPlatformMisc::MacOSXVersionCompare(10,11,6) == 0) || (FPlatformMisc::MacOSXVersionCompare(10,12,2) >= 0));
	if(GRHIAdapterName.Contains("Nvidia"))
	{
		bSupportsPointLights = true;
		GRHIVendorId = 0x10DE;
		bSupportsTiledReflections = true;
		bSupportsDistanceFields = (FPlatformMisc::MacOSXVersionCompare(10,11,4) >= 0);
		bSupportsRHIThread = (FPlatformMisc::MacOSXVersionCompare(10,12,0) >= 0);
	}
	else if(GRHIAdapterName.Contains("ATi") || GRHIAdapterName.Contains("AMD"))
	{
		bSupportsPointLights = true;
		GRHIVendorId = 0x1002;
		if((FPlatformMisc::MacOSXVersionCompare(10,12,0) < 0) && GPUDesc.GPUVendorId == GRHIVendorId)
		{
			GRHIAdapterName = FString(GPUDesc.GPUName);
		}
		bSupportsTiledReflections = true;
		bSupportsDistanceFields = (FPlatformMisc::MacOSXVersionCompare(10,11,4) >= 0);
		bSupportsRHIThread = true;
	}
	else if(GRHIAdapterName.Contains("Intel"))
	{
		bSupportsTiledReflections = false;
		bSupportsPointLights = (FPlatformMisc::MacOSXVersionCompare(10,11,4) >= 0);
		GRHIVendorId = 0x8086;
		bSupportsRHIThread = true;
		bSupportsDistanceFields = (FPlatformMisc::MacOSXVersionCompare(10,12,2) >= 0);
		// Only for 10.12.4 and later...
		bSupportsSM5 = (FPlatformMisc::MacOSXVersionCompare(10,12,4) >= 0);
	}

	bool const bRequestedSM5 = (RequestedFeatureLevel == ERHIFeatureLevel::SM5 || (!bRequestedFeatureLevel && (FParse::Param(FCommandLine::Get(),TEXT("metalsm5")) || FParse::Param(FCommandLine::Get(),TEXT("metalmrt")))));
	if(bSupportsSM5 && bRequestedSM5)
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
		if (!FParse::Param(FCommandLine::Get(),TEXT("metalmrt")))
		{
			GMaxRHIShaderPlatform = SP_METAL_SM5;
		}
		else
		{
			GMaxRHIShaderPlatform = SP_METAL_MRT_MAC;
		}
	}
	else
	{
		if (bRequestedSM5)
		{
			UE_LOG(LogMetal, Warning, TEXT("Metal Shader Model 5 support requires Mac OS X El Capitan 10.11.6 or later & an AMD or Nvidia GPU, or 10.12.4 or later for Intel. Falling back to Metal Shader Model 4."));
		}
	
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM4;
		GMaxRHIShaderPlatform = SP_METAL_SM4;
	}

	ERHIFeatureLevel::Type PreviewFeatureLevel;
	if (RHIGetPreviewFeatureLevel(PreviewFeatureLevel))
	{
		check(PreviewFeatureLevel == ERHIFeatureLevel::ES2 || PreviewFeatureLevel == ERHIFeatureLevel::ES3_1);

		// ES2/3.1 feature level emulation
		GMaxRHIFeatureLevel = PreviewFeatureLevel;
		if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2)
		{
			GMaxRHIShaderPlatform = SP_METAL_MACES2;
		}
		else if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1)
		{
			GMaxRHIShaderPlatform = SP_METAL_MACES3_1;
		}
	}

	ValidateTargetedRHIFeatureLevelExists(GMaxRHIShaderPlatform);
	
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_METAL_MACES2;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::ES3_1) ? SP_METAL_MACES3_1 : SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4) ? SP_METAL_SM4 : SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5) ? GMaxRHIShaderPlatform : SP_NumPlatforms;
	
	// Mac GPUs support layer indexing.
	GSupportsVolumeTextureRendering = (GMaxRHIShaderPlatform != SP_METAL_MRT_MAC);
	bSupportsPointLights &= (GMaxRHIShaderPlatform != SP_METAL_MRT_MAC);
	
	// Make sure the vendors match - the assumption that order in IORegistry is the order in Metal may not hold up forever.
	if(GPUDesc.GPUVendorId == GRHIVendorId)
	{
		GRHIDeviceId = GPUDesc.GPUDeviceId;
		MemoryStats.DedicatedVideoMemory = GPUDesc.GPUMemoryMB * 1024 * 1024;
		MemoryStats.TotalGraphicsMemory = GPUDesc.GPUMemoryMB * 1024 * 1024;
		MemoryStats.DedicatedSystemMemory = 0;
		MemoryStats.SharedSystemMemory = 0;
	}
	
	// Change the support depth format if we can
	bSupportsD24S8 = Device.depth24Stencil8PixelFormatSupported;
	
	// Disable tiled reflections on Mac Metal for some GPU drivers that ignore the lod-level and so render incorrectly.
	if (!bSupportsTiledReflections && !FParse::Param(FCommandLine::Get(),TEXT("metaltiledreflections")))
	{
		static auto CVarDoTiledReflections = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DoTiledReflections"));
		if(CVarDoTiledReflections && CVarDoTiledReflections->GetInt() != 0)
		{
			CVarDoTiledReflections->Set(0);
		}
	}
	
	// Disable the distance field AO & shadowing effects on GPU drivers that don't currently execute the shaders correctly.
	if (GMaxRHIShaderPlatform == SP_METAL_SM5 && !bSupportsDistanceFields && !FParse::Param(FCommandLine::Get(),TEXT("metaldistancefields")))
	{
		static auto CVarDistanceFieldAO = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFieldAO"));
		if(CVarDistanceFieldAO && CVarDistanceFieldAO->GetInt() != 0)
		{
			CVarDistanceFieldAO->Set(0);
		}
		
		static auto CVarDistanceFieldShadowing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFieldShadowing"));
		if(CVarDistanceFieldShadowing && CVarDistanceFieldShadowing->GetInt() != 0)
		{
			CVarDistanceFieldShadowing->Set(0);
		}
	}
	
#endif

	if (FApplePlatformMisc::IsOSAtLeastVersion((uint32[]){10, 13, 0}, (uint32[]){11, 0, 0}, (uint32[]){11, 0, 0}))
	{
		GMetalSupportsIndirectArgumentBuffers = true;
		GMetalSupportsStoreActionOptions = true;
	}
	if (!PLATFORM_MAC && FApplePlatformMisc::IsOSAtLeastVersion((uint32[]){0, 0, 0}, (uint32[]){11, 0, 0}, (uint32[]){11, 0, 0}))
	{
		GMetalSupportsTileShaders = true;
	}
	if (FApplePlatformMisc::IsOSAtLeastVersion((uint32[]){10, 11, 0}, (uint32[]){11, 0, 0}, (uint32[]){11, 0, 0}))
	{
		GMetalSupportsDepthClipMode = true;
	}
	if (FApplePlatformMisc::IsOSAtLeastVersion((uint32[]){10, 13, 0}, (uint32[]){10, 3, 0}, (uint32[]){10, 3, 0}))
	{
		GMetalCommandBufferHasStartEndTimeAPI = true;
	}

	GPoolSizeVRAMPercentage = 0;
	GTexturePoolSize = 0;
	GConfig->GetInt(TEXT("TextureStreaming"), TEXT("PoolSizeVRAMPercentage"), GPoolSizeVRAMPercentage, GEngineIni);
	if ( GPoolSizeVRAMPercentage > 0 && MemoryStats.TotalGraphicsMemory > 0 )
	{
		float PoolSize = float(GPoolSizeVRAMPercentage) * 0.01f * float(MemoryStats.TotalGraphicsMemory);
		
		// Truncate GTexturePoolSize to MB (but still counted in bytes)
		GTexturePoolSize = int64(FGenericPlatformMath::TruncToFloat(PoolSize / 1024.0f / 1024.0f)) * 1024 * 1024;
		
		UE_LOG(LogRHI,Log,TEXT("Texture pool is %llu MB (%d%% of %llu MB)"),
			   GTexturePoolSize / 1024 / 1024,
			   GPoolSizeVRAMPercentage,
			   MemoryStats.TotalGraphicsMemory);
	}
		
	GRHISupportsRHIThread = false;
	if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
	{
#if METAL_SUPPORTS_PARALLEL_RHI_EXECUTE
#if WITH_EDITORONLY_DATA
		GRHISupportsRHIThread = false;
#else
		GRHISupportsRHIThread = bSupportsRHIThread;
#endif
		GRHISupportsParallelRHIExecute = GRHISupportsRHIThread;
#endif
		GSupportsEfficientAsyncCompute = GRHISupportsParallelRHIExecute && (IsRHIDeviceAMD() || PLATFORM_IOS); // Only AMD currently support async. compute and it requires parallel execution to be useful.
		GSupportsParallelOcclusionQueries = GRHISupportsRHIThread;
		
		// We must always use an intermediate back-buffer for the RHI thread to work properly at present.
		if(GRHISupportsRHIThread)
		{
			static auto CVarSupportsIntermediateBackBuffer = IConsoleManager::Get().FindConsoleVariable(TEXT("rhi.Metal.SupportsIntermediateBackBuffer"));
			if(CVarSupportsIntermediateBackBuffer && CVarSupportsIntermediateBackBuffer->GetInt() != 1)
			{
				CVarSupportsIntermediateBackBuffer->Set(1);
			}
		}
	}
	else
	{
		GRHISupportsParallelRHIExecute = false;
		GSupportsEfficientAsyncCompute = false;
		GSupportsParallelOcclusionQueries = false;
	}

	if (FPlatformMisc::IsDebuggerPresent() && UE_BUILD_DEBUG)
	{
#if PLATFORM_IOS // @todo zebra : needs a RENDER_API or whatever
		// Enable GL debug markers if we're running in Xcode
		extern int32 GEmitMeshDrawEvent;
		GEmitMeshDrawEvent = 1;
#endif
		GEmitDrawEvents = true;
	}
	
	// Force disable vertex-shader-layer point light rendering on GPUs that don't support it properly yet.
	if(!bSupportsPointLights && !FParse::Param(FCommandLine::Get(),TEXT("metalpointlights")))
	{
		// Disable point light cubemap shadows on Mac Metal as currently they aren't supported.
		static auto CVarCubemapShadows = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowPointLightCubemapShadows"));
		if(CVarCubemapShadows && CVarCubemapShadows->GetInt() != 0)
		{
			CVarCubemapShadows->Set(0);
		}
	}
	
	if (!GSupportsVolumeTextureRendering && !FParse::Param(FCommandLine::Get(),TEXT("metaltlv")))
	{
		// Disable point light cubemap shadows on Mac Metal as currently they aren't supported.
		static auto CVarTranslucentLightingVolume = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TranslucentLightingVolume"));
		if(CVarTranslucentLightingVolume && CVarTranslucentLightingVolume->GetInt() != 0)
		{
			CVarTranslucentLightingVolume->Set(0);
		}
	}
	
	GEmitDrawEvents |= ENABLE_METAL_GPUEVENTS;

	GSupportsShaderFramebufferFetch = !PLATFORM_MAC;
	GHardwareHiddenSurfaceRemoval = true;
	GSupportsRenderTargetFormat_PF_G8 = false;
	GSupportsQuads = false;
	GRHISupportsTextureStreaming = true;
	GSupportsWideMRT = bCanUseWideMRTs;

	GRHIRequiresEarlyBackBufferRenderTarget = false;
	GSupportsSeparateRenderTargetBlendState = (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4);

#if PLATFORM_MAC
	check([Device supportsFeatureSet:MTLFeatureSet_OSX_GPUFamily1_v1]);
	GRHISupportsBaseVertexIndex = FPlatformMisc::MacOSXVersionCompare(10,11,2) >= 0 || !IsRHIDeviceAMD(); // Supported on macOS & iOS but not tvOS - broken on AMD prior to 10.11.2
	GRHISupportsFirstInstance = true; // Supported on macOS & iOS but not tvOS.
	GMaxTextureDimensions = 16384;
	GMaxCubeTextureDimensions = 16384;
	GMaxTextureArrayLayers = 2048;
	GMaxShadowDepthBufferSizeX = 16384;
	GMaxShadowDepthBufferSizeY = 16384;
    bSupportsD16 = !FParse::Param(FCommandLine::Get(),TEXT("nometalv2")) && [Device supportsFeatureSet:MTLFeatureSet_OSX_GPUFamily1_v2];
    GRHISupportsHDROutput = ((!GIsEditor || FPlatformMisc::MacOSXVersionCompare(10,13,0) >= 0) ? [Device supportsFeatureSet:MTLFeatureSet_OSX_GPUFamily1_v2] : false);
#else
#if PLATFORM_TVOS
	GRHISupportsBaseVertexIndex = false;
	GRHISupportsFirstInstance = false; // Supported on macOS & iOS but not tvOS.
#else
	GRHISupportsBaseVertexIndex = [Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1];
	GRHISupportsFirstInstance = GRHISupportsBaseVertexIndex;
#endif
	GMaxTextureDimensions = 4096;
	GMaxCubeTextureDimensions = 4096;
	GMaxTextureArrayLayers = 2048;
	GMaxShadowDepthBufferSizeX = 4096;
	GMaxShadowDepthBufferSizeY = 4096;
#endif
	
	GMaxTextureMipCount = FPlatformMath::CeilLogTwo( GMaxTextureDimensions ) + 1;
	GMaxTextureMipCount = FPlatformMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );

	// Initialize the platform pixel format map.
	GPixelFormats[PF_Unknown			].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_A32B32G32R32F		].PlatformFormat	= MTLPixelFormatRGBA32Float;
	GPixelFormats[PF_B8G8R8A8			].PlatformFormat	= MTLPixelFormatBGRA8Unorm;
	GPixelFormats[PF_G8					].PlatformFormat	= MTLPixelFormatR8Unorm;
	GPixelFormats[PF_G16				].PlatformFormat	= MTLPixelFormatR16Unorm;
	GPixelFormats[PF_R32G32B32A32_UINT	].PlatformFormat	= MTLPixelFormatRGBA32Uint;
	GPixelFormats[PF_R16G16_UINT		].PlatformFormat	= MTLPixelFormatRG16Uint;
		
#if PLATFORM_IOS
    GPixelFormats[PF_DXT1				].PlatformFormat	= MTLPixelFormatInvalid;
    GPixelFormats[PF_DXT3				].PlatformFormat	= MTLPixelFormatInvalid;
    GPixelFormats[PF_DXT5				].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_PVRTC2				].PlatformFormat	= MTLPixelFormatPVRTC_RGBA_2BPP;
	GPixelFormats[PF_PVRTC2				].Supported			= true;
	GPixelFormats[PF_PVRTC4				].PlatformFormat	= MTLPixelFormatPVRTC_RGBA_4BPP;
	GPixelFormats[PF_PVRTC4				].Supported			= true;
	GPixelFormats[PF_PVRTC4				].PlatformFormat	= MTLPixelFormatPVRTC_RGBA_4BPP;
	GPixelFormats[PF_PVRTC4				].Supported			= true;
	GPixelFormats[PF_ASTC_4x4			].PlatformFormat	= MTLPixelFormatASTC_4x4_LDR;
	GPixelFormats[PF_ASTC_4x4			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_6x6			].PlatformFormat	= MTLPixelFormatASTC_6x6_LDR;
	GPixelFormats[PF_ASTC_6x6			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_8x8			].PlatformFormat	= MTLPixelFormatASTC_8x8_LDR;
	GPixelFormats[PF_ASTC_8x8			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_10x10			].PlatformFormat	= MTLPixelFormatASTC_10x10_LDR;
	GPixelFormats[PF_ASTC_10x10			].Supported			= bCanUseASTC;
	GPixelFormats[PF_ASTC_12x12			].PlatformFormat	= MTLPixelFormatASTC_12x12_LDR;
	GPixelFormats[PF_ASTC_12x12			].Supported			= bCanUseASTC;
		
#if !PLATFORM_TVOS
	if (![Device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v2])
	{
		GPixelFormats[PF_FloatRGB			].PlatformFormat 	= MTLPixelFormatRGBA16Float;
		GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
		GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= MTLPixelFormatRGBA16Float;
		GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 8;
	}
	else
#endif
	{
		GPixelFormats[PF_FloatRGB			].PlatformFormat	= MTLPixelFormatRG11B10Float;
		GPixelFormats[PF_FloatRGB			].BlockBytes		= 4;
		GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= MTLPixelFormatRG11B10Float;
		GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
	}
	
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesStencilView) && FMetalCommandQueue::SupportsFeature(EMetalFeaturesCombinedDepthStencil) && !FParse::Param(FCommandLine::Get(),TEXT("metalforceseparatedepthstencil")))
	{
		GPixelFormats[PF_DepthStencil		].PlatformFormat	= MTLPixelFormatDepth32Float_Stencil8;
		GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
	}
	else
	{
		GPixelFormats[PF_DepthStencil		].PlatformFormat	= MTLPixelFormatDepth32Float;
		GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
	}
	GPixelFormats[PF_ShadowDepth		].PlatformFormat	= MTLPixelFormatDepth32Float;
	GPixelFormats[PF_ShadowDepth		].BlockBytes		= 4;
		
	GPixelFormats[PF_BC5				].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= MTLPixelFormatB5G6R5Unorm;
#else // @todo zebra : srgb?
    GPixelFormats[PF_DXT1				].PlatformFormat	= MTLPixelFormatBC1_RGBA;
    GPixelFormats[PF_DXT3				].PlatformFormat	= MTLPixelFormatBC2_RGBA;
    GPixelFormats[PF_DXT5				].PlatformFormat	= MTLPixelFormatBC3_RGBA;
	
	GPixelFormats[PF_FloatRGB			].PlatformFormat	= MTLPixelFormatRG11B10Float;
	GPixelFormats[PF_FloatRGB			].BlockBytes		= 4;
	GPixelFormats[PF_FloatR11G11B10		].PlatformFormat	= MTLPixelFormatRG11B10Float;
	GPixelFormats[PF_FloatR11G11B10		].BlockBytes		= 4;
		
	// Use Depth28_Stencil8 when it is available for consistency
	if(bSupportsD24S8)
	{
		GPixelFormats[PF_DepthStencil	].PlatformFormat	= MTLPixelFormatDepth24Unorm_Stencil8;
	}
	else
	{
		GPixelFormats[PF_DepthStencil	].PlatformFormat	= MTLPixelFormatDepth32Float_Stencil8;
	}
	GPixelFormats[PF_DepthStencil		].BlockBytes		= 4;
	if (bSupportsD16)
	{
		GPixelFormats[PF_ShadowDepth		].PlatformFormat	= MTLPixelFormatDepth16Unorm;
		GPixelFormats[PF_ShadowDepth		].BlockBytes		= 2;
	}
	else
	{
		GPixelFormats[PF_ShadowDepth		].PlatformFormat	= MTLPixelFormatDepth32Float;
		GPixelFormats[PF_ShadowDepth		].BlockBytes		= 4;
	}
	if(bSupportsD24S8)
	{
		GPixelFormats[PF_D24			].PlatformFormat	= MTLPixelFormatDepth24Unorm_Stencil8;
	}
	else
	{
		GPixelFormats[PF_D24			].PlatformFormat	= MTLPixelFormatDepth32Float;
	}
	GPixelFormats[PF_D24				].Supported			= true;
	GPixelFormats[PF_BC4				].Supported			= true;
	GPixelFormats[PF_BC4				].PlatformFormat	= MTLPixelFormatBC4_RUnorm;
	GPixelFormats[PF_BC5				].Supported			= true;
	GPixelFormats[PF_BC5				].PlatformFormat	= MTLPixelFormatBC5_RGUnorm;
	GPixelFormats[PF_BC6H				].Supported			= true;
	GPixelFormats[PF_BC6H               ].PlatformFormat	= MTLPixelFormatBC6H_RGBUfloat;
	GPixelFormats[PF_BC7				].Supported			= true;
	GPixelFormats[PF_BC7				].PlatformFormat	= MTLPixelFormatBC7_RGBAUnorm;
	GPixelFormats[PF_R5G6B5_UNORM		].PlatformFormat	= MTLPixelFormatInvalid;
#endif
	GPixelFormats[PF_UYVY				].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_FloatRGBA			].PlatformFormat	= MTLPixelFormatRGBA16Float;
	GPixelFormats[PF_FloatRGBA			].BlockBytes		= 8;
    GPixelFormats[PF_X24_G8				].PlatformFormat	= MTLPixelFormatStencil8;
    GPixelFormats[PF_X24_G8				].BlockBytes		= 1;
	GPixelFormats[PF_R32_FLOAT			].PlatformFormat	= MTLPixelFormatR32Float;
	GPixelFormats[PF_G16R16				].PlatformFormat	= MTLPixelFormatRG16Unorm;
	GPixelFormats[PF_G16R16				].Supported			= true;
	GPixelFormats[PF_G16R16F			].PlatformFormat	= MTLPixelFormatRG16Float;
	GPixelFormats[PF_G16R16F_FILTER		].PlatformFormat	= MTLPixelFormatRG16Float;
	GPixelFormats[PF_G32R32F			].PlatformFormat	= MTLPixelFormatRG32Float;
	GPixelFormats[PF_A2B10G10R10		].PlatformFormat    = MTLPixelFormatRGB10A2Unorm;
	GPixelFormats[PF_A16B16G16R16		].PlatformFormat    = MTLPixelFormatRGBA16Unorm;
	GPixelFormats[PF_R16F				].PlatformFormat	= MTLPixelFormatR16Float;
	GPixelFormats[PF_R16F_FILTER		].PlatformFormat	= MTLPixelFormatR16Float;
	GPixelFormats[PF_V8U8				].PlatformFormat	= MTLPixelFormatRG8Snorm;
	GPixelFormats[PF_A1					].PlatformFormat	= MTLPixelFormatInvalid;
	GPixelFormats[PF_A8					].PlatformFormat	= MTLPixelFormatA8Unorm;
	GPixelFormats[PF_R32_UINT			].PlatformFormat	= MTLPixelFormatR32Uint;
	GPixelFormats[PF_R32_SINT			].PlatformFormat	= MTLPixelFormatR32Sint;
	GPixelFormats[PF_R16G16B16A16_UINT	].PlatformFormat	= MTLPixelFormatRGBA16Uint;
	GPixelFormats[PF_R16G16B16A16_SINT	].PlatformFormat	= MTLPixelFormatRGBA16Sint;
	GPixelFormats[PF_R8G8B8A8			].PlatformFormat	= MTLPixelFormatRGBA8Unorm;
	GPixelFormats[PF_R8G8B8A8_UINT		].PlatformFormat	= MTLPixelFormatRGBA8Uint;
	GPixelFormats[PF_R8G8B8A8_SNORM		].PlatformFormat	= MTLPixelFormatRGBA8Snorm;
	GPixelFormats[PF_R8G8				].PlatformFormat	= MTLPixelFormatRG8Unorm;
	GPixelFormats[PF_R16_SINT			].PlatformFormat	= MTLPixelFormatR16Sint;
	GPixelFormats[PF_R16_UINT			].PlatformFormat	= MTLPixelFormatR16Uint;
	GPixelFormats[PF_R8_UINT			].PlatformFormat	= MTLPixelFormatR8Uint;

	// get driver version (todo: share with other RHIs)
	{
		FGPUDriverInfo GPUDriverInfo = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);
		
		GRHIAdapterUserDriverVersion = GPUDriverInfo.UserDriverVersion;
		GRHIAdapterInternalDriverVersion = GPUDriverInfo.InternalDriverVersion;
		GRHIAdapterDriverDate = GPUDriverInfo.DriverDate;
		
		UE_LOG(LogMetal, Display, TEXT("    Adapter Name: %s"), *GRHIAdapterName);
		UE_LOG(LogMetal, Display, TEXT("  Driver Version: %s (internal:%s, unified:%s)"), *GRHIAdapterUserDriverVersion, *GRHIAdapterInternalDriverVersion, *GPUDriverInfo.GetUnifiedDriverVersion());
		UE_LOG(LogMetal, Display, TEXT("     Driver Date: %s"), *GRHIAdapterDriverDate);
		UE_LOG(LogMetal, Display, TEXT("          Vendor: %s"), *GPUDriverInfo.ProviderName);
#if PLATFORM_MAC
		if(GPUDesc.GPUVendorId == GRHIVendorId)
		{
			UE_LOG(LogMetal, Display,  TEXT("      Vendor ID: %d"), GPUDesc.GPUVendorId);
			UE_LOG(LogMetal, Display,  TEXT("      Device ID: %d"), GPUDesc.GPUDeviceId);
			UE_LOG(LogMetal, Display,  TEXT("      VRAM (MB): %d"), GPUDesc.GPUMemoryMB);
		}
		else
		{
			UE_LOG(LogMetal, Warning,  TEXT("GPU descriptor (%s) from IORegistry failed to match Metal (%s)"), *FString(GPUDesc.GPUName), *GRHIAdapterName);
		}
#endif
	}

	((FMetalDeviceContext&)ImmediateContext.GetInternalContext()).Init();
		
	GDynamicRHI = this;

	// Without optimisation the shader loading can be so slow we mustn't attempt to preload all the shaders at load.
	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.Optimize"));
	if (CVar->GetInt() == 0)
	{
		FShaderCache::InitShaderCache(SCO_NoShaderPreload, GMaxRHIShaderPlatform);
		ImmediateContext.GetInternalContext().GetCurrentState().SetShaderCacheStateObject(FShaderCache::CreateOrFindCacheStateForContext(&ImmediateContext));
	}
	else
	{
		FShaderCache::InitShaderCache(SCO_Default, GMaxRHIShaderPlatform);
		ImmediateContext.GetInternalContext().GetCurrentState().SetShaderCacheStateObject(FShaderCache::CreateOrFindCacheStateForContext(&ImmediateContext));
	}
	
#if PLATFORM_MAC
	FShaderCache::SetMaxShaderResources(128);
#else
	FShaderCache::SetMaxShaderResources(32);
#endif

	// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitRHI();
	}
	// Dynamic resources can have dependencies on static resources (with uniform buffers) and must initialized last!
	for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
	{
		ResourceIt->InitDynamicRHI();
	}
	
	ImmediateContext.Profiler = nullptr;
#if ENABLE_METAL_GPUPROFILE
	ImmediateContext.Profiler = new FMetalGPUProfiler(ImmediateContext.Context);
#endif
	AsyncComputeContext = GSupportsEfficientAsyncCompute ? new FMetalRHIComputeContext(ImmediateContext.Profiler, new FMetalContext(ImmediateContext.Context->GetCommandQueue(), true)) : nullptr;
	}
}

FMetalDynamicRHI::~FMetalDynamicRHI()
{
	check(IsInGameThread() && IsInRenderingThread());

#if ENABLE_METAL_GPUPROFILE
	delete ImmediateContext.Profiler;
#endif
	
	// Ask all initialized FRenderResources to release their RHI resources.
	for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
	{
		FRenderResource* Resource = *ResourceIt;
		check(Resource->IsInitialized());
		Resource->ReleaseRHI();
	}
	
	for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
	{
		ResourceIt->ReleaseDynamicRHI();
	}
	
	GIsRHIInitialized = false;
}

uint64 FMetalDynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
	@autoreleasepool {
	OutAlign = 0;
	return CalcTextureSize(SizeX, SizeY, (EPixelFormat)Format, NumMips);
	}
}

uint64 FMetalDynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	@autoreleasepool {
	OutAlign = 0;
	return CalcTextureSize3D(SizeX, SizeY, SizeZ, (EPixelFormat)Format, NumMips);
	}
}

uint64 FMetalDynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags,	uint32& OutAlign)
{
	@autoreleasepool {
	OutAlign = 0;
	return CalcTextureSize(Size, Size, (EPixelFormat)Format, NumMips) * 6;
	}
}


void FMetalDynamicRHI::Init()
{
	GIsRHIInitialized = true;
}

void FMetalRHIImmediateCommandContext::RHIBeginFrame()
{
	@autoreleasepool {
	// @todo zebra: GPUProfilingData, GNumDrawCallsRHI, GNumPrimitivesDrawnRHI
#if ENABLE_METAL_GPUPROFILE
	Profiler->BeginFrame();
#endif
	((FMetalDeviceContext*)Context)->BeginFrame();
	}
}

void FMetalRHICommandContext::RHIBeginFrame()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIEndFrame()
{
	@autoreleasepool {
	// @todo zebra: GPUProfilingData.EndFrame();
#if ENABLE_METAL_GPUPROFILE
	Profiler->EndFrame();
#endif
	((FMetalDeviceContext*)Context)->EndFrame();
	}
}

void FMetalRHICommandContext::RHIEndFrame()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIBeginScene()
{
	@autoreleasepool {
	((FMetalDeviceContext*)Context)->BeginScene();
	}
}

void FMetalRHICommandContext::RHIBeginScene()
{
	check(false);
}

void FMetalRHIImmediateCommandContext::RHIEndScene()
{
	@autoreleasepool {
	((FMetalDeviceContext*)Context)->EndScene();
	}
}

void FMetalRHICommandContext::RHIEndScene()
{
	check(false);
}

void FMetalRHICommandContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
#if ENABLE_METAL_GPUEVENTS
	// @todo zebra : this was "[NSString stringWithTCHARString:Name]", an extension only on ios for some reason, it should be on Mac also
	@autoreleasepool
	{
		FPlatformMisc::BeginNamedEvent(Color, Name);
#if ENABLE_METAL_GPUPROFILE
		Profiler->PushEvent(Name, Color);
#endif
		Context->GetCurrentRenderPass().PushDebugGroup([NSString stringWithCString:TCHAR_TO_UTF8(Name) encoding:NSUTF8StringEncoding]);
	}
#endif
}

void FMetalRHICommandContext::RHIPopEvent()
{
#if ENABLE_METAL_GPUEVENTS
	@autoreleasepool {
	FPlatformMisc::EndNamedEvent();
	Context->GetCurrentRenderPass().PopDebugGroup();
#if ENABLE_METAL_GPUPROFILE
	Profiler->PopEvent();
#endif
	}
#endif
}

void FMetalDynamicRHI::RHIGetSupportedResolution( uint32 &Width, uint32 &Height )
{
#if PLATFORM_MAC
	CGDisplayModeRef DisplayMode = FPlatformApplicationMisc::GetSupportedDisplayMode(kCGDirectMainDisplay, Width, Height);
	if (DisplayMode)
	{
		Width = CGDisplayModeGetWidth(DisplayMode);
		Height = CGDisplayModeGetHeight(DisplayMode);
		CGDisplayModeRelease(DisplayMode);
	}
#else
	UE_LOG(LogMetal, Warning,  TEXT("RHIGetSupportedResolution unimplemented!"));
#endif
}

bool FMetalDynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
#if PLATFORM_MAC
	const int32 MinAllowableResolutionX = 0;
	const int32 MinAllowableResolutionY = 0;
	const int32 MaxAllowableResolutionX = 10480;
	const int32 MaxAllowableResolutionY = 10480;
	const int32 MinAllowableRefreshRate = 0;
	const int32 MaxAllowableRefreshRate = 10480;
	
	CFArrayRef AllModes = CGDisplayCopyAllDisplayModes(kCGDirectMainDisplay, NULL);
	if (AllModes)
	{
		const int32 NumModes = CFArrayGetCount(AllModes);
		const int32 Scale = (int32)FMacApplication::GetPrimaryScreenBackingScaleFactor();
		
		for (int32 Index = 0; Index < NumModes; Index++)
		{
			const CGDisplayModeRef Mode = (const CGDisplayModeRef)CFArrayGetValueAtIndex(AllModes, Index);
			const int32 Width = (int32)CGDisplayModeGetWidth(Mode) / Scale;
			const int32 Height = (int32)CGDisplayModeGetHeight(Mode) / Scale;
			const int32 RefreshRate = (int32)CGDisplayModeGetRefreshRate(Mode);
			
			if (Width >= MinAllowableResolutionX && Width <= MaxAllowableResolutionX && Height >= MinAllowableResolutionY && Height <= MaxAllowableResolutionY)
			{
				bool bAddIt = true;
				if (bIgnoreRefreshRate == false)
				{
					if (RefreshRate < MinAllowableRefreshRate || RefreshRate > MaxAllowableRefreshRate)
					{
						continue;
					}
				}
				else
				{
					// See if it is in the list already
					for (int32 CheckIndex = 0; CheckIndex < Resolutions.Num(); CheckIndex++)
					{
						FScreenResolutionRHI& CheckResolution = Resolutions[CheckIndex];
						if ((CheckResolution.Width == Width) &&
							(CheckResolution.Height == Height))
						{
							// Already in the list...
							bAddIt = false;
							break;
						}
					}
				}
				
				if (bAddIt)
				{
					// Add the mode to the list
					const int32 Temp2Index = Resolutions.AddZeroed();
					FScreenResolutionRHI& ScreenResolution = Resolutions[Temp2Index];
					
					ScreenResolution.Width = Width;
					ScreenResolution.Height = Height;
					ScreenResolution.RefreshRate = RefreshRate;
				}
			}
		}
		
		CFRelease(AllModes);
	}
	
	return true;
#else
	UE_LOG(LogMetal, Warning,  TEXT("RHIGetAvailableResolutions unimplemented!"));
	return false;
#endif
}

void FMetalDynamicRHI::RHIFlushResources()
{
	@autoreleasepool {
		((FMetalDeviceContext*)ImmediateContext.Context)->DrainHeap();
		((FMetalDeviceContext*)ImmediateContext.Context)->FlushFreeList();
		ImmediateContext.Context->SubmitCommandBufferAndWait();
		((FMetalDeviceContext*)ImmediateContext.Context)->ClearFreeList();
		ImmediateContext.Context->GetCurrentState().Reset();
	}
}

void FMetalDynamicRHI::RHIAcquireThreadOwnership()
{
	SetupRecursiveResources();
}

void FMetalDynamicRHI::RHIReleaseThreadOwnership()
{
}

void* FMetalDynamicRHI::RHIGetNativeDevice()
{
	return (void*)ImmediateContext.Context->GetDevice();
}
