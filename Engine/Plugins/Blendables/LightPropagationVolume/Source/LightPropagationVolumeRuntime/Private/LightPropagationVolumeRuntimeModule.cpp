// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightPropagationVolumeRuntimeModule.cpp: Module encapsulates the LightPropagationVolume settings
=============================================================================*/

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "LightPropagationVolumeBlendable.h"
#include "SceneView.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, LightPropagationVolumeRuntime);

//////////////////////////////////////////////////////////////////////////
// ULightPropagationVolumeBlendable

ULightPropagationVolumeBlendable::ULightPropagationVolumeBlendable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BlendWeight = 1.0f;
}

#define LERP_PP(NAME) if(Settings.bOverride_ ## NAME)	Dest.NAME = FMath::Lerp(Dest.NAME, Settings.NAME, Weight);

void ULightPropagationVolumeBlendable::OverrideBlendableSettings(FSceneView& View, float InWeight) const
{
	float Weight = FMath::Clamp(BlendWeight, 0.0f, 1.0f) * InWeight;

	FLightPropagationVolumeSettings& Dest = View.FinalPostProcessSettings.BlendableManager.GetSingleFinalData<FLightPropagationVolumeSettings>();
	const FSceneViewFamily* Family = View.Family;
		
	LERP_PP(LPVIntensity);
	LERP_PP(LPVSecondaryOcclusionIntensity);
	LERP_PP(LPVSecondaryBounceIntensity);
	LERP_PP(LPVVplInjectionBias);
	LERP_PP(LPVGeometryVolumeBias);
	LERP_PP(LPVEmissiveInjectionIntensity);
	LERP_PP(LPVDirectionalOcclusionIntensity);
	LERP_PP(LPVDirectionalOcclusionRadius);
	LERP_PP(LPVDiffuseOcclusionExponent);
	LERP_PP(LPVSpecularOcclusionExponent);
	LERP_PP(LPVDiffuseOcclusionIntensity);
	LERP_PP(LPVSpecularOcclusionIntensity);

	if(Settings.bOverride_LPVSize && Weight > 0)
	{
		// we don't want to lerp this property
		Dest.LPVSize = Settings.LPVSize;
	}
}



