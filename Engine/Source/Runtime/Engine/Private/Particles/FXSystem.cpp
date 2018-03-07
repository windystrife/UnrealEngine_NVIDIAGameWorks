// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FXSystem.cpp: Implementation of the effects system.
=============================================================================*/

#include "FXSystem.h"
#include "RenderingThread.h"
#include "VectorField.h"
#include "Particles/FXSystemPrivate.h"
#include "GPUSort.h"
#include "Particles/ParticleCurveTexture.h"
#include "VectorField/VectorField.h"
#include "Components/VectorFieldComponent.h"

/*-----------------------------------------------------------------------------
	External FX system interface.
-----------------------------------------------------------------------------*/

FFXSystemInterface* FFXSystemInterface::Create(ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform)
{
	return new FFXSystem(InFeatureLevel, InShaderPlatform);
}

void FFXSystemInterface::Destroy( FFXSystemInterface* FXSystem )
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FDestroyFXSystemCommand,
		FFXSystemInterface*, FXSystem, FXSystem,
	{
		delete FXSystem;
	});
}

FFXSystemInterface::~FFXSystemInterface()
{
}

/*------------------------------------------------------------------------------
	FX system console variables.
------------------------------------------------------------------------------*/

namespace FXConsoleVariables
{
	int32 VisualizeGPUSimulation = 0;
	int32 bAllowGPUSorting = true;
	int32 bAllowCulling = true;
	int32 bFreezeGPUSimulation = false;
	int32 bFreezeParticleSimulation = false;
	int32 bAllowAsyncTick = !WITH_EDITOR;
	float ParticleSlackGPU = 0.02f;
	int32 MaxParticleTilePreAllocation = 100;

#if WITH_FLEX	
	int32 MaxCPUParticlesPerEmitter = 16*1024;
#else
	int32 MaxCPUParticlesPerEmitter = 1000;
#endif

	int32 MaxGPUParticlesSpawnedPerFrame = 1024 * 1024;
	int32 GPUSpawnWarningThreshold = 20000;
	float GPUCollisionDepthBounds = 500.0f;
	TAutoConsoleVariable<int32> TestGPUSort(TEXT("FX.TestGPUSort"),0,TEXT("Test GPU sort. 1: Small, 2: Large, 3: Exhaustive, 4: Random"),ECVF_Cheat);

	/** Register references to flags. */
	FAutoConsoleVariableRef CVarVisualizeGPUSimulation(
		TEXT("FX.VisualizeGPUSimulation"),
		VisualizeGPUSimulation,
		TEXT("Visualize the current state of GPU simulation.\n")
		TEXT("0 = off\n")
		TEXT("1 = visualize particle state\n")
		TEXT("2 = visualize curve texture"),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarAllowGPUSorting(
		TEXT("FX.AllowGPUSorting"),
		bAllowGPUSorting,
		TEXT("Allow particles to be sorted on the GPU."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarFreezeGPUSimulation(
		TEXT("FX.FreezeGPUSimulation"),
		bFreezeGPUSimulation,
		TEXT("Freeze particles simulated on the GPU."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarFreezeParticleSimulation(
		TEXT("FX.FreezeParticleSimulation"),
		bFreezeParticleSimulation,
		TEXT("Freeze particle simulation."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarAllowAsyncTick(
		TEXT("FX.AllowAsyncTick"),
		bAllowAsyncTick,
		TEXT("allow parallel ticking of particle systems."),
		ECVF_Default
		);
	FAutoConsoleVariableRef CVarParticleSlackGPU(
		TEXT("FX.ParticleSlackGPU"),
		ParticleSlackGPU,
		TEXT("Amount of slack to allocate for GPU particles to prevent tile churn as percentage of total particles."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarMaxParticleTilePreAllocation(
		TEXT("FX.MaxParticleTilePreAllocation"),
		MaxParticleTilePreAllocation,
		TEXT("Maximum tile preallocation for GPU particles."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarMaxCPUParticlesPerEmitter(
		TEXT("FX.MaxCPUParticlesPerEmitter"),
		MaxCPUParticlesPerEmitter,
		TEXT("Maximum number of CPU particles allowed per-emitter.")
		);
	FAutoConsoleVariableRef CVarMaxGPUParticlesSpawnedPerFrame(
		TEXT("FX.MaxGPUParticlesSpawnedPerFrame"),
		MaxGPUParticlesSpawnedPerFrame,
		TEXT("Maximum number of GPU particles allowed to spawn per-frame per-emitter.")
		);
	FAutoConsoleVariableRef CVarGPUSpawnWarningThreshold(
		TEXT("FX.GPUSpawnWarningThreshold"),
		GPUSpawnWarningThreshold,
		TEXT("Warning threshold for spawning of GPU particles."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarGPUCollisionDepthBounds(
		TEXT("FX.GPUCollisionDepthBounds"),
		GPUCollisionDepthBounds,
		TEXT("Limits the depth bounds when searching for a collision plane."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarAllowCulling(
		TEXT("FX.AllowCulling"),
		bAllowCulling,
		TEXT("Allow emitters to be culled."),
		ECVF_Cheat
		);
	int32 bAllowGPUParticles = true;
	FAutoConsoleVariableRef CVarAllowGPUParticles(
		TEXT("FX.AllowGPUParticles"),
		bAllowGPUParticles,
		TEXT("If true, allow the usage of GPU particles.")
	);
}

/*------------------------------------------------------------------------------
	FX system.
------------------------------------------------------------------------------*/

FFXSystem::FFXSystem(ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform)
	: ParticleSimulationResources(NULL)
	, FeatureLevel(InFeatureLevel)
	, ShaderPlatform(InShaderPlatform)
#if WITH_EDITOR
	, bSuspended(false)
#endif // #if WITH_EDITOR
{
	InitGPUSimulation();
}

FFXSystem::~FFXSystem()
{
	DestroyGPUSimulation();
}

void FFXSystem::Tick(float DeltaSeconds)
{
	if (RHISupportsGPUParticles())
	{
		// Test GPU sorting if requested.
		if (FXConsoleVariables::TestGPUSort.GetValueOnGameThread() != 0)
		{
			TestGPUSort((EGPUSortTest)FXConsoleVariables::TestGPUSort.GetValueOnGameThread(), GetFeatureLevel());
			// Reset CVar
			static IConsoleVariable* CVarTestGPUSort = IConsoleManager::Get().FindConsoleVariable(TEXT("FX.TestGPUSort"));

			// todo: bad use of console variables, this should be a console command 
			CVarTestGPUSort->Set(0, ECVF_SetByCode);
		}

		// Before ticking GPU particles, ensure any pending curves have been
		// uploaded.
		GParticleCurveTexture.SubmitPendingCurves();
	}
}

#if WITH_EDITOR
void FFXSystem::Suspend()
{
	if (!bSuspended && RHISupportsGPUParticles())
	{
		ReleaseGPUResources();
		bSuspended = true;
	}
}

void FFXSystem::Resume()
{
	if (bSuspended && RHISupportsGPUParticles())
	{
		bSuspended = false;
		InitGPUResources();
	}
}
#endif // #if WITH_EDITOR

/*------------------------------------------------------------------------------
	Vector field instances.
------------------------------------------------------------------------------*/

void FFXSystem::AddVectorField( UVectorFieldComponent* VectorFieldComponent )
{
	if (RHISupportsGPUParticles())
	{
		check( VectorFieldComponent->VectorFieldInstance == NULL );
		check( VectorFieldComponent->FXSystem == this );

		if ( VectorFieldComponent->VectorField )
		{
			FVectorFieldInstance* Instance = new FVectorFieldInstance();
			VectorFieldComponent->VectorField->InitInstance(Instance, /*bPreviewInstance=*/ false);
			VectorFieldComponent->VectorFieldInstance = Instance;
			Instance->WorldBounds = VectorFieldComponent->Bounds.GetBox();
			Instance->Intensity = VectorFieldComponent->Intensity;
			Instance->Tightness = VectorFieldComponent->Tightness;

			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
				FAddVectorFieldCommand,
				FFXSystem*, FXSystem, this,
				FVectorFieldInstance*, Instance, Instance,
				FMatrix, ComponentToWorld, VectorFieldComponent->GetComponentTransform().ToMatrixWithScale(),
			{
				Instance->UpdateTransforms( ComponentToWorld );
				Instance->Index = FXSystem->VectorFields.AddUninitialized().Index;
				FXSystem->VectorFields[ Instance->Index ] = Instance;
			});
		}
	}
}

void FFXSystem::RemoveVectorField( UVectorFieldComponent* VectorFieldComponent )
{
	if (RHISupportsGPUParticles())
	{
		check( VectorFieldComponent->FXSystem == this );

		FVectorFieldInstance* Instance = VectorFieldComponent->VectorFieldInstance;
		VectorFieldComponent->VectorFieldInstance = NULL;

		if ( Instance )
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				FRemoveVectorFieldCommand,
				FFXSystem*, FXSystem, this,
				FVectorFieldInstance*, Instance, Instance,
			{
				if ( Instance->Index != INDEX_NONE )
				{
					FXSystem->VectorFields.RemoveAt( Instance->Index );
					delete Instance;
				}
			});
		}
	}
}

void FFXSystem::UpdateVectorField( UVectorFieldComponent* VectorFieldComponent )
{
	if (RHISupportsGPUParticles())
	{
		check( VectorFieldComponent->FXSystem == this );

		FVectorFieldInstance* Instance = VectorFieldComponent->VectorFieldInstance;

		if ( Instance )
		{
			struct FUpdateVectorFieldParams
			{
				FBox Bounds;
				FMatrix ComponentToWorld;
				float Intensity;
				float Tightness;
			};

			FUpdateVectorFieldParams UpdateParams;
			UpdateParams.Bounds = VectorFieldComponent->Bounds.GetBox();
			UpdateParams.ComponentToWorld = VectorFieldComponent->GetComponentTransform().ToMatrixWithScale();
			UpdateParams.Intensity = VectorFieldComponent->Intensity;
			UpdateParams.Tightness = VectorFieldComponent->Tightness;

			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
				FUpdateVectorFieldCommand,
				FFXSystem*, FXSystem, this,
				FVectorFieldInstance*, Instance, Instance,
				FUpdateVectorFieldParams, UpdateParams, UpdateParams,
			{
				Instance->WorldBounds = UpdateParams.Bounds;
				Instance->Intensity = UpdateParams.Intensity;
				Instance->Tightness = UpdateParams.Tightness;
				Instance->UpdateTransforms( UpdateParams.ComponentToWorld );
			});
		}
	}
}

/*-----------------------------------------------------------------------------
	Render related functionality.
-----------------------------------------------------------------------------*/

void FFXSystem::DrawDebug( FCanvas* Canvas )
{
	if (FXConsoleVariables::VisualizeGPUSimulation > 0
		&& RHISupportsGPUParticles())
	{
		VisualizeGPUParticles(Canvas);
	}
}

void FFXSystem::PreInitViews()
{
	if (RHISupportsGPUParticles())
	{
		AdvanceGPUParticleFrame();
	}
}

bool FFXSystem::UsesGlobalDistanceField() const
{
	if (RHISupportsGPUParticles())
	{
		return UsesGlobalDistanceFieldInternal();
	}

	return false;
}

DECLARE_STATS_GROUP(TEXT("Command List Markers"), STATGROUP_CommandListMarkers, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("FXPreRender_Prepare"), STAT_CLM_FXPreRender_Prepare, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("FXPreRender_Simulate"), STAT_CLM_FXPreRender_Simulate, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("FXPreRender_Finalize"), STAT_CLM_FXPreRender_Finalize, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("FXPreRender_PrepareCDF"), STAT_CLM_FXPreRender_PrepareCDF, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("FXPreRender_SimulateCDF"), STAT_CLM_FXPreRender_SimulateCDF, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("FXPreRender_FinalizeCDF"), STAT_CLM_FXPreRender_FinalizeCDF, STATGROUP_CommandListMarkers);


void FFXSystem::PreRender(FRHICommandListImmediate& RHICmdList, const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData)
{
	if (RHISupportsGPUParticles())
	{
		UpdateMultiGPUResources(RHICmdList);

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_Prepare));
		PrepareGPUSimulation(RHICmdList);

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_Simulate));
		SimulateGPUParticles(RHICmdList, EParticleSimulatePhase::Main, NULL, NULL, FTexture2DRHIParamRef(), FTexture2DRHIParamRef());

		RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_Finalize));
		FinalizeGPUSimulation(RHICmdList);

		if (IsParticleCollisionModeSupported(GetShaderPlatform(), PCM_DistanceField))
		{
			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_PrepareCDF));
			PrepareGPUSimulation(RHICmdList);

			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_SimulateCDF));
			SimulateGPUParticles(RHICmdList, EParticleSimulatePhase::CollisionDistanceField, NULL, GlobalDistanceFieldParameterData, FTexture2DRHIParamRef(), FTexture2DRHIParamRef());
			//particles rendered during basepass may need to read pos/velocity buffers.  must finalize unless we know for sure that nothingin base pass will read.
			RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_FXPreRender_FinalizeCDF));
			FinalizeGPUSimulation(RHICmdList);
		}
    }
}

void FFXSystem::PostRenderOpaque(FRHICommandListImmediate& RHICmdList, const FUniformBufferRHIParamRef ViewUniformBuffer, FTexture2DRHIParamRef SceneDepthTexture, FTexture2DRHIParamRef GBufferATexture)
{
	if (RHISupportsGPUParticles() && IsParticleCollisionModeSupported(GetShaderPlatform(), PCM_DepthBuffer))
	{
		PrepareGPUSimulation(RHICmdList, SceneDepthTexture);
		
		SimulateGPUParticles(RHICmdList, EParticleSimulatePhase::CollisionDepthBuffer, ViewUniformBuffer, NULL, SceneDepthTexture, GBufferATexture);
		
		FinalizeGPUSimulation(RHICmdList, SceneDepthTexture);

		SortGPUParticles(RHICmdList);		
	}
}
