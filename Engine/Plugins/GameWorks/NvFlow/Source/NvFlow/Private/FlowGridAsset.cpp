#include "FlowGridAsset.h"
#include "NvFlowCommon.h"

// NvFlow begin

#include "PhysicsEngine/PhysXSupport.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/CollisionProfile.h"

bool UFlowGridAsset::sGlobalDebugDraw = false;
uint32 UFlowGridAsset::sGlobalRenderChannel = eNvFlowGridTextureChannelDensity;
uint32 UFlowGridAsset::sGlobalRenderMode = eNvFlowVolumeRenderMode_rainbow;
uint32 UFlowGridAsset::sGlobalMode = eNvFlowGridDebugVisBlocks;
bool UFlowGridAsset::sGlobalDebugDrawShadow = false;
uint32 UFlowGridAsset::sGlobalMultiGPU = 1;
uint32 UFlowGridAsset::sGlobalAsyncCompute = 0;
bool UFlowGridAsset::sGlobalMultiGPUResetRequest = false;
uint32 UFlowGridAsset::sGlobalDepth = 1;
uint32 UFlowGridAsset::sGlobalDepthDebugDraw = 0;

static const float ShadowMinResidentScale_DEPRECATED_Default = 0.25f * (1.f / 64.f);
static const float ShadowMaxResidentScale_DEPRECATED_Default = 4.f * 0.25f * (1.f / 64.f);

namespace
{
	int32 ShadowResidentScaleToBlocks(float ResidentScale, EFlowShadowResolution ShadowResolution)
	{
		const int32 ShadowDim = (1 << ShadowResolution);

		const int ShadowBlockDim = 16;
		const int32 ShadowGridDim = (ShadowDim + ShadowBlockDim-1) / ShadowBlockDim;

		const int32 MaxBlocks = ShadowGridDim * ShadowGridDim * ShadowGridDim;

		return ResidentScale * MaxBlocks;
	}
}

UFlowGridAsset::UFlowGridAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	NvFlowGridDesc FlowGridDesc;
	NvFlowGridDescDefaultsInline(&FlowGridDesc);

	GridCellSize = FlowGridDesc.halfSize.x * 2 * GetFlowToUE4Scale() / FlowGridDesc.virtualDim.x;

	check(FlowGridDesc.virtualDim.x == GetVirtualGridDimension(EFGD_512));
	VirtualGridDimension = EFGD_512;

	MemoryLimitScale = 1.f;

	SimulationRate = 60.f;
	bLowLatencyMapping = true;
	bMultiAdapterEnabled = false;
	bAsyncComputeEnabled = false;

	NvFlowGridParams FlowGridParams;
	NvFlowGridParamsDefaultsInline(&FlowGridParams);

	Gravity = FVector(FlowGridParams.gravity.x, FlowGridParams.gravity.z, FlowGridParams.gravity.y) * GetFlowToUE4Scale();
	bSinglePassAdvection = FlowGridParams.singlePassAdvection;
	bPressureLegacyMode = FlowGridParams.pressureLegacyMode;
	bBigEffectMode = FlowGridParams.bigEffectMode;

	NvFlowVolumeRenderParams FlowVolumeRenderParams;
	NvFlowVolumeRenderParamsDefaultsInline(&FlowVolumeRenderParams);
	RenderMode = (EFlowRenderMode)FlowVolumeRenderParams.renderMode;
	RenderChannel = (EFlowRenderChannel)FlowVolumeRenderParams.renderChannel;
	ColorMapResolution = 64;
	bAdaptiveScreenPercentage = false;
	AdaptiveTargetFrameTime = 10.f;
	MaxScreenPercentage = 1.f;
	MinScreenPercentage = 0.5f;
	bDebugWireframe = FlowVolumeRenderParams.debugMode;
	bGenerateDepth = false;
	DepthAlphaThreshold = FlowVolumeRenderParams.depthAlphaThreshold;
	DepthIntensityThreshold = FlowVolumeRenderParams.depthIntensityThreshold;

	//Collision
	FCollisionResponseParams FlowResponseParams;
	int32 FlowChannelIdx = INDEX_NONE;
	// search engine trace channels for Flow
	for (int32 ChannelIdx = ECC_GameTraceChannel1; ChannelIdx <= ECC_GameTraceChannel18; ChannelIdx++)
	{
		if (FName(TEXT("Flow")) == UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(ChannelIdx))
		{
			FlowChannelIdx = ChannelIdx;
			break;
		}
	}
	if (FlowChannelIdx != INDEX_NONE)
	{
		ObjectType = (ECollisionChannel)FlowChannelIdx;
		FCollisionResponseTemplate Template;
		UCollisionProfile::Get()->GetProfileTemplate(UCollisionProfile::BlockAll_ProfileName, Template);
		ResponseToChannels = Template.ResponseToChannels;
	}
	else
	{
		ObjectType = ECC_WorldDynamic; /// ECC_Flow;
		FCollisionResponseTemplate Template;
		UCollisionProfile::Get()->GetProfileTemplate(TEXT("WorldDynamic"/*"Flow"*/), Template);
		ResponseToChannels = Template.ResponseToChannels;
	}

	bParticlesInteractionEnabled = false;
	InteractionChannel = EIC_Channel1;
	bParticleModeEnabled = false;

	ParticleToGridAccelTimeConstant = 0.01f;
	ParticleToGridDecelTimeConstant = 10.0f;
	ParticleToGridThresholdMultiplier = 2.f;
	GridToParticleAccelTimeConstant = 0.01f;
	GridToParticleDecelTimeConstant = 0.01f;
	GridToParticleThresholdMultiplier = 1.f;

	bDistanceFieldCollisionEnabled = false;
	MinActiveDistance = -1.0f;
	MaxActiveDistance = 0.0f;
	VelocitySlipFactor = 0.0f;
	VelocitySlipThickness = 0.0f;

	bVolumeShadowEnabled = false;
	ShadowIntensityScale = 0.5f;
	ShadowMinIntensity = 0.15f;

	ShadowBlendCompMask = { 0.0f, 0.0f, 0.0f, 0.0f };
	ShadowBlendBias = 1.0f;

	ShadowResolution = EFSR_High;
	ShadowFrustrumScale = 1.0f;
	ShadowMinResidentScale_DEPRECATED = ShadowMinResidentScale_DEPRECATED_Default;
	ShadowMaxResidentScale_DEPRECATED = ShadowMaxResidentScale_DEPRECATED_Default;
	
	ShadowMinResidentBlocks = ShadowResidentScaleToBlocks(ShadowMinResidentScale_DEPRECATED, ShadowResolution);
	ShadowMaxResidentBlocks = ShadowResidentScaleToBlocks(ShadowMaxResidentScale_DEPRECATED, ShadowResolution);

	ShadowChannel = 0;
	ShadowNearDistance = 10.0f;
}

void UFlowGridAsset::PostLoad()
{
	Super::PostLoad();

	if (ShadowMinResidentScale_DEPRECATED != ShadowMinResidentScale_DEPRECATED_Default)
	{
		ShadowMinResidentBlocks = ShadowResidentScaleToBlocks(ShadowMinResidentScale_DEPRECATED, ShadowResolution);
	}
	if (ShadowMaxResidentScale_DEPRECATED != ShadowMaxResidentScale_DEPRECATED_Default)
	{
		ShadowMaxResidentBlocks = ShadowResidentScaleToBlocks(ShadowMaxResidentScale_DEPRECATED, ShadowResolution);
	}
}

// NvFlow end