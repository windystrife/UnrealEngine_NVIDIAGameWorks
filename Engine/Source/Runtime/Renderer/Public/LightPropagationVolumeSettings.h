// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "LightPropagationVolumeSettings.generated.h"

USTRUCT(BlueprintType)
struct RENDERER_API FLightPropagationVolumeSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVIntensity:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVDirectionalOcclusionIntensity:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVDirectionalOcclusionRadius:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVDiffuseOcclusionExponent:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVSpecularOcclusionExponent:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVDiffuseOcclusionIntensity:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVSpecularOcclusionIntensity:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVSize:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVSecondaryOcclusionIntensity:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVSecondaryBounceIntensity:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVGeometryVolumeBias:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVVplInjectionBias:1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVEmissiveInjectionIntensity:1;

	/** How strong the dynamic GI from the LPV should be. 0.0 is off, 1.0 is the "normal" value, but higher values can be used to boost the effect*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVIntensity", UIMin = "0", UIMax = "20", DisplayName = "Intensity") )
	float LPVIntensity;

	/** Bias applied to light injected into the LPV in cell units. Increase to reduce bleeding through thin walls*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVVplInjectionBias", UIMin = "0", UIMax = "2", DisplayName = "Light Injection Bias") )
	float LPVVplInjectionBias;

	/** The size of the LPV volume, in Unreal units*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVSize", UIMin = "100", UIMax = "20000", DisplayName = "Size") )
	float LPVSize;

	/** Secondary occlusion strength (bounce light shadows). Set to 0 to disable*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVSecondaryOcclusionIntensity", UIMin = "0", UIMax = "1", DisplayName = "Secondary Occlusion Intensity") )
	float LPVSecondaryOcclusionIntensity;

	/** Secondary bounce light strength (bounce light shadows). Set to 0 to disable*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVSecondaryBounceIntensity", UIMin = "0", UIMax = "1", DisplayName = "Secondary Bounce Intensity") )
	float LPVSecondaryBounceIntensity;

	/** Bias applied to the geometry volume in cell units. Increase to reduce darkening due to secondary occlusion */
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVGeometryVolumeBias", UIMin = "0", UIMax = "2", DisplayName = "Geometry Volume Bias"))
	float LPVGeometryVolumeBias;

	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVEmissiveInjectionIntensity", UIMin = "0", UIMax = "20", DisplayName = "Emissive Injection Intensity") )
	float LPVEmissiveInjectionIntensity;

	/** Controls the amount of directional occlusion. Requires LPV. Values very close to 1.0 are recommended */
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVDirectionalOcclusionIntensity", UIMin = "0", UIMax = "1", DisplayName = "Occlusion Intensity") )
	float LPVDirectionalOcclusionIntensity;

	/** Occlusion Radius - 16 is recommended for most scenes */
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVDirectionalOcclusionRadius", UIMin = "1", UIMax = "16", DisplayName = "Occlusion Radius") )
	float LPVDirectionalOcclusionRadius;

	/** Diffuse occlusion exponent - increase for more contrast. 1 to 2 is recommended */
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVDiffuseOcclusionExponent", UIMin = "0.5", UIMax = "5", DisplayName = "Diffuse occlusion exponent") )
	float LPVDiffuseOcclusionExponent;

	/** Specular occlusion exponent - increase for more contrast. 6 to 9 is recommended */
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, meta=(editcondition = "bOverride_LPVSpecularOcclusionExponent", UIMin = "1", UIMax = "16", DisplayName = "Specular occlusion exponent") )
	float LPVSpecularOcclusionExponent;

	/** Diffuse occlusion intensity - higher values provide increased diffuse occlusion.*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVDiffuseOcclusionIntensity", UIMin = "0", UIMax = "4", DisplayName = "Diffuse occlusion intensity") )
	float LPVDiffuseOcclusionIntensity;

	/** Specular occlusion intensity - higher values provide increased specular occlusion.*/
	UPROPERTY(interp, BlueprintReadWrite, Category=LightPropagationVolume, AdvancedDisplay, meta=(editcondition = "bOverride_LPVSpecularOcclusionIntensity", UIMin = "0", UIMax = "4", DisplayName = "Specular occlusion intensity") )
	float LPVSpecularOcclusionIntensity;

	/** LPV Fade range - increase to fade more gradually towards the LPV edges.*/
	UPROPERTY(interp, BlueprintReadWrite, Category = LightPropagationVolume, AdvancedDisplay, meta = (editcondition = "bOverride_LPVFadeRange", UIMin = "0", UIMax = "9", DisplayName = "Fade range"))
	float LPVFadeRange;

	/** LPV Directional Occlusion Fade range - increase to fade more gradually towards the LPV edges.*/
	UPROPERTY(interp, BlueprintReadWrite, Category = LightPropagationVolume, AdvancedDisplay, meta = (editcondition = "bOverride_LPVDirectionalOcclusionFadeRange", UIMin = "0", UIMax = "9", DisplayName = "DO Fade range"))
	float LPVDirectionalOcclusionFadeRange;

	// good start values for a new volume
	FLightPropagationVolumeSettings()
	{
		// to set all bOverride_.. by default to false
		FMemory::Memzero(this, sizeof(*this));

		// default values:

		LPVIntensity = 1.0f;
		LPVSize = 5312.0f;
		LPVSecondaryOcclusionIntensity = 0.0f;
		LPVSecondaryBounceIntensity = 0.0f;
		LPVVplInjectionBias = 0.64f;
		LPVGeometryVolumeBias = 0.384f;
		LPVEmissiveInjectionIntensity = 1.0f;
		LPVDirectionalOcclusionIntensity = 0.0f;
		LPVDirectionalOcclusionRadius = 8.0f;
		LPVDiffuseOcclusionExponent = 1.0f;
		LPVSpecularOcclusionExponent = 7.0f;
		LPVDiffuseOcclusionIntensity = 1.0f;
		LPVSpecularOcclusionIntensity = 1.0f;
		LPVFadeRange = 0.0f;
		LPVDirectionalOcclusionFadeRange = 0.0f;
	}

	/**
	 * Used to define the values before any override happens.
	 * Should be as neutral as possible.
	 */		
	void SetBaseValues()
	{
		*this = FLightPropagationVolumeSettings();
	}
	
	// for type safety in FBlendableManager
	static const FName& GetFName()
	{
		static const FName Name = FName(TEXT("FLightPropagationVolumeSettings"));

		return Name;
	}
};
