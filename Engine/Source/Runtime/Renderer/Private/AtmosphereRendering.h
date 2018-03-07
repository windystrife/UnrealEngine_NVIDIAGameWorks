// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AtmosphereRendering.h: Fog rendering
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "Serialization/BulkData.h"
#include "RendererInterface.h"

class FScene;
class FSceneViewFamily;
class FShader;
class FViewInfo;
class UAtmosphericFogComponent;

namespace EAtmosphereRenderFlag
{
	enum Type
	{
		E_EnableAll = 0,
		E_DisableSunDisk = 1,
		E_DisableGroundScattering = 2,
		E_DisableLightShaft = 4, // Light Shaft shadow
		E_DisableSunAndGround = E_DisableSunDisk | E_DisableGroundScattering,
		E_DisableSunAndLightShaft = E_DisableSunDisk | E_DisableLightShaft,
		E_DisableGroundAndLightShaft = E_DisableGroundScattering | E_DisableLightShaft,
		E_DisableAll = E_DisableSunDisk | E_DisableGroundScattering | E_DisableLightShaft,
		E_RenderFlagMax = E_DisableAll + 1,
		E_LightShaftMask = (~E_DisableLightShaft),
	};
}

/** The properties of a atmospheric fog layer which are used for rendering. */
class FAtmosphericFogSceneInfo : public FRenderResource
{
public:
	/** The fog component the scene info is for. */
	const UAtmosphericFogComponent* Component;
	float SunMultiplier;
	float FogMultiplier;
	float InvDensityMultiplier;
	float DensityOffset;
	float GroundOffset;
	float DistanceScale;
	float AltitudeScale;
	float RHeight;
	float StartDistance;
	float DistanceOffset;
	float SunDiscScale;
	FLinearColor DefaultSunColor;
	FVector DefaultSunDirection;
	uint32 RenderFlag;
	uint32 InscatterAltitudeSampleNum;
	class FAtmosphereTextureResource* TransmittanceResource;
	class FAtmosphereTextureResource* IrradianceResource;
	class FAtmosphereTextureResource* InscatterResource;


#if WITH_EDITORONLY_DATA
	/** Atmosphere pre-computation related data */
	bool bNeedRecompute;
	bool bPrecomputationStarted;
	bool bPrecomputationFinished;
	bool bPrecomputationAcceptedByGameThread;
	int32 MaxScatteringOrder;
	int32 AtmospherePhase;
	int32 Atmosphere3DTextureIndex;
	int32 AtmoshpereOrder;
	class FAtmosphereTextures* AtmosphereTextures;
	FByteBulkData PrecomputeTransmittance;
	FByteBulkData PrecomputeIrradiance;
	FByteBulkData PrecomputeInscatter;
#endif

	/** Initialization constructor. */
	explicit FAtmosphericFogSceneInfo(UAtmosphericFogComponent* InComponent, const FScene* InScene);
	~FAtmosphericFogSceneInfo();

#if WITH_EDITOR
	void PrecomputeTextures(FRHICommandListImmediate& RHICmdList, const FViewInfo* View, FSceneViewFamily* ViewFamily);
	void StartPrecompute();

private:
	/** Atmosphere pre-computation related functions */
	FIntPoint GetTextureSize();
	inline void DrawQuad(FRHICommandList& RHICmdList, const FIntRect& ViewRect, FShader* VertexShader);
	void GetLayerValue(int Layer, float& AtmosphereR, FVector4& DhdH);
	void RenderAtmosphereShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FIntRect& ViewRect);
	void PrecomputeAtmosphereData(FRHICommandListImmediate& RHICmdList, const FViewInfo* View, FSceneViewFamily& ViewFamily);

	void ReadPixelsPtr(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget> RenderTarget, FColor* OutData, FIntRect InRect);
	void Read3DPixelsPtr(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget> RenderTarget, FFloat16Color* OutData, FIntRect InRect, FIntPoint InZMinMax);
#endif
};

bool ShouldRenderAtmosphere(const FSceneViewFamily& Family);
void InitAtmosphereConstantsInView(FViewInfo& View);
