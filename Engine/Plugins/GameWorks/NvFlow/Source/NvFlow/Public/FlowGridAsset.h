
#pragma once

// NvFlow begin

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"

#include "GridInteractionNvFlow.h"
#include "FlowRenderMaterial.h"
#include "FlowGridAsset.generated.h"

UENUM()
enum EFlowGridDimension
{
	EFGD_256 = 8,
	EFGD_512 = 9,
	EFGD_1024 = 10,
};

UENUM(BlueprintType)
enum EFlowRenderMode
{
	EFRM_Colormap = 0 UMETA(DisplayName = "Colormap"),
	EFRM_Raw = 1 UMETA(DisplayName = "Raw"),
	EFRM_Rainbow = 2 UMETA(DisplayName = "Rainbow"),
	EFRM_Debug = 3 UMETA(DisplayName = "Debug"),

	EFRM_MAX,
};

UENUM(BlueprintType)
enum EFlowRenderChannel
{
	EFRC_Velocity = 0 UMETA(DisplayName = "Velocity"),
	EFRC_Density = 1 UMETA(DisplayName = "Density"),
	EFRC_DensityCoarse = 2 UMETA(DisplayName = "Density Coarse"),

	EFRC_MAX,
};

UENUM()
enum EFlowShadowResolution
{
	EFSR_Low = 8 UMETA(DisplayName = "Low-256"),
	EFSR_Medium = 9 UMETA(DisplayName = "Medium-512"),
	EFSR_High = 10 UMETA(DisplayName = "High-1024"),
	EFSR_Ultra = 11 UMETA(DisplayName = "Ultra-2048"),
};


UCLASS(BlueprintType, hidecategories=object, MinimalAPI)
class UFlowGridAsset : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Grid cell size: defines resolution of simulation*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grid)
	float		GridCellSize;

	/** Grid dimension: dimension * cellSize defines size of simulation domain*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grid)
	TEnumAsByte<EFlowGridDimension> VirtualGridDimension;

	/** Allows increase of maximum number of cells*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grid)
	float		MemoryLimitScale;

	/** Simulation update rate in updates per second*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grid)
	float		SimulationRate;

	/** If true, block allocation will update faster at the cost of extra overhead.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grid)
	uint32		bLowLatencyMapping : 1;

	/** If true, multiAdapter is used if supported*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grid)
	uint32		bMultiAdapterEnabled : 1;

	/** If true, async compute is used if supported*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grid)
	uint32		bAsyncComputeEnabled : 1;

	/** If true, higher res density and volume rendering are disabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grid)
	uint32		bParticleModeEnabled : 1;

	/** Tweaks block allocation for better big effect behavior*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Grid)
	uint32		bBigEffectMode : 1;

	/** If true, grid affects GPU particles*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction with Particles")
	uint32		bParticlesInteractionEnabled : 1;

	/** Enum indicating what interaction channel this object has */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction with Particles")
	TEnumAsByte<enum EInteractionChannelNvFlow> InteractionChannel;

	/** Custom Channels for Responses */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction with Particles")
	struct FInteractionResponseContainerNvFlow ResponseToInteractionChannels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction with Particles")
	float		ParticleToGridAccelTimeConstant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction with Particles")
	float		ParticleToGridDecelTimeConstant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction with Particles")
	float		ParticleToGridThresholdMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction with Particles")
	float		GridToParticleAccelTimeConstant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction with Particles")
	float		GridToParticleDecelTimeConstant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction with Particles")
	float		GridToParticleThresholdMultiplier;

	/** Gravity vector for use by buoyancy*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity")
	FVector		Gravity;

	/** If true, enables single pass advection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advect")
	uint32		bSinglePassAdvection : 1;

	/** If true, run older less accurate pressure solver*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pressure")
	uint32		bPressureLegacyMode : 1;

	/** Enum indicating what type of object this should be considered as */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	TEnumAsByte<enum ECollisionChannel> ObjectType;

	/** Custom Channels for Responses*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	struct FCollisionResponseContainer	ResponseToChannels;

	/** Render mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	TEnumAsByte<EFlowRenderMode> RenderMode;

	/** Render channel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	TEnumAsByte<EFlowRenderChannel> RenderChannel;

	/** Color map resolution.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (UIMin = 1))
	int32		ColorMapResolution;

	/** Adaptive ScreenPercentage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	uint32		bAdaptiveScreenPercentage : 1;

	/** Target Frame Time for Adaptive, in ms */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (UIMin = 0.f, UIMax = 50.f))
	float		AdaptiveTargetFrameTime;

	/** Maximum ScreenPercentage, Default Value is Adaptive is disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (UIMin = 0.1f, UIMax = 1.0f))
	float		MaxScreenPercentage;

	/** Minimum ScreenPercentage when Adaptive is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (UIMin = 0.1f, UIMax = 1.0f))
	float		MinScreenPercentage;

	/** Debug rendering*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	uint32		bDebugWireframe : 1;

	/** Depth generation*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	uint32		bGenerateDepth : 1;

	/** Alpha threshold for depth write when generateDepth enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (UIMin = 0.0f, UIMax = 2.0f))
	float		DepthAlphaThreshold;

	/** Intensity threshold for depth write when generateDepth enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (UIMin = 0.0f, UIMax = 10.0f))
	float		DepthIntensityThreshold;

	static bool sGlobalDebugDraw;
	static uint32 sGlobalRenderChannel;
	static uint32 sGlobalRenderMode;
	static uint32 sGlobalMode;
	static bool sGlobalDebugDrawShadow;
	static uint32 sGlobalMultiGPU;
	static uint32 sGlobalAsyncCompute;
	static bool sGlobalMultiGPUResetRequest;
	static uint32 sGlobalDepth;
	static uint32 sGlobalDepthDebugDraw;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Shadow")
	uint32		bVolumeShadowEnabled : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Shadow")
	float		ShadowIntensityScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Shadow")
	float		ShadowMinIntensity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Shadow")
	FFlowRenderCompMask ShadowBlendCompMask;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Shadow")
	float		ShadowBlendBias;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Shadow")
	TEnumAsByte<EFlowShadowResolution> ShadowResolution;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Shadow", meta = (UIMin = 1.0f, UIMax = 2.0f))
	float		ShadowFrustrumScale;

	UPROPERTY()
	float		ShadowMinResidentScale_DEPRECATED;

	UPROPERTY()
	float		ShadowMaxResidentScale_DEPRECATED;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Shadow", meta = (UIMin = 1, UIMax = 10000))
	int32		ShadowMinResidentBlocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Shadow", meta = (UIMin = 1, UIMax = 10000))
	int32		ShadowMaxResidentBlocks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Shadow")
	int32		ShadowChannel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Shadow")
	float		ShadowNearDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distance Field")
	uint32		bDistanceFieldCollisionEnabled : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distance Field")
	float		MinActiveDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distance Field")
	float		MaxActiveDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distance Field")
	float		VelocitySlipFactor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distance Field")
	float		VelocitySlipThickness;

	// Helper methods

	FORCEINLINE static float GetFlowToUE4Scale() { return 100.0f; }

	FORCEINLINE static uint32 GetVirtualGridDimension(TEnumAsByte<EFlowGridDimension> GridDimension) { return static_cast<uint32>(1 << GridDimension); }

	FORCEINLINE static float GetVirtualGridExtent(float GridCellSize, TEnumAsByte<EFlowGridDimension> GridDimension) 
	{ 
		return GridCellSize * GetVirtualGridDimension(GridDimension) * 0.5f;
	}

	FORCEINLINE float GetVirtualGridDimension() const { return GetVirtualGridDimension(VirtualGridDimension); }
	FORCEINLINE float GetVirtualGridExtent() const { return GetVirtualGridExtent(GridCellSize, VirtualGridDimension); }

	virtual void PostLoad() override;
};

// NvFlow end