// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHI.cpp: Render Hardware Interface implementation.
=============================================================================*/

#include "RHI.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, RHI);

/** RHI Logging. */
DEFINE_LOG_CATEGORY(LogRHI);

// Define counter stats.
DEFINE_STAT(STAT_RHIDrawPrimitiveCalls);
DEFINE_STAT(STAT_RHITriangles);
DEFINE_STAT(STAT_RHILines);

// Define memory stats.
DEFINE_STAT(STAT_RenderTargetMemory2D);
DEFINE_STAT(STAT_RenderTargetMemory3D);
DEFINE_STAT(STAT_RenderTargetMemoryCube);
DEFINE_STAT(STAT_TextureMemory2D);
DEFINE_STAT(STAT_TextureMemory3D);
DEFINE_STAT(STAT_TextureMemoryCube);
DEFINE_STAT(STAT_UniformBufferMemory);
DEFINE_STAT(STAT_IndexBufferMemory);
DEFINE_STAT(STAT_VertexBufferMemory);
DEFINE_STAT(STAT_StructuredBufferMemory);
DEFINE_STAT(STAT_PixelBufferMemory);
DEFINE_STAT(STAT_GetOrCreatePSO);

static FAutoConsoleVariable CVarUseVulkanRealUBs(
	TEXT("r.Vulkan.UseRealUBs"),
	0,
	TEXT("0: Emulate uniform buffers on Vulkan SM4/SM5 [default]\n")
	TEXT("1: Use real uniform buffers"),
	ECVF_ReadOnly
	);

const FString FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)EResourceTransitionAccess::EMaxAccess + 1] =
{
	FString(TEXT("EReadable")),
	FString(TEXT("EWritable")),	
	FString(TEXT("ERWBarrier")),
	FString(TEXT("ERWNoBarrier")),
	FString(TEXT("ERWSubResBarrier")),
	FString(TEXT("EMetaData")),
	FString(TEXT("EMaxAccess")),
};

#if STATS
#include "Stats/StatsData.h"
static void DumpRHIMemory(FOutputDevice& OutputDevice)
{
	TArray<FStatMessage> Stats;
	GetPermanentStats(Stats);

	FName NAME_STATGROUP_RHI(FStatGroup_STATGROUP_RHI::GetGroupName());
	OutputDevice.Logf(TEXT("RHI resource memory (not tracked by our allocator)"));
	int64 TotalMemory = 0;
	for (int32 Index = 0; Index < Stats.Num(); Index++)
	{
		FStatMessage const& Meta = Stats[Index];
		FName LastGroup = Meta.NameAndInfo.GetGroupName();
		if (LastGroup == NAME_STATGROUP_RHI && Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory))
		{
			OutputDevice.Logf(TEXT("%s"), *FStatsUtils::DebugPrint(Meta));
			TotalMemory += Meta.GetValue_int64();
		}
	}
	OutputDevice.Logf(TEXT("%.3fMB total"), TotalMemory / 1024.f / 1024.f);
}

static FAutoConsoleCommandWithOutputDevice GDumpRHIMemoryCmd(
	TEXT("rhi.DumpMemory"),
	TEXT("Dumps RHI memory stats to the log"),
	FConsoleCommandWithOutputDeviceDelegate::CreateStatic(DumpRHIMemory)
	);
#endif

//DO NOT USE THE STATIC FLINEARCOLORS TO INITIALIZE THIS STUFF.  
//Static init order is undefined and you will likely end up with bad values on some platforms.
const FClearValueBinding FClearValueBinding::None(EClearBinding::ENoneBound);
const FClearValueBinding FClearValueBinding::Black(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
const FClearValueBinding FClearValueBinding::White(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
const FClearValueBinding FClearValueBinding::Transparent(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
const FClearValueBinding FClearValueBinding::DepthOne(1.0f, 0);
const FClearValueBinding FClearValueBinding::DepthZero(0.0f, 0);
const FClearValueBinding FClearValueBinding::DepthNear((float)ERHIZBuffer::NearPlane, 0);
const FClearValueBinding FClearValueBinding::DepthFar((float)ERHIZBuffer::FarPlane, 0);
const FClearValueBinding FClearValueBinding::Green(FLinearColor(0.0f, 1.0f, 0.0f, 1.0f));
// Note: this is used as the default normal for DBuffer decals.  It must decode to a value of 0 in DecodeDBufferData.
const FClearValueBinding FClearValueBinding::DefaultNormal8Bit(FLinearColor(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f));

TLockFreePointerListUnordered<FRHIResource, PLATFORM_CACHE_LINE_SIZE> FRHIResource::PendingDeletes;
FRHIResource* FRHIResource::CurrentlyDeleting = nullptr;
TArray<FRHIResource::ResourcesToDelete> FRHIResource::DeferredDeletionQueue;
uint32 FRHIResource::CurrentFrame = 0;

bool FRHIResource::Bypass()
{
	return GRHICommandList.Bypass();
}

DECLARE_CYCLE_STAT(TEXT("Delete Resources"), STAT_DeleteResources, STATGROUP_RHICMDLIST);

void FRHIResource::FlushPendingDeletes()
{
	SCOPE_CYCLE_COUNTER(STAT_DeleteResources);

	check(IsInRenderingThread());
	FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	FRHICommandListExecutor::CheckNoOutstandingCmdLists();

	auto Delete = [](TArray<FRHIResource*>& ToDelete)
	{
		for (int32 Index = 0; Index < ToDelete.Num(); Index++)
		{
			FRHIResource* Ref = ToDelete[Index];
			check(Ref->MarkedForDelete == 1);
			if (Ref->GetRefCount() == 0) // caches can bring dead objects back to life
			{
				CurrentlyDeleting = Ref;
				delete Ref;
				CurrentlyDeleting = nullptr;
			}
			else
			{
				Ref->MarkedForDelete = 0;
				FPlatformMisc::MemoryBarrier();
			}
		}
	};

	while (1)
	{
		if (PendingDeletes.IsEmpty())
		{
			break;
		}
		if (PlatformNeedsExtraDeletionLatency())
		{
			const int32 Index = DeferredDeletionQueue.AddDefaulted();
			ResourcesToDelete& ResourceBatch = DeferredDeletionQueue[Index];
			ResourceBatch.FrameDeleted = CurrentFrame;
			PendingDeletes.PopAll(ResourceBatch.Resources);
			check(ResourceBatch.Resources.Num());
		}
		else
		{
			TArray<FRHIResource*> ToDelete;
			PendingDeletes.PopAll(ToDelete);
			check(ToDelete.Num());
			Delete(ToDelete);
		}
	}

	const uint32 NumFramesToExpire = 3;

	if (DeferredDeletionQueue.Num())
	{
		int32 DeletedBatchCount = 0;
		while (DeletedBatchCount < DeferredDeletionQueue.Num())
		{
			ResourcesToDelete& ResourceBatch = DeferredDeletionQueue[DeletedBatchCount];
			if (((ResourceBatch.FrameDeleted + NumFramesToExpire) < CurrentFrame) || !GIsRHIInitialized)
			{
				Delete(ResourceBatch.Resources);
				++DeletedBatchCount;
			}
			else
			{
				break;
			}
		}

		if (DeletedBatchCount)
		{
			DeferredDeletionQueue.RemoveAt(0, DeletedBatchCount);
		}

		++CurrentFrame;
	}
}

static_assert(ERHIZBuffer::FarPlane != ERHIZBuffer::NearPlane, "Near and Far planes must be different!");
static_assert((int32)ERHIZBuffer::NearPlane == 0 || (int32)ERHIZBuffer::NearPlane == 1, "Invalid Values for Near Plane, can only be 0 or 1!");
static_assert((int32)ERHIZBuffer::FarPlane == 0 || (int32)ERHIZBuffer::FarPlane == 1, "Invalid Values for Far Plane, can only be 0 or 1");


/**
 * RHI configuration settings.
 */

static TAutoConsoleVariable<int32> ResourceTableCachingCvar(
	TEXT("rhi.ResourceTableCaching"),
	1,
	TEXT("If 1, the RHI will cache resource table contents within a frame. Otherwise resource tables are rebuilt for every draw call.")
	);
static TAutoConsoleVariable<int32> GSaveScreenshotAfterProfilingGPUCVar(
	TEXT("r.ProfileGPU.Screenshot"),
	1,
	TEXT("Whether a screenshot should be taken when profiling the GPU. 0:off, 1:on (default)"),
	ECVF_RenderThreadSafe);
static TAutoConsoleVariable<int32> GShowProfilerAfterProfilingGPUCVar(
	TEXT("r.ProfileGPU.ShowUI"),
	1,
	TEXT("Whether the user interface profiler should be displayed after profiling the GPU.\n")
	TEXT("The results will always go to the log/console\n")
	TEXT("0:off, 1:on (default)"),
	ECVF_RenderThreadSafe);
static TAutoConsoleVariable<float> GGPUHitchThresholdCVar(
	TEXT("RHI.GPUHitchThreshold"),
	100.0f,
	TEXT("Threshold for detecting hitches on the GPU (in milliseconds).")
	);
static TAutoConsoleVariable<int32> GCVarRHIRenderPass(
	TEXT("r.RHIRenderPasses"),
	0,
	TEXT(""),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarGPUCrashDebugging(
	TEXT("r.GPUCrashDebugging"),
	0,
	TEXT("Enable vendor specific GPU crash analysis tools"),
	ECVF_ReadOnly
	);


namespace RHIConfig
{
	bool ShouldSaveScreenshotAfterProfilingGPU()
	{
		return GSaveScreenshotAfterProfilingGPUCVar.GetValueOnAnyThread() != 0;
	}

	bool ShouldShowProfilerAfterProfilingGPU()
	{
		return GShowProfilerAfterProfilingGPUCVar.GetValueOnAnyThread() != 0;
	}

	float GetGPUHitchThreshold()
	{
		return GGPUHitchThresholdCVar.GetValueOnAnyThread() * 0.001f;
	}
}

/**
 * RHI globals.
 */

bool GIsRHIInitialized = false;
int32 GMaxTextureMipCount = MAX_TEXTURE_MIP_COUNT;
bool GSupportsQuadBufferStereo = false;
bool GSupportsDepthFetchDuringDepthTest = true;
FString GRHIAdapterName;
FString GRHIAdapterInternalDriverVersion;
FString GRHIAdapterUserDriverVersion;
FString GRHIAdapterDriverDate;
uint32 GRHIVendorId = 0;
uint32 GRHIDeviceId = 0;
uint32 GRHIDeviceRevision = 0;
bool GRHIDeviceIsAMDPreGCNArchitecture = false;
bool GSupportsRenderDepthTargetableShaderResources = true;
TRHIGlobal<bool> GSupportsRenderTargetFormat_PF_G8(true);
TRHIGlobal<bool> GSupportsRenderTargetFormat_PF_FloatRGBA(true);
bool GSupportsShaderFramebufferFetch = false;
bool GSupportsShaderDepthStencilFetch = false;
bool GSupportsTimestampRenderQueries = false;
bool GHardwareHiddenSurfaceRemoval = false;
bool GRHISupportsAsyncTextureCreation = false;
bool GSupportsQuads = false;
bool GSupportsGenerateMips = false;
bool GSupportsVolumeTextureRendering = true;
bool GSupportsSeparateRenderTargetBlendState = false;
bool GSupportsDepthRenderTargetWithoutColorRenderTarget = true;
bool GRHINeedsUnatlasedCSMDepthsWorkaround = false;
bool GSupportsTexture3D = true;
bool GSupportsMobileMultiView = false;
bool GSupportsImageExternal = false;
bool GSupportsResourceView = true;
TRHIGlobal<bool> GSupportsMultipleRenderTargets(true);
bool GSupportsWideMRT = true;
float GMinClipZ = 0.0f;
float GProjectionSignY = 1.0f;
bool GRHINeedsExtraDeletionLatency = false;
TRHIGlobal<int32> GMaxShadowDepthBufferSizeX(2048);
TRHIGlobal<int32> GMaxShadowDepthBufferSizeY(2048);
TRHIGlobal<int32> GMaxTextureDimensions(2048);
TRHIGlobal<int32> GMaxCubeTextureDimensions(2048);
int32 GMaxTextureArrayLayers = 256;
int32 GMaxTextureSamplers = 16;
bool GUsingNullRHI = false;
int32 GDrawUPVertexCheckCount = MAX_int32;
int32 GDrawUPIndexCheckCount = MAX_int32;
bool GTriggerGPUProfile = false;
FString GGPUTraceFileName;
bool GRHISupportsTextureStreaming = false;
bool GSupportsDepthBoundsTest = false;
bool GSupportsEfficientAsyncCompute = false;
bool GRHISupportsBaseVertexIndex = true;
TRHIGlobal<bool> GRHISupportsInstancing(true);
bool GRHISupportsFirstInstance = false;
bool GRHIRequiresEarlyBackBufferRenderTarget = true;
bool GRHISupportsRHIThread = false;
bool GRHISupportsRHIOnTaskThread = false;
bool GRHISupportsParallelRHIExecute = false;
bool GSupportsHDR32bppEncodeModeIntrinsic = false;
bool GSupportsParallelOcclusionQueries = false;
bool GSupportsRenderTargetWriteMask = false;
bool GSupportsTransientResourceAliasing = false;
bool GRHIRequiresRenderTargetForPixelShaderUAVs = false;

bool GRHISupportsMSAADepthSampleAccess = false;
bool GRHISupportsResolveCubemapFaces = false;

bool GRHISupportsHDROutput = false;
EPixelFormat GRHIHDRDisplayOutputFormat = PF_FloatRGBA;

/** Whether we are profiling GPU hitches. */
bool GTriggerGPUHitchProfile = false;

#if WITH_SLI
int32 GNumActiveGPUsForRendering = 1;
#endif

FVertexElementTypeSupportInfo GVertexElementTypeSupport;

RHI_API int32 volatile GCurrentTextureMemorySize = 0;
RHI_API int32 volatile GCurrentRendertargetMemorySize = 0;
RHI_API int64 GTexturePoolSize = 0 * 1024 * 1024;
RHI_API int32 GPoolSizeVRAMPercentage = 0;

RHI_API EShaderPlatform GShaderPlatformForFeatureLevel[ERHIFeatureLevel::Num] = {SP_NumPlatforms,SP_NumPlatforms,SP_NumPlatforms,SP_NumPlatforms};

RHI_API int32 GNumDrawCallsRHI = 0;
RHI_API int32 GNumPrimitivesDrawnRHI = 0;

/** Called once per frame only from within an RHI. */
void RHIPrivateBeginFrame()
{
	GNumDrawCallsRHI = 0;
	GNumPrimitivesDrawnRHI = 0;
}

//
// The current shader platform.
//

RHI_API EShaderPlatform GMaxRHIShaderPlatform = SP_PCD3D_SM5;

/** The maximum feature level supported on this machine */
RHI_API ERHIFeatureLevel::Type GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
static bool bTessOn = true;

RHI_API void RHIAllowTessellation(bool bAllowTessellation)
{
	bTessOn = bAllowTessellation;
}

RHI_API bool RHITessellationAllowed()
{
	return bTessOn;
}

static int IsVoxelizing = 0;
RHI_API void RHIPushVoxelizationFlag()
{
	IsVoxelizing++;
}
RHI_API void RHIPopVoxelizationFlag()
{
	check(IsVoxelizing > 0);
	IsVoxelizing--;
}
RHI_API bool RHIIsVoxelizing()
{
	return IsVoxelizing > 0;
}

#endif
// NVCHANGE_END: Add VXGI

FName FeatureLevelNames[] = 
{
	FName(TEXT("ES2")),
	FName(TEXT("ES3_1")),
	FName(TEXT("SM4")),
	FName(TEXT("SM5")),
};

static_assert(ARRAY_COUNT(FeatureLevelNames) == ERHIFeatureLevel::Num, "Missing entry from feature level names.");

RHI_API bool GetFeatureLevelFromName(FName Name, ERHIFeatureLevel::Type& OutFeatureLevel)
{
	for (int32 NameIndex = 0; NameIndex < ARRAY_COUNT(FeatureLevelNames); NameIndex++)
	{
		if (FeatureLevelNames[NameIndex] == Name)
		{
			OutFeatureLevel = (ERHIFeatureLevel::Type)NameIndex;
			return true;
		}
	}

	OutFeatureLevel = ERHIFeatureLevel::Num;
	return false;
}

RHI_API void GetFeatureLevelName(ERHIFeatureLevel::Type InFeatureLevel, FString& OutName)
{
	check(InFeatureLevel < ARRAY_COUNT(FeatureLevelNames));
	FeatureLevelNames[(int32)InFeatureLevel].ToString(OutName);
}

RHI_API void GetFeatureLevelName(ERHIFeatureLevel::Type InFeatureLevel, FName& OutName)
{
	check(InFeatureLevel < ARRAY_COUNT(FeatureLevelNames));
	OutName = FeatureLevelNames[(int32)InFeatureLevel];
}

static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
static FName NAME_PCD3D_SM4(TEXT("PCD3D_SM4"));
static FName NAME_PCD3D_ES3_1(TEXT("PCD3D_ES31"));
static FName NAME_PCD3D_ES2(TEXT("PCD3D_ES2"));
static FName NAME_GLSL_150(TEXT("GLSL_150"));
static FName NAME_SF_PS4(TEXT("SF_PS4"));
static FName NAME_SF_XBOXONE_D3D12(TEXT("SF_XBOXONE_D3D12"));
static FName NAME_GLSL_430(TEXT("GLSL_430"));
static FName NAME_GLSL_150_ES2(TEXT("GLSL_150_ES2"));
static FName NAME_GLSL_150_ES2_NOUB(TEXT("GLSL_150_ES2_NOUB"));
static FName NAME_GLSL_150_ES31(TEXT("GLSL_150_ES31"));
static FName NAME_GLSL_ES2(TEXT("GLSL_ES2"));
static FName NAME_GLSL_ES2_WEBGL(TEXT("GLSL_ES2_WEBGL"));
static FName NAME_GLSL_ES2_IOS(TEXT("GLSL_ES2_IOS"));
static FName NAME_SF_METAL(TEXT("SF_METAL"));
static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));
static FName NAME_SF_METAL_MRT_MAC(TEXT("SF_METAL_MRT_MAC"));
static FName NAME_GLSL_310_ES_EXT(TEXT("GLSL_310_ES_EXT"));
static FName NAME_GLSL_ES3_1_ANDROID(TEXT("GLSL_ES3_1_ANDROID"));
static FName NAME_SF_METAL_SM5(TEXT("SF_METAL_SM5"));
static FName NAME_VULKAN_ES3_1_ANDROID(TEXT("SF_VULKAN_ES31_ANDROID"));
static FName NAME_VULKAN_ES3_1(TEXT("SF_VULKAN_ES31"));
static FName NAME_VULKAN_SM4_UB(TEXT("SF_VULKAN_SM4_UB"));
static FName NAME_VULKAN_SM4(TEXT("SF_VULKAN_SM4"));
static FName NAME_VULKAN_SM5_UB(TEXT("SF_VULKAN_SM5_UB"));
static FName NAME_VULKAN_SM5(TEXT("SF_VULKAN_SM5"));
static FName NAME_SF_METAL_SM4(TEXT("SF_METAL_SM4"));
static FName NAME_SF_METAL_MACES3_1(TEXT("SF_METAL_MACES3_1"));
static FName NAME_SF_METAL_MACES2(TEXT("SF_METAL_MACES2"));
static FName NAME_GLSL_SWITCH(TEXT("GLSL_SWITCH"));
static FName NAME_GLSL_SWITCH_FORWARD(TEXT("GLSL_SWITCH_FORWARD"));

FName LegacyShaderPlatformToShaderFormat(EShaderPlatform Platform)
{
	switch(Platform)
	{
	case SP_PCD3D_SM5:
		return NAME_PCD3D_SM5;
	case SP_PCD3D_SM4:
		return NAME_PCD3D_SM4;
	case SP_PCD3D_ES3_1:
		return NAME_PCD3D_ES3_1;
	case SP_PCD3D_ES2:
		return NAME_PCD3D_ES2;
	case SP_OPENGL_SM4:
		return NAME_GLSL_150;
	case SP_PS4:
		return NAME_SF_PS4;
	case SP_XBOXONE_D3D12:
		return NAME_SF_XBOXONE_D3D12;
	case SP_OPENGL_SM5:
		return NAME_GLSL_430;
	case SP_OPENGL_PCES2:
	{
		static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("OpenGL.UseEmulatedUBs"));
		return (CVar && CVar->GetValueOnAnyThread() != 0) ? NAME_GLSL_150_ES2_NOUB : NAME_GLSL_150_ES2;
	}
	case SP_OPENGL_PCES3_1:
		return NAME_GLSL_150_ES31;
	case SP_OPENGL_ES2_ANDROID:
		return NAME_GLSL_ES2;
	case SP_OPENGL_ES2_WEBGL:
		return NAME_GLSL_ES2_WEBGL;
	case SP_OPENGL_ES2_IOS:
		return NAME_GLSL_ES2_IOS;
	case SP_METAL:
		return NAME_SF_METAL;
	case SP_METAL_MRT:
		return NAME_SF_METAL_MRT;
	case SP_METAL_MRT_MAC:
		return NAME_SF_METAL_MRT_MAC;
	case SP_METAL_SM4:
		return NAME_SF_METAL_SM4;
	case SP_METAL_SM5:
		return NAME_SF_METAL_SM5;
	case SP_METAL_MACES3_1:
		return NAME_SF_METAL_MACES3_1;
	case SP_METAL_MACES2:
		return NAME_SF_METAL_MACES2;
	case SP_OPENGL_ES31_EXT:
		return NAME_GLSL_310_ES_EXT;
	case SP_OPENGL_ES3_1_ANDROID:
		return NAME_GLSL_ES3_1_ANDROID;
	case SP_VULKAN_SM4:
		return (CVarUseVulkanRealUBs->GetInt() != 0) ? NAME_VULKAN_SM4_UB : NAME_VULKAN_SM4;
	case SP_VULKAN_SM5:
		return (CVarUseVulkanRealUBs->GetInt() != 0) ? NAME_VULKAN_SM5_UB : NAME_VULKAN_SM5;
	case SP_VULKAN_PCES3_1:
		return NAME_VULKAN_ES3_1;
	case SP_VULKAN_ES3_1_ANDROID:
		return NAME_VULKAN_ES3_1_ANDROID;
	case SP_SWITCH:
		return NAME_GLSL_SWITCH;
	case SP_SWITCH_FORWARD:
		return NAME_GLSL_SWITCH_FORWARD;

	default:
		check(0);
		return NAME_PCD3D_SM5;
	}
}

EShaderPlatform ShaderFormatToLegacyShaderPlatform(FName ShaderFormat)
{
	if (ShaderFormat == NAME_PCD3D_SM5)				return SP_PCD3D_SM5;	
	if (ShaderFormat == NAME_PCD3D_SM4)				return SP_PCD3D_SM4;
	if (ShaderFormat == NAME_PCD3D_ES3_1)			return SP_PCD3D_ES3_1;
	if (ShaderFormat == NAME_PCD3D_ES2)				return SP_PCD3D_ES2;
	if (ShaderFormat == NAME_GLSL_150)				return SP_OPENGL_SM4;
	if (ShaderFormat == NAME_SF_PS4)				return SP_PS4;
	if (ShaderFormat == NAME_SF_XBOXONE_D3D12)		return SP_XBOXONE_D3D12;
	if (ShaderFormat == NAME_GLSL_430)				return SP_OPENGL_SM5;
	if (ShaderFormat == NAME_GLSL_150_ES2)			return SP_OPENGL_PCES2;
	if (ShaderFormat == NAME_GLSL_150_ES2_NOUB)		return SP_OPENGL_PCES2;
	if (ShaderFormat == NAME_GLSL_150_ES31)			return SP_OPENGL_PCES3_1;
	if (ShaderFormat == NAME_GLSL_ES2)				return SP_OPENGL_ES2_ANDROID;
	if (ShaderFormat == NAME_GLSL_ES2_WEBGL)		return SP_OPENGL_ES2_WEBGL;
	if (ShaderFormat == NAME_GLSL_ES2_IOS)			return SP_OPENGL_ES2_IOS;
	if (ShaderFormat == NAME_SF_METAL)				return SP_METAL;
	if (ShaderFormat == NAME_SF_METAL_MRT)			return SP_METAL_MRT;
	if (ShaderFormat == NAME_SF_METAL_MRT_MAC)		return SP_METAL_MRT_MAC;
	if (ShaderFormat == NAME_GLSL_310_ES_EXT)		return SP_OPENGL_ES31_EXT;
	if (ShaderFormat == NAME_SF_METAL_SM5)			return SP_METAL_SM5;
	if (ShaderFormat == NAME_VULKAN_SM4)			return SP_VULKAN_SM4;
	if (ShaderFormat == NAME_VULKAN_SM5)			return SP_VULKAN_SM5;
	if (ShaderFormat == NAME_VULKAN_ES3_1_ANDROID)	return SP_VULKAN_ES3_1_ANDROID;
	if (ShaderFormat == NAME_VULKAN_ES3_1)			return SP_VULKAN_PCES3_1;
	if (ShaderFormat == NAME_VULKAN_SM4_UB)			return SP_VULKAN_SM4;
	if (ShaderFormat == NAME_VULKAN_SM5_UB)			return SP_VULKAN_SM5;
	if (ShaderFormat == NAME_SF_METAL_SM4)			return SP_METAL_SM4;
	if (ShaderFormat == NAME_SF_METAL_MACES3_1)		return SP_METAL_MACES3_1;
	if (ShaderFormat == NAME_SF_METAL_MACES2)		return SP_METAL_MACES2;
	if (ShaderFormat == NAME_GLSL_ES3_1_ANDROID)	return SP_OPENGL_ES3_1_ANDROID;
	if (ShaderFormat == NAME_GLSL_SWITCH)			return SP_SWITCH;
	if (ShaderFormat == NAME_GLSL_SWITCH_FORWARD)	return SP_SWITCH_FORWARD;
	
	return SP_NumPlatforms;
}


RHI_API bool IsRHIDeviceAMD()
{
	check(GRHIVendorId != 0);
	// AMD's drivers tested on July 11 2013 have hitching problems with async resource streaming, setting single threaded for now until fixed.
	return GRHIVendorId == 0x1002;
}

RHI_API bool IsRHIDeviceIntel()
{
	check(GRHIVendorId != 0);
	// Intel GPUs are integrated and use both DedicatedVideoMemory and SharedSystemMemory.
	return GRHIVendorId == 0x8086;
}

RHI_API bool IsRHIDeviceNVIDIA()
{
	check(GRHIVendorId != 0);
	// NVIDIA GPUs are discrete and use DedicatedVideoMemory only.
	return GRHIVendorId == 0x10DE;
}

RHI_API const TCHAR* RHIVendorIdToString()
{
	switch (GRHIVendorId)
	{
	case 0x1002: return TEXT("AMD");
	case 0x1010: return TEXT("ImgTec");
	case 0x10DE: return TEXT("NVIDIA");
	case 0x13B5: return TEXT("ARM");
	case 0x5143: return TEXT("Qualcomm");
	case 0x8086: return TEXT("Intel");
	default: return TEXT("Unknown");
	}
}

RHI_API uint32 RHIGetShaderLanguageVersion(const EShaderPlatform Platform)
{
	uint32 Version = 0;
	if (IsMetalPlatform(Platform))
	{
		if (IsPCPlatform(Platform))
		{
			static int32 MaxShaderVersion = -1;
			if (MaxShaderVersion < 0)
			{
				MaxShaderVersion = 2;
				if(!GConfig->GetInt(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("MaxShaderLanguageVersion"), MaxShaderVersion, GEngineIni))
				{
					MaxShaderVersion = 2;
				}
			}
			Version = (uint32)MaxShaderVersion;
		}
		else
		{
			static int32 MaxShaderVersion = -1;
			if (MaxShaderVersion < 0)
			{
				MaxShaderVersion = 0;
				if(!GConfig->GetInt(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("MaxShaderLanguageVersion"), MaxShaderVersion, GEngineIni))
				{
					MaxShaderVersion = 0;
				}
			}
			Version = (uint32)MaxShaderVersion;
		}
	}
	return Version;
}

RHI_API bool RHISupportsTessellation(const EShaderPlatform Platform)
{
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	if (RHITessellationAllowed() == false)
	{
		return false;
	}
#endif
	// NVCHANGE_END: Add VXGI

	if (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsMetalPlatform(Platform))
	{
		return (Platform == SP_PCD3D_SM5) || (Platform == SP_XBOXONE_D3D12) || (Platform == SP_OPENGL_SM5) || (Platform == SP_OPENGL_ES31_EXT)/* || (Platform == SP_VULKAN_SM5)*/;
	}
	// For Metal we can only support tessellation if we are willing to sacrifice backward compatibility with OS versions.
	// As such it becomes an opt-in project setting.
	else if (Platform == SP_METAL_SM5)
	{
		return (RHIGetShaderLanguageVersion(Platform) >= 2);
	}
	return false;
}

RHI_API bool RHISupportsPixelShaderUAVs(const EShaderPlatform Platform)
{
	if (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsMetalPlatform(Platform))
	{
		return true;
	}
	else if (Platform == SP_METAL_SM5)
	{
		return (RHIGetShaderLanguageVersion(Platform) >= 2);
	}
	return false;
}

static ERHIFeatureLevel::Type GRHIMobilePreviewFeatureLevel = ERHIFeatureLevel::Num;
RHI_API void RHISetMobilePreviewFeatureLevel(ERHIFeatureLevel::Type MobilePreviewFeatureLevel)
{
	check(MobilePreviewFeatureLevel == ERHIFeatureLevel::ES2 || MobilePreviewFeatureLevel == ERHIFeatureLevel::ES3_1);
	check(GRHIMobilePreviewFeatureLevel == ERHIFeatureLevel::Num);
	check(!GIsEditor);
	GRHIMobilePreviewFeatureLevel = MobilePreviewFeatureLevel;
}

bool RHIGetPreviewFeatureLevel(ERHIFeatureLevel::Type& PreviewFeatureLevelOUT)
{
	static bool bForceFeatureLevelES2 = !GIsEditor && FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES2"));
	static bool bForceFeatureLevelES3_1 = !GIsEditor && (FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES31")) || FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES3_1")));

	if (bForceFeatureLevelES2)
	{
		PreviewFeatureLevelOUT = ERHIFeatureLevel::ES2;
	}
	else if (bForceFeatureLevelES3_1)
	{
		PreviewFeatureLevelOUT = ERHIFeatureLevel::ES3_1;
	}
	else if (!GIsEditor && GRHIMobilePreviewFeatureLevel != ERHIFeatureLevel::Num)
	{
		PreviewFeatureLevelOUT = GRHIMobilePreviewFeatureLevel;
	}
	else
	{
		return false;
	}
	return true;
}

RHI_API void FRHIRenderPassInfo::ConvertToRenderTargetsInfo(FRHISetRenderTargetsInfo& OutRTInfo) const
{
	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		if (!ColorRenderTargets[Index].RenderTarget)
		{
			break;
		}

		OutRTInfo.ColorRenderTarget[Index].Texture = ColorRenderTargets[Index].RenderTarget;
		ERenderTargetLoadAction LoadAction = GetLoadAction(ColorRenderTargets[Index].Action);
		OutRTInfo.ColorRenderTarget[Index].LoadAction = LoadAction;
		OutRTInfo.ColorRenderTarget[Index].StoreAction = GetStoreAction(ColorRenderTargets[Index].Action);
		OutRTInfo.ColorRenderTarget[Index].ArraySliceIndex = ColorRenderTargets[Index].ArraySlice;
		OutRTInfo.ColorRenderTarget[Index].MipIndex = ColorRenderTargets[Index].MipIndex;
		++OutRTInfo.NumColorRenderTargets;

		OutRTInfo.bClearColor |= (LoadAction == ERenderTargetLoadAction::EClear);
	}

	ERenderTargetActions DepthActions = GetDepthActions(DepthStencilRenderTarget.Action);
	ERenderTargetActions StencilActions = GetStencilActions(DepthStencilRenderTarget.Action);
	ERenderTargetLoadAction DepthLoadAction = GetLoadAction(DepthActions);
	ERenderTargetStoreAction DepthStoreAction = GetStoreAction(DepthActions);
	ERenderTargetLoadAction StencilLoadAction = GetLoadAction(StencilActions);
	ERenderTargetStoreAction StencilStoreAction = GetStoreAction(StencilActions);

	if (bDEPRECATEDHasEDS)
	{
		OutRTInfo.DepthStencilRenderTarget = FRHIDepthRenderTargetView(DepthStencilRenderTarget.DepthStencilTarget,
			DepthLoadAction,
			GetStoreAction(DepthActions),
			StencilLoadAction,
			GetStoreAction(StencilActions),
			DEPRECATED_EDS);
	}
	else
	{
		OutRTInfo.DepthStencilRenderTarget = FRHIDepthRenderTargetView(DepthStencilRenderTarget.DepthStencilTarget,
			DepthLoadAction,
			GetStoreAction(DepthActions),
			StencilLoadAction,
			GetStoreAction(StencilActions));
	}
	OutRTInfo.bClearDepth = (DepthLoadAction == ERenderTargetLoadAction::EClear);
	OutRTInfo.bClearStencil = (StencilLoadAction == ERenderTargetLoadAction::EClear);
}

RHI_API bool RHIUseRenderPasses()
{
	static auto* RenderPassCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RHIRenderPasses"));
	return RenderPassCVar && RenderPassCVar->GetValueOnRenderThread() > 0;
}
