// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Sound/SoundAttenuation.h"
#include "EngineDefines.h"
#include "AudioDevice.h"

/*-----------------------------------------------------------------------------
	USoundAttenuation implementation.
-----------------------------------------------------------------------------*/

void FSoundAttenuationSettings::PostSerialize(const FArchive& Ar)
{
	if (Ar.UE4Ver() < VER_UE4_ATTENUATION_SHAPES)
	{
		FalloffDistance = RadiusMax_DEPRECATED - RadiusMin_DEPRECATED;

		switch(DistanceType_DEPRECATED)
		{
		case SOUNDDISTANCE_Normal:
			AttenuationShape = EAttenuationShape::Sphere;
			AttenuationShapeExtents = FVector(RadiusMin_DEPRECATED, 0.f, 0.f);
			break;

		case SOUNDDISTANCE_InfiniteXYPlane:
			AttenuationShape = EAttenuationShape::Box;
			AttenuationShapeExtents = FVector(WORLD_MAX, WORLD_MAX, RadiusMin_DEPRECATED);
			break;

		case SOUNDDISTANCE_InfiniteXZPlane:
			AttenuationShape = EAttenuationShape::Box;
			AttenuationShapeExtents = FVector(WORLD_MAX, RadiusMin_DEPRECATED, WORLD_MAX);
			break;

		case SOUNDDISTANCE_InfiniteYZPlane:
			AttenuationShape = EAttenuationShape::Box;
			AttenuationShapeExtents = FVector(RadiusMin_DEPRECATED, WORLD_MAX, WORLD_MAX);
			break;
		}
	}
}

float FSoundAttenuationSettings::GetFocusPriorityScale(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const
{
	float Focus = FocusSettings.FocusPriorityScale * FocusPriorityScale;
	float NonFocus = FocusSettings.NonFocusPriorityScale * NonFocusPriorityScale;
	float Result = FMath::Lerp(Focus, NonFocus, FocusFactor);
	return FMath::Max(0.0f, Result);
}

float FSoundAttenuationSettings::GetFocusAttenuation(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const
{
	float Focus = FocusSettings.FocusVolumeScale * FocusVolumeAttenuation;
	float NonFocus = FocusSettings.NonFocusVolumeScale * NonFocusVolumeAttenuation;
	float Result = FMath::Lerp(Focus, NonFocus, FocusFactor);
	return FMath::Max(0.0f, Result);
}

float FSoundAttenuationSettings::GetFocusDistanceScale(const struct FGlobalFocusSettings& FocusSettings, float FocusFactor) const
{
	float Focus = FocusSettings.FocusDistanceScale * FocusDistanceScale;
	float NonFocus = FocusSettings.NonFocusDistanceScale * NonFocusDistanceScale;
	float Result = FMath::Lerp(Focus, NonFocus, FocusFactor);
	return FMath::Max(0.0f, Result);
}

bool FSoundAttenuationSettings::operator==(const FSoundAttenuationSettings& Other) const
{
	return (   bAttenuate			    == Other.bAttenuate
			&& bSpatialize			    == Other.bSpatialize
			&& dBAttenuationAtMax	    == Other.dBAttenuationAtMax
			&& OmniRadius				== Other.OmniRadius
			&& bApplyNormalizationToStereoSounds == Other.bApplyNormalizationToStereoSounds
			&& StereoSpread				== Other.StereoSpread
			&& DistanceAlgorithm	    == Other.DistanceAlgorithm
			&& AttenuationShape		    == Other.AttenuationShape
			&& bAttenuateWithLPF		== Other.bAttenuateWithLPF
			&& LPFRadiusMin				== Other.LPFRadiusMax
			&& FalloffDistance		    == Other.FalloffDistance
			&& AttenuationShapeExtents	== Other.AttenuationShapeExtents
			&& SpatializationAlgorithm == Other.SpatializationAlgorithm
			&& SpatializationPluginSettings == Other.SpatializationPluginSettings
			&& LPFFrequencyAtMax		== Other.LPFFrequencyAtMax
			&& LPFFrequencyAtMin		== Other.LPFFrequencyAtMin
			&& HPFFrequencyAtMax		== Other.HPFFrequencyAtMax
			&& HPFFrequencyAtMin		== Other.HPFFrequencyAtMin
			&& bEnableLogFrequencyScaling == Other.bEnableLogFrequencyScaling
			&& bEnableListenerFocus == Other.bEnableListenerFocus
			&& FocusAzimuth				== Other.FocusAzimuth
			&& NonFocusAzimuth			== Other.NonFocusAzimuth
			&& FocusDistanceScale		== Other.FocusDistanceScale
			&& FocusPriorityScale		== Other.FocusPriorityScale
			&& NonFocusPriorityScale	== Other.NonFocusPriorityScale
			&& FocusVolumeAttenuation	== Other.FocusVolumeAttenuation
			&& NonFocusVolumeAttenuation == Other.NonFocusVolumeAttenuation
			&& OcclusionTraceChannel	== Other.OcclusionTraceChannel
			&& OcclusionLowPassFilterFrequency == Other.OcclusionLowPassFilterFrequency
			&& OcclusionVolumeAttenuation == Other.OcclusionVolumeAttenuation
			&& OcclusionInterpolationTime == Other.OcclusionInterpolationTime
			&& OcclusionPluginSettings	== Other.OcclusionPluginSettings
			&& bEnableReverbSend		== Other.bEnableReverbSend
			&& ReverbPluginSettings		== Other.ReverbPluginSettings
			&& ReverbWetLevelMin		== Other.ReverbWetLevelMin
			&& ReverbWetLevelMax		== Other.ReverbWetLevelMax
			&& ReverbDistanceMin		== Other.ReverbDistanceMin
			&& ReverbDistanceMax		== Other.ReverbDistanceMax);
}

void FSoundAttenuationSettings::CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, AttenuationShapeDetails>& ShapeDetailsMap) const
{
	if (bAttenuate)
	{
		FBaseAttenuationSettings::CollectAttenuationShapesForVisualization(ShapeDetailsMap);
	}
}

USoundAttenuation::USoundAttenuation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
