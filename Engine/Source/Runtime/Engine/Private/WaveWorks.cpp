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

/*=============================================================================
	WaveWorks.cpp: UWaveWorks implementation.
=============================================================================*/

#include "Engine/WaveWorks.h"
#include "GFSDK_WaveWorks.h"
#include "WaveWorksResource.h"

/*-----------------------------------------------------------------------------
UWaveWorks
-----------------------------------------------------------------------------*/

UWaveWorks::UWaveWorks(const class FObjectInitializer& PCIP)
	: Super(PCIP)
	// waveworks properties
	, TimeScale(1.0f)	
	, DetailLevel(WaveWorksSimulationDetailLevel::High)
	, FFTPeriod(40000)
	, AnisoLevel(4)
	, bReadbackDisplacements(true)
	, WindSpeed(1.0f)
	, WindDirection(FVector2D(1.0f, 1.0f))
	, BeaufortScale(4.0f)
	, WindDependency(0.95f)
	, SmallWaveFraction(0.0f)
	, bUseBeaufortScale(true)
	, WaveAmplitude(0.8f)
	, ChoppyScale(1.2f)
	, FoamGenerationThreshold(0.0f)
	, FoamGenerationAmount(0.8f)
	, FoamDissipationSpeed(0.05f)
	, FoamFalloffSpeed(0.95f)
	// shoreline properties
	, GerstnerSteepness(1.0f)
	, GerstnerParallelity(0.2f)
	, GerstnerWaves(1)
	, MaxPixelsToShoreline(0)
	, FoamTurbulentEnergyMultiplier(3.0f)
	, FoamWaveHatsMultiplier(15.0f)
	, GerstnerAmplitudeMultiplier(1.0f)
	, GerstnerWaveLengthMultiplier(1.0f)
	, GerstnerWaveSpeedMultiplier(1.0f)
	, WaveWorksResource(nullptr)
	// others
	, Time(0.0f)
	, ShorelineTime(0.0f)
	, Settings(new GFSDK_WaveWorks_Simulation_Settings)
	, Params(new GFSDK_WaveWorks_Simulation_Params)
{
	memset(Settings, 0, sizeof(*Settings));
	memset(Params, 0, sizeof(*Params));
}

void UWaveWorks::BeginDestroy()
{
	Super::BeginDestroy();

	delete Settings;
	Settings = nullptr;

	delete Params;
	Params = nullptr;

	// synchronize with the rendering thread by inserting a fence
	if(!ReleaseCodecFence)
	{
		ReleaseCodecFence = new FRenderCommandFence();
	}
	ReleaseCodecFence->BeginFence();
}

bool UWaveWorks::IsReadyForFinishDestroy()
{
	// ready to call FinishDestroy if the codec flushing fence has been hit
	return(
		Super::IsReadyForFinishDestroy() &&
		ReleaseCodecFence &&
		ReleaseCodecFence->IsFenceComplete()
		);
}

void UWaveWorks::FinishDestroy()
{
	ReleaseResource();

	delete ReleaseCodecFence;
	ReleaseCodecFence = NULL;

	Super::FinishDestroy();
}

void UWaveWorks::ReleaseResource()
{
	if (WaveWorksResource)
	{
		// Free the resource.
		ReleaseResourceAndFlush(WaveWorksResource);
		WaveWorksResource = nullptr;
	}
}

void UWaveWorks::UpdateResource()
{
	// Release the existing texture resource.
	ReleaseResource();

	//Dedicated servers have no texture internals
	if (FApp::CanEverRender() && !HasAnyFlags(RF_ClassDefaultObject))
	{
		// Create a new texture resource.
		WaveWorksResource = new FWaveWorksResource(this);
		BeginInitResource(WaveWorksResource);
	}
}

#if WITH_EDITOR
void UWaveWorks::PreEditChange(UProperty* PropertyAboutToChange)
{
	// this will release the FWaveWorksResource
	Super::PreEditChange(PropertyAboutToChange);

	// synchronize with the rendering thread by flushing all render commands
	FlushRenderingCommands();
}

void UWaveWorks::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// this will recreate the FWaveWorksResource
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UWaveWorks::UpdateProperties()
{
}

bool UWaveWorks::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		// shoreline paramters
		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, GerstnerSteepness)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, GerstnerWaves)			
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, FoamTurbulentEnergyMultiplier)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, FoamWaveHatsMultiplier)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, GerstnerAmplitudeMultiplier)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, GerstnerWaveLengthMultiplier)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, GerstnerWaveSpeedMultiplier)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, GerstnerParallelity)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, MaxPixelsToShoreline)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, ShorelineCaptureOrthoSize)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, ShorelineCapturePosition)
			)
		{
			return bUseShoreline;
		}
		// waveworks simulation parameters
		else if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, WaveAmplitude)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, ChoppyScale)			
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, WindSpeed)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, FoamGenerationThreshold)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, FoamGenerationAmount)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, FoamDissipationSpeed)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWaveWorks, FoamFalloffSpeed)
			)
		{
			return !bUseBeaufortScale;
		}
	}

	return true;
}
#endif // WITH_EDITOR


void UWaveWorks::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	UpdateProperties();
#endif

	// we won't initialize this on build machines
	if ( !HasAnyFlags(RF_ClassDefaultObject) && !GIsBuildMachine)
	{
		// recreate the FWaveWorksResource
		this->UpdateResource();
	}
}

void UWaveWorks::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

FString UWaveWorks::GetDesc()
{
	return FString::Printf( TEXT("WaveWorks") );
}

void UWaveWorks::Tick(float DeltaTime)
{
	Time += DeltaTime;
	ShorelineTime += DeltaTime * TimeScale;
}

bool UWaveWorks::IsTickable() const
{
	return true;
}

bool UWaveWorks::IsTickableInEditor() const
{
	return true;
}

TStatId UWaveWorks::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWaveWorks, STATGROUP_Tickables);
}

class FWaveWorksResource* UWaveWorks::GetWaveWorksResource()
{
	FWaveWorksResource* Result = NULL;
	if( WaveWorksResource && WaveWorksResource->IsInitialized() )
	{
		Result = (FWaveWorksResource*)WaveWorksResource;
	}
	return Result;
}

bool UWaveWorks::IsPropertiesChanged() const
{
	return Settings->detail_level != (GFSDK_WaveWorks_Simulation_DetailLevel)(int)DetailLevel
		|| Settings->fft_period != FFTPeriod
		|| Settings->readback_displacements != bReadbackDisplacements
		|| Settings->aniso_level != AnisoLevel
		|| Settings->use_Beaufort_scale != bUseBeaufortScale
		|| Params->wave_amplitude != WaveAmplitude
		|| Params->wind_dir.x != -WindDirection.X
		|| Params->wind_dir.y != -WindDirection.Y
		|| Params->wind_speed != (bUseBeaufortScale ? BeaufortScale : WindSpeed)
		|| Params->wind_dependency != WindDependency
		|| Params->choppy_scale != ChoppyScale
		|| Params->small_wave_fraction != SmallWaveFraction
		|| Params->time_scale != TimeScale
		|| Params->foam_generation_threshold != FoamGenerationThreshold
		|| Params->foam_generation_amount != FoamGenerationAmount
		|| Params->foam_dissipation_speed != FoamDissipationSpeed
		|| Params->foam_falloff_speed != FoamFalloffSpeed;
}

const GFSDK_WaveWorks_Simulation_Settings& UWaveWorks::GetSettings() const
{
	Settings->fft_period = FFTPeriod;
	Settings->detail_level = (GFSDK_WaveWorks_Simulation_DetailLevel)(int)DetailLevel;	
	Settings->readback_displacements = bReadbackDisplacements;
	Settings->num_readback_FIFO_entries = (bReadbackDisplacements) ? 4 : 0;
	Settings->aniso_level = (gfsdk_U8)AnisoLevel;
	Settings->CPU_simulation_threading_model = GFSDK_WaveWorks_Simulation_CPU_Threading_Model_Automatic;
	Settings->use_Beaufort_scale = bUseBeaufortScale;				
	Settings->num_GPUs = 1;
	Settings->enable_CUDA_timers = true;
	Settings->enable_gfx_timers = true;
	Settings->enable_CPU_timers = true;

	return *Settings;
}

const GFSDK_WaveWorks_Simulation_Params& UWaveWorks::GetParams() const
{
	Params->time_scale = TimeScale;
	Params->wave_amplitude = WaveAmplitude;
	Params->wind_dir = { -WindDirection.X, -WindDirection.Y };
	Params->wind_speed = (bUseBeaufortScale) ? BeaufortScale : WindSpeed;
	Params->wind_dependency = WindDependency;
	Params->choppy_scale = ChoppyScale;
	Params->small_wave_fraction = SmallWaveFraction;	
	Params->foam_generation_threshold = FoamGenerationThreshold;
	Params->foam_generation_amount = FoamGenerationAmount;
	Params->foam_dissipation_speed = FoamDissipationSpeed;
	Params->foam_falloff_speed = FoamFalloffSpeed;

	return *Params;
}


/*-----------------------------------------------------------------------------
FWaveWorksResource
-----------------------------------------------------------------------------*/

/**
* Initializes the dynamic RHI resource and/or RHI render target used by this resource.
* Called when the resource is initialized, or when reseting all RHI resources.
* This is only called by the rendering thread.
*/
void FWaveWorksResource::InitDynamicRHI()
{
	WaveWorksRHI = GDynamicRHI->RHIGetDefaultContext()->RHICreateWaveWorks(Owner->GetSettings(), Owner->GetParams());		
}

/**
* Release the dynamic RHI resource and/or RHI render target used by this resource.
* Called when the resource is released, or when reseting all RHI resources.
* This is only called by the rendering thread.
*/
void FWaveWorksResource::ReleaseDynamicRHI()
{
	// release the FTexture RHI resources here as well
	ReleaseRHI();

	WaveWorksRHI.SafeRelease();
	WaveWorksShorelineUniformBuffer.SafeRelease();	
}

void FWaveWorksResource::CustomAddToDeferredUpdateList()
{
	if (!bAddedToDeferredUpdateList)
	{
		AddToDeferredUpdateList(false);
		bAddedToDeferredUpdateList = true;
	}		
}

void FWaveWorksResource::CustomRemoveFromDeferredUpdateList()
{
	if (bAddedToDeferredUpdateList)
	{
		RemoveFromDeferredUpdateList();
		bAddedToDeferredUpdateList = false;
	}	
}

/**
* Updates the WaveWorks simulation.
* This is only called by the rendering thread.
*/
void FWaveWorksResource::UpdateDeferredResource(FRHICommandListImmediate & CMLI, bool bClearRenderTarget)
{
	if (WaveWorksRHI->Simulation)
	{
		if (Owner->IsPropertiesChanged())
			GFSDK_WaveWorks_Simulation_UpdateProperties(WaveWorksRHI->Simulation, Owner->GetSettings(), Owner->GetParams());
		
		WaveWorksRHI->UpdateTick(Owner->GetTime());
	}	
}

float FWaveWorksResource::GetGerstnerAmplitude()
{
	float amplitude = 0.0f;

	if (WaveWorksRHI->Simulation)
		amplitude = GFSDK_WaveWorks_Simulation_GetConservativeMaxDisplacementEstimate(WaveWorksRHI->Simulation) / 4.0;

	return amplitude;
}