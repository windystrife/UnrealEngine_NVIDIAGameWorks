// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
#include "Serialization/BulkData.h"
#include "AtmosphericFogComponent.generated.h"

/** Structure storing Data for pre-computation */
USTRUCT(BlueprintType)
struct FAtmospherePrecomputeParameters
{
	GENERATED_USTRUCT_BODY()
	
	/** Rayleigh scattering density height scale, ranges from [0...1] */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AtmosphereParam)
	float DensityHeight;

	UPROPERTY()
	float DecayHeight_DEPRECATED;

	/** Maximum scattering order */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AtmosphereParam)
	int32 MaxScatteringOrder;

	/** Transmittance Texture Width */
	UPROPERTY()
	int32 TransmittanceTexWidth;

	/** Transmittance Texture Height */
	UPROPERTY()
	int32 TransmittanceTexHeight;

	/** Irradiance Texture Width */
	UPROPERTY()
	int32 IrradianceTexWidth;

	/** Irradiance Texture Height */
	UPROPERTY()
	int32 IrradianceTexHeight;

	/** Number of different altitudes at which to sample inscatter color (size of 3D texture Z dimension)*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=AtmosphereParam) 
	int32 InscatterAltitudeSampleNum;

	/** Inscatter Texture Height */
	UPROPERTY() 
	int32 InscatterMuNum;

	/** Inscatter Texture Width */
	UPROPERTY()
	int32 InscatterMuSNum;

	/** Inscatter Texture Width */
	UPROPERTY()
	int32 InscatterNuNum;

	FAtmospherePrecomputeParameters();

	bool operator == ( const FAtmospherePrecomputeParameters& Other ) const
	{
		return (DensityHeight == Other.DensityHeight)
			&& (MaxScatteringOrder == Other.MaxScatteringOrder)
			&& (TransmittanceTexWidth == Other.TransmittanceTexWidth)
			&& (TransmittanceTexHeight == Other.TransmittanceTexHeight)
			&& (IrradianceTexWidth == Other.IrradianceTexWidth)
			&& (IrradianceTexHeight == Other.IrradianceTexHeight)
			&& (InscatterAltitudeSampleNum == Other.InscatterAltitudeSampleNum)
			&& (InscatterMuNum == Other.InscatterMuNum)
			&& (InscatterMuSNum == Other.InscatterMuSNum)
			&& (InscatterNuNum == Other.InscatterNuNum);
	}

	bool operator != ( const FAtmospherePrecomputeParameters& Other ) const
	{
		return (DensityHeight != Other.DensityHeight)
			|| (MaxScatteringOrder != Other.MaxScatteringOrder)
			|| (TransmittanceTexWidth != Other.TransmittanceTexWidth)
			|| (TransmittanceTexHeight != Other.TransmittanceTexHeight)
			|| (IrradianceTexWidth != Other.IrradianceTexWidth)
			|| (IrradianceTexHeight != Other.IrradianceTexHeight)
			|| (InscatterAltitudeSampleNum != Other.InscatterAltitudeSampleNum)
			|| (InscatterMuNum != Other.InscatterMuNum)
			|| (InscatterMuSNum != Other.InscatterMuSNum)
			|| (InscatterNuNum != Other.InscatterNuNum);
	}
};

/**
 *	Used to create fogging effects such as clouds.
 */
UCLASS(ClassGroup=Rendering, collapsecategories, hidecategories=(Object, Mobility, Activation, "Components|Activation"), editinlinenew, meta=(BlueprintSpawnableComponent), MinimalAPI)
class UAtmosphericFogComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	~UAtmosphericFogComponent();

	/** Global scattering factor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category=Atmosphere)
	float SunMultiplier;

	/** Scattering factor on object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category=Atmosphere)
	float FogMultiplier;

	/** Fog density control factor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category=Atmosphere)
	float DensityMultiplier;

	/** Fog density offset to control opacity [-1.f ~ 1.f]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category=Atmosphere)
	float DensityOffset;

	/** Distance scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category=Atmosphere)
	float DistanceScale;

	/** Altitude scale (only Z scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category=Atmosphere)
	float AltitudeScale;

	/** Distance offset, in km (to handle large distance) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category=Atmosphere)
	float DistanceOffset;

	/** Ground offset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category=Atmosphere)
	float GroundOffset;

	/** Start Distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category=Atmosphere)
	float StartDistance;

	/** Distance offset, in km (to handle large distance) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category = Atmosphere)
	float SunDiscScale;

	/** Default light brightness. Used when there is no sunlight placed in the level. Unit is lumens */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category=Lighting)
	float DefaultBrightness;

	/** Default light color. Used when there is no sunlight placed in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category=Lighting)
	FColor DefaultLightColor;

	/** Disable Sun Disk rendering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category=Lighting)
	uint32 bDisableSunDisk : 1;

	/** Disable Color scattering from ground. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category=Lighting)
	uint32 bDisableGroundScattering : 1;

	/** Set brightness of the light */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void SetDefaultBrightness(float NewBrightness);

	/** Set color of the light */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void SetDefaultLightColor(FLinearColor NewLightColor);

	/** Set SunMultiplier */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void SetSunMultiplier(float NewSunMultiplier);

	/** Set FogMultiplier */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void SetFogMultiplier(float NewFogMultiplier);

	/** Set DensityMultiplier */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void SetDensityMultiplier(float NewDensityMultiplier);

	/** Set DensityOffset */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void SetDensityOffset(float NewDensityOffset);

	/** Set DistanceScale */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void SetDistanceScale(float NewDistanceScale);

	/** Set AltitudeScale */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void SetAltitudeScale(float NewAltitudeScale);

	/** Set StartDistance */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void SetStartDistance(float NewStartDistance);

	/** Set DistanceOffset */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void SetDistanceOffset(float NewDistanceOffset);

	/** Set DisableSunDisk */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void DisableSunDisk(bool NewSunDisk);

	/** Set DisableGroundScattering */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void DisableGroundScattering(bool NewGroundScattering);

	/** Set PrecomputeParams, only valid in Editor mode */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	ENGINE_API void SetPrecomputeParams(float DensityHeight, int32 MaxScatteringOrder, int32 InscatterAltitudeSampleNum);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Atmosphere)
	FAtmospherePrecomputeParameters PrecomputeParams;

public:
	UPROPERTY()
	class UTexture2D* TransmittanceTexture_DEPRECATED;

	UPROPERTY()
	class UTexture2D* IrradianceTexture_DEPRECATED;

	enum EPrecomputeState
	{
		EInvalid = 0,
		EValid = 2,
	};

	// this is mostly a legacy thing, it is only modified by the game thread
	uint32 PrecomputeCounter;
	// When non-zero, the component should flush rendering commands and see if there is any atmosphere stuff to deal with, then decrement it
	mutable FThreadSafeCounter GameThreadServiceRequest;

	UFUNCTION(BlueprintCallable, Category="Rendering|Components|AtmosphericFog")
	void StartPrecompute();	

protected:
	//~ Begin UActorComponent Interface.
	virtual void CreateRenderState_Concurrent() override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	//~ End UActorComponent Interface.

	 void AddFogIfNeeded();

public:
	/** The resource for Inscatter. */
	class FAtmosphereTextureResource* TransmittanceResource;
	class FAtmosphereTextureResource* IrradianceResource;
	class FAtmosphereTextureResource* InscatterResource;
	
	/** Source vector data. */
	mutable FByteBulkData TransmittanceData;
	mutable FByteBulkData IrradianceData;
	mutable FByteBulkData InscatterData;
	
	//~ Begin UObject Interface. 
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void UpdatePrecomputedData();
#endif // WITH_EDITOR
	virtual void PostInterpChange(UProperty* PropertyThatChanged) override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	ENGINE_API void InitResource();
	ENGINE_API void ReleaseResource();

	//~ Begin UActorComponent Interface.
	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;
	//~ End UActorComponent Interface.

	void ApplyComponentInstanceData(class FAtmospherePrecomputeInstanceData* ComponentInstanceData);
	const FAtmospherePrecomputeParameters& GetPrecomputeParameters() const { return PrecomputeParams;  }

private:
#if WITH_EDITORONLY_DATA
	class FAtmospherePrecomputeDataHandler* PrecomputeDataHandler;
#endif

	friend class FAtmosphericFogSceneInfo;
};

