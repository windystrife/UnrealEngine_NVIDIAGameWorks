// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * PostProcessVolume:  a post process settings volume
 * Used to affect post process settings in the game and editor.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"
#include "GameFramework/Volume.h"
#include "Engine/BlendableInterface.h"
#include "Engine/Scene.h"
#include "Interfaces/Interface_PostProcessVolume.h"

#include "PostProcessVolume.generated.h"

	// for FPostprocessSettings
UCLASS(autoexpandcategories=PostProcessVolume, hidecategories=(Advanced, Collision, Volume, Brush, Attachment), MinimalAPI)
class APostProcessVolume : public AVolume, public IInterface_PostProcessVolume
{
	GENERATED_UCLASS_BODY()

	/** Post process settings to use for this volume. */
	UPROPERTY(interp, Category=PostProcessVolumeSettings)
	struct FPostProcessSettings Settings;

	/**
	 * Priority of this volume. In the case of overlapping volumes the one with the highest priority
	 * overrides the lower priority ones. The order is undefined if two or more overlapping volumes have the same priority.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PostProcessVolumeSettings)
	float Priority;

	/** World space radius around the volume that is used for blending (only if not unbound).			*/
	UPROPERTY(interp, Category=PostProcessVolumeSettings, meta=(ClampMin = "0.0", UIMin = "0.0", UIMax = "6000.0"))
	float BlendRadius;

	/** 0:no effect, 1:full effect */
	UPROPERTY(interp, Category=PostProcessVolumeSettings, BlueprintReadWrite, meta=(UIMin = "0.0", UIMax = "1.0"))
	float BlendWeight;

	/** Whether this volume is enabled or not.															*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PostProcessVolumeSettings)
	uint32 bEnabled:1;

	/** Whether this volume covers the whole world, or just the area inside its bounds.								*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PostProcessVolumeSettings, meta=(DisplayName = "Infinite Extent (Unbound)"))
	uint32 bUnbound:1;

	//~ Begin IInterface_PostProcessVolume Interface
	ENGINE_API virtual bool EncompassesPoint(FVector Point, float SphereRadius/*=0.f*/, float* OutDistanceToPoint) override;
	ENGINE_API virtual FPostProcessVolumeProperties GetProperties() const override
	{
		FPostProcessVolumeProperties Ret;
		Ret.bIsEnabled = bEnabled != 0;
		Ret.bIsUnbound = bUnbound != 0;
		Ret.BlendRadius = BlendRadius;
		Ret.BlendWeight = BlendWeight;
		Ret.Priority = Priority;
		Ret.Settings = &Settings;
		return Ret;
	}
	//~ End IInterface_PostProcessVolume Interface


	//~ Begin AActor Interface
	virtual void PostUnregisterAllComponents( void ) override;

protected:
	virtual void PostRegisterAllComponents() override;
	//~ End AActor Interface
public:
	
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void Serialize(FArchive& Ar);
#endif // WITH_EDITOR
	//~ End UObject Interface

	/** Adds an Blendable (implements IBlendableInterface) to the array of Blendables (if it doesn't exist) and update the weight */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	ENGINE_API void AddOrUpdateBlendable(TScriptInterface<IBlendableInterface> InBlendableObject, float InWeight = 1.0f) { Settings.AddBlendable(InBlendableObject, InWeight); }
};



