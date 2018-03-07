/*
* This code contains NVIDIA Confidential Information and is disclosed
* under the Mutual Non-Disclosure Agreement.
*
* Notice
* ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES
* NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
* THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT,
* MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
*
* NVIDIA Corporation assumes no responsibility for the consequences of use of such
* information or for any infringement of patents or other rights of third parties that may
* result from its use. No license is granted by implication or otherwise under any patent
* or patent rights of NVIDIA Corporation. No third party distribution is allowed unless
* expressly authorized by NVIDIA.  Details are subject to change without notice.
* This code supersedes and replaces all information previously supplied.
* NVIDIA Corporation products are not authorized for use as critical
* components in life support devices or systems without express written approval of
* NVIDIA Corporation.
*
* Copyright ?2008- 2016 NVIDIA Corporation. All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and proprietary
* rights in and to this software and related documentation and any modifications thereto.
* Any use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation is
* strictly prohibited.
*/

/**
 * WaveWorks
 * WaveWorks support base class. Used to create WaveWorks Asset
 */

#pragma once
#include "Tickable.h"
#include "WaveWorks.generated.h"

UENUM()
namespace WaveWorksSimulationDetailLevel
{
	enum Type
	{
		Normal, 
		High, 
		Extreme
	};
}

UCLASS(hidecategories=Object,BlueprintType,MinimalAPI)
class UWaveWorks : public UObject, public FTickableGameObject
{
	GENERATED_UCLASS_BODY()

public:		
	/** The detail level of the simulation: this drives the resolution of the FFT and also determines whether the simulation workload is done */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Simulation)
	TEnumAsByte<WaveWorksSimulationDetailLevel::Type> DetailLevel;

	/** The repeat interval for the fft simulation, in world units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Simulation, meta = (ClampMin = "1.0"))
	float FFTPeriod;

	/** True if wind_speed in  GFSDK_WaveWorks_Simulation_Params should accept Beaufort scale value,
	False if wind_speed in GFSDK_WaveWorks_Simulation_Params should accept meters/second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Simulation)
	bool bUseBeaufortScale;

	/** Should the displacement data be read back to the CPU? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Simulation)
	bool bReadbackDisplacements;

	/** Set max aniso degree for sampling of gradient maps */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Simulation, meta = (ClampMin = "0"))
	int32 AnisoLevel;

	/** The global time multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta = (ClampMin = "0.0"))
	float TimeScale;

	/** The direction of the wind inducing the waves */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters)
	FVector2D WindDirection;

	/** The speed of the wind inducing the waves. it is interpreted as metres per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta = (ClampMin = "0.0"))
	float WindSpeed;

	/** The beaufort scale when bUseBeaufortScale is true */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta = (ClampMin = "0.0"))
	float BeaufortScale;

	/** The degree to which waves appear to move in the wind direction (vs. standing waves), in the [0,1] range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WindDependency;

	/** The simulation spectrum is low-pass filtered to eliminate wavelengths that could end up under-sampled, this controls
	how much of the frequency range is considered 'high frequency' (i.e. small wave). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SmallWaveFraction;

	/** Global scale factor for simulated wave amplitude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta = (ClampMin = "0.0"))
	float WaveAmplitude;

	/** In addition to height displacements, the simulation also applies lateral displacements. This controls the non-linearity
	and therefore 'choppiness' in the resulting wave shapes. Should normally be set in the [0,1] range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta = (ClampMin = "0.0"))
	float ChoppyScale;

	/** The turbulent energy representing foam and bubbles spread in water starts generating on the tips of the waves if
	Jacobian of wave curvature gets higher than this threshold. The range is [0,1], the typical values are [0.2,0.4] range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FoamGenerationThreshold;

	/** The amount of turbulent energy injected in areas defined by foam_generation_threshold parameter on each simulation step.
	The range is [0,1], the typical values are [0,0.1] range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FoamGenerationAmount;

	/** The speed of spatial dissipation of turbulent energy. The range is [0,1], the typical values are in [0.5,1] range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FoamDissipationSpeed;

	/** In addition to spatial dissipation, the turbulent energy dissolves over time. This parameter sets the speed of
	dissolving over time. The range is [0,1], the typical values are in [0.9,0.99] range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Parameters, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FoamFalloffSpeed;

	/** True if use shoreline effect */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline)
	bool bUseShoreline;

	/** Shoreline distance field texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline)
	class UTexture2D* ShorelineDistanceFieldTexture;

	/** Gerstner waves steepness */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline, meta = (ClampMin = "0.0"))
	float GerstnerSteepness;

	/** Gerstner waves parallelity, 0 is parrel to the shoreline, 1 is wind direction , range(0 - 1)*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GerstnerParallelity;

	/** Gerstner waves count */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline, meta = (ClampMin = "1"))
	int GerstnerWaves;

	/** Shoreline foam turbulence energy multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline, meta = (ClampMin = "0.0"))
	float FoamTurbulentEnergyMultiplier;

	/** Shoreline wave hats energy multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline, meta = (ClampMin = "0.0"))
	float FoamWaveHatsMultiplier;	

	/** Shoreline wave amplitude multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline, meta = (ClampMin = "0.0"))
	float GerstnerAmplitudeMultiplier;

	/** Shoreline wave length multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline, meta = (ClampMin = "0.0"))
	float GerstnerWaveLengthMultiplier;

	/** Shoreline wave speed multiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline, meta = (ClampMin = "0.0"))
	float GerstnerWaveSpeedMultiplier;

	/** Shoreline max pixels to shoreline */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline, meta = (ClampMin = "0"))
	int MaxPixelsToShoreline;

	/** Shoreline capture ortho size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline, meta = (ClampMin = "0"))
	float ShorelineCaptureOrthoSize;

	/** Shoreline capture position */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shoreline, meta = (ClampMin = "0"))
	FVector ShorelineCapturePosition;

public:
	// Begin UObject interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
private:
	void UpdateProperties();
public:
#endif // WITH_EDITOR	

	virtual bool IsReadyForFinishDestroy() override;
	virtual void FinishDestroy() override;
	virtual FString GetDesc() override;
	// End UObject interface.

	// Begin FTickableGameObject interface.
	virtual void Tick(float DeltaTime);
	virtual bool IsTickable() const;
	virtual bool IsTickableInEditor() const;
	virtual TStatId GetStatId() const;
	// End FTickableGameObject interface

	/**
	* Is WaveWorks's parameters changed
	*/
	bool IsPropertiesChanged() const;

	/**
	 * Access the WaveWorks target resource for this movie texture object
	 *
	 * @return pointer to resource or NULL if not initialized
	 */
	class FWaveWorksResource* GetWaveWorksResource();
	
	/**
	* Access the WaveWorks Simulation Settings
	*
	* @return Simulation Settings
	*/
	const struct GFSDK_WaveWorks_Simulation_Settings& GetSettings() const;

	/**
	* Access the WaveWorks Simulation Parameters
	*
	* @return Simulation Parameters
	*/
	const struct GFSDK_WaveWorks_Simulation_Params& GetParams() const;

	/**
	* Access the WaveWorks Shoreline time
	*
	* @return shoreline running time
	*/
	const float GetShorelineTime() const { return ShorelineTime; }

	/**
	* Access the WaveWorks time
	*
	* @return waveworks running time
	*/
	const float GetTime() const { return Time; }

	/**
	* Resets the resource for the texture.
	*/
	ENGINE_API void ReleaseResource();

	/**
	* Creates a new resource for the texture, and updates any cached references to the resource.
	*/
	virtual void UpdateResource();

	/** WaveWorks resource */
	class FWaveWorksResource* WaveWorksResource;

private:
	/** The waveworks simulation settings */
	struct GFSDK_WaveWorks_Simulation_Settings* Settings;

	/** The waveworks simulation params */
	struct GFSDK_WaveWorks_Simulation_Params* Params;

	/** The time */
	float Time;
	float ShorelineTime;

	/** Set in order to synchronize codec access to this WaveWorks resource from the render thread */
	FRenderCommandFence* ReleaseCodecFence;
};