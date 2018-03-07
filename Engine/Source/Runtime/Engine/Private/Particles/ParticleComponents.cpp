// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnParticleComponent.cpp: Particle component implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "UObject/FrameworkObjectVersion.h"
#include "Misc/App.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectBaseUtility.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineDefines.h"
#include "EngineGlobals.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Engine/CollisionProfile.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/PropertyPortFlags.h"
#include "Particles/ParticleSystem.h"
#include "Particles/Emitter.h"
#include "ParticleHelper.h"
#include "Distributions/DistributionFloat.h"
#include "Particles/Orientation/ParticleModuleOrientationAxisLock.h"
#include "ParticleEmitterInstances.h"
#include "Particles/ParticleSystemComponent.h"
#include "Distributions/DistributionFloatConstant.h"
#include "Distributions/DistributionFloatUniform.h"
#include "Distributions/DistributionVectorConstant.h"
#include "Distributions/DistributionVectorUniform.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UnrealEngine.h"
#include "Distributions/DistributionVectorConstantCurve.h"
#include "StaticMeshResources.h"
#include "Particles/EmitterCameraLensEffectBase.h"
#include "FXSystem.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "Engine/InterpCurveEdSetup.h"
#include "ObjectEditorUtils.h"
#endif

#include "Particles/Camera/ParticleModuleCameraOffset.h"
#include "Particles/Collision/ParticleModuleCollision.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Scalability.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/Event/ParticleModuleEventGenerator.h"
#include "Particles/Event/ParticleModuleEventReceiverBase.h"
#include "Particles/Lifetime/ParticleModuleLifetimeBase.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Light/ParticleModuleLight.h"
#include "Particles/Material/ParticleModuleMeshMaterial.h"
#include "Particles/Modules/Location/ParticleModulePivotOffset.h"
#include "Particles/Orbit/ParticleModuleOrbit.h"
#include "Particles/Parameter/ParticleModuleParameterDynamic.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Spawn/ParticleModuleSpawnBase.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/TypeData/ParticleModuleTypeDataBase.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Particles/ParticleSpriteEmitter.h"
#include "Particles/TypeData/ParticleModuleTypeDataGpu.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Particles/ParticleEventManager.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleSystemReplay.h"
#include "Distributions/DistributionFloatConstantCurve.h"
#include "Particles/SubUV/ParticleModuleSubUV.h"
#include "GameFramework/GameState.h"
#include "HAL/LowLevelMemTracker.h"
#include "FrameworkObjectVersion.h"
#if WITH_FLEX
#include "PhysicsEngine/FlexFluidSurface.h"
#include "PhysicsEngine/FlexFluidSurfaceComponent.h"
#include "PhysicsEngine/FlexContainerInstance.h"
#endif
#include "PhysicsPublic.h"


DECLARE_CYCLE_STAT(TEXT("ParticleComponent InitParticles"), STAT_ParticleSystemComponent_InitParticles, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent SendRenderDynamicData"), STAT_ParticleSystemComponent_SendRenderDynamicData_Concurrent, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent SendRenderTransform Concurrent"), STAT_ParticleSystemComponent_SendRenderTransform_Concurrent, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent DestroyRenderState Concurrent"), STAT_ParticleSystemComponent_DestroyRenderState_Concurrent, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent CreateDynamicData"), STAT_ParticleSystemComponent_CreateDynamicData, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent CreateDynamicData Replay"), STAT_ParticleSystemComponent_CreateDynamicData_Replay, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent CreateDynamicData Capture"), STAT_ParticleSystemComponent_CreateDynamicData_Capture, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent CreateDynamicData Gather"), STAT_ParticleSystemComponent_CreateDynamicData_Gather, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent GetDynamicData"), STAT_ParticleSystemComponent_GetDynamicData, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent GetDynamicData Selected"), STAT_ParticleSystemComponent_GetDynamicData_Selected, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent CreateDynamicData GatherCapture"), STAT_ParticleSystemComponent_CreateDynamicData_GatherCapture, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent UpdateDynamicData"), STAT_ParticleSystemComponent_UpdateDynamicData, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent OrientZAxisTowardCamera"), STAT_UParticleSystemComponent_OrientZAxisTowardCamera, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent QueueFinalize"), STAT_UParticleSystemComponent_QueueFinalize, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent CheckForReset"), STAT_UParticleSystemComponent_CheckForReset, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent LOD_Inactive"), STAT_UParticleSystemComponent_LOD_Inactive, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent LOD"), STAT_UParticleSystemComponent_LOD, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent ResetAndCheckParallel"), STAT_UParticleSystemComponent_ResetAndCheckParallel, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent QueueTasks"), STAT_UParticleSystemComponent_QueueTasks, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent QueueAsync"), STAT_UParticleSystemComponent_QueueAsync, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent WaitForAsyncAndFinalize"), STAT_UParticleSystemComponent_WaitForAsyncAndFinalize, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleComponent CreateRenderState Concurrent"), STAT_ParticleSystemComponent_CreateRenderState_Concurrent, STATGROUP_Particles);

#include "InGamePerformanceTracker.h"

#define LOCTEXT_NAMESPACE "ParticleComponents"

DEFINE_LOG_CATEGORY(LogParticles);

int32 GParticleLODBias = 0;
FAutoConsoleVariableRef CVarParticleLODBias(
	TEXT("r.ParticleLODBias"),
	GParticleLODBias,
	TEXT("LOD bias for particle systems, default is 0"),
	ECVF_Scalability
	);

static TAutoConsoleVariable<float> CVarQLSpawnRateReferenceLevel(
	TEXT("fx.QualityLevelSpawnRateScaleReferenceLevel"),
	2,
	TEXT("Controls the reference level for quality level based spawn rate scaling. This is the FX quality level\n")
	TEXT("at which spawn rate is not scaled down; Spawn rate scaling will happen by each emitter's\n")
	TEXT("QualityLevelSpawnRateScale value for each reduction in level below the reference level.\n")
	TEXT("\n")
	TEXT("Default = 2. Value should range from 0 to the maximum FX quality level."),
	ECVF_Scalability);


/** Whether to allow particle systems to perform work. */
bool GIsAllowingParticles = true;

/** Whether to calculate LOD on the GameThread in-game. */
bool GbEnableGameThreadLODCalculation = true;

// Comment this in to debug empty emitter instance templates...
//#define _PSYSCOMP_DEBUG_INVALID_EMITTER_INSTANCE_TEMPLATES_

/*-----------------------------------------------------------------------------
	Particle scene view
-----------------------------------------------------------------------------*/
FSceneView*			GParticleView = NULL;

/*-----------------------------------------------------------------------------
	Conversion functions
-----------------------------------------------------------------------------*/
void Particle_ModifyFloatDistribution(UDistributionFloat* pkDistribution, float fScale)
{
	if (pkDistribution->IsA(UDistributionFloatConstant::StaticClass()))
	{
		UDistributionFloatConstant* pkDistConstant = Cast<UDistributionFloatConstant>(pkDistribution);
		pkDistConstant->Constant *= fScale;
	}
	else
	if (pkDistribution->IsA(UDistributionFloatUniform::StaticClass()))
	{
		UDistributionFloatUniform* pkDistUniform = Cast<UDistributionFloatUniform>(pkDistribution);
		pkDistUniform->Min *= fScale;
		pkDistUniform->Max *= fScale;
	}
	else
	if (pkDistribution->IsA(UDistributionFloatConstantCurve::StaticClass()))
	{
		UDistributionFloatConstantCurve* pkDistCurve = Cast<UDistributionFloatConstantCurve>(pkDistribution);

		int32 iKeys = pkDistCurve->GetNumKeys();
		int32 iCurves = pkDistCurve->GetNumSubCurves();

		for (int32 KeyIndex = 0; KeyIndex < iKeys; KeyIndex++)
		{
			float fKeyIn = pkDistCurve->GetKeyIn(KeyIndex);
			for (int32 SubIndex = 0; SubIndex < iCurves; SubIndex++)
			{
				float fKeyOut = pkDistCurve->GetKeyOut(SubIndex, KeyIndex);
				float ArriveTangent;
				float LeaveTangent;
				pkDistCurve->GetTangents(SubIndex, KeyIndex, ArriveTangent, LeaveTangent);

				pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * fScale);
				pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * fScale, LeaveTangent * fScale);
			}
		}
	}
}

void Particle_ModifyVectorDistribution(UDistributionVector* pkDistribution, FVector& vScale)
{
	if (pkDistribution->IsA(UDistributionVectorConstant::StaticClass()))
	{
		UDistributionVectorConstant* pkDistConstant = Cast<UDistributionVectorConstant>(pkDistribution);
		pkDistConstant->Constant *= vScale;
	}
	else
	if (pkDistribution->IsA(UDistributionVectorUniform::StaticClass()))
	{
		UDistributionVectorUniform* pkDistUniform = Cast<UDistributionVectorUniform>(pkDistribution);
		pkDistUniform->Min *= vScale;
		pkDistUniform->Max *= vScale;
	}
	else
	if (pkDistribution->IsA(UDistributionVectorConstantCurve::StaticClass()))
	{
		UDistributionVectorConstantCurve* pkDistCurve = Cast<UDistributionVectorConstantCurve>(pkDistribution);

		int32 iKeys = pkDistCurve->GetNumKeys();
		int32 iCurves = pkDistCurve->GetNumSubCurves();

		for (int32 KeyIndex = 0; KeyIndex < iKeys; KeyIndex++)
		{
			float fKeyIn = pkDistCurve->GetKeyIn(KeyIndex);
			for (int32 SubIndex = 0; SubIndex < iCurves; SubIndex++)
			{
				float fKeyOut = pkDistCurve->GetKeyOut(SubIndex, KeyIndex);
				float ArriveTangent;
				float LeaveTangent;
				pkDistCurve->GetTangents(SubIndex, KeyIndex, ArriveTangent, LeaveTangent);

				switch (SubIndex)
				{
				case 1:
					pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * vScale.Y);
					pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * vScale.Y, LeaveTangent * vScale.Y);
					break;
				case 2:
					pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * vScale.Z);
					pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * vScale.Z, LeaveTangent * vScale.Z);
					break;
				case 0:
				default:
					pkDistCurve->SetKeyOut(SubIndex, KeyIndex, fKeyOut * vScale.X);
					pkDistCurve->SetTangents(SubIndex, KeyIndex, ArriveTangent * vScale.X, LeaveTangent * vScale.X);
					break;
				}
			}
		}
	}
}

/** Console command to reset all particle components. */
static void ResetAllParticleComponents()
{
	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
	{
		UParticleSystemComponent* ParticleSystemComponent = *It;
		ParticleSystemComponent->ResetParticles();
		ParticleSystemComponent->ActivateSystem(true);
		ParticleSystemComponent->bIsViewRelevanceDirty = true;
		ParticleSystemComponent->CachedViewRelevanceFlags.Empty();
		ParticleSystemComponent->ConditionalCacheViewRelevanceFlags();
		ParticleSystemComponent->ReregisterComponent();
	}
}
static FAutoConsoleCommand GResetAllParticleComponentsCmd(
	TEXT("FX.RestartAll"),
	TEXT("Restarts all particle system components"),
	FConsoleCommandDelegate::CreateStatic(ResetAllParticleComponents)
	);

/*-----------------------------------------------------------------------------
	UParticleLODLevel implementation.
-----------------------------------------------------------------------------*/
UParticleLODLevel::UParticleLODLevel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bEnabled = true;
	ConvertedModules = true;
	PeakActiveParticles = 0;
}


void UParticleLODLevel::CompileModules( FParticleEmitterBuildInfo& EmitterBuildInfo )
{
	check( RequiredModule );
	check( SpawnModule );

	// Store a few special modules.
	EmitterBuildInfo.RequiredModule = RequiredModule;
	EmitterBuildInfo.SpawnModule = SpawnModule;

	// Compile those special modules.
	RequiredModule->CompileModule( EmitterBuildInfo );
	if ( SpawnModule->bEnabled )
	{
		SpawnModule->CompileModule( EmitterBuildInfo );
	}

	// Compile all remaining modules.
	const int32 ModuleCount = Modules.Num();
	for ( int32 ModuleIndex = 0; ModuleIndex < ModuleCount; ++ModuleIndex )
	{
		UParticleModule* Module = Modules[ModuleIndex];
		if ( Module && Module->bEnabled )
		{
			Module->CompileModule( EmitterBuildInfo );
		}
	}

	// Estimate the maximum number of active particles.
	EmitterBuildInfo.EstimatedMaxActiveParticleCount = CalculateMaxActiveParticleCount();
}

void UParticleLODLevel::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	checkf(SpawnModule, TEXT("Missing spawn module on %s (%s)"), *(GetPathName()), 
		(GetOuter() ? (GetOuter()->GetOuter() ? *(GetOuter()->GetOuter()->GetPathName()) : *(GetOuter()->GetPathName())) : TEXT("???")));
#endif // WITH_EDITORONLY_DATA

	{
		RequiredModule->ConditionalPostLoad();
	}
	if ( SpawnModule )
	{
		SpawnModule->ConditionalPostLoad();
	}

	for (UParticleModule* ParticleModule : Modules)
	{
		ParticleModule->ConditionalPostLoad();
	}

	// shouldn't ever set another UObjects serialized variable in post load
	// this causes determinisitc cooking issues due to load order being different
	/*if (RequiredModule)
	{
		RequiredModule->ConditionalPostLoad();
		if (RequiredModule->bEnabled != bEnabled)
		{
			RequiredModule->bEnabled = bEnabled;
		}
	}*/
}

void UParticleLODLevel::UpdateModuleLists()
{
	SpawningModules.Empty();
	SpawnModules.Empty();
	UpdateModules.Empty();
	OrbitModules.Empty();
	EventReceiverModules.Empty();
	EventGenerator = NULL;

	UParticleModule* Module;
	int32 TypeDataModuleIndex = -1;

	for (int32 i = 0; i < Modules.Num(); i++)
	{
		Module = Modules[i];
		if (!Module)
		{
			continue;
		}

		if (Module->bSpawnModule)
		{
			SpawnModules.Add(Module);
		}
		if (Module->bUpdateModule || Module->bFinalUpdateModule)
		{
			UpdateModules.Add(Module);
		}

		if (Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
		{
			TypeDataModule = CastChecked<UParticleModuleTypeDataBase>(Module);
			if (!Module->bSpawnModule && !Module->bUpdateModule)
			{
				// For now, remove it from the list and set it as the TypeDataModule
				TypeDataModuleIndex = i;
			}
		}
		else
		if (Module->IsA(UParticleModuleSpawnBase::StaticClass()))
		{
			UParticleModuleSpawnBase* SpawnBase = CastChecked<UParticleModuleSpawnBase>(Module);
			SpawningModules.Add(SpawnBase);
		}
		else
		if (Module->IsA(UParticleModuleOrbit::StaticClass()))
		{
			UParticleModuleOrbit* Orbit = CastChecked<UParticleModuleOrbit>(Module);
			OrbitModules.Add(Orbit);
		}
		else
		if (Module->IsA(UParticleModuleEventGenerator::StaticClass()))
		{
			EventGenerator = CastChecked<UParticleModuleEventGenerator>(Module);
		}
		else
		if (Module->IsA(UParticleModuleEventReceiverBase::StaticClass()))
		{
			UParticleModuleEventReceiverBase* Event = CastChecked<UParticleModuleEventReceiverBase>(Module);
			EventReceiverModules.Add(Event);
		}
	}

	if (EventGenerator)
	{
		// Force the event generator module to the top of the module stack...
		Modules.RemoveSingle(EventGenerator);
		Modules.Insert(EventGenerator, 0);
	}

	if (TypeDataModuleIndex != -1)
	{
		Modules.RemoveAt(TypeDataModuleIndex);
	}

	if (TypeDataModule /**&& (Level == 0)**/)
	{
		UParticleModuleTypeDataMesh* MeshTD = Cast<UParticleModuleTypeDataMesh>(TypeDataModule);
		if (MeshTD
			&& MeshTD->Mesh
			&& MeshTD->Mesh->HasValidRenderData())
		{
			UParticleSpriteEmitter* SpriteEmitter = Cast<UParticleSpriteEmitter>(GetOuter());
			if (SpriteEmitter && (MeshTD->bOverrideMaterial == false))
			{
				FStaticMeshSection& Section = MeshTD->Mesh->RenderData->LODResources[0].Sections[0];
				UMaterialInterface* Material = MeshTD->Mesh->GetMaterial(Section.MaterialIndex);
				if (Material)
				{
					RequiredModule->Material = Material;
				}
			}
		}
	}
}


bool UParticleLODLevel::GenerateFromLODLevel(UParticleLODLevel* SourceLODLevel, float Percentage, bool bGenerateModuleData)
{
	// See if there are already modules in place
	if (Modules.Num() > 0)
	{
		UE_LOG(LogParticles, Log, TEXT("ERROR? - GenerateFromLODLevel - modules already present!"));
		return false;
	}

	bool	bResult	= true;

	// Allocate slots in the array...
	Modules.InsertZeroed(0, SourceLODLevel->Modules.Num());

	// Set the enabled flag
	bEnabled = SourceLODLevel->bEnabled;

	// Set up for undo/redo!
	SetFlags(RF_Transactional);

	// Required module...
	RequiredModule = CastChecked<UParticleModuleRequired>(
		SourceLODLevel->RequiredModule->GenerateLODModule(SourceLODLevel, this, Percentage, bGenerateModuleData));

	// Spawn module...
	SpawnModule = CastChecked<UParticleModuleSpawn>(
		SourceLODLevel->SpawnModule->GenerateLODModule(SourceLODLevel, this, Percentage, bGenerateModuleData));

	// TypeData module, if present...
	if (SourceLODLevel->TypeDataModule)
	{
		TypeDataModule = 
			CastChecked<UParticleModuleTypeDataBase>(
			SourceLODLevel->TypeDataModule->GenerateLODModule(SourceLODLevel, this, Percentage, bGenerateModuleData));
		check(TypeDataModule == SourceLODLevel->TypeDataModule); // Code expects typedata to be the same across LODs
	}

	// The remaining modules...
	for (int32 ModuleIndex = 0; ModuleIndex < SourceLODLevel->Modules.Num(); ModuleIndex++)
	{
		if (SourceLODLevel->Modules[ModuleIndex])
		{
			Modules[ModuleIndex] = SourceLODLevel->Modules[ModuleIndex]->GenerateLODModule(SourceLODLevel, this, Percentage, bGenerateModuleData);
		}
		else
		{
			Modules[ModuleIndex] = NULL;
		}
	}

	return bResult;
}


int32	UParticleLODLevel::CalculateMaxActiveParticleCount()
{
	check(RequiredModule != NULL);

	// Determine the lifetime for particles coming from the emitter
	float ParticleLifetime = 0.0f;
	float MaxSpawnRate = SpawnModule->GetEstimatedSpawnRate();
	int32 MaxBurstCount = SpawnModule->GetMaximumBurstCount();
	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ModuleIndex++)
	{
		UParticleModuleLifetimeBase* LifetimeMod = Cast<UParticleModuleLifetimeBase>(Modules[ModuleIndex]);
		if (LifetimeMod != NULL)
		{
			ParticleLifetime += LifetimeMod->GetMaxLifetime();
		}

		UParticleModuleSpawnBase* SpawnMod = Cast<UParticleModuleSpawnBase>(Modules[ModuleIndex]);
		if (SpawnMod != NULL)
		{
			MaxSpawnRate += SpawnMod->GetEstimatedSpawnRate();
			MaxBurstCount += SpawnMod->GetMaximumBurstCount();
		}
	}

	// Determine the maximum duration for this particle system
	float MaxDuration = 0.0f;
	float TotalDuration = 0.0f;
	int32 TotalLoops = 0;
	if (RequiredModule != NULL)
	{
		// We don't care about delay wrt spawning...
		MaxDuration = FMath::Max<float>(RequiredModule->EmitterDuration, RequiredModule->EmitterDurationLow);
		TotalLoops = RequiredModule->EmitterLoops;
		TotalDuration = MaxDuration * TotalLoops;
	}

	// Determine the max
	int32 MaxAPC = 0;

	if (TotalDuration != 0.0f)
	{
		if (TotalLoops == 1)
		{
			// Special case for one loop... 
			if (ParticleLifetime < MaxDuration)
			{
				MaxAPC += FMath::CeilToInt(ParticleLifetime * MaxSpawnRate);
			}
			else
			{
				MaxAPC += FMath::CeilToInt(MaxDuration * MaxSpawnRate);
			}
			// Safety zone...
			MaxAPC += 1;
			// Add in the bursts...
			MaxAPC += MaxBurstCount;
		}
		else
		{
			if (ParticleLifetime < MaxDuration)
			{
				MaxAPC += FMath::CeilToInt(ParticleLifetime * MaxSpawnRate);
			}
			else
			{
				MaxAPC += (FMath::CeilToInt(FMath::CeilToInt(MaxDuration * MaxSpawnRate) * ParticleLifetime));
			}
			// Safety zone...
			MaxAPC += 1;
			// Add in the bursts...
			MaxAPC += MaxBurstCount;
			if (ParticleLifetime > MaxDuration)
			{
				MaxAPC += MaxBurstCount * FMath::CeilToInt(ParticleLifetime - MaxDuration);
			}
		}
	}
	else
	{
		// We are infinite looping... 
		// Single loop case is all we will worry about. Safer base estimate - but not ideal.
		if (ParticleLifetime < MaxDuration)
		{
			MaxAPC += FMath::CeilToInt(ParticleLifetime * FMath::CeilToInt(MaxSpawnRate));
		}
		else
		{
			if (ParticleLifetime != 0.0f)
			{
				if (ParticleLifetime <= MaxDuration)
				{
					MaxAPC += FMath::CeilToInt(MaxDuration * MaxSpawnRate);
				}
				else //if (ParticleLifetime > MaxDuration)
				{
					MaxAPC += FMath::CeilToInt(MaxDuration * MaxSpawnRate) * ParticleLifetime;
				}
			}
			else
			{
				// No lifetime, no duration...
				MaxAPC += FMath::CeilToInt(MaxSpawnRate);
			}
		}
		// Safety zone...
		MaxAPC += FMath::Max<int32>(FMath::CeilToInt(MaxSpawnRate * 0.032f), 2);
		// Burst
		MaxAPC += MaxBurstCount;
	}

	PeakActiveParticles = MaxAPC;

	return MaxAPC;
}


void UParticleLODLevel::ConvertToSpawnModule()
{
#if WITH_EDITOR
	// Move the required module SpawnRate and Burst information to a new SpawnModule.
	if (SpawnModule)
	{
//		UE_LOG(LogParticles, Warning, TEXT("LOD Level already has a spawn module!"));
		return;
	}

	UParticleEmitter* EmitterOuter = CastChecked<UParticleEmitter>(GetOuter());
	SpawnModule = NewObject<UParticleModuleSpawn>(EmitterOuter->GetOuter());
	check(SpawnModule);

	UDistributionFloat* SourceDist = RequiredModule->SpawnRate.Distribution;
	if (SourceDist)
	{
		SpawnModule->Rate.Distribution = Cast<UDistributionFloat>(StaticDuplicateObject(SourceDist, SpawnModule));
		SpawnModule->Rate.Distribution->bIsDirty = true;
		SpawnModule->Rate.Initialize();
	}

	// Now the burst list.
	int32 BurstCount = RequiredModule->BurstList.Num();
	if (BurstCount > 0)
	{
		SpawnModule->BurstList.AddZeroed(BurstCount);
		for (int32 BurstIndex = 0; BurstIndex < BurstCount; BurstIndex++)
		{
			SpawnModule->BurstList[BurstIndex].Count = RequiredModule->BurstList[BurstIndex].Count;
			SpawnModule->BurstList[BurstIndex].CountLow = RequiredModule->BurstList[BurstIndex].CountLow;
			SpawnModule->BurstList[BurstIndex].Time = RequiredModule->BurstList[BurstIndex].Time;
		}
	}

	MarkPackageDirty();
#endif	//#if WITH_EDITOR
}


int32 UParticleLODLevel::GetModuleIndex(UParticleModule* InModule)
{
	if (InModule)
	{
		if (InModule == RequiredModule)
		{
			return INDEX_REQUIREDMODULE;
		}
		else if (InModule == SpawnModule)
		{
			return INDEX_SPAWNMODULE;
		}
		else if (InModule == TypeDataModule)
		{
			return INDEX_TYPEDATAMODULE;
		}
		else
		{
			for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ModuleIndex++)
			{
				if (InModule == Modules[ModuleIndex])
				{
					return ModuleIndex;
				}
			}
		}
	}

	return INDEX_NONE;
}


UParticleModule* UParticleLODLevel::GetModuleAtIndex(int32 InIndex)
{
	// 'Normal' modules
	if (InIndex > INDEX_NONE)
	{
		if (InIndex < Modules.Num())
		{
			return Modules[InIndex];
		}

		return NULL;
	}

	switch (InIndex)
	{
	case INDEX_REQUIREDMODULE:		return RequiredModule;
	case INDEX_SPAWNMODULE:			return SpawnModule;
	case INDEX_TYPEDATAMODULE:		return TypeDataModule;
	}

	return NULL;
}


void UParticleLODLevel::SetLevelIndex(int32 InLevelIndex)
{
	// Remove the 'current' index from the validity flags and set the new one.
	RequiredModule->LODValidity &= ~(1 << Level);
	RequiredModule->LODValidity |= (1 << InLevelIndex);
	SpawnModule->LODValidity &= ~(1 << Level);
	SpawnModule->LODValidity |= (1 << InLevelIndex);
	if (TypeDataModule)
	{
		TypeDataModule->LODValidity &= ~(1 << Level);
		TypeDataModule->LODValidity |= (1 << InLevelIndex);
	}
	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ModuleIndex++)
	{
		UParticleModule* CheckModule = Modules[ModuleIndex];
		if (CheckModule)
		{
			CheckModule->LODValidity &= ~(1 << Level);
			CheckModule->LODValidity |= (1 << InLevelIndex);
		}
	}

	Level = InLevelIndex;
}


bool UParticleLODLevel::IsModuleEditable(UParticleModule* InModule)
{
	// If the module validity flag is not set for this level, it is not editable.
	if ((InModule->LODValidity & (1 << Level)) == 0)
	{
		return false;
	}

	// If the module is shared w/ higher LOD levels, then it is not editable...
	int32 Validity = 0;
	if (Level > 0)
	{
		int32 Check = Level - 1;
		while (Check >= 0)
		{
			Validity |= (1 << Check);
			Check--;
		}

		if ((Validity & InModule->LODValidity) != 0)
		{
			return false;
		}
	}

	return true;
}

/*-----------------------------------------------------------------------------
	UParticleEmitter implementation.
-----------------------------------------------------------------------------*/
UParticleEmitter::UParticleEmitter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, QualityLevelSpawnRateScale(1.0f)
	, bDisabledLODsKeepEmitterAlive(false)
	, bDisableWhenInsignficant(0)
	, SignificanceLevel(EParticleSignificanceLevel::Critical)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FName NAME_Particle_Emitter;
		FConstructorStatics()
			: NAME_Particle_Emitter(TEXT("Particle Emitter"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	EmitterName = ConstructorStatics.NAME_Particle_Emitter;
	ConvertedModules = true;
	PeakActiveParticles = 0;
#if WITH_EDITORONLY_DATA
	EmitterEditorColor = FColor(0, 150, 150, 255);
#endif // WITH_EDITORONLY_DATA

	// Flex
	Mass = 1.0f;
	bLocalSpace = false;
}

FParticleEmitterInstance* UParticleEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
	UE_LOG(LogParticles, Fatal,TEXT("UParticleEmitter::CreateInstance is pure virtual")); 
	return NULL; 
}

void UParticleEmitter::UpdateModuleLists()
{
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			LODLevel->UpdateModuleLists();
		}
	}
	Build();
}

/**
 *	Helper function for fixing up LODValidity issues on particle modules...
 *
 *	@param	LODIndex		The index of the LODLevel the module is from.
 *	@param	ModuleIndex		The index of the module being checked.
 *	@param	Emitter			The emitter owner.
 *	@param	CurrModule		The module being checked.
 *
 *	@return	 0		If there was no problem.
 *			 1		If there was a problem and it was fixed.
 *			-1		If there was a problem that couldn't be fixed.
 */
int32 ParticleEmitterHelper_FixupModuleLODErrors( int32 LODIndex, int32 ModuleIndex, 
	const UParticleEmitter* Emitter, UParticleModule* CurrModule )
{
	int32 Result = 1;
	bool bIsDirty = false;

	UObject* ModuleOuter = CurrModule->GetOuter();
	UObject* EmitterOuter = Emitter->GetOuter();
	if (ModuleOuter != EmitterOuter)
	{
		// Module has an incorrect outer
		CurrModule->Rename(NULL, EmitterOuter, REN_ForceNoResetLoaders|REN_DoNotDirty);
		bIsDirty = true;
	}

	if (CurrModule->LODValidity == 0)
	{
		// Immediately tag it for this lod level...
		CurrModule->LODValidity = (1 << LODIndex);
		bIsDirty = true;
	}
	else
	if (CurrModule->IsUsedInLODLevel(LODIndex) == false)
	{
		// Why was this even called here?? 
		// The assumption is that it should be called for the module in the given lod level...
		// So, we will tag it with this index.
		CurrModule->LODValidity |= (1 << LODIndex);
		bIsDirty = true;
	}

	if (LODIndex > 0)
	{
		int32 CheckIndex = LODIndex - 1;
		while (CheckIndex >= 0)
		{
			if (CurrModule->IsUsedInLODLevel(CheckIndex))
			{
				// Ensure that it is the same as the one it THINKS it is shared with...
				UParticleLODLevel* CheckLODLevel = Emitter->LODLevels[CheckIndex];

				if (CurrModule->IsA(UParticleModuleSpawn::StaticClass()))
				{
					if (CheckLODLevel->SpawnModule != CurrModule)
					{
						// Fix it up... Turn off the higher LOD flag
						CurrModule->LODValidity &= ~(1 << CheckIndex);
						bIsDirty = true;
					}
				}
				else
				if (CurrModule->IsA(UParticleModuleRequired::StaticClass()))
				{
					if (CheckLODLevel->RequiredModule != CurrModule)
					{
						// Fix it up... Turn off the higher LOD flag
						CurrModule->LODValidity &= ~(1 << CheckIndex);
						bIsDirty = true;
					}
				}
				else
				if (CurrModule->IsA(UParticleModuleTypeDataBase::StaticClass()))
				{
					if (CheckLODLevel->TypeDataModule != CurrModule)
					{
						// Fix it up... Turn off the higher LOD flag
						CurrModule->LODValidity &= ~(1 << CheckIndex);
						bIsDirty = true;
					}
				}
				else
				{
					if (ModuleIndex >= CheckLODLevel->Modules.Num())
					{
						UE_LOG(LogParticles, Warning, TEXT("\t\tMismatched module count at %2d in %s"), LODIndex, *(Emitter->GetPathName()));
						Result = -1;
					}
					else
					{
						UParticleModule* CheckModule = CheckLODLevel->Modules[ModuleIndex];
						if (CheckModule != CurrModule)
						{
							// Fix it up... Turn off the higher LOD flag
							CurrModule->LODValidity &= ~(1 << CheckIndex);
							bIsDirty = true;
						}
					}
				}
			}

			CheckIndex--;
		}
	}

	if ((bIsDirty == true) && IsRunningCommandlet())
	{
		CurrModule->MarkPackageDirty();
		Emitter->MarkPackageDirty();
	}

	return Result;
}

void UParticleEmitter::PostLoad()
{
	Super::PostLoad();

	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			LODLevel->ConditionalPostLoad();

			FLinkerLoad* LODLevelLinker = LODLevel->GetLinker();
			if (LODLevel->SpawnModule == NULL)
			{
				// Force the conversion to SpawnModule
				UParticleSystem* PSys = Cast<UParticleSystem>(GetOuter());
				if (PSys)
				{
					UE_LOG(LogParticles, Warning, TEXT("LODLevel %d was not converted to spawn module - forcing: %s"), 
						LODLevel->Level, *(PSys->GetPathName()));
				}
				LODLevel->ConvertToSpawnModule();
			}

			check(LODLevel->SpawnModule);

		}
	}

#if WITH_EDITOR
	if ((GIsEditor == true) && 1)//(IsRunningCommandlet() == false))
	{
		ConvertedModules = false;
		PeakActiveParticles = 0;

		// Check for improper outers...
		UObject* EmitterOuter = GetOuter();
		bool bWarned = false;
		for (int32 LODIndex = 0; (LODIndex < LODLevels.Num()) && !bWarned; LODIndex++)
		{
			UParticleLODLevel* LODLevel = LODLevels[LODIndex];
			if (LODLevel)
			{
				LODLevel->ConditionalPostLoad();

				UParticleModule* Module = LODLevel->TypeDataModule;
				if (Module)
				{
					Module->ConditionalPostLoad();

					UObject* OuterObj = Module->GetOuter();
					check(OuterObj);
					if (OuterObj != EmitterOuter)
					{
						UE_LOG(LogParticles, Warning, TEXT("UParticleModule %s has an incorrect outer on %s... run FixupEmitters on package %s (%s)"),
							*(Module->GetPathName()), 
							*(EmitterOuter->GetPathName()),
							*(OuterObj->GetOutermost()->GetPathName()),
							*(GetOutermost()->GetPathName()));
						UE_LOG(LogParticles, Warning, TEXT("\tModule Outer..............%s"), *(OuterObj->GetPathName()));
						UE_LOG(LogParticles, Warning, TEXT("\tModule Outermost..........%s"), *(Module->GetOutermost()->GetPathName()));
						UE_LOG(LogParticles, Warning, TEXT("\tEmitter Outer.............%s"), *(EmitterOuter->GetPathName()));
						UE_LOG(LogParticles, Warning, TEXT("\tEmitter Outermost.........%s"), *(GetOutermost()->GetPathName()));
						bWarned = true;
					}
				}

				if (!bWarned)
				{
					for (int32 ModuleIndex = 0; (ModuleIndex < LODLevel->Modules.Num()) && !bWarned; ModuleIndex++)
					{
						Module = LODLevel->Modules[ModuleIndex];
						if (Module)
						{
							Module->ConditionalPostLoad();

							UObject* OuterObj = Module->GetOuter();
							check(OuterObj);
							if (OuterObj != EmitterOuter)
							{
								UE_LOG(LogParticles, Warning, TEXT("UParticleModule %s has an incorrect outer on %s... run FixupEmitters on package %s (%s)"),
									*(Module->GetPathName()), 
									*(EmitterOuter->GetPathName()),
									*(OuterObj->GetOutermost()->GetPathName()),
									*(GetOutermost()->GetPathName()));
								UE_LOG(LogParticles, Warning, TEXT("\tModule Outer..............%s"), *(OuterObj->GetPathName()));
								UE_LOG(LogParticles, Warning, TEXT("\tModule Outermost..........%s"), *(Module->GetOutermost()->GetPathName()));
								UE_LOG(LogParticles, Warning, TEXT("\tEmitter Outer.............%s"), *(EmitterOuter->GetPathName()));
								UE_LOG(LogParticles, Warning, TEXT("\tEmitter Outermost.........%s"), *(GetOutermost()->GetPathName()));
								bWarned = true;
							}
						}
					}
				}
			}
		}
	}
	else
#endif
	{
		for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
		{
			UParticleLODLevel* LODLevel = LODLevels[LODIndex];
			if (LODLevel)
			{
				LODLevel->ConditionalPostLoad();
			}
		}
	}
   

	ConvertedModules = true;

	// this will look at all of the emitters and then remove ones that some how have become NULL (e.g. from a removal of an Emitter where content
	// is still referencing it)
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			for (int32 ModuleIndex = LODLevel->Modules.Num()-1; ModuleIndex >= 0; ModuleIndex--)
			{
				UParticleModule* ParticleModule = LODLevel->Modules[ModuleIndex];
				if( ParticleModule == NULL )
				{
					LODLevel->Modules.RemoveAt(ModuleIndex);
					MarkPackageDirty();
				}
			}
		}
	}


	UObject* MyOuter = GetOuter();
	UParticleSystem* PSysOuter = Cast<UParticleSystem>(MyOuter);
	bool bRegenDup = false;
	if (PSysOuter)
	{
		bRegenDup = PSysOuter->bRegenerateLODDuplicate;
	}

	// Clamp the detail spawn rate scale...
	QualityLevelSpawnRateScale = FMath::Clamp<float>(QualityLevelSpawnRateScale, 0.0f, 1.0f);

	UpdateModuleLists();
}

#if WITH_EDITOR
void UParticleEmitter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	check(GIsEditor);

	// Reset the peak active particle counts.
	// This could check for changes to SpawnRate and Burst and only reset then,
	// but since we reset the particle system after any edited property, it
	// may as well just autoreset the peak counts.
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			LODLevel->PeakActiveParticles	= 1;
		}
	}

	UpdateModuleLists();

	for (TObjectIterator<UParticleSystemComponent> It;It;++It)
	{
		if (It->Template)
		{
			int32 i;

			for (i=0; i<It->Template->Emitters.Num(); i++)
			{
				if (It->Template->Emitters[i] == this)
				{
					It->UpdateInstances();
				}
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (CalculateMaxActiveParticleCount() == false)
	{
		//
	}

	// Clamp the detail spawn rate scale...
	QualityLevelSpawnRateScale = FMath::Clamp<float>(QualityLevelSpawnRateScale, 0.0f, 1.0f);
}
#endif // WITH_EDITOR

void UParticleEmitter::SetEmitterName(FName Name)
{
	EmitterName = Name;
}

FName& UParticleEmitter::GetEmitterName()
{
	return EmitterName;
}

void UParticleEmitter::SetLODCount(int32 LODCount)
{
	// 
}

void UParticleEmitter::AddEmitterCurvesToEditor(UInterpCurveEdSetup* EdSetup)
{
	UE_LOG(LogParticles, Log, TEXT("UParticleEmitter::AddEmitterCurvesToEditor> Should no longer be called..."));
	return;
}

void UParticleEmitter::RemoveEmitterCurvesFromEditor(UInterpCurveEdSetup* EdSetup)
{
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		// Remove the typedata curves...
		if (LODLevel->TypeDataModule && LODLevel->TypeDataModule->IsDisplayedInCurveEd(EdSetup))
		{
			LODLevel->TypeDataModule->RemoveModuleCurvesFromEditor(EdSetup);
		}

		// Remove the spawn module curves...
		if (LODLevel->SpawnModule && LODLevel->SpawnModule->IsDisplayedInCurveEd(EdSetup))
		{
			LODLevel->SpawnModule->RemoveModuleCurvesFromEditor(EdSetup);
		}

		// Remove each modules curves as well.
		for (int32 ii = 0; ii < LODLevel->Modules.Num(); ii++)
		{
			if (LODLevel->Modules[ii]->IsDisplayedInCurveEd(EdSetup))
			{
				// Remove it from the curve editor!
				LODLevel->Modules[ii]->RemoveModuleCurvesFromEditor(EdSetup);
			}
		}
	}
}

void UParticleEmitter::ChangeEditorColor(FColor& Color, UInterpCurveEdSetup* EdSetup)
{
#if WITH_EDITORONLY_DATA
	UParticleLODLevel* LODLevel = LODLevels[0];
	EmitterEditorColor = Color;
	for (int32 TabIndex = 0; TabIndex < EdSetup->Tabs.Num(); TabIndex++)
	{
		FCurveEdTab*	Tab = &(EdSetup->Tabs[TabIndex]);
		for (int32 CurveIndex = 0; CurveIndex < Tab->Curves.Num(); CurveIndex++)
		{
			FCurveEdEntry* Entry	= &(Tab->Curves[CurveIndex]);
			if (LODLevel->SpawnModule->Rate.Distribution == Entry->CurveObject)
			{
				Entry->CurveColor	= Color;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UParticleEmitter::AutoPopulateInstanceProperties(UParticleSystemComponent* PSysComp)
{
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel	= LODLevels[LODIndex];
		for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
		{
			UParticleModule* Module = LODLevel->Modules[ModuleIndex];	
			LODLevel->SpawnModule->AutoPopulateInstanceProperties(PSysComp);
			LODLevel->RequiredModule->AutoPopulateInstanceProperties(PSysComp);
			if (LODLevel->TypeDataModule)
			{
				LODLevel->TypeDataModule->AutoPopulateInstanceProperties(PSysComp);
			}

			Module->AutoPopulateInstanceProperties(PSysComp);
		}
	}
}


int32 UParticleEmitter::CreateLODLevel(int32 LODLevel, bool bGenerateModuleData)
{
	int32					LevelIndex		= -1;
	UParticleLODLevel*	CreatedLODLevel	= NULL;

	if (LODLevels.Num() == 0)
	{
		LODLevel = 0;
	}

	// Is the requested index outside a viable range?
	if ((LODLevel < 0) || (LODLevel > LODLevels.Num()))
	{
		return -1;
	}

	// NextHighestLODLevel is the one that will be 'copied'
	UParticleLODLevel*	NextHighestLODLevel	= NULL;
	int32 NextHighIndex = -1;
	// NextLowestLODLevel is the one (and all ones lower than it) that will have their LOD indices updated
	UParticleLODLevel*	NextLowestLODLevel	= NULL;
	int32 NextLowIndex = -1;

	// Grab the two surrounding LOD levels...
	if (LODLevel == 0)
	{
		// It is being added at the front of the list... (highest)
		if (LODLevels.Num() > 0)
		{
			NextHighestLODLevel = LODLevels[0];
			NextHighIndex = 0;
			NextLowestLODLevel = NextHighestLODLevel;
			NextLowIndex = 0;
		}
	}
	else
	if (LODLevel > 0)
	{
		NextHighestLODLevel = LODLevels[LODLevel - 1];
		NextHighIndex = LODLevel - 1;
		if (LODLevel < LODLevels.Num())
		{
			NextLowestLODLevel = LODLevels[LODLevel];
			NextLowIndex = LODLevel;
		}
	}
	
	// Update the LODLevel index for the lower levels and
	// offset the LOD validity flags for the modules...
	if (NextLowestLODLevel)
	{
		NextLowestLODLevel->ConditionalPostLoad();
		for (int32 LowIndex = LODLevels.Num() - 1; LowIndex >= NextLowIndex; LowIndex--)
		{
			UParticleLODLevel* LowRemapLevel = LODLevels[LowIndex];
			if (LowRemapLevel)
			{
				LowRemapLevel->SetLevelIndex(LowIndex + 1);
			}
		}
	}

	// Create a ParticleLODLevel
	CreatedLODLevel = NewObject<UParticleLODLevel>(this);
	check(CreatedLODLevel);

	CreatedLODLevel->Level = LODLevel;
	CreatedLODLevel->bEnabled = true;
	CreatedLODLevel->ConvertedModules = true;
	CreatedLODLevel->PeakActiveParticles = 0;

	// Determine where to place it...
	if (LODLevels.Num() == 0)
	{
		LODLevels.InsertZeroed(0, 1);
		LODLevels[0] = CreatedLODLevel;
		CreatedLODLevel->Level	= 0;
	}
	else
	{
		LODLevels.InsertZeroed(LODLevel, 1);
		LODLevels[LODLevel] = CreatedLODLevel;
		CreatedLODLevel->Level = LODLevel;
	}

	if (NextHighestLODLevel)
	{
		NextHighestLODLevel->ConditionalPostLoad();

		// Generate from the higher LOD level
		if (CreatedLODLevel->GenerateFromLODLevel(NextHighestLODLevel, 100.0, bGenerateModuleData) == false)
		{
			UE_LOG(LogParticles, Warning, TEXT("Failed to generate LOD level %d from level %d"), LODLevel, NextHighestLODLevel->Level);
		}
	}
	else
	{
		// Create the RequiredModule
		UParticleModuleRequired* RequiredModule = NewObject<UParticleModuleRequired>(GetOuter());
		check(RequiredModule);
		RequiredModule->SetToSensibleDefaults(this);
		CreatedLODLevel->RequiredModule	= RequiredModule;

		// The SpawnRate for the required module
		RequiredModule->bUseLocalSpace			= false;
		RequiredModule->bKillOnDeactivate		= false;
		RequiredModule->bKillOnCompleted		= false;
		RequiredModule->EmitterDuration			= 1.0f;
		RequiredModule->EmitterLoops			= 0;
		RequiredModule->ParticleBurstMethod		= EPBM_Instant;
#if WITH_EDITORONLY_DATA
		RequiredModule->ModuleEditorColor		= FColor::MakeRandomColor();
#endif // WITH_EDITORONLY_DATA
		RequiredModule->InterpolationMethod		= PSUVIM_None;
		RequiredModule->SubImages_Horizontal	= 1;
		RequiredModule->SubImages_Vertical		= 1;
		RequiredModule->bScaleUV				= false;
		RequiredModule->RandomImageTime			= 0.0f;
		RequiredModule->RandomImageChanges		= 0;
		RequiredModule->bEnabled				= true;

		RequiredModule->LODValidity = (1 << LODLevel);

		// There must be a spawn module as well...
		UParticleModuleSpawn* SpawnModule = NewObject<UParticleModuleSpawn>(GetOuter());
		check(SpawnModule);
		CreatedLODLevel->SpawnModule = SpawnModule;
		SpawnModule->LODValidity = (1 << LODLevel);
		UDistributionFloatConstant* ConstantSpawn	= Cast<UDistributionFloatConstant>(SpawnModule->Rate.Distribution);
		ConstantSpawn->Constant					= 10;
		ConstantSpawn->bIsDirty					= true;
		SpawnModule->BurstList.Empty();

		// Copy the TypeData module
		CreatedLODLevel->TypeDataModule			= NULL;
	}

	LevelIndex	= CreatedLODLevel->Level;

	MarkPackageDirty();

	return LevelIndex;
}


bool UParticleEmitter::IsLODLevelValid(int32 LODLevel)
{
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* CheckLODLevel	= LODLevels[LODIndex];
		if (CheckLODLevel->Level == LODLevel)
		{
			return true;
		}
	}

	return false;
}


void UParticleEmitter::EditorUpdateCurrentLOD(FParticleEmitterInstance* Instance)
{
#if WITH_EDITORONLY_DATA
	UParticleLODLevel*	CurrentLODLevel	= NULL;
	UParticleLODLevel*	Higher			= NULL;

	int32 SetLODLevel = -1;
	if (Instance->Component && Instance->Component->Template)
	{
		int32 DesiredLODLevel = Instance->Component->Template->EditorLODSetting;
		if (GIsEditor && GEngine->bEnableEditorPSysRealtimeLOD)
		{
			DesiredLODLevel = Instance->Component->GetCurrentLODIndex();
		}

		for (int32 LevelIndex = 0; LevelIndex < LODLevels.Num(); LevelIndex++)
		{
			Higher	= LODLevels[LevelIndex];
			if (Higher && (Higher->Level == DesiredLODLevel))
			{
				SetLODLevel = LevelIndex;
				break;
			}
		}
	}

	if (SetLODLevel == -1)
	{
		SetLODLevel = 0;
	}
	Instance->SetCurrentLODIndex(SetLODLevel, false);
#endif // WITH_EDITORONLY_DATA
}



UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODLevel)
{
	if (LODLevel >= LODLevels.Num())
	{
		return NULL;
	}

	return LODLevels[LODLevel];
}


bool UParticleEmitter::AutogenerateLowestLODLevel(bool bDuplicateHighest)
{
	// Didn't find it?
	if (LODLevels.Num() == 1)
	{
		// We need to generate it...
		LODLevels.InsertZeroed(1, 1);
		UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>(this);
		check(LODLevel);
		LODLevels[1]					= LODLevel;
		LODLevel->Level					= 1;
		LODLevel->ConvertedModules		= true;
		LODLevel->PeakActiveParticles	= 0;

		// Grab LODLevel 0 for creation
		UParticleLODLevel* SourceLODLevel	= LODLevels[0];

		LODLevel->bEnabled				= SourceLODLevel->bEnabled;

		float Percentage	= 10.0f;
		if (SourceLODLevel->TypeDataModule)
		{
			UParticleModuleTypeDataBeam2*	Beam2TD		= Cast<UParticleModuleTypeDataBeam2>(SourceLODLevel->TypeDataModule);

			if (Beam2TD)
			{
				// For now, don't support LOD on beams and trails
				Percentage	= 100.0f;
			}
		}

		if (bDuplicateHighest == true)
		{
			Percentage = 100.0f;
		}

		if (LODLevel->GenerateFromLODLevel(SourceLODLevel, Percentage) == false)
		{
			UE_LOG(LogParticles, Warning, TEXT("Failed to generate LOD level %d from LOD level 0"), 1);
			return false;
		}

		MarkPackageDirty();
		return true;
	}

	return true;
}


bool UParticleEmitter::CalculateMaxActiveParticleCount()
{
	int32	CurrMaxAPC = 0;

	int32 MaxCount = 0;
	
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel && LODLevel->bEnabled)
		{
			bool bForceMaxCount = false;
			// Check for beams or trails
			if ((LODLevel->Level == 0) && (LODLevel->TypeDataModule != NULL))
			{
				UParticleModuleTypeDataBeam2* BeamTD = Cast<UParticleModuleTypeDataBeam2>(LODLevel->TypeDataModule);
				if (BeamTD)
				{
					bForceMaxCount = true;
					MaxCount = BeamTD->MaxBeamCount + 2;
				}
			}

			int32 LODMaxAPC = LODLevel->CalculateMaxActiveParticleCount();
			if (bForceMaxCount == true)
			{
				LODLevel->PeakActiveParticles = MaxCount;
				LODMaxAPC = MaxCount;
			}

			if (LODMaxAPC > CurrMaxAPC)
			{
				if (LODIndex > 0)
				{
					// Check for a ridiculous difference in counts...
					if ((CurrMaxAPC > 0) && (LODMaxAPC / CurrMaxAPC) > 2)
					{
						//UE_LOG(LogParticles, Log, TEXT("MaxActiveParticleCount Discrepancy?\n\tLOD %2d, Emitter %16s"), LODIndex, *GetName());
					}
				}
				CurrMaxAPC = LODMaxAPC;
			}
		}
	}

#if WITH_EDITOR
	if ((GIsEditor == true) && (CurrMaxAPC > 500))
	{
		//@todo. Added an option to the emitter to disable this warning - for 
		// the RARE cases where it is really required to render that many.
		UE_LOG(LogParticles, Warning, TEXT("MaxCount = %4d for Emitter %s (%s)"),
			CurrMaxAPC, *(GetName()), GetOuter() ? *(GetOuter()->GetPathName()) : TEXT("????"));
	}
#endif
	return true;
}


void UParticleEmitter::GetParametersUtilized(TArray<FString>& ParticleSysParamList,
											 TArray<FString>& ParticleParameterList)
{
	// Clear the lists
	ParticleSysParamList.Empty();
	ParticleParameterList.Empty();

	TArray<UParticleModule*> ProcessedModules;
	ProcessedModules.Empty();

	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			int32 FindIndex;
			// Grab that parameters from each module...
			check(LODLevel->RequiredModule);
			if (ProcessedModules.Find(LODLevel->RequiredModule, FindIndex) == false)
			{
				LODLevel->RequiredModule->GetParticleSysParamsUtilized(ParticleSysParamList);
				LODLevel->RequiredModule->GetParticleParametersUtilized(ParticleParameterList);
				ProcessedModules.AddUnique(LODLevel->RequiredModule);
			}

			check(LODLevel->SpawnModule);
			if (ProcessedModules.Find(LODLevel->SpawnModule, FindIndex) == false)
			{
				LODLevel->SpawnModule->GetParticleSysParamsUtilized(ParticleSysParamList);
				LODLevel->SpawnModule->GetParticleParametersUtilized(ParticleParameterList);
				ProcessedModules.AddUnique(LODLevel->SpawnModule);
			}

			if (LODLevel->TypeDataModule)
			{
				if (ProcessedModules.Find(LODLevel->TypeDataModule, FindIndex) == false)
				{
					LODLevel->TypeDataModule->GetParticleSysParamsUtilized(ParticleSysParamList);
					LODLevel->TypeDataModule->GetParticleParametersUtilized(ParticleParameterList);
					ProcessedModules.AddUnique(LODLevel->TypeDataModule);
				}
			}
			
			for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
			{
				UParticleModule* Module = LODLevel->Modules[ModuleIndex];
				if (Module)
				{
					if (ProcessedModules.Find(Module, FindIndex) == false)
					{
						Module->GetParticleSysParamsUtilized(ParticleSysParamList);
						Module->GetParticleParametersUtilized(ParticleParameterList);
						ProcessedModules.AddUnique(Module);
					}
				}
			}
		}
	}
}


void UParticleEmitter::Build()
{
	const int32 LODCount = LODLevels.Num();
	if ( LODCount > 0 )
	{
		UParticleLODLevel* HighLODLevel = LODLevels[0];
		check(HighLODLevel);
		if (HighLODLevel->TypeDataModule != nullptr)
		{
			if(HighLODLevel->TypeDataModule->RequiresBuild())
		{
			FParticleEmitterBuildInfo EmitterBuildInfo;
#if WITH_EDITOR
				HighLODLevel->CompileModules( EmitterBuildInfo );
#endif
				HighLODLevel->TypeDataModule->Build( EmitterBuildInfo );
			}

			// Allow TypeData module to cache pointers to modules
			HighLODLevel->TypeDataModule->CacheModuleInfo(this);
		}

		// Cache particle size/offset data for all LOD Levels
		CacheEmitterModuleInfo();
	}
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	// This assert makes sure that packing is as expected.
	// Added FBaseColor...
	// Linear color change
	// Added Flags field	
	static_assert(sizeof(FBaseParticle) == 128, "FBaseParticle size");


	bRequiresLoopNotification = false;
	bAxisLockEnabled = false;
	bMeshRotationActive = false;
	LockAxisFlags = EPAL_NONE;
	ModuleOffsetMap.Empty();
	ModuleInstanceOffsetMap.Empty();
	ModulesNeedingInstanceData.Empty();
	MeshMaterials.Empty();
	DynamicParameterDataOffset = 0;
	LightDataOffset = 0;
	LightVolumetricScatteringIntensity = 0;
	CameraPayloadOffset = 0;
	ParticleSize = sizeof(FBaseParticle);
	ReqInstanceBytes = 0;
	PivotOffset = FVector2D(-0.5f, -0.5f);
	TypeDataOffset = 0;
	TypeDataInstanceOffset = -1;
	SubUVAnimation = nullptr;

	UParticleLODLevel* HighLODLevel = GetLODLevel(0);
	check(HighLODLevel);

	UParticleModuleTypeDataBase* HighTypeData = HighLODLevel->TypeDataModule;
	if (HighTypeData)
	{
		int32 ReqBytes = HighTypeData->RequiredBytes(static_cast<UParticleModuleTypeDataBase*>(nullptr));
		if (ReqBytes)
		{
			TypeDataOffset = ParticleSize;
			ParticleSize += ReqBytes;
		}

		int32 TempInstanceBytes = HighTypeData->RequiredBytesPerInstance();
		if (TempInstanceBytes)
		{
			TypeDataInstanceOffset = ReqInstanceBytes;
			ReqInstanceBytes += TempInstanceBytes;
		}
	}

	// Grab required module
	UParticleModuleRequired* RequiredModule = HighLODLevel->RequiredModule;
	check(RequiredModule);
	// mesh rotation active if alignment is set
	bMeshRotationActive = (RequiredModule->ScreenAlignment == PSA_Velocity || RequiredModule->ScreenAlignment == PSA_AwayFromCenter);

	// NOTE: This code assumes that the same module order occurs in all LOD levels

	for (int32 ModuleIdx = 0; ModuleIdx < HighLODLevel->Modules.Num(); ModuleIdx++)
	{
		UParticleModule* ParticleModule = HighLODLevel->Modules[ModuleIdx];
		check(ParticleModule);

		// Loop notification?
		bRequiresLoopNotification |= (ParticleModule->bEnabled && ParticleModule->RequiresLoopingNotification());

		if (ParticleModule->IsA(UParticleModuleTypeDataBase::StaticClass()) == false)
		{
			int32 ReqBytes = ParticleModule->RequiredBytes(HighTypeData);
			if (ReqBytes)
			{
				ModuleOffsetMap.Add(ParticleModule, ParticleSize);
				if (ParticleModule->IsA(UParticleModuleParameterDynamic::StaticClass()) && (DynamicParameterDataOffset == 0))
				{
					DynamicParameterDataOffset = ParticleSize;
				}
				if (ParticleModule->IsA(UParticleModuleLight::StaticClass()) && (LightDataOffset == 0))
				{
					UParticleModuleLight* ParticleModuleLight = Cast<UParticleModuleLight>(ParticleModule);
					LightVolumetricScatteringIntensity = ParticleModuleLight->VolumetricScatteringIntensity;
					LightDataOffset = ParticleSize;
				}
				if (ParticleModule->IsA(UParticleModuleCameraOffset::StaticClass()) && (CameraPayloadOffset == 0))
				{
					CameraPayloadOffset = ParticleSize;
				}
				ParticleSize += ReqBytes;
			}

			int32 TempInstanceBytes = ParticleModule->RequiredBytesPerInstance();
			if (TempInstanceBytes > 0)
			{
				// Add the high-lodlevel offset to the lookup map
				ModuleInstanceOffsetMap.Add(ParticleModule, ReqInstanceBytes);
				// Remember that this module has emitter-instance data
				ModulesNeedingInstanceData.Add(ParticleModule);

				// Add all the other LODLevel modules, using the same offset.
				// This removes the need to always also grab the HighestLODLevel pointer.
				for (int32 LODIdx = 1; LODIdx < LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* CurLODLevel = LODLevels[LODIdx];
					ModuleInstanceOffsetMap.Add(CurLODLevel->Modules[ModuleIdx], ReqInstanceBytes);
				}
				ReqInstanceBytes += TempInstanceBytes;
			}
		}

		if (ParticleModule->IsA(UParticleModuleOrientationAxisLock::StaticClass()))
		{
			UParticleModuleOrientationAxisLock* Module_AxisLock = CastChecked<UParticleModuleOrientationAxisLock>(ParticleModule);
			bAxisLockEnabled = Module_AxisLock->bEnabled;
			LockAxisFlags = Module_AxisLock->LockAxisFlags;
		}
		else if (ParticleModule->IsA(UParticleModulePivotOffset::StaticClass()))
		{
			PivotOffset += Cast<UParticleModulePivotOffset>(ParticleModule)->PivotOffset;
		}
		else if (ParticleModule->IsA(UParticleModuleMeshMaterial::StaticClass()))
		{
			UParticleModuleMeshMaterial* MeshMaterialModule = CastChecked<UParticleModuleMeshMaterial>(ParticleModule);
			if (MeshMaterialModule->bEnabled)
			{
				MeshMaterials = MeshMaterialModule->MeshMaterials;
			}
		}
		else if (ParticleModule->IsA(UParticleModuleSubUV::StaticClass()))
		{
			USubUVAnimation* ModuleSubUVAnimation = Cast<UParticleModuleSubUV>(ParticleModule)->Animation;
			SubUVAnimation = ModuleSubUVAnimation && ModuleSubUVAnimation->SubUVTexture && ModuleSubUVAnimation->IsBoundingGeometryValid()
				? ModuleSubUVAnimation
				: NULL;
		}

		// Set bMeshRotationActive if module says so
		if(!bMeshRotationActive && ParticleModule->TouchesMeshRotation())
		{
			bMeshRotationActive = true;
		}
	}
}

float UParticleEmitter::GetQualityLevelSpawnRateMult()
{
	int32 EffectsQuality = Scalability::GetEffectsQualityDirect(IsInGameThread());
	int32 ReferenceLevel = CVarQLSpawnRateReferenceLevel.GetValueOnAnyThread(true);
	float Level = (ReferenceLevel - EffectsQuality);
	float Q = FMath::Pow(QualityLevelSpawnRateScale, Level);
	return FMath::Min(1.0f, Q);
}

bool UParticleEmitter::HasAnyEnabledLODs()const
{
	for (UParticleLODLevel* LodLevel : LODLevels)
	{
		if (LodLevel && LodLevel->bEnabled)
		{
			return true;
		}
	}
	
	return false;
}

#if STATS

DEFINE_STAT(STAT_EmittersStatGroupTester);

void UParticleEmitter::CreateStatID() const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UParticleEmitterCreateStatID);

	UObject* Outer = GetOuter();
	FName OuterName = Outer ? Outer->GetFName() : NAME_None;
	FString LongName;
	LongName = FString(TEXT("Emitter")) / OuterName.ToString() / EmitterName.ToString();
	StatID = FDynamicStats::CreateStatId<FStatGroup_STATGROUP_Emitters>(LongName);
}
#endif

bool UParticleEmitter::IsSignificant(EParticleSignificanceLevel RequiredSignificance)
{
	UParticleSystem* PSysOuter = CastChecked<UParticleSystem>(GetOuter());
	EParticleSignificanceLevel Significance = FMath::Min(PSysOuter->MaxSignificanceLevel, SignificanceLevel);
	return Significance >= RequiredSignificance;
}

/*-----------------------------------------------------------------------------
	UParticleSpriteEmitter implementation.
-----------------------------------------------------------------------------*/
UParticleSpriteEmitter::UParticleSpriteEmitter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void UParticleSpriteEmitter::PostLoad()
{
	Super::PostLoad();

	// Postload the materials
	for (int32 LODIndex = 0; LODIndex < LODLevels.Num(); LODIndex++)
	{
		UParticleLODLevel* LODLevel = LODLevels[LODIndex];
		if (LODLevel)
		{
			UParticleModuleRequired* RequiredModule = LODLevel->RequiredModule;
			if (RequiredModule)
			{
				if (RequiredModule->Material)
				{
					RequiredModule->Material->ConditionalPostLoad();
				}
			}
		}
	}
}

FParticleEmitterInstance* UParticleSpriteEmitter::CreateInstance(UParticleSystemComponent* InComponent)
{
	// If this emitter was cooked out or has no valid LOD levels don't create an instance for it.
	if ((bCookedOut == true) || (LODLevels.Num() == 0))
	{
		return NULL;
	}

	FParticleEmitterInstance* Instance = 0;

	UParticleLODLevel* LODLevel	= GetLODLevel(0);
	check(LODLevel);

	if (LODLevel->TypeDataModule)
	{
		//@todo. This will NOT work for trails/beams!
		Instance = LODLevel->TypeDataModule->CreateInstance(this, InComponent);
	}
	else
	{
		check(InComponent);
		Instance = new FParticleSpriteEmitterInstance();
		check(Instance);
		Instance->InitParameters(this, InComponent);
	}

	if (Instance)
	{
		Instance->CurrentLODLevelIndex	= 0;
		Instance->CurrentLODLevel		= LODLevels[Instance->CurrentLODLevelIndex];
		Instance->Init();
	}

	return Instance;
}

void UParticleSpriteEmitter::SetToSensibleDefaults()
{
#if WITH_EDITOR
	PreEditChange(NULL);
#endif // WITH_EDITOR

	UParticleLODLevel* LODLevel = LODLevels[0];

	// Spawn rate
	LODLevel->SpawnModule->LODValidity = 1;
	UDistributionFloatConstant* SpawnRateDist = Cast<UDistributionFloatConstant>(LODLevel->SpawnModule->Rate.Distribution);
	if (SpawnRateDist)
	{
		SpawnRateDist->Constant = 20.f;
	}

	// Create basic set of modules

	// Lifetime module
	UParticleModuleLifetime* LifetimeModule = NewObject<UParticleModuleLifetime>(GetOuter());
	UDistributionFloatUniform* LifetimeDist = Cast<UDistributionFloatUniform>(LifetimeModule->Lifetime.Distribution);
	if (LifetimeDist)
	{
		LifetimeDist->Min = 1.0f;
		LifetimeDist->Max = 1.0f;
		LifetimeDist->bIsDirty = true;
	}
	LifetimeModule->LODValidity = 1;
	LODLevel->Modules.Add(LifetimeModule);

	// Size module
	UParticleModuleSize* SizeModule = NewObject<UParticleModuleSize>(GetOuter());
	UDistributionVectorUniform* SizeDist = Cast<UDistributionVectorUniform>(SizeModule->StartSize.Distribution);
	if (SizeDist)
	{
		SizeDist->Min = FVector(25.f, 25.f, 25.f);
		SizeDist->Max = FVector(25.f, 25.f, 25.f);
		SizeDist->bIsDirty = true;
	}
	SizeModule->LODValidity = 1;
	LODLevel->Modules.Add(SizeModule);

	// Initial velocity module
	UParticleModuleVelocity* VelModule = NewObject<UParticleModuleVelocity>(GetOuter());
	UDistributionVectorUniform* VelDist = Cast<UDistributionVectorUniform>(VelModule->StartVelocity.Distribution);
	if (VelDist)
	{
		VelDist->Min = FVector(-10.f, -10.f, 50.f);
		VelDist->Max = FVector(10.f, 10.f, 100.f);
		VelDist->bIsDirty = true;
	}
	VelModule->LODValidity = 1;
	LODLevel->Modules.Add(VelModule);

	// Color over life module
	UParticleModuleColorOverLife* ColorModule = NewObject<UParticleModuleColorOverLife>(GetOuter());
	UDistributionVectorConstantCurve* ColorCurveDist = Cast<UDistributionVectorConstantCurve>(ColorModule->ColorOverLife.Distribution);
	if (ColorCurveDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (int32 Key = 0; Key < 2; Key++)
		{
			int32	KeyIndex = ColorCurveDist->CreateNewKey(Key * 1.0f);
			for (int32 SubIndex = 0; SubIndex < 3; SubIndex++)
			{
				ColorCurveDist->SetKeyOut(SubIndex, KeyIndex, 1.0f);
			}
		}
		ColorCurveDist->bIsDirty = true;
	}
	ColorModule->AlphaOverLife.Distribution = NewObject<UDistributionFloatConstantCurve>(ColorModule);
	UDistributionFloatConstantCurve* AlphaCurveDist = Cast<UDistributionFloatConstantCurve>(ColorModule->AlphaOverLife.Distribution);
	if (AlphaCurveDist)
	{
		// Add two points, one at time 0.0f and one at 1.0f
		for (int32 Key = 0; Key < 2; Key++)
		{
			int32	KeyIndex = AlphaCurveDist->CreateNewKey(Key * 1.0f);
			if (Key == 0)
			{
				AlphaCurveDist->SetKeyOut(0, KeyIndex, 1.0f);
			}
			else
			{
				AlphaCurveDist->SetKeyOut(0, KeyIndex, 0.0f);
			}
		}
		AlphaCurveDist->bIsDirty = true;
	}
	ColorModule->LODValidity = 1;
	LODLevel->Modules.Add(ColorModule);

#if WITH_EDITOR
	PostEditChange();
#endif // WITH_EDITOR
}

/*-----------------------------------------------------------------------------
	UParticleSystem implementation.
-----------------------------------------------------------------------------*/

UParticleSystem::UParticleSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, OcclusionBoundsMethod(EPSOBM_ParticleBounds)
	, HighestSignificance(EParticleSignificanceLevel::Critical)
	, LowestSignificance(EParticleSignificanceLevel::Low)
	, bAnyEmitterLoopsForever(false)
	, bIsImmortal(false)
	, bWillBecomeZombie(false)
{
#if WITH_EDITORONLY_DATA
	ThumbnailDistance = 200.0;
	ThumbnailWarmup = 1.0;
#endif // WITH_EDITORONLY_DATA
	UpdateTime_FPS = 60.0f;
	UpdateTime_Delta = 1.0f/60.0f;
	WarmupTime = 0.0f;
	WarmupTickRate = 0.0f;
#if WITH_EDITORONLY_DATA
	EditorLODSetting = 0;
#endif // WITH_EDITORONLY_DATA
	FixedRelativeBoundingBox.Min = FVector(-1.0f, -1.0f, -1.0f);
	FixedRelativeBoundingBox.Max = FVector(1.0f, 1.0f, 1.0f);
	FixedRelativeBoundingBox.IsValid = true;

	LODMethod = PARTICLESYSTEMLODMETHOD_Automatic;
	LODDistanceCheckTime = 0.25f;
	bRegenerateLODDuplicate = false;
	ThumbnailImageOutOfDate = true;
#if WITH_EDITORONLY_DATA
	FloorMesh = TEXT("/Engine/EditorMeshes/AnimTreeEd_PreviewFloor.AnimTreeEd_PreviewFloor");
	FloorPosition = FVector(0.0f, 0.0f, 0.0f);
	FloorRotation = FRotator(0.0f, 0.0f, 0.0f);
	FloorScale = 1.0f;
	FloorScale3D = FVector(1.0f, 1.0f, 1.0f);
#endif // WITH_EDITORONLY_DATA

	MacroUVPosition = FVector(0.0f, 0.0f, 0.0f);

	MacroUVRadius = 200.0f;
	bAutoDeactivate = true;
	MinTimeBetweenTicks = 0;
	InsignificantReaction = EParticleSystemInsignificanceReaction::Auto;
	InsignificanceDelay = 0.0f;
	MaxSignificanceLevel = EParticleSignificanceLevel::Critical;
	bShouldManageSignificance = false;
}




ParticleSystemLODMethod UParticleSystem::GetCurrentLODMethod()
{
	return ParticleSystemLODMethod(LODMethod);
}


int32 UParticleSystem::GetLODLevelCount()
{
	return LODDistances.Num();
}


float UParticleSystem::GetLODDistance(int32 LODLevelIndex)
{
	if (LODLevelIndex >= LODDistances.Num())
	{
		return -1.0f;
	}

	return LODDistances[LODLevelIndex];
}

void UParticleSystem::SetCurrentLODMethod(ParticleSystemLODMethod InMethod)
{
	LODMethod = InMethod;
}


bool UParticleSystem::SetLODDistance(int32 LODLevelIndex, float InDistance)
{
	if (LODLevelIndex >= LODDistances.Num())
	{
		return false;
	}

	LODDistances[LODLevelIndex] = InDistance;

	return true;
}

bool UParticleSystem::DoesAnyEmitterHaveMotionBlur(int32 LODLevelIndex) const
{
	for (auto& EmitterIter : Emitters)
	{
		auto* EmitterLOD = EmitterIter->GetLODLevel(LODLevelIndex);
		if (!EmitterLOD)
		{
			continue;
		}

		if (EmitterLOD->TypeDataModule && EmitterLOD->TypeDataModule->IsMotionBlurEnabled())
		{
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR
void UParticleSystem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UpdateTime_Delta = 1.0f / UpdateTime_FPS;

	//If the property is NULL then we don't really know what's happened. 
	//Could well be a module change, requiring all instances to be destroyed and recreated.
	bool bEmptyInstances = PropertyChangedEvent.Property == NULL;
	for (TObjectIterator<UParticleSystemComponent> It;It;++It)
	{
		if (It->Template == this)
		{
			It->UpdateInstances(bEmptyInstances);
		}
	}

	// Ensure the bounds have a positive size
	if (FixedRelativeBoundingBox.IsValid)
	{
		if (FixedRelativeBoundingBox.Min.X > FixedRelativeBoundingBox.Max.X)
		{
			Swap(FixedRelativeBoundingBox.Min.X, FixedRelativeBoundingBox.Max.X);
		}
		if (FixedRelativeBoundingBox.Min.Y > FixedRelativeBoundingBox.Max.Y)
		{
			Swap(FixedRelativeBoundingBox.Min.Y, FixedRelativeBoundingBox.Max.Y);
		}
		if (FixedRelativeBoundingBox.Min.Z > FixedRelativeBoundingBox.Max.Z)
		{
			Swap(FixedRelativeBoundingBox.Min.Z, FixedRelativeBoundingBox.Max.Z);
		}
	}

	// Recompute the looping flag
	bAnyEmitterLoopsForever = false;
	HighestSignificance = EParticleSignificanceLevel::Low;
	LowestSignificance = EParticleSignificanceLevel::Critical;
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter != nullptr)
		{
			for (const UParticleLODLevel* LODLevel : Emitter->LODLevels)
			{
				if (LODLevel != nullptr)
				{
					if (LODLevel->bEnabled)
					{
						const UParticleModuleRequired* RequiredModule = LODLevel->RequiredModule;
						if ((RequiredModule != nullptr) && (RequiredModule->EmitterLoops == 0))
						{
							bAnyEmitterLoopsForever = true;

							UParticleModuleSpawn* SpawnModule = LODLevel->SpawnModule;
							check(SpawnModule);

							// check if any emitter will cause the system to never be deleted
							// terms here are zombie (burst-only, so will stop spawning but emitter instances and psys component will continue existing)
							// and immortal (any emitter will loop indefinitely and does not have finite duration)
							if (RequiredModule->EmitterDuration == 0.0f)
							{
								bIsImmortal = true;
								if (SpawnModule->GetMaximumSpawnRate() == 0.0f && !bAutoDeactivate)
								{
									bWillBecomeZombie = true;
								}
							}

						}
					}
				}
			}

			EParticleSignificanceLevel EmitterSignificance = FMath::Min(MaxSignificanceLevel, Emitter->SignificanceLevel);
			if (EmitterSignificance > HighestSignificance)
			{
				HighestSignificance = EmitterSignificance;
			}
			if (EmitterSignificance < LowestSignificance)
			{
				LowestSignificance = EmitterSignificance;
			}
		}
	}

	bShouldManageSignificance = GetLowestSignificance() != EParticleSignificanceLevel::Critical/* && !ContainsEmitterType(UParticleModuleTypeDataBeam2::StaticClass())*/;

	//cap the WarmupTickRate to realistic values
	if (WarmupTickRate <= 0)
	{
		WarmupTickRate = 0;
	}
	else if (WarmupTickRate > WarmupTime)
	{
		WarmupTickRate = WarmupTime;
	}

	ThumbnailImageOutOfDate = true;

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UParticleSystem::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
#if WITH_EDITORONLY_DATA
	// Ensure that soloing is undone...
	int32 NumEmitters = FMath::Min(Emitters.Num(),SoloTracking.Num());
	for (int32 EmitterIdx = 0; EmitterIdx < NumEmitters; EmitterIdx++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIdx];
		Emitter->bIsSoloing = false;
		FLODSoloTrack& SoloTrack = SoloTracking[EmitterIdx];
		int32 NumLODs = FMath::Min(Emitter->LODLevels.Num(),SoloTrack.SoloEnableSetting.Num());
		for (int32 LODIdx = 0; LODIdx < NumLODs; LODIdx++)
		{
			UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
			{
				// Restore the enabled settings - ie turn off soloing...
				LODLevel->bEnabled = SoloTrack.SoloEnableSetting[LODIdx];
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UParticleSystem::PostLoad()
{
	Super::PostLoad();

	// Run thru all of the emitters, load them up and compute some flags based on them
	bHasPhysics = false;
	bAnyEmitterLoopsForever = false;
	HighestSignificance = EParticleSignificanceLevel::Low;
	LowestSignificance = EParticleSignificanceLevel::Critical;
	for (int32 i = Emitters.Num() - 1; i >= 0; i--)
	{
		// Remove any old emitters
		UParticleEmitter* Emitter = Emitters[i];
		if (Emitter == NULL)
		{
			// Empty emitter slots are ok with cooked content.
			if( !FPlatformProperties::RequiresCookedData() && !GIsServer)
			{
				UE_LOG(LogParticles, Warning, TEXT("ParticleSystem contains empty emitter slots - %s"), *GetFullName());
			}
			continue;
		}

		Emitter->ConditionalPostLoad();

		bool bCookedOut = false;
		if (UParticleSpriteEmitter* SpriteEmitter = Cast<UParticleSpriteEmitter>(Emitter))
		{
			bCookedOut = SpriteEmitter->bCookedOut;
		}

		if (!bCookedOut)
		{
			UParticleLODLevel* LODLevel = Emitter->LODLevels[0];
			check(LODLevel);

			LODLevel->ConditionalPostLoad();
				
			//@todo. Move these flag calculations into the editor and serialize?
			for (UParticleLODLevel* ParticleLODLevel : Emitter->LODLevels)
			{
				if (ParticleLODLevel)
				{
					//@todo. This is a temporary fix for emitters that apply physics.
					// Check for collision modules with bApplyPhysics set to true
					for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
					{
						UParticleModuleCollision* CollisionModule = Cast<UParticleModuleCollision>(ParticleLODLevel->Modules[ModuleIndex]);
						if (CollisionModule)
						{
							if (CollisionModule->bApplyPhysics == true)
							{
								bHasPhysics = true;
								break;
							}
						}
					}

					if (LODLevel->bEnabled)
					{
						const UParticleModuleRequired* RequiredModule = LODLevel->RequiredModule;
						if ((RequiredModule != nullptr) && (RequiredModule->EmitterLoops == 0))
						{
							bAnyEmitterLoopsForever = true;
						}

						UParticleModuleSpawn* SpawnModule = LODLevel->SpawnModule;
						check(SpawnModule);

						if (RequiredModule->EmitterDuration == 0.0f)
						{
							bIsImmortal = true;
							if (SpawnModule->GetMaximumSpawnRate() == 0.0f && !bAutoDeactivate)
							{
								bWillBecomeZombie = true;
							}
						}
					}
				}
			}

			EParticleSignificanceLevel EmitterSignificance = FMath::Min(MaxSignificanceLevel, Emitter->SignificanceLevel);
			if (EmitterSignificance > HighestSignificance)
			{
				HighestSignificance = EmitterSignificance;
			}
			if (EmitterSignificance < LowestSignificance)
			{
				LowestSignificance = EmitterSignificance;
			}
		}
	}

	bShouldManageSignificance = GetLowestSignificance() != EParticleSignificanceLevel::Critical /* && !ContainsEmitterType(UParticleModuleTypeDataBeam2::StaticClass())*/;

	if (LODSettings.Num() == 0)
	{
		if (Emitters.Num() > 0)
		{
			UParticleEmitter* Emitter = Emitters[0];
			if (Emitter)
			{
				LODSettings.AddUninitialized(Emitter->LODLevels.Num());
				for (int32 LODIndex = 0; LODIndex < LODSettings.Num(); LODIndex++)
				{
					LODSettings[LODIndex] = FParticleSystemLOD::CreateParticleSystemLOD();
				}
			}
		}
		else
		{
			LODSettings.AddUninitialized();
			LODSettings[0] = FParticleSystemLOD::CreateParticleSystemLOD();
		}
	}

	// Add default LOD Distances
	if( LODDistances.Num() == 0 && Emitters.Num() > 0 )
	{
		UParticleEmitter* Emitter = Emitters[0];
		if (Emitter)
		{
			LODDistances.AddUninitialized(Emitter->LODLevels.Num());
			for (int32 LODIndex = 0; LODIndex < LODDistances.Num(); LODIndex++)
			{
				LODDistances[LODIndex] = LODIndex * 2500.0f;
			}
		}
	}

#if WITH_EDITOR
	// Due to there still being some ways that LODLevel counts get mismatched,
	// when loading in the editor LOD levels will always be checked and fixed
	// up... This can be removed once all the edge cases that lead to the
	// problem are found and fixed.
	if (GIsEditor)
	{
		// Fix the LOD distance array and mismatched lod levels
		int32 LODCount_0 = -1;
		for (int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); EmitterIndex++)
		{
			UParticleEmitter* Emitter  = Emitters[EmitterIndex];
			if (Emitter)
			{
				if (LODCount_0 == -1)
				{
					LODCount_0 = Emitter->LODLevels.Num();
				}
				else
				{
					int32 EmitterLODCount = Emitter->LODLevels.Num();
					if (EmitterLODCount != LODCount_0)
					{
						UE_LOG(LogParticles, Warning, TEXT("Emitter %d has mismatched LOD level count - expected %d, found %d. PS = %s"),
							EmitterIndex, LODCount_0, EmitterLODCount, *GetPathName());
						UE_LOG(LogParticles, Warning, TEXT("Fixing up now... Package = %s"), *(GetOutermost()->GetPathName()));

						if (EmitterLODCount > LODCount_0)
						{
							Emitter->LODLevels.RemoveAt(LODCount_0, EmitterLODCount - LODCount_0);
						}
						else
						{
							for (int32 NewLODIndex = EmitterLODCount; NewLODIndex < LODCount_0; NewLODIndex++)
							{
								if (Emitter->CreateLODLevel(NewLODIndex) != NewLODIndex)
								{
									UE_LOG(LogParticles, Warning, TEXT("Failed to add LOD level %s"), NewLODIndex);
								}
							}
						}
					}
				}
			}
		}

		if (LODCount_0 > 0)
		{
			if (LODDistances.Num() < LODCount_0)
			{
				for (int32 DistIndex = LODDistances.Num(); DistIndex < LODCount_0; DistIndex++)
				{
					float Distance = DistIndex * 2500.0f;
					LODDistances.Add(Distance);
				}
			}
			else
			if (LODDistances.Num() > LODCount_0)
			{
				LODDistances.RemoveAt(LODCount_0, LODDistances.Num() - LODCount_0);
			}
		}
		else
		{
			LODDistances.Empty();
		}

		if (LODCount_0 > 0)
		{
			if (LODSettings.Num() < LODCount_0)
			{
				for (int32 DistIndex = LODSettings.Num(); DistIndex < LODCount_0; DistIndex++)
				{
					LODSettings.Add(FParticleSystemLOD::CreateParticleSystemLOD());
				}
			}
			else
				if (LODSettings.Num() > LODCount_0)
				{
					LODSettings.RemoveAt(LODCount_0, LODSettings.Num() - LODCount_0);
				}
		}
		else
		{
			LODSettings.Empty();
		}
	}
#endif

#if WITH_EDITORONLY_DATA
	// Reset cascade's UI LOD setting to 0.
	EditorLODSetting = 0;
#endif // WITH_EDITORONLY_DATA

	FixedRelativeBoundingBox.IsValid = true;

	// Set up the SoloTracking...
	SetupSoloing();
}

void UParticleSystem::UpdateColorModuleClampAlpha(UParticleModuleColorBase* ColorModule)
{
	if (ColorModule)
	{
		TArray<const FCurveEdEntry*> CurveEntries;
		ColorModule->RemoveModuleCurvesFromEditor(CurveEdSetup);
		ColorModule->AddModuleCurvesToEditor(CurveEdSetup, CurveEntries);
	}
}

void UParticleSystem::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	OutTags.Add( FAssetRegistryTag("HasGPUEmitter", HasGPUEmitter() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical) );

	const float BoundsSize = FixedRelativeBoundingBox.GetSize().GetMax();
	OutTags.Add(FAssetRegistryTag("FixedBoundsSize", bUseFixedRelativeBoundingBox ? FString::Printf(TEXT("%.2f"), BoundsSize) : FString(TEXT("None")), FAssetRegistryTag::TT_Numerical));

	OutTags.Add(FAssetRegistryTag("NumEmitters", Lex::ToString(Emitters.Num()), FAssetRegistryTag::TT_Numerical));

	OutTags.Add(FAssetRegistryTag("NumLODs", Lex::ToString(LODDistances.Num()), FAssetRegistryTag::TT_Numerical));

	OutTags.Add(FAssetRegistryTag("WarmupTime", Lex::ToString(WarmupTime), FAssetRegistryTag::TT_Numerical));

	// Done here instead of as an AssetRegistrySearchable string to avoid the long prefix on the enum value string
	FString LODMethodString = TEXT("Unknown");
	switch (LODMethod)
	{
	case PARTICLESYSTEMLODMETHOD_Automatic:
		LODMethodString = TEXT("Automatic");
		break;
	case PARTICLESYSTEMLODMETHOD_DirectSet:
		LODMethodString = TEXT("DirectSet");
		break;
	case PARTICLESYSTEMLODMETHOD_ActivateAutomatic:
		LODMethodString = TEXT("Activate Automatic");
		break;
	default:
		check(false); // Missing enum entry
		break;
	}
	OutTags.Add(FAssetRegistryTag("LODMethod", LODMethodString, FAssetRegistryTag::TT_Alphabetical));

	OutTags.Add(FAssetRegistryTag("CPUCollision", UsesCPUCollision() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag("Looping", bAnyEmitterLoopsForever ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag("Immortal", IsImmortal() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag("Becomes Zombie", WillBecomeZombie() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag("CanBeOccluded", OcclusionBoundsMethod == EParticleSystemOcclusionBoundsMethod::EPSOBM_None ? TEXT("False") : TEXT("True"), FAssetRegistryTag::TT_Alphabetical));

	uint32 NumEmittersAtEachSig[(int32)EParticleSignificanceLevel::Num] = { 0, 0, 0, 0 };
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter)
		{
			++NumEmittersAtEachSig[(int32)Emitter->SignificanceLevel];			
		}
	}
	OutTags.Add(FAssetRegistryTag("Critical Emitters", Lex::ToString(NumEmittersAtEachSig[(int32)EParticleSignificanceLevel::Critical]), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("High Emitters", Lex::ToString(NumEmittersAtEachSig[(int32)EParticleSignificanceLevel::High]), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Medium Emitters", Lex::ToString(NumEmittersAtEachSig[(int32)EParticleSignificanceLevel::Medium]), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("Low Emitters", Lex::ToString(NumEmittersAtEachSig[(int32)EParticleSignificanceLevel::Low]), FAssetRegistryTag::TT_Numerical));

	Super::GetAssetRegistryTags(OutTags);
}

bool UParticleSystem::UsesCPUCollision() const
{
	for (const UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter != nullptr)
		{
			// If we have not yet found a CPU collision module, and we have some enabled LODs to look in..
			if (Emitter->HasAnyEnabledLODs() && Emitter->LODLevels.Num() > 0)
			{
				if (const UParticleLODLevel* HighLODLevel = Emitter->LODLevels[0])
				{
					// Iterate over modules of highest LOD (will have all the modules)
					for (const UParticleModule* Module : HighLODLevel->Modules)
					{
						// If an enabled CPU collision module 
						if (Module->bEnabled && Module->IsA<UParticleModuleCollision>())
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

bool UParticleSystem::CanBeClusterRoot() const
{
	return true;
}

bool UParticleSystem::CalculateMaxActiveParticleCounts()
{
	bool bSuccess = true;

	for (int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); EmitterIndex++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		if (Emitter)
		{
			if (Emitter->CalculateMaxActiveParticleCount() == false)
			{
				bSuccess = false;
			}
		}
	}

	return bSuccess;
}


void UParticleSystem::GetParametersUtilized(TArray<TArray<FString> >& ParticleSysParamList,
											TArray<TArray<FString> >& ParticleParameterList)
{
	ParticleSysParamList.Empty();
	ParticleParameterList.Empty();

	for (int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); EmitterIndex++)
	{
		int32 CheckIndex;
		CheckIndex = ParticleSysParamList.AddZeroed();
		check(CheckIndex == EmitterIndex);
		CheckIndex = ParticleParameterList.AddZeroed();
		check(CheckIndex == EmitterIndex);

		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		if (Emitter)
		{
			Emitter->GetParametersUtilized(
				ParticleSysParamList[EmitterIndex],
				ParticleParameterList[EmitterIndex]);
		}
	}
}


void UParticleSystem::SetupSoloing()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (Emitters.Num())
		{
			// Store the settings of bEnabled for each LODLevel in each emitter
			SoloTracking.Empty(Emitters.Num());
			SoloTracking.AddZeroed(Emitters.Num());
			for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
			{
				UParticleEmitter* Emitter = Emitters[EmitterIdx];
				if (Emitter != nullptr)
				{
					FLODSoloTrack& SoloTrack = SoloTracking[EmitterIdx];
					SoloTrack.SoloEnableSetting.Empty(Emitter->LODLevels.Num());
					SoloTrack.SoloEnableSetting.AddZeroed(Emitter->LODLevels.Num());
				}
			}

			for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
			{
				UParticleEmitter* Emitter = Emitters[EmitterIdx];
				if (Emitter != NULL)
				{
					FLODSoloTrack& SoloTrack = SoloTracking[EmitterIdx];
					int32 MaxLOD = FMath::Min(SoloTrack.SoloEnableSetting.Num(), Emitter->LODLevels.Num());
					for (int32 LODIdx = 0; LODIdx < MaxLOD; LODIdx++)
					{
						UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
						check(LODLevel);
						SoloTrack.SoloEnableSetting[LODIdx] = LODLevel->bEnabled;
					}
				}
			}
		}
	}
#endif
}


bool UParticleSystem::ToggleSoloing(class UParticleEmitter* InEmitter)
{
	bool bSoloingReturn = false;
	if (InEmitter != NULL)
	{
		bool bOtherEmitterIsSoloing = false;
		// Set the given one
		int32 SelectedIndex = -1;
		for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = Emitters[EmitterIdx];
			check(Emitter != NULL);
			if (Emitter == InEmitter)
			{
				SelectedIndex = EmitterIdx;
			}
			else
			{
				if (Emitter->bIsSoloing == true)
				{
					bOtherEmitterIsSoloing = true;
					bSoloingReturn = true;
				}
			}
		}

		if (SelectedIndex != -1)
		{
			InEmitter->bIsSoloing = !InEmitter->bIsSoloing;
			for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
			{
				UParticleEmitter* Emitter = Emitters[EmitterIdx];
				FLODSoloTrack& SoloTrack = SoloTracking[EmitterIdx];
				if (EmitterIdx == SelectedIndex)
				{
					for (int32 LODIdx = 0; LODIdx < InEmitter->LODLevels.Num(); LODIdx++)
					{
						UParticleLODLevel* LODLevel = InEmitter->LODLevels[LODIdx];
						if (InEmitter->bIsSoloing == false)
						{
							if (bOtherEmitterIsSoloing == false)
							{
								// Restore the enabled settings - ie turn off soloing...
								LODLevel->bEnabled = SoloTrack.SoloEnableSetting[LODIdx];
							}
							else
							{
								// Disable the emitter
								LODLevel->bEnabled = false;
							}
						}
						else 
						if (bOtherEmitterIsSoloing == true)
						{
							// Need to restore old settings of this emitter as it is now soloing
							LODLevel->bEnabled = SoloTrack.SoloEnableSetting[LODIdx];
						}
					}
				}
				else
				{
					// Restore all other emitters if this disables soloing...
					if ((InEmitter->bIsSoloing == false) && (bOtherEmitterIsSoloing == false))
					{
						for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
						{
							UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
							// Restore the enabled settings - ie turn off soloing...
							LODLevel->bEnabled = SoloTrack.SoloEnableSetting[LODIdx];
						}
					}
					else
					{
						if (Emitter->bIsSoloing == false)
						{
							for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
							{
								UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
								// Disable the emitter
								LODLevel->bEnabled = false;
							}
						}
					}
				}
			}
		}

		// We checked the other emitters above...
		// Make sure we catch the case of the first one toggled to true!
		if (InEmitter->bIsSoloing == true)
		{
			bSoloingReturn = true;
		}
	}

	return bSoloingReturn;
}


bool UParticleSystem::TurnOffSoloing()
{
	for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIdx];
		if (Emitter != NULL)
		{
			FLODSoloTrack& SoloTrack = SoloTracking[EmitterIdx];
			for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
			{
				UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
				if (LODLevel != NULL)
				{
					// Restore the enabled settings - ie turn off soloing...
					LODLevel->bEnabled = SoloTrack.SoloEnableSetting[LODIdx];
				}
			}
			Emitter->bIsSoloing = false;
		}
	}

	return true;
}


void UParticleSystem::SetupLODValidity()
{
	for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIdx];
		if (Emitter != NULL)
		{
			for (int32 Pass = 0; Pass < 2; Pass++)
			{
				for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
					if (LODLevel != NULL)
					{
						for (int32 ModuleIdx = -3; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							int32 ModuleFetchIdx;
							switch (ModuleIdx)
							{
							case -3:	ModuleFetchIdx = INDEX_REQUIREDMODULE;	break;
							case -2:	ModuleFetchIdx = INDEX_SPAWNMODULE;		break;
							case -1:	ModuleFetchIdx = INDEX_TYPEDATAMODULE;	break;
							default:	ModuleFetchIdx = ModuleIdx;				break;
							}

							UParticleModule* Module = LODLevel->GetModuleAtIndex(ModuleFetchIdx);
							if (Module != NULL)
							{
								// On pass 1, clear the LODValidity flags
								// On pass 2, set it
								if (Pass == 0)
								{
									Module->LODValidity = 0;
								}
								else
								{
									Module->LODValidity |= (1 << LODIdx);
								}
							}
						}
					}
				}
			}
		}
	}
}

#if WITH_EDITOR

bool UParticleSystem::RemoveAllDuplicateModules(bool bInMarkForCooker, TMap<UObject*,bool>* OutRemovedModules)
{
	// Generate a map of module classes used to instances of those modules 
	TMap<UClass*,TMap<UParticleModule*,int32> > ClassToModulesMap;
	for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIdx];
		if (Emitter != NULL)
		{
			if (Emitter->bCookedOut == false)
			{
				for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
				{
					UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
					if (LODLevel != NULL)
					{
						for (int32 ModuleIdx = -1; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
						{
							UParticleModule* Module = NULL;
							if (ModuleIdx == -1)
							{
								Module = LODLevel->SpawnModule;
							}
							else
							{
								Module = LODLevel->Modules[ModuleIdx];
							}
							if (Module != NULL)
							{
								TMap<UParticleModule*,int32>* ModuleList = ClassToModulesMap.Find(Module->GetClass());
								if (ModuleList == NULL)
								{
									TMap<UParticleModule*,int32> TempModuleList;
									ClassToModulesMap.Add(Module->GetClass(), TempModuleList);
									ModuleList = ClassToModulesMap.Find(Module->GetClass());
								}
								check(ModuleList);
								int32* ModuleCount = ModuleList->Find(Module);
								if (ModuleCount == NULL)
								{
									int32 TempModuleCount = 0;
									ModuleList->Add(Module, TempModuleCount);
									ModuleCount = ModuleList->Find(Module);
								}
								check(ModuleCount);
								(*ModuleCount)++;
							}
						}
					}
				}
			}
		}
	}

	// Now we have a list of module classes and the modules they contain...
	// Find modules of the same class that have the exact same settings.
	TMap<UParticleModule*, TArray<UParticleModule*> > DuplicateModules;
	TMap<UParticleModule*,bool> FoundAsADupeModules;
	TMap<UParticleModule*, UParticleModule*> ReplaceModuleMap;
	bool bRemoveDuplicates = true;
	for (TMap<UClass*,TMap<UParticleModule*,int32> >::TIterator ModClassIt(ClassToModulesMap); ModClassIt; ++ModClassIt)
	{
		UClass* ModuleClass = ModClassIt.Key();
		TMap<UParticleModule*,int32>& ModuleMap = ModClassIt.Value();
		if (ModuleMap.Num() > 1)
		{
			// There is more than one of this module, so see if there are dupes...
			TArray<UParticleModule*> ModuleArray;
			for (TMap<UParticleModule*,int32>::TIterator ModuleIt(ModuleMap); ModuleIt; ++ModuleIt)
			{
				ModuleArray.Add(ModuleIt.Key());
			}

			// For each module, see if it it a duplicate of another
			for (int32 ModuleIdx = 0; ModuleIdx < ModuleArray.Num(); ModuleIdx++)
			{
				UParticleModule* SourceModule = ModuleArray[ModuleIdx];
				if (FoundAsADupeModules.Find(SourceModule) == NULL)
				{
					for (int32 InnerModuleIdx = ModuleIdx + 1; InnerModuleIdx < ModuleArray.Num(); InnerModuleIdx++)
					{
						UParticleModule* CheckModule = ModuleArray[InnerModuleIdx];
						if (FoundAsADupeModules.Find(CheckModule) == NULL)
						{
							bool bIsDifferent = false;
							static const FName CascadeCategory(TEXT("Cascade"));
							// Copy non component properties from the old actor to the new actor
							for (UProperty* Property = ModuleClass->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
							{
								bool bIsTransient = (Property->PropertyFlags & CPF_Transient) != 0;
								bool bIsEditorOnly = (Property->PropertyFlags & CPF_EditorOnly) != 0;
								bool bIsCascade = (FObjectEditorUtils::GetCategoryFName(Property) == CascadeCategory);
								// Ignore 'Cascade' category, transient, native and EditorOnly properties...
								if (!bIsTransient && !bIsEditorOnly && !bIsCascade)
								{
									for( int32 iProp=0; iProp<Property->ArrayDim; iProp++ )
									{
										bool bIsIdentical = Property->Identical_InContainer(SourceModule, CheckModule, iProp, PPF_DeepComparison);
										if (bIsIdentical == false)
										{
											bIsDifferent = true;
											break;
										}
									}
								}
							}

							if (bIsDifferent == false)
							{
								TArray<UParticleModule*>* DupedModules = DuplicateModules.Find(SourceModule);
								if (DupedModules == NULL)
								{
									TArray<UParticleModule*> TempDupedModules;
									DuplicateModules.Add(SourceModule, TempDupedModules);
									DupedModules = DuplicateModules.Find(SourceModule);
								}
								check(DupedModules);
								if (ReplaceModuleMap.Find(CheckModule) == NULL)
								{
									ReplaceModuleMap.Add(CheckModule, SourceModule);
								}
								else
								{
									UE_LOG(LogParticles, Error, TEXT("Module already in replacement map - ABORTING CONVERSION!!!!"));
									bRemoveDuplicates = false;
								}
								DupedModules->AddUnique(CheckModule);
								FoundAsADupeModules.Add(CheckModule, true);
							}
						}
					}
				}
			}
		}
	}

	// If not errors were found, and there are duplicates, remove them...
	if (bRemoveDuplicates && (ReplaceModuleMap.Num() > 0))
	{
		TArray<UParticleModule*> RemovedModules;
		for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
		{
			UParticleEmitter* Emitter = Emitters[EmitterIdx];
			if (Emitter != NULL)
			{
				if (Emitter->bCookedOut == false)
				{
					for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
					{
						UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
						if (LODLevel != NULL)
						{
							for (int32 ModuleIdx = -1; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
							{
								UParticleModule* Module = NULL;
								if (ModuleIdx == -1)
								{
									Module = LODLevel->SpawnModule;
								}
								else
								{
									Module = LODLevel->Modules[ModuleIdx];
								}
								if (Module != NULL)
								{
									UParticleModule** ReplacementModule = ReplaceModuleMap.Find(Module);
									if (ReplacementModule != NULL)
									{
										UParticleModule* ReplaceMod = *ReplacementModule;
										if (ModuleIdx == -1)
										{
											LODLevel->SpawnModule = CastChecked<UParticleModuleSpawn>(ReplaceMod);
										}
										else
										{
											LODLevel->Modules[ModuleIdx] = ReplaceMod;
										}

										if (bInMarkForCooker == true)
										{
											RemovedModules.AddUnique(Module);
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (bInMarkForCooker == true)
		{
			for (int32 MarkIdx = 0; MarkIdx < RemovedModules.Num(); MarkIdx++)
			{
				UParticleModule* RemovedModule = RemovedModules[MarkIdx];
				RemovedModule->SetFlags(RF_Transient);
				if (OutRemovedModules != NULL)
				{
					OutRemovedModules->Add(RemovedModule, true);
				}
			}
		}

		// Update the list of modules in each emitter
		UpdateAllModuleLists();
	}

	return true;
}


void UParticleSystem::UpdateAllModuleLists()
{
	for (int32 EmitterIdx = 0; EmitterIdx < Emitters.Num(); EmitterIdx++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIdx];
		if (Emitter != NULL)
		{
			for (int32 LODIdx = 0; LODIdx < Emitter->LODLevels.Num(); LODIdx++)
			{
				UParticleLODLevel* LODLevel = Emitter->LODLevels[LODIdx];
				if (LODLevel != NULL)
				{
					LODLevel->UpdateModuleLists();
				}
			}

			// Allow type data module to cache any module info
			if(Emitter->LODLevels.Num() > 0)
			{
				UParticleLODLevel* HighLODLevel = Emitter->LODLevels[0];
				if (HighLODLevel != nullptr && HighLODLevel->TypeDataModule != nullptr)
				{
					// Allow TypeData module to cache pointers to modules
					HighLODLevel->TypeDataModule->CacheModuleInfo(Emitter);
				}
			}

			// Update any cached info from modules on the emitter
			Emitter->CacheEmitterModuleInfo();
		}
	}
}
#endif


void UParticleSystem::BuildEmitters()
{
	const int32 EmitterCount = Emitters.Num();
	for ( int32 EmitterIndex = 0; EmitterIndex < EmitterCount; ++EmitterIndex )
	{
		if (UParticleEmitter* Emitter = Emitters[EmitterIndex])
		{
			Emitter->Build();
		}
	}
}

static bool LogReasoningForAnyThreadTicking()
{
	static bool bLogThreadedParticleTicking = FParse::Param(FCommandLine::Get(), TEXT("LogThreadedParticleTicking"));
	return bLogThreadedParticleTicking;
}

void UParticleSystem::ComputeCanTickInAnyThread()
{
	check(!bIsElligibleForAsyncTickComputed);
	bIsElligibleForAsyncTickComputed = true;

	bIsElligibleForAsyncTick = true; // assume everything is async
	int32 EmitterIndex;
	for (EmitterIndex = 0; EmitterIndex < Emitters.Num(); EmitterIndex++)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		if (Emitter)
		{
			for (int32 LevelIndex = 0; LevelIndex < Emitter->LODLevels.Num(); LevelIndex++)
			{
				UParticleLODLevel* LODLevel	= Emitter->LODLevels[LevelIndex];
				if (LODLevel)
				{
					for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
					{
						UParticleModule* Module	= LODLevel->Modules[ModuleIndex];
						if (Module && !Module->CanTickInAnyThread())
						{
							bIsElligibleForAsyncTick = false;
							if (LogReasoningForAnyThreadTicking())
							{
								UE_LOG(LogParticles, Display, TEXT("Cannot tick %s in parallel because module %s in Emitter %s cannot tick in in parallel."), *GetFullName(), *Module->GetFullName(), *Emitter->GetFullName());
							}
							else
							{
								return;
							}
						}
					}
				}
			}

		}
	}
	if (LogReasoningForAnyThreadTicking() && bIsElligibleForAsyncTick)
	{
		UE_LOG(LogParticles, Display, TEXT("Can tick %s in parallel."), *GetFullName());
	}
}

bool UParticleSystem::ContainsEmitterType(UClass* TypeData)
{
	for ( int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); ++EmitterIndex)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		if (Emitter)
		{
			UParticleLODLevel* LODLevel = Emitter->LODLevels[0];
			if (LODLevel && LODLevel->TypeDataModule && LODLevel->TypeDataModule->IsA(TypeData))
			{
				return true;
			}
		}
	}
	
	return false;
}

bool UParticleSystem::HasGPUEmitter() const
{
	for (int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); ++EmitterIndex)
	{
		if (Emitters[EmitterIndex] == nullptr)
		{
			continue;
		}
		// We can just check for the GPU type data at the highest LOD.
		UParticleLODLevel* LODLevel = Emitters[EmitterIndex]->LODLevels[0];
		if( LODLevel )
		{
			UParticleModule* TypeDataModule = LODLevel->TypeDataModule;
			if( TypeDataModule && TypeDataModule->IsA(UParticleModuleTypeDataGpu::StaticClass()) )
			{
				return true;
			}
		}
	}
	return false;
}

FOnSystemPreActivationChange UParticleSystemComponent::OnSystemPreActivationChange;

UParticleSystemComponent::UParticleSystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), FXSystem(NULL), ReleaseResourcesFence(NULL)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	bTickInEditor = true;
	MaxTimeBeforeForceUpdateTransform = 5.0f;
	bAutoActivate = true;
	bResetOnDetach = false;
	OldPosition = FVector(0.0f, 0.0f, 0.0f);

	PartSysVelocity = FVector(0.0f, 0.0f, 0.0f);

	WarmupTime = 0.0f;
	SecondsBeforeInactive = 1.0f;
	bIsTransformDirty = false;
	bSkipUpdateDynamicDataDuringTick = false;
	bIsViewRelevanceDirty = true;
	CustomTimeDilation = 1.0f;
	bAllowConcurrentTick = true;
	bAsyncWorkOutstanding = false;
	bWasActive = false;
#if WITH_EDITORONLY_DATA
	EditorDetailMode = -1;
#endif // WITH_EDITORONLY_DATA
	LastCheckedDetailMode = -1;
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bGenerateOverlapEvents = false;

	bCastVolumetricTranslucentShadow = true;

	// Disable receiving decals by default.
	bReceivesDecals = false;

	// Don't need to call OnUpdateTransform, no physics state to update
	bWantsOnUpdateTransform = false;

	SavedAutoAttachRelativeScale3D = FVector(1.f, 1.f, 1.f);
	TimeSinceLastTick = 0;

	RequiredSignificance = EParticleSignificanceLevel::Low;
	LastSignificantTime = 0.0f;
	bIsManagingSignificance = 0;
	bWasManagingSignificance = 0;

	FlexFluidSurfaceOverride = NULL;
}

void UParticleSystemComponent::SetRequiredSignificance(EParticleSignificanceLevel NewRequiredSignificance)
{
	if (Template)
	{
		RequiredSignificance = NewRequiredSignificance;

		EParticleSystemInsignificanceReaction Reaction = Template->InsignificantReaction;
		if (Template->InsignificantReaction == EParticleSystemInsignificanceReaction::Auto)
		{
			Reaction = Template->IsLooping() ? EParticleSystemInsignificanceReaction::DisableTick : EParticleSystemInsignificanceReaction::Complete;
		}

		//If our tick is disabled we need to work out if we should re-enable it based on this new significance
		if (!IsComponentTickEnabled() && Reaction == EParticleSystemInsignificanceReaction::DisableTick && Template->GetHighestSignificance() >= NewRequiredSignificance)
		{
			//Set us to be significant again.
			OnSignificanceChanged(true, true, true);
		}
	}
}

void UParticleSystemComponent::OnSignificanceChanged(bool bSignificant, bool bApplyToEmitters, bool bAsync)
{
	ForceAsyncWorkCompletion(STALL, false);
	int32 LocalNumSignificantEmitters = 0;
	if (bSignificant)
	{
		if (bAsync)
		{
			SetComponentTickEnabledAsync(true);
		}
		else
		{
			SetComponentTickEnabled(true);
		}

		if (bApplyToEmitters && EmitterInstances.Num() > 0)
		{
			//Mark any emitters as significant if needed.
			for (FParticleEmitterInstance* Inst : EmitterInstances)
			{
				if (Inst)
				{
					if (Inst->SpriteTemplate->IsSignificant(RequiredSignificance))
					{
						Inst->bEnabled = true;
						Inst->SetHaltSpawning(false);
						Inst->SetFakeBurstWhenSpawningSupressed(false);
						++LocalNumSignificantEmitters;
					}
				}
				else
				{
					++LocalNumSignificantEmitters;//Set significant for missing emitters due to other reasons such as detail mode.
				}
			}

			if (LocalNumSignificantEmitters == 0)
			{
				UE_LOG(LogParticles, Warning, TEXT("Setting PSC as significant but it has no significant emitters. %s Template: %s"), *GetFullName(), *Template->GetFullName());
			}
			NumSignificantEmitters = LocalNumSignificantEmitters;
		}
	}
	else
	{
		if (bAsync)
		{
			SetComponentTickEnabledAsync(false);
		}
		else
		{
			SetComponentTickEnabled(false);
		}

		if (bApplyToEmitters && EmitterInstances.Num() > 0)
		{
			//Mark any emitters as significant if needed.
			for (FParticleEmitterInstance* Inst : EmitterInstances)
			{
				if (Inst)
				{
					UParticleLODLevel* SpriteLODLevel = Inst->SpriteTemplate->GetCurrentLODLevel(Inst);
					if (SpriteLODLevel && SpriteLODLevel->bEnabled)//Checking these too as they can stop us from marking emitters as signficant during update and trigger setting insignificant.
					{
						if (Inst->SpriteTemplate->IsSignificant(RequiredSignificance))
						{
							++LocalNumSignificantEmitters;
						}
						else
						{
							Inst->bEnabled = false;
							Inst->SetHaltSpawning(true);
							Inst->SetFakeBurstWhenSpawningSupressed(true);
						}
					}
				}
			}

			if (LocalNumSignificantEmitters > 0)
			{
				UE_LOG(LogParticles, Warning, TEXT("Setting PSC as not significant but it has some significant emitters. %s Template: %s"), *GetFullName(), *Template->GetFullName());
			}

			NumSignificantEmitters = LocalNumSignificantEmitters;
		}

		EParticleSystemInsignificanceReaction Reaction = Template->InsignificantReaction;
		if (Template->InsignificantReaction == EParticleSystemInsignificanceReaction::Auto)
		{
			Reaction = Template->IsLooping() ? EParticleSystemInsignificanceReaction::DisableTick : EParticleSystemInsignificanceReaction::Complete;
		}

		switch (Reaction)
		{
		case EParticleSystemInsignificanceReaction::Complete:
		{
			Complete();
		}
			break;
		case EParticleSystemInsignificanceReaction::DisableTick:
		{
			SetComponentTickEnabled(false);
		}
			break;
		case EParticleSystemInsignificanceReaction::DisableTickAndKill:
		{
			KillParticlesForced();//TODO: Make this actually free memory.
			SetComponentTickEnabled(false);
		}
			break;
		}
	}
}

bool UParticleSystemComponent::ShouldManageSignificance() const
{
	return Template ? Template->ShouldManageSignificance() : false;
}

float UParticleSystemComponent::GetApproxDistanceSquared(FVector Point) const
{
	return Bounds.ComputeSquaredDistanceFromBoxToPoint(Point);
	//TODO: Consider beam line segment?
}

bool UParticleSystemComponent::CanBeOccluded()const
{
	return Template->OcclusionBoundsMethod != EPSOBM_None && 
		(Template->FixedRelativeBoundingBox.IsValid || (Template->OcclusionBoundsMethod == EPSOBM_CustomBounds)); //We can only be occluded if we have fixed bounds or custom occlusion bounds.
}

bool UParticleSystemComponent::CanConsiderInvisible()const
{
	UWorld* World = GetWorld();
	if (World && Template)
	{
		const float MaxSecondsBeforeInactive = FMath::Max(SecondsBeforeInactive, Template->SecondsBeforeInactive);

		// Clamp MaxSecondsBeforeInactive to be at least twice the maximum smoothed frame time (45.45ms) because the rendering thread runs one 
		// frame behind the game thread and so smaller time differences cannot be reliably detected.
		const float ClampedMaxSecondsBeforeInactive = MaxSecondsBeforeInactive > 0 ? FMath::Max(MaxSecondsBeforeInactive, 0.1f) : 0.0f;
		if (ClampedMaxSecondsBeforeInactive > 0.0f && AccumTickTime > ClampedMaxSecondsBeforeInactive && World->IsGameWorld())
		{
			return World->GetTimeSeconds() > (LastRenderTime + ClampedMaxSecondsBeforeInactive);
		}
	}
	return false;
}

#if WITH_EDITOR
void UParticleSystemComponent::CheckForErrors()
{
	check(IsInGameThread());
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	for (int32 IPIndex = 0; IPIndex < InstanceParameters.Num(); IPIndex++)
	{
		FParticleSysParam& Param = InstanceParameters[IPIndex];
		if (Param.ParamType == PSPT_Actor)
		{
			if (Param.Actor == NULL)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("InstanceParamIndex"), IPIndex);
				Arguments.Add(TEXT("PathName"), FText::FromString(GetPathName()));
				FMessageLog("MapCheck").Warning()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_PSysCompErrorEmptyActorRef", "PSysComp has an empty parameter actor reference at index {InstanceParamIndex} ({PathName})" ), Arguments ) ))
					->AddToken(FMapErrorToken::Create(FMapErrors::PSysCompErrorEmptyActorRef));
			}
		}
		else
		if (Param.ParamType == PSPT_Material)
		{
			if (Param.Material == NULL)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("InstanceParamIndex"), IPIndex);
				Arguments.Add(TEXT("PathName"), FText::FromString(GetPathName()));
				FMessageLog("MapCheck").Warning()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(FText::Format( LOCTEXT( "MapCheck_Message_PSysCompErrorEmptyMaterialRef", "PSysComp has an empty parameter material reference at index {InstanceParamIndex} ({PathName})" ), Arguments ) ))
					->AddToken(FMapErrorToken::Create(FMapErrors::PSysCompErrorEmptyMaterialRef));
			}
		}
	}
}
#endif

void UParticleSystemComponent::PostLoad()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	Super::PostLoad();

	if (Template)
	{
		Template->ConditionalPostLoad();
	}
	bIsViewRelevanceDirty = true;
}

void UParticleSystemComponent::Serialize( FArchive& Ar )
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	Super::Serialize( Ar );
	
	// Take instance particle count/ size into account.
	for (int32 InstanceIndex = 0; InstanceIndex < EmitterInstances.Num(); InstanceIndex++)
	{
		FParticleEmitterInstance* EmitterInstance = EmitterInstances[InstanceIndex];
		if( EmitterInstance != NULL )
		{
			int32 Num, Max;
			EmitterInstance->GetAllocatedSize(Num, Max);
			Ar.CountBytes(Num, Max);
		}
	}

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
#if WITH_EDITORONLY_DATA

	if (Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::ExplicitAttachmentRules)
	{
		USceneComponent::ConvertAttachLocation(AutoAttachLocationType_DEPRECATED, AutoAttachLocationRule, AutoAttachRotationRule, AutoAttachScaleRule);
	}
#endif
}

void UParticleSystemComponent::BeginDestroy()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	Super::BeginDestroy();
	ResetParticles(true);
}

void UParticleSystemComponent::FinishDestroy()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
	{
		FParticleEmitterInstance* EmitInst = EmitterInstances[EmitterIndex];
		if (EmitInst)
		{
#if STATS
			EmitInst->PreDestructorCall();
#endif
			delete EmitInst;
			EmitterInstances[EmitterIndex] = NULL;
		}
	}
	Super::FinishDestroy();
}


void UParticleSystemComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);

	Super::GetResourceSizeEx(CumulativeResourceSize);
	for (int32 EmitterIdx = 0; EmitterIdx < EmitterInstances.Num(); EmitterIdx++)
	{
		FParticleEmitterInstance* EmitterInstance = EmitterInstances[EmitterIdx];
		if (EmitterInstance != NULL)
		{
			// If the data manager has the PSys, force it to report, regardless of a PSysComp scene info being present...
			EmitterInstance->GetResourceSizeEx(CumulativeResourceSize);
		}
	}
}


bool UParticleSystemComponent::ParticleLineCheck(FHitResult& Hit, AActor* SourceActor, const FVector& End, const FVector& Start, const FVector& HalfExtent, const FCollisionObjectQueryParams& ObjectParams)
{
	check(GetWorld());
	if ( HalfExtent.IsZero() )
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ParticleCollision), true, SourceActor);
		QueryParams.bReturnPhysicalMaterial = true;
		return GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, QueryParams);
	}
	else
	{
		FCollisionQueryParams BoxParams(SCENE_QUERY_STAT(ParticleCollision));
		BoxParams.AddIgnoredActor(SourceActor);
		BoxParams.bReturnPhysicalMaterial = true;
		return GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeBox(HalfExtent), BoxParams);
	}
}

void UParticleSystemComponent::OnRegister()
{
	ForceAsyncWorkCompletion(STALL);
	check(FXSystem == nullptr);

	UWorld* World = GetWorld();
	check(World != nullptr);

	if (World->Scene)
	{
		FXSystem = World->Scene->GetFXSystem();
	}

	if (bAutoManageAttachment && !IsActive())
	{
		// Detach from current parent, we are supposed to wait for activation.
		if (GetAttachParent())
		{
			// If no auto attach parent override, use the current parent when we activate
			if (!AutoAttachParent.IsValid())
			{
				AutoAttachParent = GetAttachParent();
			}
			// If no auto attach socket override, use current socket when we activate
			if (AutoAttachSocketName == NAME_None)
			{
				AutoAttachSocketName = GetAttachSocketName();
			}

			// Prevent attachment before Super::OnRegister() tries to attach us, since we only attach when activated.
			if (GetAttachParent()->GetAttachChildren().Contains(this))
			{
				// Only detach if we are not about to auto attach to the same target, that would be wasteful.
				if (!bAutoActivate || (AutoAttachLocationRule != EAttachmentRule::KeepRelative && AutoAttachRotationRule != EAttachmentRule::KeepRelative && AutoAttachScaleRule != EAttachmentRule::KeepRelative) || (AutoAttachSocketName != GetAttachSocketName()) || (AutoAttachParent != GetAttachParent()))
				{
					DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative, /*bCallModify=*/ false));
				}
			}
			else
			{
				SetupAttachment(nullptr, NAME_None);
			}
		}

		SavedAutoAttachRelativeLocation = RelativeLocation;
		SavedAutoAttachRelativeRotation = RelativeRotation;
		SavedAutoAttachRelativeScale3D = RelativeScale3D;
	}

	Super::OnRegister();

	// If we were active before but are not now, activate us
	if (bWasActive && !bIsActive)
	{
		Activate(true);
	}

	UE_LOG(LogParticles,Verbose,
		TEXT("OnRegister %s Component=0x%p Scene=0x%p FXSystem=0x%p"),
		Template != NULL ? *Template->GetName() : TEXT("NULL"), this, World->Scene, FXSystem);

	if (LODLevel == -1)
	{
		// Force it to LODLevel 0
		LODLevel = 0;
	}
}

void UParticleSystemComponent::OnUnregister()
{
	ForceAsyncWorkCompletion(STALL);
	UE_LOG(LogParticles,Verbose,
		TEXT("OnUnregister %s Component=0x%p Scene=0x%p FXSystem=0x%p"),
		Template != NULL ? *Template->GetName() : TEXT("NULL"), this, GetWorld()->Scene, FXSystem);

	bWasActive = bIsActive;

	ResetParticles(true);
	FXSystem = NULL;
	Super::OnUnregister();

	// sanity check
	check(FXSystem == NULL);
}

void UParticleSystemComponent::CreateRenderState_Concurrent()
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_CreateRenderState_Concurrent);

	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	check( GetWorld() );
	UE_LOG(LogParticles,Verbose,
		TEXT("CreateRenderState_Concurrent @ %fs %s"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"));

	// NULL out template if we're not allowing particles. This is not done in the Editor to avoid clobbering content via PIE.
	if( !GIsAllowingParticles && !GIsEditor )
	{
		Template = NULL;
	}

	if (Template && Template->bHasPhysics)
	{
		PrimaryComponentTick.TickGroup = TG_PrePhysics;
			
		AEmitter* EmitterOwner = Cast<AEmitter>(GetOwner());
		if (EmitterOwner)
		{
			EmitterOwner->PrimaryActorTick.TickGroup = TG_PrePhysics;
		}
	}

	Super::CreateRenderState_Concurrent();

	bJustRegistered = true;
}



void UParticleSystemComponent::SendRenderTransform_Concurrent()
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_SendRenderTransform_Concurrent);

	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (bIsActive)
	{
		if (bSkipUpdateDynamicDataDuringTick == false)
		{
			Super::SendRenderTransform_Concurrent();
			return;
		}
	}
	// skip the Primitive component update to avoid updating the render thread
	UActorComponent::SendRenderTransform_Concurrent();
}

void UParticleSystemComponent::SendRenderDynamicData_Concurrent()
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_SendRenderDynamicData_Concurrent);

	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	Super::SendRenderDynamicData_Concurrent();

	check(!bAsyncDataCopyIsValid);
	check(!bParallelRenderThreadUpdate);
	bParallelRenderThreadUpdate = true;

#if WITH_FLEX
	ClearFlexSurfaceDynamicData();
#endif

	FParticleSystemSceneProxy* PSysSceneProxy = (FParticleSystemSceneProxy*)SceneProxy;
	if (PSysSceneProxy != NULL)
	{
		// check to see if this PSC is active.  When you attach a PSC it gets added to the DataManager
		// even if it might be bIsActive = false  (e.g. attach and later in the frame activate it)
		// or also for PSCs that are attached to a SkelComp which is being attached and reattached but the PSC itself is not active!
		if (bIsActive)
		{
			UpdateDynamicData();
		}
		else
		{
			// so if we just were deactivated we want to update the renderer with NULL so the renderer will clear out the data there and not have outdated info which may/will cause a crash
			if (bWasDeactivated || bWasCompleted)
			{
				PSysSceneProxy->UpdateData(NULL);
			}
		}
	}
	bParallelRenderThreadUpdate = false;
}

void UParticleSystemComponent::DestroyRenderState_Concurrent()
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_DestroyRenderState_Concurrent);

	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	check( GetWorld() );
	UE_LOG(LogParticles,Verbose,
		TEXT("DestroyRenderState_Concurrent @ %fs %s"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"));

	if (bResetOnDetach)
	{
		// Empty the EmitterInstance array.
		ResetParticles();
	}

	if (bRenderStateCreated)
	{
		Super::DestroyRenderState_Concurrent();
	}
}


FDynamicEmitterDataBase* UParticleSystemComponent::CreateDynamicDataFromReplay( FParticleEmitterInstance* EmitterInstance, 
	const FDynamicEmitterReplayDataBase* EmitterReplayData, bool bSelected, ERHIFeatureLevel::Type InFeatureLevel )
{
	checkSlow(EmitterInstance && EmitterInstance->CurrentLODLevel);
	check( EmitterReplayData != NULL );

	FScopeCycleCounterEmitter AdditionalScope(EmitterInstance);
#if WITH_EDITOR
	uint32 StartTime = FPlatformTime::Cycles();
#endif

	// Allocate the appropriate type of emitter data
	FDynamicEmitterDataBase* EmitterData = NULL;

	switch( EmitterReplayData->eEmitterType )
	{
		case DET_Sprite:
			{
				// Allocate the dynamic data
				FDynamicSpriteEmitterData* NewEmitterData = new FDynamicSpriteEmitterData(EmitterInstance->CurrentLODLevel->RequiredModule);

				// Fill in the source data
				const FDynamicSpriteEmitterReplayData* SpriteEmitterReplayData =
					static_cast< const FDynamicSpriteEmitterReplayData* >( EmitterReplayData );
				NewEmitterData->Source = *SpriteEmitterReplayData;

				// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
				NewEmitterData->Init( bSelected );

				EmitterData = NewEmitterData;
			}
			break;

		case DET_Mesh:
			{
				// Allocate the dynamic data
				// PVS-Studio does not understand the checkSlow above, so it is warning us that EmitterInstance->CurrentLODLevel may be null.
				FDynamicMeshEmitterData* NewEmitterData = ::new FDynamicMeshEmitterData(EmitterInstance->CurrentLODLevel->RequiredModule); //-V595

				// Fill in the source data
				const FDynamicMeshEmitterReplayData* MeshEmitterReplayData =
					static_cast< const FDynamicMeshEmitterReplayData* >( EmitterReplayData );
				NewEmitterData->Source = *MeshEmitterReplayData;

				// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.

				// @todo: Currently we're assuming the original emitter instance is bound to the same mesh as
				//        when the replay was generated (safe), and various mesh/material indices are intact.  If
				//        we ever support swapping meshes/material on the fly, we'll need cache the mesh
				//        reference and mesh component/material indices in the actual replay data.

				if( EmitterInstance != NULL )
				{
					FParticleMeshEmitterInstance* MeshEmitterInstance =
						static_cast< FParticleMeshEmitterInstance* >( EmitterInstance );
					NewEmitterData->Init(
						bSelected,
						MeshEmitterInstance,
						MeshEmitterInstance->MeshTypeData->Mesh,
						InFeatureLevel);
					EmitterData = NewEmitterData;
				}
			}
			break;

		case DET_Beam2:
			{
				// Allocate the dynamic data
				FDynamicBeam2EmitterData* NewEmitterData = new FDynamicBeam2EmitterData(EmitterInstance->CurrentLODLevel->RequiredModule);

				// Fill in the source data
				const FDynamicBeam2EmitterReplayData* Beam2EmitterReplayData =
					static_cast< const FDynamicBeam2EmitterReplayData* >( EmitterReplayData );
				NewEmitterData->Source = *Beam2EmitterReplayData;

				// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
				NewEmitterData->Init( bSelected );

				EmitterData = NewEmitterData;
			}
			break;

		case DET_Ribbon:
			{
				// Allocate the dynamic data
				FDynamicRibbonEmitterData* NewEmitterData = new FDynamicRibbonEmitterData(EmitterInstance->CurrentLODLevel->RequiredModule);

				// Fill in the source data
				const FDynamicRibbonEmitterReplayData* Trail2EmitterReplayData = static_cast<const FDynamicRibbonEmitterReplayData*>(EmitterReplayData);
				NewEmitterData->Source = *Trail2EmitterReplayData;
				// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
				NewEmitterData->Init(bSelected);
				EmitterData = NewEmitterData;
			}
			break;

		case DET_AnimTrail:
			{
				// Allocate the dynamic data
				FDynamicAnimTrailEmitterData* NewEmitterData = new FDynamicAnimTrailEmitterData(EmitterInstance->CurrentLODLevel->RequiredModule);
				// Fill in the source data
				const FDynamicTrailsEmitterReplayData* AnimTrailEmitterReplayData = static_cast<const FDynamicTrailsEmitterReplayData*>(EmitterReplayData);
				NewEmitterData->Source = *AnimTrailEmitterReplayData;
				// Setup dynamic render data.  Only call this AFTER filling in source data for the emitter.
				NewEmitterData->Init(bSelected);
				EmitterData = NewEmitterData;
			}
			break;

		default:
			{
				// @todo: Support capture of other particle system types
			}
			break;
	}
#if STATS
	if (EmitterData)
	{
		EmitterData->StatID = EmitterInstance->SpriteTemplate->GetStatID();
	}
#endif

#if WITH_EDITOR
	uint32 EndTime = FPlatformTime::Cycles();
	EmitterInstance->LastTickDurationMs += FPlatformTime::ToMilliseconds(EndTime - StartTime);
#endif

	return EmitterData;
}




FParticleDynamicData* UParticleSystemComponent::CreateDynamicData(ERHIFeatureLevel::Type InFeatureLevel)
{
	//SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_CreateDynamicData);

	FInGameScopedCycleCounter InGameCycleCounter(GetWorld(), EInGamePerfTrackers::VFXSignificance, EInGamePerfTrackerThreads::GameThread, bIsManagingSignificance);

	// Only proceed if we have any live particles or if we're actively replaying/capturing
	if (EmitterInstances.Num() > 0)
	{
		int32 LiveCount = 0;
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
		{
			FParticleEmitterInstance* EmitInst = EmitterInstances[EmitterIndex];
			if (EmitInst)
			{
				if (EmitInst->ActiveParticles > 0)
				{
					LiveCount++;
				}
			}
		}

		if (!bForceLODUpdateFromRenderer
			&& LiveCount == 0
			&& ReplayState == PRS_Disabled)
		{
			return NULL;
		}
	}


	FParticleDynamicData* ParticleDynamicData = new FParticleDynamicData();
	INC_DWORD_STAT(STAT_DynamicPSysCompCount);
	INC_DWORD_STAT_BY(STAT_DynamicPSysCompMem, sizeof(FParticleDynamicData));

	if (Template)
	{
		ParticleDynamicData->SystemPositionForMacroUVs = GetComponentTransform().TransformPosition(Template->MacroUVPosition);
		ParticleDynamicData->SystemRadiusForMacroUVs = Template->MacroUVRadius;
	}

	if( ReplayState == PRS_Replaying )
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_CreateDynamicData_Replay);
		// Do we have any replay data to play back?
		UParticleSystemReplay* ReplayData = FindReplayClipForIDNumber( ReplayClipIDNumber );
		if( ReplayData != NULL )
		{
			// Make sure the current frame index is in a valid range
			if( ReplayData->Frames.IsValidIndex( ReplayFrameIndex ) )
			{
				// Grab the current particle system replay frame
				const FParticleSystemReplayFrame& CurReplayFrame = ReplayData->Frames[ ReplayFrameIndex ];


				// Fill the emitter dynamic buffers with data from our replay
				ParticleDynamicData->DynamicEmitterDataArray.Reset();
				ParticleDynamicData->DynamicEmitterDataArray.Reserve(CurReplayFrame.Emitters.Num());

				for (int32 CurEmitterIndex = 0; CurEmitterIndex < CurReplayFrame.Emitters.Num(); ++CurEmitterIndex)
				{
					const FParticleEmitterReplayFrame& CurEmitter = CurReplayFrame.Emitters[ CurEmitterIndex ];

					const FDynamicEmitterReplayDataBase* CurEmitterReplay = CurEmitter.FrameState;
					check( CurEmitterReplay != NULL );

					FParticleEmitterInstance* EmitterInst = NULL;
					if( EmitterInstances.IsValidIndex( CurEmitter.OriginalEmitterIndex ) )
					{
						// Fill dynamic data from the replay frame data for this emitter so we can render it
						// Grab the original emitter instance for that this replay was generated from
						FDynamicEmitterDataBase* NewDynamicEmitterData =
							CreateDynamicDataFromReplay( EmitterInstances[ CurEmitter.OriginalEmitterIndex ], CurEmitterReplay, IsOwnerSelected(), InFeatureLevel );


						if( NewDynamicEmitterData != NULL )
						{
							ParticleDynamicData->DynamicEmitterDataArray.Add(NewDynamicEmitterData);
							NewDynamicEmitterData->EmitterIndex = CurEmitterIndex;

#if WITH_FLEX
							UpdateFlexSurfaceDynamicData(EmitterInstances[CurEmitter.OriginalEmitterIndex], NewDynamicEmitterData);
#endif
						}
					}
				}
			}
		}
	}
	else
	{
		FParticleSystemReplayFrame* NewReplayFrame = NULL;
		if( ReplayState == PRS_Capturing )
		{
			SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_CreateDynamicData_Capture);
			ForceAsyncWorkCompletion(ENSURE_AND_STALL);
			check(IsInGameThread());
			// If we don't have any replay data for this component yet, create some now
			UParticleSystemReplay* ReplayData = FindReplayClipForIDNumber( ReplayClipIDNumber );
			if( ReplayData == NULL )
			{
				// Create a new replay clip!
				ReplayData = NewObject<UParticleSystemReplay>(this);

				// Set the clip ID number
				ReplayData->ClipIDNumber = ReplayClipIDNumber;

				// Add this to the component's list of clips
				ReplayClips.Add( ReplayData );

				// We're modifying the component by adding a new replay clip
				MarkPackageDirty();
			}


			// Add a new frame!
			{
				const int32 NewFrameIndex = ReplayData->Frames.Num();
				new( ReplayData->Frames ) FParticleSystemReplayFrame;
				NewReplayFrame = &ReplayData->Frames[ NewFrameIndex ];

				// We're modifying the component by adding a new frame
				MarkPackageDirty();
			}
		}

		// Is the particle system allowed to run?
		if( bForcedInActive == false )
		{
			//SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_CreateDynamicData_Gather);
			ParticleDynamicData->DynamicEmitterDataArray.Reset();
			ParticleDynamicData->DynamicEmitterDataArray.Reserve(EmitterInstances.Num());

			//QUICK_SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_GetDynamicData);
			for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
			{
				FDynamicEmitterDataBase* NewDynamicEmitterData = NULL;
				FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
				if (EmitterInst)
				{
					FScopeCycleCounterEmitter AdditionalScope(EmitterInst);
#if WITH_EDITOR
					uint32 StartTime = FPlatformTime::Cycles();
#endif

					// Generate the dynamic data for this emitter
					{
						//SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_GetDynamicData);
						bool bIsOwnerSeleted = false;
#if WITH_EDITOR
						{
							SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_GetDynamicData_Selected);
							bIsOwnerSeleted = IsOwnerSelected();
						}
#endif
						NewDynamicEmitterData = EmitterInst->GetDynamicData(bIsOwnerSeleted, InFeatureLevel);
					}
					if( NewDynamicEmitterData != NULL )
					{
#if STATS
						NewDynamicEmitterData->StatID = EmitterInst->SpriteTemplate->GetStatID();
#endif
						NewDynamicEmitterData->bValid = true;
						ParticleDynamicData->DynamicEmitterDataArray.Add( NewDynamicEmitterData );
						NewDynamicEmitterData->EmitterIndex = EmitterIndex;

#if WITH_FLEX
						UpdateFlexSurfaceDynamicData(EmitterInst, NewDynamicEmitterData);
#endif

						// Are we current capturing particle state?
						if( ReplayState == PRS_Capturing )
						{
							SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_CreateDynamicData_GatherCapture);
							// Capture replay data for this particle system
							// NOTE: This call should always succeed if GetDynamicData succeeded earlier
							FDynamicEmitterReplayDataBase* NewEmitterReplayData = EmitterInst->GetReplayData();
							check( NewEmitterReplayData != NULL );


							// @todo: We could drastically reduce the size of replays in memory and
							//		on disk by implementing delta compression here.

							// Allocate a new emitter frame
							check(NewReplayFrame != NULL);
							const int32 NewFrameEmitterIndex = NewReplayFrame->Emitters.Num();
							new( NewReplayFrame->Emitters ) FParticleEmitterReplayFrame;
							FParticleEmitterReplayFrame* NewEmitterReplayFrame = &NewReplayFrame->Emitters[ NewFrameEmitterIndex ];

							// Store the replay state for this emitter frame.  Note that this will be
							// deleted when the parent object is garbage collected.
							NewEmitterReplayFrame->EmitterType = NewEmitterReplayData->eEmitterType;
							NewEmitterReplayFrame->OriginalEmitterIndex = EmitterIndex;
							NewEmitterReplayFrame->FrameState = NewEmitterReplayData;
						}
					}
#if WITH_EDITOR
					uint32 EndTime = FPlatformTime::Cycles();
					EmitterInst->LastTickDurationMs += FPlatformTime::ToMilliseconds(EndTime - StartTime);
#endif
				}
			}
		}
	}

	return ParticleDynamicData;
}

int32 UParticleSystemComponent::GetNumMaterials() const
{
	if (Template)
	{
		return Template->Emitters.Num();
	}
	return 0;
}

UMaterialInterface* UParticleSystemComponent::GetMaterial(int32 ElementIndex) const
{
	if (EmitterMaterials.IsValidIndex(ElementIndex) && EmitterMaterials[ElementIndex] != NULL)
	{
		return EmitterMaterials[ElementIndex];
	}
	if (Template && Template->Emitters.IsValidIndex(ElementIndex))
	{
		UParticleEmitter* Emitter = Template->Emitters[ElementIndex];
		if (Emitter && Emitter->LODLevels.Num() > 0)
		{
			UParticleLODLevel* EmitterLODLevel = Emitter->LODLevels[0];
			if (EmitterLODLevel && EmitterLODLevel->RequiredModule)
			{
				return EmitterLODLevel->RequiredModule->Material;
			}
		}
	}
	return NULL;
}

void UParticleSystemComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
{
	ForceAsyncWorkCompletion(STALL);
	if (Template && Template->Emitters.IsValidIndex(ElementIndex))
	{
		if (!EmitterMaterials.IsValidIndex(ElementIndex))
		{
			EmitterMaterials.AddZeroed(ElementIndex + 1 - EmitterMaterials.Num());
		}
		EmitterMaterials[ElementIndex] = Material;
		bIsViewRelevanceDirty = true;

		for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); ++EmitterIndex)
		{
			if (FParticleEmitterInstance* Inst = EmitterInstances[EmitterIndex])
			{
				if (!Inst->Tick_MaterialOverrides())
				{
					if (EmitterMaterials.IsValidIndex(EmitterIndex))
					{
						if (EmitterMaterials[EmitterIndex])
						{
							Inst->CurrentMaterial = EmitterMaterials[EmitterIndex];
						}
					}
				}
			}
		}
		MarkRenderDynamicDataDirty();
	}
}

void UParticleSystemComponent::ClearDynamicData()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (SceneProxy)
	{
		FParticleSystemSceneProxy* ParticleSceneProxy = (FParticleSystemSceneProxy*)SceneProxy;
		ParticleSceneProxy->UpdateData(NULL);
	}
}

void UParticleSystemComponent::UpdateDynamicData()
{
	//SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_UpdateDynamicData);

	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (SceneProxy)
	{
		// Create the dynamic data for rendering this particle system
		FParticleDynamicData* ParticleDynamicData = CreateDynamicData(SceneProxy->GetScene().GetFeatureLevel());

		FParticleSystemSceneProxy* Proxy = (FParticleSystemSceneProxy*)SceneProxy;
		// Render the particles
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		//@todo.SAS. Remove thisline  - it is used for debugging purposes...
		Proxy->SetLastDynamicData(Proxy->GetDynamicData());
		//@todo.SAS. END
		Proxy->SetVisualizeLODIndex(GetCurrentLODIndex());
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		Proxy->UpdateData( ParticleDynamicData );
	}
}

#if WITH_FLEX
void UParticleSystemComponent::UpdateFlexSurfaceDynamicData(FParticleEmitterInstance* EmitterInstance, FDynamicEmitterDataBase* EmitterDynamicData)
{
	check(EmitterInstance);
	check(EmitterDynamicData);

	if (SceneProxy)
	{
		UFlexFluidSurface* FlexFluidSurface = FlexFluidSurfaceOverride ? FlexFluidSurfaceOverride : EmitterInstance->SpriteTemplate->FlexFluidSurfaceTemplate;
		if (FlexFluidSurface)
		{
			UFlexFluidSurfaceComponent* SurfaceComponent = GetWorld()->GetFlexFluidSurface(FlexFluidSurface);
			check(SurfaceComponent);
			SurfaceComponent->SendRenderEmitterDynamicData_Concurrent(
				(FParticleSystemSceneProxy*)SceneProxy,
				EmitterDynamicData);
		}
	}
}

void UParticleSystemComponent::ClearFlexSurfaceDynamicData()
{
	if (SceneProxy)
	{
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
		{
			FParticleEmitterInstance* EmitterInstance = EmitterInstances[EmitterIndex];
			if (EmitterInstance && EmitterInstance->SpriteTemplate->FlexFluidSurfaceTemplate)
			{
				UFlexFluidSurface* FlexFluidSurface = FlexFluidSurfaceOverride ? FlexFluidSurfaceOverride : EmitterInstance->SpriteTemplate->FlexFluidSurfaceTemplate;
				UFlexFluidSurfaceComponent* SurfaceComponent = GetWorld()->GetFlexFluidSurface(FlexFluidSurface);
				if (SurfaceComponent)
				{
					SurfaceComponent->SendRenderEmitterDynamicData_Concurrent(
						(FParticleSystemSceneProxy*)SceneProxy,
						nullptr);
				}
			}
		}
	}
}

void UParticleSystemComponent::AttachFlexToComponent(USceneComponent* Component, float Radius)
{
	// Forward to all Flex emitters
	// TODO: check for actual overlaps first
	for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
	{
		FParticleEmitterInstance* EmitterInstance = EmitterInstances[EmitterIndex];
		if (EmitterInstance && 
			EmitterInstance->SpriteTemplate && 
			EmitterInstance->SpriteTemplate->FlexContainerTemplate &&
			EmitterInstance->FlexEmitterInstance)
		{
			EmitterInstance->AttachFlexToComponent(Component, Radius);
		}
	}

}

#endif

UFlexContainer* UParticleSystemComponent::GetFirstFlexContainerTemplate()
{
#if WITH_FLEX
	for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
	{
		FParticleEmitterInstance* EmitterInstance = EmitterInstances[EmitterIndex];
		if (EmitterInstance && EmitterInstance->SpriteTemplate && EmitterInstance->SpriteTemplate->FlexContainerTemplate)
		{
			FPhysScene* Scene = EmitterInstance->Component->GetWorld()->GetPhysicsScene();
			FFlexContainerInstance* ContainerInstance = Scene->GetFlexContainer(EmitterInstance->SpriteTemplate->FlexContainerTemplate);
			return ContainerInstance ? ContainerInstance->Template : nullptr;
		}
	}
#endif
	return nullptr;
}

void UParticleSystemComponent::UpdateLODInformation()
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (GetWorld()->IsGameWorld() || (GIsEditor && GEngine->bEnableEditorPSysRealtimeLOD))
	{
		if (SceneProxy)
		{
			if (EmitterInstances.Num() > 0)
			{
				uint8 CheckLODMethod = PARTICLESYSTEMLODMETHOD_DirectSet;
				if (bOverrideLODMethod)
				{
					CheckLODMethod = LODMethod;
				}
				else
				{
					if (Template)
					{
						CheckLODMethod = Template->LODMethod;
					}
				}

				if (CheckLODMethod == PARTICLESYSTEMLODMETHOD_Automatic)
				{
					FParticleSystemSceneProxy* ParticleSceneProxy = (FParticleSystemSceneProxy*)SceneProxy;
					float PendingDistance = ParticleSceneProxy->GetPendingLODDistance();
					if (PendingDistance > 0.0f)
					{
						int32 LODIndex = 0;
						for (int32 LODDistIndex = 1; LODDistIndex < Template->LODDistances.Num(); LODDistIndex++)
						{
							if (Template->LODDistances[LODDistIndex] > ParticleSceneProxy->GetPendingLODDistance())
							{
								break;
							}
							LODIndex = LODDistIndex;
						}

						if (LODIndex != LODLevel)
						{
							SetLODLevel(LODIndex);
						}
					}
				}
			}
		}
	}
	else
	{
#if WITH_EDITORONLY_DATA
		if (LODLevel != EditorLODLevel)
		{
			SetLODLevel(EditorLODLevel);
		}
#endif // WITH_EDITORONLY_DATA
	}
}

void UParticleSystemComponent::OrientZAxisTowardCamera()
{
	SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_OrientZAxisTowardCamera);
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);

	//@TODO: CAMERA: How does this work for stereo and/or split-screen?
	APlayerController* PlayerController = nullptr;
	if (GetWorld() && GetWorld()->GetGameInstance())
	{
		PlayerController = GetWorld()->GetGameInstance()->GetFirstLocalPlayerController();
	}

	// Orient the Z axis toward the camera
	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		// Direction of the camera
		FVector DirToCamera = PlayerController->PlayerCameraManager->GetCameraLocation() - GetComponentLocation();
		DirToCamera.Normalize();

		// Convert the camera direction to local space
		DirToCamera = GetComponentTransform().InverseTransformVectorNoScale(DirToCamera);
		
		// Local Z axis
		const FVector LocalZAxis = FVector(0,0,1);

		// Find angle between z-axis and the camera direction
		const FQuat PointTo = FQuat::FindBetweenNormals(LocalZAxis, DirToCamera);
		
		// Adjust our rotation
		const FRotator AdjustmentAngle(PointTo);
		RelativeRotation += AdjustmentAngle;

		// Mark the component transform as dirty if the rotation has changed.
		bIsTransformDirty |= !AdjustmentAngle.IsZero();
	}
}

#if WITH_EDITOR
void UParticleSystemComponent::PreEditChange(UProperty* PropertyThatWillChange)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	bool bShouldResetParticles = true;

	if (PropertyThatWillChange)
	{
		const FName PropertyName = PropertyThatWillChange->GetFName();

		// Don't reset particles for properties that won't affect the particles
		if (PropertyName == TEXT("bCastVolumetricTranslucentShadow")
			|| PropertyName == TEXT("bCastDynamicShadow")
			|| PropertyName == TEXT("bAffectDynamicIndirectLighting")
			|| PropertyName == TEXT("CastShadow"))
		{
			bShouldResetParticles = false;
		}
	}

	if (bShouldResetParticles)
	{
		ResetParticles();
	}
	
	Super::PreEditChange(PropertyThatWillChange);
}

void UParticleSystemComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	check(IsInGameThread());
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (PropertyChangedEvent.PropertyChain.Num() > 0)
	{
		UProperty* MemberProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
		if ( MemberProperty != NULL )
		{
			FName PropertyName = PropertyChangedEvent.Property->GetFName();
			if ((PropertyName == TEXT("Color")) ||
				(PropertyName == TEXT("R")) ||
				(PropertyName == TEXT("G")) ||
				(PropertyName == TEXT("B")))
			{
				//@todo. once the property code can give the correct index, only update
				// the entry that was actually changed!
				// This function does not return an index into the array at the moment...
				// int32 InstParamIdx = PropertyChangedEvent.GetArrayIndex(TEXT("InstanceParameters"));
				for (int32 InstIdx = 0; InstIdx < InstanceParameters.Num(); InstIdx++)
				{
					FParticleSysParam& PSysParam = InstanceParameters[InstIdx];
					if ((PSysParam.ParamType == PSPT_Vector) || (PSysParam.ParamType == PSPT_VectorRand))
					{
						PSysParam.Vector.X = PSysParam.Color.R / 255.0f;
						PSysParam.Vector.Y = PSysParam.Color.G / 255.0f;
						PSysParam.Vector.Z = PSysParam.Color.B / 255.0f;
					}
				}
			}
		}
	}

	bIsViewRelevanceDirty = true;
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

FBoxSphereBounds UParticleSystemComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox;
	BoundingBox.Init();

	if (FXConsoleVariables::bAllowCulling == false)
	{
		BoundingBox.Min = FVector(-HALF_WORLD_MAX);
		BoundingBox.Max = FVector(+HALF_WORLD_MAX);
	}
	else if(Template && Template->bUseFixedRelativeBoundingBox)
	{
		// Use hardcoded relative bounding box from template.
		BoundingBox	= Template->FixedRelativeBoundingBox.TransformBy(LocalToWorld);
	}
	else
	{
		for (int32 i=0; i<EmitterInstances.Num(); i++)
		{
			FParticleEmitterInstance* EmitterInstance = EmitterInstances[i];
			if( EmitterInstance && EmitterInstance->HasActiveParticles() )
			{
				BoundingBox += EmitterInstance->GetBoundingBox();
			}
		}

		// Expand the actual bounding-box slightly so it will be valid longer in the case of expanding particle systems.
		const FVector ExpandAmount = BoundingBox.GetExtent() * 0.1f;
		BoundingBox = FBox(BoundingBox.Min - ExpandAmount, BoundingBox.Max + ExpandAmount);
	}

	return FBoxSphereBounds(BoundingBox);
}

class FParticleFinalizeTask
{
	UParticleSystemComponent* Target;
public:
	FParticleFinalizeTask(UParticleSystemComponent* InTarget)
		: Target(InTarget)
	{

	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParticleFinalizeTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::GameThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Target->FinalizeTickComponent();
	}
};

FAutoConsoleTaskPriority CPrio_ParticleAsyncTask(
	TEXT("TaskGraph.TaskPriorities.ParticleAsyncTask"),
	TEXT("Task and thread priority for FParticleAsyncTask."),
	ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);


class FParticleAsyncTask
{
	UParticleSystemComponent* Target;
public:
	FParticleAsyncTask(UParticleSystemComponent* InTarget)
		: Target(InTarget)
	{

	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParticleAsyncTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_ParticleAsyncTask.Get();
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Target->ComputeTickComponent_Concurrent();
#if !WITH_EDITOR  // otherwise this is queued by the calling code because we need to be able to block and wait on it
		{
			SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_QueueFinalize);
			FGraphEventRef Finalize = TGraphTask<FParticleFinalizeTask>::CreateTask(nullptr, CurrentThread).ConstructAndDispatchWhenReady(Target);
			MyCompletionGraphEvent->DontCompleteUntil(Finalize);
		}
#endif
	}
};

TAutoConsoleVariable<int32> CVarFXEarlySchedule(TEXT("FX.EarlyScheduleAsync"), 0, TEXT("If 1, particle system components that can run async will be scheduled earlier in the frame"));

DECLARE_CYCLE_STAT(TEXT("PSys Comp Marshall Time"),STAT_UParticleSystemComponent_Marshall,STATGROUP_Particles);

bool UParticleSystemComponent::IsReadyForOwnerToAutoDestroy() const
{
	return (!bIsActive && bWasCompleted);
}

void UParticleSystemComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	LLM_SCOPE(ELLMTag::Particles);

	FInGameScopedCycleCounter InGameCycleCounter(GetWorld(), EInGamePerfTrackers::VFXSignificance, EInGamePerfTrackerThreads::GameThread, bIsManagingSignificance);

	if (Template == nullptr || Template->Emitters.Num() == 0)
	{
		return;
	}
	// control tick rate
	// don't tick if enough time hasn't passed
	if (TimeSinceLastTick + static_cast<uint32>(DeltaTime*1000.0f) < Template->MinTimeBetweenTicks)
	{
		TimeSinceLastTick += static_cast<uint32>(DeltaTime*1000.0f);
		return;
	}
	// if enough time has passed, and some of it in previous frames, need to take that into account for DeltaTime
	DeltaTime += TimeSinceLastTick / 1000.0f;
	TimeSinceLastTick = 0;


	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	SCOPE_CYCLE_COUNTER(STAT_PSysCompTickTime);

	if (bWasManagingSignificance != bIsManagingSignificance)
	{	
		bWasManagingSignificance = bIsManagingSignificance;
		MarkRenderStateDirty();
	}

	bool bDisallowAsync = false;

	// Bail out if inactive and not AutoActivate
	if ((bIsActive == false) && (bAutoActivate == false))
	{
		// Disable our tick here, will be enabled when activating
		SetComponentTickEnabled(false);
		return;
	}
	DeltaTimeTick = DeltaTime;

	// Bail out if we are running on a dedicated server and we don't want to update on those
	if ((bUpdateOnDedicatedServer == false) && (IsNetMode(NM_DedicatedServer)))
	{
		if (bAutoDestroy)
		{
			// We need to destroy the component if the user is expecting us to do it automatically otherwise this component will live forever because HasCompleted() will never get checked
			DestroyComponent();
		}
		else
		{
			SetComponentTickEnabled(false);
		}
		return;
	}

	UWorld* World = GetWorld();
	check(World);

	bool bRequiresReset = bResetTriggered;
	bResetTriggered = false;

	// System settings may have been lowered. Support late deactivation.
	int32 DetailModeCVar = GetCurrentDetailMode();
	const bool bDetailModeAllowsRendering	= DetailMode <= DetailModeCVar;
	if ( bDetailModeAllowsRendering == false )
	{
		if ( bIsActive )
		{
			DeactivateSystem();
			Super::MarkRenderDynamicDataDirty();
		}
		return;
	} 
	// See if DetailMode has changed since the last time we checked
	else if (bWarmingUp == false && LastCheckedDetailMode != DetailModeCVar)
	{
		// Save the detail mode we've checked
		LastCheckedDetailMode = DetailModeCVar;

		if (!bRequiresReset)
		{
			SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_CheckForReset);
			for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); ++EmitterIndex)
			{
				FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
				if (Instance && Instance->SpriteTemplate)
				{
					if (Instance->SpriteTemplate->DetailMode > DetailModeCVar)
					{
						bRequiresReset = true;
						break;
					}
				}
			}
		}
	}
	
	bool bHasFlexEmitter = false;

#if WITH_FLEX

	for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); ++EmitterIndex)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
		if (Instance && Instance->SpriteTemplate)
		{
			if (Instance->SpriteTemplate->FlexContainerTemplate != NULL)
			{
				bHasFlexEmitter = true;
				bDisallowAsync = true;
				break;
			}
		}
	}

#endif
	
	if (bRequiresReset)
	{
#if WITH_EDITOR
		//If we're resetting in the editor, cached emitter values may now be invalid.
		Template->UpdateAllModuleLists();
#endif

		bool bOldActive = bIsActive;
		ResetParticles(true);
		if (bOldActive)
		{
			ActivateSystem();
		}
		else
		{
			InitializeSystem();
		}
	}

	// Bail out if MaxSecondsBeforeInactive > 0 and we haven't been rendered the last MaxSecondsBeforeInactive seconds.
	if (bWarmingUp == false)
	{
		//For now, we're only allowing the SecondsBeforeInactive optimization on looping emitters as it can cause leaks with non-looping effects.
		//Longer term, there is likely a better solution.
		if (Template->IsLooping() && CanConsiderInvisible())
		{
			SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_LOD_Inactive);
			bForcedInActive = true;
			SpawnEvents.Empty();
			DeathEvents.Empty();
			CollisionEvents.Empty();
			KismetEvents.Empty();

			if (bIsManagingSignificance && Template->GetHighestSignificance() < RequiredSignificance)
			{
				//We're definitely insignificant so we can stop ticking entirely.
				OnSignificanceChanged(false, true);
			}

			return;
		}

		AccumLODDistanceCheckTime += DeltaTime;
		if (AccumLODDistanceCheckTime > Template->LODDistanceCheckTime)
		{
			SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_LOD);
			AccumLODDistanceCheckTime = 0.0f;

			if (ShouldComputeLODFromGameThread())
			{
				bool bCalculateLODLevel = 
					(bOverrideLODMethod == true) ? (LODMethod == PARTICLESYSTEMLODMETHOD_Automatic) : 
						(Template->LODMethod == PARTICLESYSTEMLODMETHOD_Automatic);
				if (bCalculateLODLevel == true)
				{
					FVector EffectPosition = GetComponentLocation();
					int32 DesiredLODLevel = DetermineLODLevelForLocation(EffectPosition);
					SetLODLevel(DesiredLODLevel);
				}
			}
			else
			{
				// Periodically force an LOD update from the renderer if we are
				// using rendering results to make LOD decisions.
				bForceLODUpdateFromRenderer = true;
				UpdateLODInformation();
			}
		}
	}

	bForcedInActive = false;
	DeltaTime *= CustomTimeDilation;
	DeltaTimeTick = DeltaTime;

	AccumTickTime += DeltaTime;

	// Save player locations
	PlayerLocations.Reset();
	PlayerLODDistanceFactor.Reset();

#if WITH_EDITOR
	// clear tick timers
	for (auto Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->LastTickDurationMs = 0.0f;
		}
	}
#endif

	if (World->IsGameWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = Iterator->Get();
			if (PlayerController->IsLocalPlayerController())
			{
				FVector POVLoc;
				FRotator POVRotation;
				PlayerController->GetPlayerViewPoint(POVLoc, POVRotation);

				PlayerLocations.Add(POVLoc);
				PlayerLODDistanceFactor.Add(PlayerController->LocalPlayerCachedLODDistanceFactor);
			}
		}
	}

	// Orient the Z axis toward the camera
	if (Template->bOrientZAxisTowardCamera)
	{
		OrientZAxisTowardCamera();
	}

	if (Template->SystemUpdateMode == EPSUM_FixedTime)
	{
		// Use the fixed delta time!
		DeltaTime = Template->UpdateTime_Delta;
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_ResetAndCheckParallel);
		// Clear out the events.
		SpawnEvents.Reset();
		DeathEvents.Reset();
		CollisionEvents.Reset();
		BurstEvents.Reset();
		TotalActiveParticles = 0;
		bNeedsFinalize = true;
		if (!ThisTickFunction || !ThisTickFunction->IsCompletionHandleValid() || !CanTickInAnyThread() || FXConsoleVariables::bFreezeParticleSimulation || !FXConsoleVariables::bAllowAsyncTick ||
			GDistributionType == 0) // this may not be absolutely required, however if you are using distributions it will be glacial anyway. If you want to get rid of this, note that some modules use this indirectly as their criteria for CanTickInAnyThread
		{
			bDisallowAsync = true;
		}
	}
	if (bDisallowAsync)
	{
		if (!FXConsoleVariables::bFreezeParticleSimulation)
		{
			ComputeTickComponent_Concurrent();
		}
		FinalizeTickComponent();
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_QueueTasks);
		{
			SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_Marshall)
		bAsyncDataCopyIsValid = true;
		check(!bParallelRenderThreadUpdate);
		AsyncComponentToWorld = GetComponentTransform();
		AsyncInstanceParameters.Reset();
		AsyncInstanceParameters.Append(InstanceParameters);
			AsyncBounds = Bounds;
			AsyncPartSysVelocity = PartSysVelocity;

			//cache component to world of each actor that trails may use
			for (FParticleSysParam& ParticleSysParam : AsyncInstanceParameters)
			{
				ParticleSysParam.UpdateAsyncActorCache();
			}

		bAsyncWorkOutstanding = true;
		}
		
		{
			SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_QueueAsync);
			AsyncWork = TGraphTask<FParticleAsyncTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(this);
#if !WITH_EDITOR  // we need to not complete until this is done because the game thread finalize task has not beed queued yet
			ThisTickFunction->GetCompletionHandle()->DontCompleteUntil(AsyncWork);
#endif
		}
#if WITH_EDITOR  // we need to queue this here because we need to be able to block and wait on it
		{
			SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_QueueFinalize);
			FGraphEventArray Prereqs;
			Prereqs.Add(AsyncWork);
			FGraphEventRef Finalize = TGraphTask<FParticleFinalizeTask>::CreateTask(&Prereqs, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(this);
			ThisTickFunction->GetCompletionHandle()->DontCompleteUntil(Finalize);
		}
#endif

	}


	// do not change the tick group if there is a Flex emitter
	// present, as the component must be ticked in the EndPhysics phase
	if (bHasFlexEmitter == false)
	{
		if(CVarFXEarlySchedule.GetValueOnGameThread())
		{
			PrimaryComponentTick.TickGroup = TG_PrePhysics; 
			PrimaryComponentTick.EndTickGroup = TG_PostPhysics;
		}
		else
		{
			PrimaryComponentTick.TickGroup = TG_DuringPhysics;
		}
	}
}

int32 UParticleSystemComponent::GetCurrentDetailMode() const
{
#if WITH_EDITORONLY_DATA
	if (!GEngine->bEnableEditorPSysRealtimeLOD && EditorDetailMode >= 0)
	{
		return EditorDetailMode;
	}
	else
#endif
	{
		return GetCachedScalabilityCVars().DetailMode;
	}
}

void UParticleSystemComponent::ComputeTickComponent_Concurrent()
{
	FInGameScopedCycleCounter InGameCycleCounter(GetWorld(), EInGamePerfTrackers::VFXSignificance, IsInGameThread() ? EInGamePerfTrackerThreads::GameThread : EInGamePerfTrackerThreads::OtherThread, bIsManagingSignificance);

	SCOPE_CYCLE_COUNTER(STAT_ParticleComputeTickTime);
	FScopeCycleCounterUObject AdditionalScope(AdditionalStatObject(), GET_STATID(STAT_ParticleComputeTickTime));
	// Tick Subemitters.
	int32 EmitterIndex;
	NumSignificantEmitters = 0;
	for (EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
		FScopeCycleCounterEmitter AdditionalScopeInner(Instance);
#if WITH_EDITOR
		uint32 StartTime = FPlatformTime::Cycles();
#endif

		if (EmitterIndex + 1 < EmitterInstances.Num())
		{
			FParticleEmitterInstance* NextInstance = EmitterInstances[EmitterIndex+1];
			FPlatformMisc::Prefetch(NextInstance);
		}

		if (Instance && Instance->SpriteTemplate)
		{
			check(Instance->SpriteTemplate->LODLevels.Num() > 0);

			UParticleLODLevel* SpriteLODLevel = Instance->SpriteTemplate->GetCurrentLODLevel(Instance);
			if (SpriteLODLevel && SpriteLODLevel->bEnabled)
			{
				if (bIsManagingSignificance)
				{
					bool bEmitterIsSignificant = Instance->SpriteTemplate->IsSignificant(RequiredSignificance);
					if (bEmitterIsSignificant)
					{
						++NumSignificantEmitters;
						Instance->SetHaltSpawning(false);
						Instance->SetFakeBurstWhenSpawningSupressed(false);
						Instance->bEnabled = true;
					}
					else
					{
						Instance->SetHaltSpawning(true);
						Instance->SetFakeBurstWhenSpawningSupressed(true);
						if (Instance->SpriteTemplate->bDisableWhenInsignficant)
						{
							Instance->bEnabled = false;
						}
					}
				}
				else
				{
					++NumSignificantEmitters;
				}

				Instance->Tick(DeltaTimeTick, bSuppressSpawning);

				if (!Instance->Tick_MaterialOverrides())
				{
					if (EmitterMaterials.IsValidIndex(EmitterIndex))
					{
						if (EmitterMaterials[EmitterIndex])
						{
							Instance->CurrentMaterial = EmitterMaterials[EmitterIndex];
						}
					}
				}
				TotalActiveParticles += Instance->ActiveParticles;
			}

#if WITH_EDITOR
			uint32 EndTime = FPlatformTime::Cycles();
			Instance->LastTickDurationMs += FPlatformTime::ToMilliseconds(EndTime - StartTime);
#endif
		}
	}
	if (bAsyncWorkOutstanding)
	{
		FPlatformMisc::MemoryBarrier();
		bAsyncWorkOutstanding = false;
	}
}

void UParticleSystemComponent::FinalizeTickComponent()
{
	FInGameScopedCycleCounter InGameCycleCounter(GetWorld(), EInGamePerfTrackers::VFXSignificance, IsInGameThread() ? EInGamePerfTrackerThreads::GameThread : EInGamePerfTrackerThreads::OtherThread, bIsManagingSignificance);

	SCOPE_CYCLE_COUNTER(STAT_ParticleFinalizeTickTime);

	if(bAsyncDataCopyIsValid)
	{
		//reset async actor to world
		for (FParticleSysParam& ParticleSysParam : AsyncInstanceParameters)
		{
			ParticleSysParam.ResetAsyncActorCache();
		}
	}

	bAsyncDataCopyIsValid = false;
	if (!bNeedsFinalize)
	{
		return;
	}
	AsyncWork = NULL; // this task is done
	bNeedsFinalize = false;

	if (FXConsoleVariables::bFreezeParticleSimulation == false)
	{
		int32 EmitterIndex;
		// Now, process any events that have occurred.
		for (EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
		{
			FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
			if (Instance && Instance->bEnabled)
			{
				if (EmitterIndex + 1 < EmitterInstances.Num())
				{
				FParticleEmitterInstance* NextInstance = EmitterInstances[EmitterIndex+1];
					FPlatformMisc::Prefetch(NextInstance);
				}

				if (Instance->SpriteTemplate)
				{
					UParticleLODLevel* SpriteLODLevel = Instance->SpriteTemplate->GetCurrentLODLevel(Instance);
					if (SpriteLODLevel && SpriteLODLevel->bEnabled)
					{
						Instance->ProcessParticleEvents(DeltaTimeTick, bSuppressSpawning);
					}
				}
			}
		}

		UWorld* World = GetWorld();
		AParticleEventManager* EventManager = (World ? World->MyParticleEventManager : NULL);
		if (EventManager)
		{
			if (SpawnEvents.Num() > 0) EventManager->HandleParticleSpawnEvents(this, SpawnEvents);
			if (DeathEvents.Num() > 0) EventManager->HandleParticleDeathEvents(this, DeathEvents);
			if (CollisionEvents.Num() > 0) EventManager->HandleParticleCollisionEvents(this, CollisionEvents);
			if (BurstEvents.Num() > 0) EventManager->HandleParticleBurstEvents(this, BurstEvents);
		}
	}
	// Clear out the Kismet events, as they should have been processed by now...
	KismetEvents.Empty();

	// Indicate that we have been ticked since being registered.
	bJustRegistered = false;

	float CurrTime = GetWorld()->GetTimeSeconds();

	//Are we still significant?
	if ((bIsActive && !bWasDeactivated) && bIsManagingSignificance && NumSignificantEmitters == 0 && CurrTime >= LastSignificantTime + Template->InsignificanceDelay)
	{
		OnSignificanceChanged(false, true);
	}
	else
	{
		LastSignificantTime = CurrTime;
	// If component has just totally finished, call script event.
	const bool bIsCompleted = HasCompleted(); 
	if (bIsCompleted && !bWasCompleted)
	{
			Complete();
		}
		bWasCompleted = bIsCompleted;
		}

	// Update bounding box.
	if (!bWarmingUp && !bWasCompleted && !Template->bUseFixedRelativeBoundingBox && !bIsTransformDirty)
	{
		// Force an update every once in a while to shrink the bounds.
		TimeSinceLastForceUpdateTransform += DeltaTimeTick;
		if(TimeSinceLastForceUpdateTransform > MaxTimeBeforeForceUpdateTransform)
		{
			bIsTransformDirty = true;
		}
		else
		{
			// Compute the new system bounding box.
			FBox BoundingBox;
			BoundingBox.Init();

			for (int32 i=0; i<EmitterInstances.Num(); i++)
			{
				FParticleEmitterInstance* Instance = EmitterInstances[i];
				if (Instance && Instance->SpriteTemplate)
				{
					UParticleLODLevel* SpriteLODLevel = Instance->SpriteTemplate->GetCurrentLODLevel(Instance);
					if (SpriteLODLevel && SpriteLODLevel->bEnabled)
					{
						BoundingBox += Instance->GetBoundingBox();
					}
				}
			}

			// Only update the primitive's bounding box in the octree if the system bounding box has gotten larger.
			if(!Bounds.GetBox().IsInside(BoundingBox.Min) || !Bounds.GetBox().IsInside(BoundingBox.Max))
			{
				bIsTransformDirty = true;
			}
		}
	}

	// Update if the component transform has been dirtied.
	if(bIsTransformDirty)
	{
		UpdateComponentToWorld();
		
		TimeSinceLastForceUpdateTransform = 0.0f;
		bIsTransformDirty = false;
	}

	const float InvDeltaTime = (DeltaTimeTick > 0.0f) ? 1.0f / DeltaTimeTick : 0.0f;
	PartSysVelocity = (GetComponentLocation() - OldPosition) * InvDeltaTime;
	OldPosition = GetComponentLocation();

	if (bIsViewRelevanceDirty)
	{
		ConditionalCacheViewRelevanceFlags();
	}

	if (bSkipUpdateDynamicDataDuringTick == false)
	{
		Super::MarkRenderDynamicDataDirty();
	}

}

void UParticleSystemComponent::WaitForAsyncAndFinalize(EForceAsyncWorkCompletion Behavior, bool bDefinitelyGameThread) const
{
	if (AsyncWork.GetReference() && !AsyncWork->IsComplete())
	{
		double StartTime = FPlatformTime::Seconds();
		if (bDefinitelyGameThread)
		{
			check(IsInGameThread());
			SCOPE_CYCLE_COUNTER(STAT_GTSTallTime);
			SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_WaitForAsyncAndFinalize);
#if WITH_EDITOR
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(AsyncWork, ENamedThreads::GameThread_Local);
#else
			// since in the non-editor case the completion is chained to a game thread task (not a gamethread_local one), and we don't want to execute arbitrary tasks 
			// in what is probably a very, very deep callstack, we will spin here and wait for the async task to finish. The we will do the finalize. The finalize will be attempted again later but do nothing
			while (bAsyncWorkOutstanding)
			{
				FPlatformProcess::SleepNoStats(0.0f);
			}
#endif
		}
		else
		{
			SCOPE_CYCLE_COUNTER(STAT_UParticleSystemComponent_WaitForAsyncAndFinalize);
			while (bAsyncWorkOutstanding)
			{
				FPlatformProcess::SleepNoStats(0.0f);
			}
		}

		float ThisTime = float(FPlatformTime::Seconds() - StartTime) * 1000.0f;
		if (Behavior != SILENT && ThisTime >= KINDA_SMALL_NUMBER)
		{
			if (bDefinitelyGameThread || IsInGameThread())
			{
				UE_LOG(LogParticles, Warning, TEXT("Stalled gamethread waiting for particles %5.6fms '%s' '%s'"), ThisTime, *GetFullNameSafe(this), *GetFullNameSafe(Template));
			}
			else
			{
				UE_LOG(LogParticles, Warning, TEXT("Stalled worker thread waiting for particles %5.6fms '%s' '%s'"), ThisTime, *GetFullNameSafe(this), *GetFullNameSafe(Template));
			}
		}
		const_cast<UParticleSystemComponent*>(this)->FinalizeTickComponent();
	}
}

void UParticleSystemComponent::InitParticles()
{
	LLM_SCOPE(ELLMTag::Particles);

	SCOPE_CYCLE_COUNTER(STAT_ParticleSystemComponent_InitParticles);

	if (IsTemplate() == true)
	{
		return;
	}
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);

	check(GetWorld());
	UE_LOG(LogParticles,Verbose,
		TEXT("InitParticles @ %fs %s"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"));

	if (Template != NULL)
	{
		WarmupTime = Template->WarmupTime;
		WarmupTickRate = Template->WarmupTickRate;
		bIsViewRelevanceDirty = true;
		const int32 GlobalDetailMode = GetCurrentDetailMode();
		const bool bCanEverRender = CanEverRender();

		//simplified version.
		int32 NumInstances = EmitterInstances.Num();
		int32 NumEmitters = Template->Emitters.Num();
		const bool bIsFirstCreate = NumInstances == 0;
		EmitterInstances.SetNumZeroed(NumEmitters);

		bWasCompleted = bIsFirstCreate ? false : bWasCompleted;

		bool bClearDynamicData = false;
		int32 PreferredLODLevel = LODLevel;
		bool bSetLodLevels = LODLevel > 0; //We should set the lod level even when creating all emitters if the requested LOD is not 0. 

		for (int32 Idx = 0; Idx < NumEmitters; Idx++)
		{
			UParticleEmitter* Emitter = Template->Emitters[Idx];
			FParticleEmitterInstance* Instance = NumInstances == 0 ? NULL : EmitterInstances[Idx];
			
			const bool bDetailModeAllowsRendering = DetailMode <= GlobalDetailMode && Emitter->DetailMode <= GlobalDetailMode;
			const bool bShouldCreateAndOrInit = bDetailModeAllowsRendering && Emitter->HasAnyEnabledLODs() && bCanEverRender;

			if (bShouldCreateAndOrInit)
			{
				if (Instance)
				{
					Instance->SetHaltSpawning(false);
					Instance->SetHaltSpawningExternal(false);
				}
				else
				{
					Instance = Emitter->CreateInstance(this);
					EmitterInstances[Idx] = Instance;
				}

				if (Instance)
				{
					Instance->bEnabled = true;
						Instance->InitParameters(Emitter, this);
						Instance->Init();

						PreferredLODLevel = FMath::Min(PreferredLODLevel, Emitter->LODLevels.Num());
						bSetLodLevels |= !bIsFirstCreate;//Only set lod levels if we init any instances and it's not the first creation time.
					}
				}
			else
			{
				if (Instance)
				{
#if STATS
						Instance->PreDestructorCall();
#endif
						delete Instance;
						EmitterInstances[Idx] = NULL;
						bClearDynamicData = true;
					}
			}
		}

		if (bClearDynamicData)
		{
			ClearDynamicData();
		}

		if (bSetLodLevels)
		{
			if (PreferredLODLevel != LODLevel)
			{
				// This should never be higher...
				check(PreferredLODLevel < LODLevel);
				LODLevel = PreferredLODLevel;
			}

			for (int32 Idx = 0; Idx < EmitterInstances.Num(); Idx++)
			{
				FParticleEmitterInstance* Instance = EmitterInstances[Idx];
				// set the LOD levels here
				if (Instance)
				{
					Instance->CurrentLODLevelIndex	= LODLevel;

					// small safety net for OR-11322; can be removed if the ensure never fires after the change in SetTemplate (reset all instances LOD indices to 0)
					if (Instance->CurrentLODLevelIndex >= Instance->SpriteTemplate->LODLevels.Num())
					{
						Instance->CurrentLODLevelIndex = Instance->SpriteTemplate->LODLevels.Num()-1;
						ensureMsgf(false, TEXT("LOD access out of bounds (OR-11322). Please let olaf.piesche or simon.tovey know."));
					}
					Instance->CurrentLODLevel = Instance->SpriteTemplate->LODLevels[Instance->CurrentLODLevelIndex];
				}
			}
		}
	}

}

void UParticleSystemComponent::ResetParticles(bool bEmptyInstances)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	UE_LOG(LogParticles,Verbose,
		TEXT("ResetParticles @ %fs %s bEmptyInstances=%s"), GetWorld() ? GetWorld()->TimeSeconds : 0.0f,
		Template != NULL ? *Template->GetName() : TEXT("NULL"), bEmptyInstances ? TEXT("true") : TEXT("false"));

	UWorld* OwningWorld = GetWorld();

	//Also consider this deactivation.
	if (bIsActive)
	{
		OnSystemPreActivationChange.Broadcast(this, false);
	}

	const bool bIsGameWorld = OwningWorld ? OwningWorld->IsGameWorld() : !GIsEditor;

#if WITH_FLEX
	ClearFlexSurfaceDynamicData();
#endif

	// Remove instances from scene.
	for( int32 InstanceIndex=0; InstanceIndex<EmitterInstances.Num(); InstanceIndex++ )
	{
		FParticleEmitterInstance* EmitterInstance = EmitterInstances[InstanceIndex];
		if( EmitterInstance )
		{
			if (GbEnableGameThreadLODCalculation == false)
			{
				if (!(!bIsGameWorld || bEmptyInstances))
				{
					EmitterInstance->SpriteTemplate	= NULL;
					EmitterInstance->Component	= NULL;
				}
			}
		}
	}

	// Set the system as inactive
	bIsActive	= false;

	// Remove instances if we're not running gameplay.ww
	if (!bIsGameWorld || bEmptyInstances)
	{
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
		{
			FParticleEmitterInstance* EmitInst = EmitterInstances[EmitterIndex];
			if (EmitInst)
			{
#if STATS
				EmitInst->PreDestructorCall();
#endif
				delete EmitInst;
				EmitterInstances[EmitterIndex] = NULL;
			}
		}
		EmitterInstances.Empty();
		ClearDynamicData();
	}
	else
	{
		for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
		{
			FParticleEmitterInstance* EmitInst = EmitterInstances[EmitterIndex];
			if (EmitInst)
			{
				EmitInst->Rewind();
			}
		}
	}

	// Mark render state dirty to deregister the component with the scene.
	MarkRenderStateDirty();
}

void UParticleSystemComponent::ResetBurstLists()
{
	ForceAsyncWorkCompletion(STALL);
	for (int32 i=0; i<EmitterInstances.Num(); i++)
	{
		if (EmitterInstances[i])
		{
			EmitterInstances[i]->ResetBurstList();
		}
	}
}

void UParticleSystemComponent::SetTemplate(class UParticleSystem* NewTemplate)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleSetTemplateTime);
	ForceAsyncWorkCompletion(STALL);

	if( GIsAllowingParticles || GIsEditor ) 
	{
		bIsViewRelevanceDirty = true;

		bool bIsTemplate = IsTemplate();
		bWasCompleted = false;
		// remember if we were active and therefore should restart after setting up the new template
		bWasActive = bIsActive && !bWasDeactivated; 
		bool bResetInstances = false;
		if (NewTemplate != Template)
		{
			bIsElligibleForAsyncTick = false;
			bIsElligibleForAsyncTickComputed = false;
			bResetInstances = true;
		}
		if (bIsTemplate == false)
		{
#if WITH_FLEX
			// Maintain the FlexFluidSurface (and Material Instance) override
			if (FlexFluidSurfaceOverride)
			{
				UFlexFluidSurfaceComponent* SurfaceComponent = GetWorld()->GetFlexFluidSurface(FlexFluidSurfaceOverride);
				
				// This is necessary because we need to hold the reference to the fluid surface so it doesn't go away with a SetTemplate() call
				SurfaceComponent->SetEnabledReferenceCounting(false);
			}
#endif

			ResetParticles(bResetInstances);
		}

		Template = NewTemplate;
		if (Template)
		{
			WarmupTime = Template->WarmupTime;
		}
		else
		{
			WarmupTime = 0.0f;
		}

		// set the LOD level to 0 in case we're recycling the component, so InitParticles doesn't mistakenly grab an invalid LOD level
		// speculative fix for OR-11322. May become permanent if the ensure in InitParticles never fires.
		LODLevel = 0;

		if(NewTemplate && IsRegistered())
		{
			if ((bAutoActivate || bWasActive) && (bIsTemplate == false))
			{
				ActivateSystem();
			}
			else
			{
				InitializeSystem();
			}

			if (!SceneProxy || bResetInstances)
			{
				MarkRenderStateDirty();
			}
		}
	}
	else
	{
		Template = NULL;
	}
	EmitterMaterials.Empty();

	for (int32 Idx = 0; Idx < EmitterInstances.Num(); Idx++)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[Idx];
		// set the LOD levels to 0 in case we're recycling the component, so InitParticles doesn't mistakenly grab an invalid LOD level
		if (Instance)
		{
			Instance->CurrentLODLevelIndex = 0;
		}

#if WITH_FLEX
		// Maintain the FlexFluidSurface (and Material Instance) override
		if (FlexFluidSurfaceOverride)
		{
			if (Instance &&
				Instance->SpriteTemplate &&
				Instance->SpriteTemplate->FlexFluidSurfaceTemplate &&
				Instance->SpriteTemplate->FlexFluidSurfaceTemplate->Material)
			{
				Instance->RegisterNewFlexFluidSurfaceComponent(FlexFluidSurfaceOverride);
	}
		}
#endif
	}

#if WITH_FLEX
	// Maintain the FlexFluidSurface (and Material Instance) override
	if (FlexFluidSurfaceOverride)
	{
		UFlexFluidSurfaceComponent* SurfaceComponent = GetWorld()->GetFlexFluidSurface(FlexFluidSurfaceOverride);

		// This is necessary because we need to hold the reference to the fluid surface so it doesn't go away with a SetTemplate() call
		SurfaceComponent->SetEnabledReferenceCounting(true);
	}
#endif

	if (SceneProxy)
	{
		static_cast<FParticleSystemSceneProxy*>(SceneProxy)->MarkVertexFactoriesDirty();
	}
}

void UParticleSystemComponent::ActivateSystem(bool bFlagAsJustAttached)
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleActivateTime);
	ForceAsyncWorkCompletion(STALL);

	if (IsTemplate() == true || !IsRegistered() || 	!FApp::CanEverRender())
	{
		return;
	}

	check(GetWorld());
	UE_LOG(LogParticles,Verbose,
		TEXT("ActivateSystem @ %fs %s"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"));

	const bool bIsGameWorld = GetWorld()->IsGameWorld();

	if (UE_LOG_ACTIVE(LogParticles,VeryVerbose))
	{
		if (Template)
		{
			if (EmitterInstances.Num() > 0)
			{
				int32 LiveCount = 0;

				for (int32 EmitterIndex =0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
				{
					FParticleEmitterInstance* EmitInst = EmitterInstances[EmitterIndex];
					if (EmitInst)
					{
						LiveCount += EmitInst->ActiveParticles;
					}
				}

				if (LiveCount > 0)
				{
					UE_LOG(LogParticles, Log, TEXT("ActivateSystem called on PSysComp w/ live particles - %5d, %s"),
						LiveCount, *(Template->GetFullName()));
				}
			}
		}
	}

	// System settings may have been lowered. Support late deactivation.
	const bool bDetailModeAllowsRendering = DetailMode <= GetCurrentDetailMode();

	if( GIsAllowingParticles && bDetailModeAllowsRendering && Template )
	{
		// Auto attach if requested
		const bool bWasAutoAttached = bDidAutoAttach;
		bDidAutoAttach = false;
		if (bAutoManageAttachment)
		{
			USceneComponent* NewParent = AutoAttachParent.Get();
			if (NewParent)
			{
				const bool bAlreadyAttached = GetAttachParent() && (GetAttachParent() == NewParent) && (GetAttachSocketName() == AutoAttachSocketName) && GetAttachParent()->GetAttachChildren().Contains(this);
				if (!bAlreadyAttached)
				{
					bDidAutoAttach = bWasAutoAttached;
					CancelAutoAttachment(true);
					SavedAutoAttachRelativeLocation = RelativeLocation;
					SavedAutoAttachRelativeRotation = RelativeRotation;
					SavedAutoAttachRelativeScale3D = RelativeScale3D;
					AttachToComponent(NewParent, FAttachmentTransformRules(AutoAttachLocationRule, AutoAttachRotationRule, AutoAttachScaleRule, false), AutoAttachSocketName);
				}

				bDidAutoAttach = true;
				bFlagAsJustAttached = true;
			}
			else
			{
				CancelAutoAttachment(true);
			}
		}

		AccumTickTime = 0.0;

		if (!bIsActive)
		{
			LastSignificantTime = GetWorld()->GetTimeSeconds();
			RequiredSignificance = EParticleSignificanceLevel::Low;

		//Call this now after any attachment has happened.
			OnSystemPreActivationChange.Broadcast(this, true);
		}

		//We start this here as before the PreActivation call above, we don't know if this component is managing significance or not.
		FInGameScopedCycleCounter InGameCycleCounter(GetWorld(), EInGamePerfTrackers::VFXSignificance, EInGamePerfTrackerThreads::GameThread, bIsManagingSignificance);

		if (bFlagAsJustAttached)
		{
			bJustRegistered = true;
		}
	
		// Stop suppressing particle spawning.
		bSuppressSpawning = false;
		
		// Set the system as active
		bool bNeedToUpdateTransform = bWasDeactivated;
		bWasCompleted = false;
		bWasDeactivated = false;
		bIsActive = true;
		bWasActive = false; // Set to false now, it may get set to true when it's deactivated due to unregister
		SetComponentTickEnabled(true);

		// if no instances, or recycling
		if (EmitterInstances.Num() == 0 || (bIsGameWorld && (!bAutoActivate || bHasBeenActivated)))
		{
			InitializeSystem();
		}
		else if (EmitterInstances.Num() > 0 && !bIsGameWorld)
		{
			// If currently running, re-activating rewinds the emitter to the start. Existing particles should stick around.
			for (int32 i=0; i<EmitterInstances.Num(); i++)
			{
				if (EmitterInstances[i])
				{
					EmitterInstances[i]->Rewind();
					EmitterInstances[i]->SetHaltSpawning(false);
					EmitterInstances[i]->SetHaltSpawningExternal(false);
				}
			}
		}


		// Force an LOD update
		if ((bIsGameWorld || (GIsEditor && GEngine->bEnableEditorPSysRealtimeLOD)) && (GbEnableGameThreadLODCalculation == true))
		{
			FVector EffectPosition = GetComponentLocation();
			int32 DesiredLODLevel = DetermineLODLevelForLocation(EffectPosition);
			SetLODLevel(DesiredLODLevel);
		}
		else
		{
			bForceLODUpdateFromRenderer = true;
		}


		// Flag the system as having been activated at least once
		bHasBeenActivated = true;


		// Clear tick time
		TimeSinceLastTick = 0;

		int32 DesiredLODLevel = 0;
		bool bCalculateLODLevel = 
			(bOverrideLODMethod == true) ? (LODMethod != PARTICLESYSTEMLODMETHOD_DirectSet) : 
				(Template ? (Template->LODMethod != PARTICLESYSTEMLODMETHOD_DirectSet) : false);

		if (bCalculateLODLevel)
		{
			FVector EffectPosition = GetComponentLocation();
			DesiredLODLevel = DetermineLODLevelForLocation(EffectPosition);
			if (GbEnableGameThreadLODCalculation == true)
			{
				if (DesiredLODLevel != LODLevel)
				{
					bIsActive = true;
					SetComponentTickEnabled(true);
				}
				SetLODLevel(DesiredLODLevel);
			}
		}

		if (WarmupTime != 0.0f)
		{
			bool bSaveSkipUpdate = bSkipUpdateDynamicDataDuringTick;
			bSkipUpdateDynamicDataDuringTick = true;
			bWarmingUp = true;
			ResetBurstLists();

			float WarmupElapsed = 0.f;
			float WarmupTimestep = 0.032f;
			if (WarmupTickRate > 0)
			{
				WarmupTimestep = (WarmupTickRate <= WarmupTime) ? WarmupTickRate : WarmupTime;
			}

			while (WarmupElapsed < WarmupTime)
			{
				TickComponent(WarmupTimestep, LEVELTICK_All, NULL);
				WarmupElapsed += WarmupTimestep;
			}

			bWarmingUp = false;
			WarmupTime = 0.0f;
			bSkipUpdateDynamicDataDuringTick = bSaveSkipUpdate;
		}

		//We are definitely insignificant already so set insignificant before we ever begin ticking.
		if (bIsManagingSignificance && Template->GetHighestSignificance() < RequiredSignificance && Template->InsignificanceDelay == 0.0f)
		{
			OnSignificanceChanged(false, true);
		}
	}

	// Mark render state dirty to ensure the scene proxy is added and registered with the scene.
	MarkRenderStateDirty();

	if(!bWasDeactivated && !bWasCompleted && ensure(GetWorld()))
	{
		LastRenderTime = GetWorld()->GetTimeSeconds();
	}
}

void UParticleSystemComponent::Complete()
{
	UE_LOG(LogParticles, Verbose,
		TEXT("HasCompleted()==true @ %fs %s"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"));

	OnSystemFinished.Broadcast(this);

	// When system is done - destroy all subemitters etc. We don't need them any more.
	ResetParticles();
	bIsActive = false;
	SetComponentTickEnabled(false);

	if (bAutoDestroy)
	{
		DestroyComponent();
	}
	else if (bAutoManageAttachment)
	{
		CancelAutoAttachment(/*bDetachFromParent=*/ true);
	}
}

void UParticleSystemComponent::DeactivateSystem()
{
	FInGameScopedCycleCounter InGameCycleCounter(GetWorld(), EInGamePerfTrackers::VFXSignificance, EInGamePerfTrackerThreads::GameThread, bIsManagingSignificance);

	if (IsTemplate() == true)
	{
		return;
	}
	ForceAsyncWorkCompletion(STALL);

#if WITH_FLEX
	ClearFlexSurfaceDynamicData();
#endif

	check(GetWorld());
	UE_LOG(LogParticles,Verbose,
		TEXT("DeactivateSystem @ %fs %s"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"));

	if (bIsActive)
	{
		OnSystemPreActivationChange.Broadcast(this, false);
	}

	bSuppressSpawning = true;
	bWasDeactivated = true;

	bool bShouldMarkRenderStateDirty = false;
	for (int32 i = 0; i < EmitterInstances.Num(); i++)
	{
		FParticleEmitterInstance*	Instance = EmitterInstances[i];
		if (Instance)
		{
			if (Instance->bKillOnDeactivate)
			{
				//UE_LOG(LogParticles, Log, TEXT("%s killed on deactivate"),EmitterInstances(i)->GetName());
#if STATS
				Instance->PreDestructorCall();
#endif
				// clean up other instances that may point to this one
				for (int32 InnerIndex=0; InnerIndex < EmitterInstances.Num(); InnerIndex++)
				{
					if (InnerIndex != i && EmitterInstances[InnerIndex] != NULL)
					{
						EmitterInstances[InnerIndex]->OnEmitterInstanceKilled(Instance);
					}
				}
				delete Instance;
				EmitterInstances[i] = NULL;
				bShouldMarkRenderStateDirty = true;
			}
			else
			{
				Instance->OnDeactivateSystem();
			}
		}
	}

	if (bShouldMarkRenderStateDirty)
	{
		ClearDynamicData();
		MarkRenderStateDirty();
	}

	//We have to ensure ticking is enabled so that this component completes and is can be destroyed etc correctly.
	//TODO: What if there are immortal particles but bKillOnDeactivate is false? Need to mark emitters with currently immortal particles, kill them and warn the user.
	SetComponentTickEnabled(true);

	LastRenderTime = GetWorld()->GetTimeSeconds();
}

void UParticleSystemComponent::CancelAutoAttachment(bool bDetachFromParent)
{
	if (bAutoManageAttachment)
	{
		if (bDidAutoAttach)
		{
			// Restore relative transform from before attachment. Actual transform will be updated as part of DetachFromParent().
			RelativeLocation = SavedAutoAttachRelativeLocation;
			RelativeRotation = SavedAutoAttachRelativeRotation;
			RelativeScale3D = SavedAutoAttachRelativeScale3D;
			bDidAutoAttach = false;
		}

		if (bDetachFromParent)
		{
			DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		}
	}
}

void UParticleSystemComponent::ComputeCanTickInAnyThread()
{
	check(!bIsElligibleForAsyncTickComputed);
	bIsElligibleForAsyncTick = false;
	if (Template)
	{
		bIsElligibleForAsyncTickComputed = true;
		bIsElligibleForAsyncTick = Template->CanTickInAnyThread();
	}
}

bool UParticleSystemComponent::ShouldActivate() const
{
	return (Super::ShouldActivate() || (bWasDeactivated || bWasCompleted));
}

void UParticleSystemComponent::Activate(bool bReset) 
{
	// If the particle system can't ever render (ie on dedicated server or in a commandlet) than do not activate...
	// Occasionaly we can arrive here with no template so check that here too.
	if (FApp::CanEverRender() && Template)
	{
		ForceAsyncWorkCompletion(STALL);
		if (bReset || ShouldActivate()==true)
		{
			ActivateSystem(bReset);

			if (bIsActive)
			{
				OnComponentActivated.Broadcast(this, bReset);
			}
		}
	}
}

void UParticleSystemComponent::Deactivate()
{
	ForceAsyncWorkCompletion(STALL);
	if (ShouldActivate()==false)
	{
		DeactivateSystem();

		if (bWasDeactivated)
		{
			OnComponentDeactivated.Broadcast(this);
		}
	}
}

void UParticleSystemComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	OldPosition+= InOffset;
	
	for (auto It = EmitterInstances.CreateIterator(); It; ++It)
	{
		FParticleEmitterInstance* EmitterInstance = *It;
		if (EmitterInstance != NULL)
		{
			EmitterInstance->ApplyWorldOffset(InOffset, bWorldShift);
		}
	}
}

void UParticleSystemComponent::ResetToDefaults()
{
	ForceAsyncWorkCompletion(STALL);
	if (!IsTemplate())
	{
		// make sure we're fully stopped and unregistered
		DeactivateSystem();
		SetTemplate(NULL);

		if(IsRegistered())
		{
			UnregisterComponent();
		}

		UParticleSystemComponent* Default = (UParticleSystemComponent*)(GetArchetype());

		// copy all non-native, non-duplicatetransient, non-Component properties we have from all classes up to and including UActorComponent
		for (UProperty* Property = GetClass()->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
		{
			if ( !(Property->PropertyFlags & CPF_DuplicateTransient) && !(Property->PropertyFlags & (CPF_InstancedReference | CPF_ContainsInstancedReference)) &&
				Property->GetOwnerClass()->IsChildOf(UActorComponent::StaticClass()) )
			{
				Property->CopyCompleteValue_InContainer(this, Default);
			}
		}
	}
}

void UParticleSystemComponent::UpdateInstances(bool bEmptyInstances)
{
	if (GIsEditor && IsRegistered())
	{
		ForceAsyncWorkCompletion(STALL);
		ResetParticles(bEmptyInstances);

		InitializeSystem();
		if (bAutoActivate)
		{
			ActivateSystem();
		}

		if (Template && Template->bUseFixedRelativeBoundingBox)
		{
			UpdateComponentToWorld();
		}
	}
}

int32 UParticleSystemComponent::GetNumActiveParticles() const
{
	ForceAsyncWorkCompletion(STALL);
	int32 NumParticles = 0;
	for (int32 i=0; i<EmitterInstances.Num(); i++)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[i];

		if (Instance != NULL)
		{
			NumParticles += Instance->ActiveParticles;
		}
	}
	return NumParticles;
}

void UParticleSystemComponent::GetOwnedTrailEmitters(UParticleSystemComponent::TrailEmitterArray& OutTrailEmitters, const void* InOwner, bool bSetOwner)
{
	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (Inst && Inst->IsTrailEmitter())
		{
			FParticleAnimTrailEmitterInstance* TrailEmitter = (FParticleAnimTrailEmitterInstance*)Inst;
			if (bSetOwner)
			{
				TrailEmitter->Owner = InOwner;
				OutTrailEmitters.Add(TrailEmitter);
			}
			else if (TrailEmitter->Owner == InOwner)
			{
				OutTrailEmitters.Add(TrailEmitter);
			}
		}
	}
}

void UParticleSystemComponent::BeginTrails(FName InFirstSocketName, FName InSecondSocketName, ETrailWidthMode InWidthMode, float InWidth)
{
	ActivateSystem(true);
	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (Inst)
		{
			Inst->BeginTrail();
			Inst->SetTrailSourceData(InFirstSocketName, InSecondSocketName, InWidthMode, InWidth);
		}
	}
}

void UParticleSystemComponent::EndTrails()
{
	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (Inst)
		{
			Inst->EndTrail();
		}
	}
	DeactivateSystem();
}

void UParticleSystemComponent::SetTrailSourceData(FName InFirstSocketName, FName InSecondSocketName, ETrailWidthMode InWidthMode, float InWidth)
{
	for (FParticleEmitterInstance* Inst : EmitterInstances)
	{
		if (Inst)
		{
			Inst->SetTrailSourceData(InFirstSocketName, InSecondSocketName, InWidthMode, InWidth);
		}
	}
}

bool UParticleSystemComponent::HasCompleted()
{
	ForceAsyncWorkCompletion(STALL);
	bool bHasCompleted = true;
	bool bCanBeDeactivated = true;

	// If we're currently capturing or replaying captured frames, then we'll stay active for that
	if( ReplayState != PRS_Disabled )
	{
		// While capturing, we want to stay active so that we'll just record empty frame data for
		// completed particle systems.  While replaying, we never want our particles/meshes removed from
		// the scene, so we'll force the system to stay alive!
		return false;
	}

	bool bClearDynamicData = false;
	for (int32 i=0; i<EmitterInstances.Num(); i++)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[i];

		if (Instance && Instance->CurrentLODLevel && Instance->bEnabled)
		{
			if (!Instance->bEmitterIsDone)
			{
				bCanBeDeactivated = false;
			}

			if (Instance->CurrentLODLevel->bEnabled)
			{
				if (Instance->CurrentLODLevel->RequiredModule->EmitterLoops > 0 || Instance->IsTrailEmitter())
				{
					if (bWasDeactivated && bSuppressSpawning)
					{
						if (Instance->ActiveParticles != 0)
						{
							bHasCompleted = false;
						}
					}
					else
					{
						if (Instance->HasCompleted())
						{
							if (Instance->bKillOnCompleted)
							{
#if STATS
								Instance->PreDestructorCall();
#endif
								// clean up other instances that may point to this one
								for (int32 InnerIndex=0; InnerIndex < EmitterInstances.Num(); InnerIndex++)
								{
									if (InnerIndex != i && EmitterInstances[InnerIndex] != NULL)
									{
										EmitterInstances[InnerIndex]->OnEmitterInstanceKilled(Instance);
									}
								}
								delete Instance;
								EmitterInstances[i] = NULL;
								bClearDynamicData = true;
							}
						}
						else
						{
							bHasCompleted = false;
						}
					}
				}
				else
				{
					if (bWasDeactivated)
					{
						if (Instance->ActiveParticles != 0)
						{
							bHasCompleted = false;
						}
					}
					else
					{
						bHasCompleted = false;
					}
				}
			}
			else
			{
				UParticleEmitter* Em = CastChecked<UParticleEmitter>(Instance->CurrentLODLevel->GetOuter());
				if (Em && Em->bDisabledLODsKeepEmitterAlive)
				{
					bHasCompleted = false;
				}				
			}
		}
	}

	if (bCanBeDeactivated && Template && Template->bAutoDeactivate)
	{
		DeactivateSystem();
	}

	if (bClearDynamicData)
	{
		ClearDynamicData();
	}
	
	return bHasCompleted;
}

void UParticleSystemComponent::InitializeSystem()
{
	SCOPE_CYCLE_COUNTER(STAT_ParticleInitializeTime);
	ForceAsyncWorkCompletion(STALL);

	if (!IsRegistered() || (FXSystem == NULL))
	{
		// Don't warn in a commandlet, we're expected not to have a scene / FX system.
		if (!IsRunningCommandlet() && !IsRunningDedicatedServer())
		{
			// We're also not expected to have a scene / FX system when we belong to an inactive world.
			UWorld* OwnerWorld = GetWorld();
			if (!OwnerWorld || OwnerWorld->WorldType != EWorldType::Inactive)
			{
				UE_LOG(LogParticles,Warning,TEXT("InitializeSystem called on an unregistered component. Template=%s Component=%s"),
					Template ? *Template->GetPathName() : TEXT("NULL"),
					*GetPathName()
					);
			}
		}
		return;
	}

	// At this point the component must be associated with an FX system.
	check( FXSystem != NULL );

	check(GetWorld());
	UE_LOG(LogParticles,Verbose,
		TEXT("InitializeSystem @ %fs %s Component=0x%p FXSystem=0x%p"), GetWorld()->TimeSeconds,
		Template != NULL ? *Template->GetName() : TEXT("NULL"), this, FXSystem);

	// System settings may have been lowered. Support late deactivation.
	const bool bDetailModeAllowsRendering = DetailMode <= GetCurrentDetailMode();

	if( GIsAllowingParticles && bDetailModeAllowsRendering )
	{
		if (IsTemplate() == true)
		{
			return;
		}

		if( Template != NULL )
		{
			EmitterDelay = Template->Delay;

			if( Template->bUseDelayRange )
			{
				const float	Rand = FMath::FRand();
				EmitterDelay	 = Template->DelayLow + ((Template->Delay - Template->DelayLow) * Rand);
			}
		}

		// Allocate the emitter instances and particle data
		InitParticles();
		if (IsRegistered())
		{
			AccumTickTime = 0.0;
			if ((bIsActive == false) && (bAutoActivate == true) && (bWasDeactivated == false))
			{
				SetActive(true);
			}
		}
	}
}


FString UParticleSystemComponent::GetDetailedInfoInternal() const
{
	FString Result;  

	if( Template != NULL )
	{
		Result = Template->GetPathName( NULL );
	}
	else
	{
		Result = TEXT("No_ParticleSystem");
	}

	return Result;  
}




void UParticleSystemComponent::ConditionalCacheViewRelevanceFlags(class UParticleSystem* NewTemplate)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	if (NewTemplate && (NewTemplate != Template))
	{
		bIsViewRelevanceDirty = true;
	}

	if (bIsViewRelevanceDirty)
	{
		UParticleSystem* TemplateToCache = Template;
		if (NewTemplate)
		{
			TemplateToCache = NewTemplate;
		}
		CacheViewRelevanceFlags(TemplateToCache);
		MarkRenderStateDirty();
	}
}

void UParticleSystemComponent::CacheViewRelevanceFlags(UParticleSystem* TemplateToCache)
{
	ForceAsyncWorkCompletion(ENSURE_AND_STALL);
	CachedViewRelevanceFlags.Empty();

	if (TemplateToCache)
	{
		for (int32 EmitterIndex = 0; EmitterIndex < TemplateToCache->Emitters.Num(); EmitterIndex++)
		{
			UParticleSpriteEmitter* Emitter = Cast<UParticleSpriteEmitter>(TemplateToCache->Emitters[EmitterIndex]);
			if (Emitter == NULL)
			{
				// Handle possible empty slots in the emitter array.
				continue;
			}
			FParticleEmitterInstance* EmitterInst = NULL;
			if (EmitterIndex < EmitterInstances.Num())
			{
				EmitterInst = EmitterInstances[EmitterIndex];
			}

			//@TODO I suspect this function can get called before emitter instances are created. That is bad and should be fixed up.
			if ( EmitterInst == NULL )
			{
				continue;
			}

			for (int32 LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
			{
				UParticleLODLevel* EmitterLODLevel = Emitter->LODLevels[LODIndex];

				// Prime the array
				// This code assumes that the particle system emitters all have the same number of LODLevels. 
				if (LODIndex >= CachedViewRelevanceFlags.Num())
				{
					CachedViewRelevanceFlags.AddZeroed(1);
				}
				FMaterialRelevance& LODViewRel = CachedViewRelevanceFlags[LODIndex];
				check(EmitterLODLevel->RequiredModule);

				if (EmitterLODLevel->bEnabled == true)
				{
					auto World = GetWorld();
					EmitterInst->GatherMaterialRelevance(&LODViewRel, EmitterLODLevel, World ? World->FeatureLevel : GMaxRHIFeatureLevel);
				}
			}
		}
	}
	bIsViewRelevanceDirty = false;
}

void UParticleSystemComponent::RewindEmitterInstances()
{
	ForceAsyncWorkCompletion(STALL);
	for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->Rewind();
		}
	}
}


void UParticleSystemComponent::SetBeamEndPoint(int32 EmitterIndex,FVector NewEndPoint)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamEndPoint(NewEndPoint);
		}
	}
}


void UParticleSystemComponent::SetBeamSourcePoint(int32 EmitterIndex,FVector NewSourcePoint,int32 SourceIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamSourcePoint(NewSourcePoint, SourceIndex);
		}
	}
}


void UParticleSystemComponent::SetBeamSourceTangent(int32 EmitterIndex,FVector NewTangentPoint,int32 SourceIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamSourceTangent(NewTangentPoint, SourceIndex);
		}
	}
}


void UParticleSystemComponent::SetBeamSourceStrength(int32 EmitterIndex,float NewSourceStrength,int32 SourceIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamSourceStrength(NewSourceStrength, SourceIndex);
		}
	}
}


void UParticleSystemComponent::SetBeamTargetPoint(int32 EmitterIndex,FVector NewTargetPoint,int32 TargetIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamTargetPoint(NewTargetPoint, TargetIndex);
		}
	}
}


void UParticleSystemComponent::SetBeamTargetTangent(int32 EmitterIndex,FVector NewTangentPoint,int32 TargetIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamTargetTangent(NewTangentPoint, TargetIndex);
		}
	}
}

void UParticleSystemComponent::SetBeamTargetStrength(int32 EmitterIndex,float NewTargetStrength,int32 TargetIndex)
{
	ForceAsyncWorkCompletion(STALL);
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			EmitterInst->SetBeamTargetStrength(NewTargetStrength, TargetIndex);
		}
	}
}

bool UParticleSystemComponent::GetBeamEndPoint(int32 EmitterIndex, FVector& OutSourcePoint) const
{
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			return EmitterInst->GetBeamEndPoint(OutSourcePoint);
		}
	}

	return false;
}

bool UParticleSystemComponent::GetBeamSourcePoint(int32 EmitterIndex, int32 SourceIndex, FVector& OutSourcePoint) const
{
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			return EmitterInst->GetBeamSourcePoint(SourceIndex, OutSourcePoint);
		}
	}

	return false;
}

bool UParticleSystemComponent::GetBeamSourceTangent(int32 EmitterIndex, int32 SourceIndex, FVector& OutSourcePoint) const
{
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			return EmitterInst->GetBeamSourceTangent(SourceIndex, OutSourcePoint);
		}
	}

	return false;
}

bool UParticleSystemComponent::GetBeamSourceStrength(int32 EmitterIndex, int32 SourceIndex, float& OutSourceStrength) const
{
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			return EmitterInst->GetBeamSourceStrength(SourceIndex, OutSourceStrength);
		}
	}

	return false;
}
bool UParticleSystemComponent::GetBeamTargetPoint(int32 EmitterIndex, int32 TargetIndex, FVector& OutTargetPoint) const
{
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			return EmitterInst->GetBeamTargetPoint(TargetIndex, OutTargetPoint);
		}
	}

	return false;
}

bool UParticleSystemComponent::GetBeamTargetTangent(int32 EmitterIndex, int32 TargetIndex, FVector& OutTangentPoint) const
{
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			return EmitterInst->GetBeamTargetTangent(TargetIndex, OutTangentPoint);
		}
	}

	return false;
}

bool UParticleSystemComponent::GetBeamTargetStrength(int32 EmitterIndex, int32 TargetIndex, float& OutTargetStrength) const
{
	if ((EmitterIndex >= 0) && (EmitterIndex < EmitterInstances.Num()))
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			return EmitterInst->GetBeamTargetStrength(TargetIndex, OutTargetStrength);
		}
	}

	return false;
}


void UParticleSystemComponent::SetEmitterEnable(FName EmitterName, bool bNewEnableState)
{
	ForceAsyncWorkCompletion(STALL);
	for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); ++EmitterIndex)
	{
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst && EmitterInst->SpriteTemplate)
		{
			if (EmitterInst->SpriteTemplate->EmitterName == EmitterName)
			{
				EmitterInst->SetHaltSpawningExternal(!bNewEnableState);
			}
		}
	}
}

int32 UParticleSystemComponent::DetermineLODLevelForLocation(const FVector& EffectLocation)
{
	// No particle system, ignore
	if (Template == NULL)
	{
		return 0;
	}

	// Don't bother if we only have 1 LOD level... Or if we want to ignore distance comparisons.
	if (Template->LODDistances.Num() <= 1 || Template->LODMethod == PARTICLESYSTEMLODMETHOD_DirectSet)
	{
		return 0;
	}

	check(IsInGameThread());
	int32 Retval = 0;
	
	// Run this for all local player controllers.
	// If several are found (split screen?). Take the closest for highest LOD.
	UWorld* World = GetWorld();
	if(World != NULL)
	{
		TArray<FVector,TInlineAllocator<8> > PlayerViewLocations;
		if (World->GetPlayerControllerIterator())
		{
			for( FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator )
			{
				APlayerController* PlayerController = Iterator->Get();
				if(PlayerController->IsLocalPlayerController())
				{
					FVector* POVLoc = new(PlayerViewLocations) FVector;
					FRotator POVRotation;
					PlayerController->GetPlayerViewPoint(*POVLoc, POVRotation);
				}
			}
		}
		else
		{
			PlayerViewLocations.Append(World->ViewLocationsRenderedLastFrame);
		}

		// This will now put everything in LODLevel 0 (high detail) by default
		float LODDistanceSqr = (PlayerViewLocations.Num() ? FMath::Square(WORLD_MAX) : 0.0f);
		for (const FVector& ViewLocation : PlayerViewLocations)
		{
			const float DistanceToEffectSqr = FVector(ViewLocation - EffectLocation).SizeSquared();
			if (DistanceToEffectSqr < LODDistanceSqr)
			{
				LODDistanceSqr = DistanceToEffectSqr;
			}
		}

		// Find appropriate LOD based on distance
		Retval = Template->LODDistances.Num() - 1;
		for (int32 LODIdx = 1; LODIdx < Template->LODDistances.Num(); ++LODIdx)
		{
			if (LODDistanceSqr < FMath::Square(Template->LODDistances[LODIdx]))
			{
				Retval = LODIdx - 1;
				break;
			}
		}
	}

	return Retval;
}

void UParticleSystemComponent::SetLODLevel(int32 InLODLevel)
{
	ForceAsyncWorkCompletion(STALL);
	if (Template == NULL)
	{
		return;
	}

	if (Template->LODDistances.Num() == 0)
	{
		return;
	}

	int32 NewLODLevel = FMath::Clamp(InLODLevel + GParticleLODBias, 0, Template->GetLODLevelCount() - 1);
	if (LODLevel != NewLODLevel)
	{
		MarkRenderStateDirty();

		const int32 OldLODLevel = LODLevel;
		LODLevel = NewLODLevel;

		for (int32 i=0; i<EmitterInstances.Num(); i++)
		{
			FParticleEmitterInstance* Instance = EmitterInstances[i];
			if (Instance)
			{
				Instance->SetCurrentLODIndex(LODLevel, true);
			}
		}
	}
}

int32 UParticleSystemComponent::GetLODLevel()
{
	return LODLevel;
}

/** 
 *	Set a named float instance parameter on this ParticleSystemComponent. 
 *	Updates the parameter if it already exists, or creates a new entry if not. 
 */
void UParticleSystemComponent::SetFloatParameter(FName Name, float Param)
{
	if(Name == NAME_None)
	{
		return;
	}
	check(IsInGameThread());

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == Name && P.ParamType == PSPT_Scalar)
		{
			P.Scalar = Param;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = Name;
	InstanceParameters[NewParamIndex].ParamType = PSPT_Scalar;
	InstanceParameters[NewParamIndex].Scalar = Param;
}


void UParticleSystemComponent::SetFloatRandParameter(FName ParameterName,float Param,float ParamLow)
{
	if (ParameterName == NAME_None)
	{
		return;
	}
	check(IsInGameThread());

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == ParameterName && P.ParamType == PSPT_ScalarRand)
		{
			P.Scalar = Param;
			P.Scalar_Low = ParamLow;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = ParameterName;
	InstanceParameters[NewParamIndex].ParamType = PSPT_ScalarRand;
	InstanceParameters[NewParamIndex].Scalar = Param;
	InstanceParameters[NewParamIndex].Scalar_Low = ParamLow;
}


void UParticleSystemComponent::SetVectorParameter(FName Name, FVector Param)
{
	if (Name == NAME_None)
	{
		return;
	}
	check(IsInGameThread());

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == Name && P.ParamType == PSPT_Vector)
		{
			P.Vector = Param;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = Name;
	InstanceParameters[NewParamIndex].ParamType = PSPT_Vector;
	InstanceParameters[NewParamIndex].Vector = Param;
}


void UParticleSystemComponent::SetVectorRandParameter(FName ParameterName,const FVector& Param,const FVector& ParamLow)
{
	if (ParameterName == NAME_None)
	{
		return;
	}
	check(IsInGameThread());

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == ParameterName && P.ParamType == PSPT_VectorRand)
		{
			P.Vector = Param;
			P.Vector_Low = ParamLow;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = ParameterName;
	InstanceParameters[NewParamIndex].ParamType = PSPT_VectorRand;
	InstanceParameters[NewParamIndex].Vector = Param;
	InstanceParameters[NewParamIndex].Vector_Low = ParamLow;
}


void UParticleSystemComponent::SetColorParameter(FName Name, FLinearColor Param)
{
	if(Name == NAME_None)
	{
		return;
	}
	check(IsInGameThread());

	FColor NewColor(Param.ToFColor(true));

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == Name && P.ParamType == PSPT_Color)
		{
			P.Color = NewColor;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = Name;
	InstanceParameters[NewParamIndex].ParamType = PSPT_Color;
	InstanceParameters[NewParamIndex].Color = NewColor;
}


void UParticleSystemComponent::SetActorParameter(FName Name, AActor* Param)
{
	if(Name == NAME_None)
	{
		return;
	}
	check(IsInGameThread());

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == Name && P.ParamType == PSPT_Actor)
		{
			P.Actor = Param;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = Name;
	InstanceParameters[NewParamIndex].ParamType = PSPT_Actor;
	InstanceParameters[NewParamIndex].Actor = Param;
}


void UParticleSystemComponent::SetMaterialParameter(FName Name, UMaterialInterface* Param)
{
	if(Name == NAME_None)
	{
		return;
	}
	check(IsInGameThread());

	// First see if an entry for this name already exists
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		FParticleSysParam& P = InstanceParameters[i];
		if (P.Name == Name && P.ParamType == PSPT_Material)
		{
			bIsViewRelevanceDirty = (P.Material != Param) ? true : false;
			P.Material = Param;
			return;
		}
	}

	// We didn't find one, so create a new one.
	int32 NewParamIndex = InstanceParameters.AddZeroed();
	InstanceParameters[NewParamIndex].Name = Name;
	InstanceParameters[NewParamIndex].ParamType = PSPT_Material;
	bIsViewRelevanceDirty = (InstanceParameters[NewParamIndex].Material != Param) ? true : false;
	InstanceParameters[NewParamIndex].Material = Param;
}


bool UParticleSystemComponent::GetFloatParameter(const FName InName,float& OutFloat)
{
	// Always fail if we pass in no name.
	if(InName == NAME_None)
	{
		return false;
	}

	const TArray<struct FParticleSysParam>& UseInstanceParameters = GetAsyncInstanceParameters();
	for (int32 i = 0; i < UseInstanceParameters.Num(); i++)
	{
		const FParticleSysParam& Param = UseInstanceParameters[i];
		if (Param.Name == InName)
		{
			if (Param.ParamType == PSPT_Scalar)
			{
				OutFloat = Param.Scalar;
				return true;
			}
			else if (Param.ParamType == PSPT_ScalarRand)
			{
				// check(IsInGameThread()); this isn't exactly cool to call from multiple threads, but it isn't terrible.
				OutFloat = Param.Scalar + (Param.Scalar_Low - Param.Scalar) * FMath::SRand();
				return true;
			}
		}
	}

	return false;
}


bool UParticleSystemComponent::GetVectorParameter(const FName InName,FVector& OutVector)
{
	// Always fail if we pass in no name.
	if(InName == NAME_None)
	{
		return false;
	}

	const TArray<struct FParticleSysParam>& UseInstanceParameters = GetAsyncInstanceParameters();
	for (int32 i = 0; i < UseInstanceParameters.Num(); i++)
	{
		const FParticleSysParam& Param = UseInstanceParameters[i];
		if (Param.Name == InName)
		{
			if (Param.ParamType == PSPT_Vector)
			{
				OutVector = Param.Vector;
				return true;
			}
			else if (Param.ParamType == PSPT_VectorRand)
			{
				check(IsInGameThread());
				FVector RandValue(FMath::SRand(), FMath::SRand(), FMath::SRand());
				OutVector = Param.Vector + (Param.Vector_Low - Param.Vector) * RandValue;
				return true;
			}
		}
	}

	return false;
}

bool UParticleSystemComponent::GetAnyVectorParameter(const FName InName,FVector& OutVector)
{
	// Always fail if we pass in no name.
	if(InName == NAME_None)
	{
		return false;
	}

	const TArray<struct FParticleSysParam>& UseInstanceParameters = GetAsyncInstanceParameters();
	for (int32 i = 0; i < UseInstanceParameters.Num(); i++)
	{
		const FParticleSysParam& Param = UseInstanceParameters[i];
		if (Param.Name == InName)
		{
			if (Param.ParamType == PSPT_Vector)
			{
				OutVector = Param.Vector;
				return true;
			}
			if (Param.ParamType == PSPT_VectorRand)
			{
				//check(IsInGameThread());
				FVector RandValue(FMath::SRand(), FMath::SRand(), FMath::SRand());
				OutVector = Param.Vector + (Param.Vector_Low - Param.Vector) * RandValue;
				return true;
			}
			if (Param.ParamType == PSPT_Scalar)
			{
				float OutFloat = Param.Scalar;
				OutVector = FVector(OutFloat, OutFloat, OutFloat);
				return true;
			}
			if (Param.ParamType == PSPT_ScalarRand)
			{
				// check(IsInGameThread()); this isn't exactly cool to call from multiple threads, but it isn't terrible.
				float OutFloat = Param.Scalar + (Param.Scalar_Low - Param.Scalar) * FMath::SRand();
				OutVector = FVector(OutFloat, OutFloat, OutFloat);
				return true;
			}
			if (Param.ParamType == PSPT_Color)
			{
				OutVector = FVector(FLinearColor(Param.Color));
				return true;
			}
		}
	}

	return false;
}


bool UParticleSystemComponent::GetColorParameter(const FName InName,FLinearColor& OutColor)
{
	// Always fail if we pass in no name.
	if(InName == NAME_None)
	{
		return false;
	}
	const TArray<struct FParticleSysParam>& UseInstanceParameters = GetAsyncInstanceParameters();

	for (int32 i = 0; i < UseInstanceParameters.Num(); i++)
	{
		const FParticleSysParam& Param = UseInstanceParameters[i];
		if (Param.Name == InName && Param.ParamType == PSPT_Color)
		{
			OutColor = FLinearColor(Param.Color);
			return true;
		}
	}

	return false;
}


bool UParticleSystemComponent::GetActorParameter(const FName InName,class AActor*& OutActor)
{
	// Always fail if we pass in no name.
	if (InName == NAME_None)
	{
		return false;
	}

	const TArray<struct FParticleSysParam>& UseInstanceParameters = GetAsyncInstanceParameters();
	for (int32 i = 0; i < UseInstanceParameters.Num(); i++)
	{
		const FParticleSysParam& Param = UseInstanceParameters[i];
		if (Param.Name == InName && Param.ParamType == PSPT_Actor)
		{
			OutActor = Param.Actor;
			return true;
		}
	}

	return false;
}


bool UParticleSystemComponent::GetMaterialParameter(const FName InName,class UMaterialInterface*& OutMaterial)
{
	// Always fail if we pass in no name.
	if (InName == NAME_None)
	{
		return false;
	}

	const TArray<struct FParticleSysParam>& UseInstanceParameters = GetAsyncInstanceParameters();
	for (int32 i = 0; i < UseInstanceParameters.Num(); i++)
	{
		const FParticleSysParam& Param = UseInstanceParameters[i];
		if (Param.Name == InName && Param.ParamType == PSPT_Material)
		{
			OutMaterial = Param.Material;
			return true;
		}
	}

	return false;
}


void UParticleSystemComponent::ClearParameter(FName ParameterName, EParticleSysParamType ParameterType)
{
	check(IsInGameThread());
	for (int32 i = 0; i < InstanceParameters.Num(); i++)
	{
		if (InstanceParameters[i].Name == ParameterName && (ParameterType == PSPT_None || InstanceParameters[i].ParamType == ParameterType))
		{
			InstanceParameters.RemoveAt(i--);
		}
	}
}


void UParticleSystemComponent::AutoPopulateInstanceProperties()
{
	check(IsInGameThread());
	if (Template)
	{
		for (int32 EmitterIndex = 0; EmitterIndex < Template->Emitters.Num(); EmitterIndex++)
		{
			UParticleEmitter* Emitter = Template->Emitters[EmitterIndex];
			Emitter->AutoPopulateInstanceProperties(this);
		}
	}
}

void UParticleSystemComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials ) const
{
	if (Template)
	{
		for (int32 EmitterIdx = 0; EmitterIdx < Template->Emitters.Num(); ++EmitterIdx)
		{
			const UParticleEmitter* Emitter = Template->Emitters[EmitterIdx];
			if (!Emitter)
			{
				continue;
			}

			for (int32 LodIndex = 0; LodIndex < Emitter->LODLevels.Num(); ++LodIndex)
			{
				const UParticleLODLevel* LOD = Emitter->LODLevels[LodIndex];

				// Only process enabled emitters
				if (LOD->bEnabled)
				{
					const UParticleModuleTypeDataMesh* MeshTypeData = Cast<UParticleModuleTypeDataMesh>(LOD->TypeDataModule);

					if (MeshTypeData && MeshTypeData->Mesh)
					{
						const FStaticMeshLODResources& LODModel = MeshTypeData->Mesh->RenderData->LODResources[0];

						// Gather the materials applied to the LOD.
						for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
						{
							UMaterialInterface* Material = NULL;
							
							TArray<FName>& NamedOverrides = LOD->RequiredModule->NamedMaterialOverrides;
							TArray<FNamedEmitterMaterial>& Slots = Template->NamedMaterialSlots;

							if (NamedOverrides.IsValidIndex(SectionIndex))
							{	
								//If we have named material overrides then get it's index into the emitter materials array.	
								for (int32 CheckIdx = 0; CheckIdx < Slots.Num(); ++CheckIdx)
								{
									if (NamedOverrides[SectionIndex] == Slots[CheckIdx].Name)
									{
										//Default to the default material for that slot.
										Material = Slots[CheckIdx].Material;
										if (EmitterMaterials.IsValidIndex(CheckIdx) && nullptr != EmitterMaterials[CheckIdx] )
										{
											//This material has been overridden externally, e.g. from a BP so use that one.
											Material = EmitterMaterials[CheckIdx];
										}

										break;
									}
								}
							}

							// See if there is a mesh material module.
							if (Material == NULL)
							{
								// Walk in reverse order as in the case of multiple modules, only the final result will be applied
								for (int32 ModuleIndex = LOD->Modules.Num()-1; ModuleIndex >= 0; --ModuleIndex)
								{
									UParticleModuleMeshMaterial* MeshMatModule = Cast<UParticleModuleMeshMaterial>(LOD->Modules[ModuleIndex]);
									if (MeshMatModule && MeshMatModule->bEnabled)
									{
										if (SectionIndex < MeshMatModule->MeshMaterials.Num())
										{
											Material = MeshMatModule->MeshMaterials[SectionIndex];
											break;
										}
									}
								}
							}

							// Overriding the material?
							if (Material == NULL && MeshTypeData->bOverrideMaterial == true)
							{
								Material = LOD->RequiredModule->Material;
							}

							// Use the material set on the mesh.
							if (Material == NULL)
							{
								Material = MeshTypeData->Mesh->GetMaterial(LODModel.Sections[SectionIndex].MaterialIndex);
							}

							if (Material)
							{
								OutMaterials.Add(Material);
							}
						}
					}
					else
					{
						UMaterialInterface* Material = NULL;
							
						TArray<FName>& NamedOverrides = LOD->RequiredModule->NamedMaterialOverrides;
						TArray<FNamedEmitterMaterial>& Slots = Template->NamedMaterialSlots;

						if (NamedOverrides.Num() > 0)
						{
							for (int32 CheckIdx = 0; CheckIdx < Slots.Num(); ++CheckIdx)
							{
								if (NamedOverrides[0] == Slots[CheckIdx].Name)
								{
									//Default to the default material for that slot.
									Material = Slots[CheckIdx].Material;
									if (EmitterMaterials.IsValidIndex(CheckIdx) && nullptr != EmitterMaterials[CheckIdx])
									{
										//This material has been overridden externally, e.g. from a BP so use that one.
										Material = EmitterMaterials[CheckIdx];
									}
        
									break;
								}
							}
						}

						if (!Material)
						{
							Material = LOD->RequiredModule->Material;
						}

						OutMaterials.Add(Material);
					}
				}
			}
		}
	}

	OutMaterials.Append(EmitterMaterials);
}

FBodyInstance* UParticleSystemComponent::GetBodyInstance(FName BoneName /*= NAME_None*/, bool bGetWelded /*= true*/) const
{
	return nullptr;
}

void UParticleSystemComponent::ReportEventSpawn(const FName InEventName, const float InEmitterTime, 
	const FVector InLocation, const FVector InVelocity, const TArray<UParticleModuleEventSendToGame*>& InEventData)
{
	FParticleEventSpawnData* SpawnData = new(SpawnEvents)FParticleEventSpawnData;
	SpawnData->Type = EPET_Spawn;
	SpawnData->EventName = InEventName;
	SpawnData->EmitterTime = InEmitterTime;
	SpawnData->Location = InLocation;
	SpawnData->Velocity = InVelocity;
	SpawnData->EventData = InEventData;
}

void UParticleSystemComponent::ReportEventDeath(const FName InEventName, const float InEmitterTime, 
	const FVector InLocation, const FVector InVelocity, const TArray<UParticleModuleEventSendToGame*>& InEventData, const float InParticleTime)
{
	FParticleEventDeathData* DeathData = new(DeathEvents)FParticleEventDeathData;
	DeathData->Type = EPET_Death;
	DeathData->EventName = InEventName;
	DeathData->EmitterTime = InEmitterTime;
	DeathData->Location = InLocation;
	DeathData->Velocity = InVelocity;
	DeathData->EventData = InEventData;
	DeathData->ParticleTime = InParticleTime;
}

void UParticleSystemComponent::ReportEventCollision(const FName InEventName, const float InEmitterTime, 
	const FVector InLocation, const FVector InDirection, const FVector InVelocity, const TArray<UParticleModuleEventSendToGame*>& InEventData,
	const float InParticleTime, const FVector InNormal, const float InTime, const int32 InItem, const FName InBoneName, UPhysicalMaterial* PhysMat)
{
	FParticleEventCollideData* CollideData = new(CollisionEvents)FParticleEventCollideData;
	CollideData->Type = EPET_Collision;
	CollideData->EventName = InEventName;
	CollideData->EmitterTime = InEmitterTime;
	CollideData->Location = InLocation;
	CollideData->Direction = InDirection;
	CollideData->Velocity = InVelocity;
	CollideData->EventData = InEventData;
	CollideData->ParticleTime = InParticleTime;
	CollideData->Normal = InNormal;
	CollideData->Time = InTime;
	CollideData->Item = InItem;
	CollideData->BoneName = InBoneName;
	CollideData->PhysMat = PhysMat;
}

void UParticleSystemComponent::ReportEventBurst(const FName InEventName, const float InEmitterTime, const int32 InParticleCount,
	const FVector InLocation, const TArray<UParticleModuleEventSendToGame*>& InEventData)
{
	FParticleEventBurstData* BurstData = new(BurstEvents)FParticleEventBurstData;
	BurstData->Type = EPET_Burst;
	BurstData->EventName = InEventName;
	BurstData->EmitterTime = InEmitterTime;
	BurstData->ParticleCount = InParticleCount;
	BurstData->Location = InLocation;
	BurstData->EventData = InEventData;
}

void UParticleSystemComponent::GenerateParticleEvent(const FName InEventName, const float InEmitterTime,
	const FVector InLocation, const FVector InDirection, const FVector InVelocity)
{
	FParticleEventKismetData* KismetData = new(KismetEvents)FParticleEventKismetData;
	KismetData->Type = EPET_Blueprint;
	KismetData->EventName = InEventName;
	KismetData->EmitterTime = InEmitterTime;
	KismetData->Location = InLocation;
	KismetData->Velocity = InVelocity;
}

void UParticleSystemComponent::KillParticlesForced()
{
	ForceAsyncWorkCompletion(STALL);
	for (int32 EmitterIndex=0;EmitterIndex<EmitterInstances.Num();EmitterIndex++)
	{
		if (EmitterInstances[EmitterIndex])
		{
			EmitterInstances[EmitterIndex]->KillParticlesForced();
		}
	}
}


void UParticleSystemComponent::ForceUpdateBounds()
{
	ForceAsyncWorkCompletion(STALL);
	FBox BoundingBox;

	BoundingBox.Init();

	const int32 EmitterCount = EmitterInstances.Num();
	for ( int32 EmitterIndex = 0; EmitterIndex < EmitterCount; ++EmitterIndex )
	{
		FParticleEmitterInstance* Instance = EmitterInstances[EmitterIndex];
		if ( Instance )
		{
			Instance->ForceUpdateBoundingBox();
			BoundingBox += Instance->GetBoundingBox();
		}
	}

	// Expand the actual bounding-box slightly so it will be valid longer in the case of expanding particle systems.
	const FVector ExpandAmount = BoundingBox.GetExtent() * 0.1f;
	BoundingBox = FBox(BoundingBox.Min - ExpandAmount, BoundingBox.Max + ExpandAmount);

	// Update our bounds.
	Bounds = BoundingBox;
}


bool UParticleSystemComponent::ShouldComputeLODFromGameThread()
{
	bool bUseGameThread = false;
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && GbEnableGameThreadLODCalculation)
	{
		check(IsInGameThread());

		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = Iterator->Get();
			if (PlayerController->IsLocalPlayerController())
			{
				bUseGameThread = true;
				break;
			}
		}
	}
	return bUseGameThread;
}


UParticleSystemReplay* UParticleSystemComponent::FindReplayClipForIDNumber( const int32 InClipIDNumber )
{
	// @todo: If we ever end up with more than a few clips, consider changing this to a hash
	for( int32 CurClipIndex = 0; CurClipIndex < ReplayClips.Num(); ++CurClipIndex )
	{
		UParticleSystemReplay* CurReplayClip = ReplayClips[ CurClipIndex ];
		if( CurReplayClip != NULL )
		{
			if( CurReplayClip->ClipIDNumber == InClipIDNumber )
			{
				// Found it!  We're done.
				return CurReplayClip;
			}
		}
	}

	// Not found
	return NULL;
}

UMaterialInstanceDynamic* UParticleSystemComponent::CreateNamedDynamicMaterialInstance(FName Name, class UMaterialInterface* SourceMaterial)
{
	int32 Index = GetNamedMaterialIndex(Name);
	if (INDEX_NONE == Index)
	{
		UE_LOG(LogParticles, Warning, TEXT("CreateNamedDynamicMaterialInstance on %s: This material wasn't found. Check the particle system's named material slots in cascade."), *GetPathName(), *Name.ToString());
		return NULL;
	}

	if (SourceMaterial)
	{
		SetMaterial(Index, SourceMaterial);
	}

	UMaterialInterface* MaterialInstance = GetMaterial(Index);
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MaterialInstance);

	if (MaterialInstance && !MID)
	{
		// Create and set the dynamic material instance.
		MID = UMaterialInstanceDynamic::Create(MaterialInstance, this);
		SetMaterial(Index, MID);
	}
	else if (!MaterialInstance)
	{
		UE_LOG(LogParticles, Warning, TEXT("CreateDynamicMaterialInstance on %s: Material index %d is invalid."), *GetPathName(), Index);
	}

	return MID;
}

UMaterialInterface* UParticleSystemComponent::GetNamedMaterial(FName Name) const
{
	int32 Index = GetNamedMaterialIndex(Name);
	if (INDEX_NONE != Index)
	{
		if (EmitterMaterials.IsValidIndex(Index) && nullptr == EmitterMaterials[Index])
		{
			//Material has been overridden externally
			return EmitterMaterials[Index];
		}
		else
		{
			//This slot hasn't been overridden so just used the default.
			return Template ? Template->NamedMaterialSlots[Index].Material : nullptr;
		}
	}
	//Could not find this named materials slot.
	return nullptr;
}

int32 UParticleSystemComponent::GetNamedMaterialIndex(FName Name) const
{
	if (Template != nullptr)
	{
		TArray<FNamedEmitterMaterial>& Slots = Template->NamedMaterialSlots;
		for (int32 SlotIdx = 0; SlotIdx < Slots.Num(); ++SlotIdx)
		{
			if (Name == Slots[SlotIdx].Name)
			{
				return SlotIdx;
			}
		}
	}
	return INDEX_NONE;
}

UMaterialInstanceDynamic* UParticleSystemComponent::CreateFlexDynamicMaterialInstance(class UMaterialInterface* SourceMaterial)
{
#if WITH_FLEX
	if (!SourceMaterial)
	{
		return NULL;
	}

	for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
	{
		FParticleEmitterInstance* EmitterInstance = EmitterInstances[EmitterIndex];
		if (EmitterInstance && 
			EmitterInstance->SpriteTemplate && 
			EmitterInstance->SpriteTemplate->FlexFluidSurfaceTemplate &&
			EmitterInstance->SpriteTemplate->FlexFluidSurfaceTemplate->Material)
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(SourceMaterial);

			if (!MID)
			{
				// Create and set the dynamic material instance.
				MID = UMaterialInstanceDynamic::Create(SourceMaterial, this);
			}

			if (MID)
			{
				// Make a copy of the FlexFluidSurfaceTemplate
				auto NewFlexFluidSurface = DuplicateObject<UFlexFluidSurface>(EmitterInstance->SpriteTemplate->FlexFluidSurfaceTemplate, this);
				
				// Set the material in the new FlexFluidSurfaceTemplate
				NewFlexFluidSurface->Material = MID;

				// Set the FlexFluidSurfaceTemplate override in this class
				FlexFluidSurfaceOverride = NewFlexFluidSurface;

				// Tell the ParticleEmiterInstance to update its FlexFluidSurfaceComponent
				EmitterInstance->RegisterNewFlexFluidSurfaceComponent(FlexFluidSurfaceOverride);
			}
			else
			{
				UE_LOG(LogParticles, Warning, TEXT("CreateFlexDynamicMaterialInstance on %s: Material is invalid."), *GetPathName());
			}

			return MID;
		}
	}
#endif
	return NULL;
}


UParticleSystemReplay::UParticleSystemReplay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UParticleSystemReplay::Serialize( FArchive& Ar )
{
	LLM_SCOPE(ELLMTag::Particles);

	Super::Serialize( Ar );

	// Serialize clip ID number
	Ar << ClipIDNumber;

	// Serialize our native members
	Ar << Frames;
}

/** FParticleSystemReplayFrame serialization operator */
FArchive& operator<<( FArchive& Ar, FParticleSystemReplayFrame& Obj )
{
	if( Ar.IsLoading() )
	{
		// Zero out the struct if we're loading from disk since we won't be cleared by default
		FMemory::Memzero( &Obj, sizeof( FParticleEmitterReplayFrame ) );
	}

	// Serialize emitter frames
	Ar << Obj.Emitters;

	return Ar;
}



/** FParticleEmitterReplayFrame serialization operator */
FArchive& operator<<( FArchive& Ar, FParticleEmitterReplayFrame& Obj )
{
	if( Ar.IsLoading() )
	{
		// Zero out the struct if we're loading from disk since we won't be cleared by default
		FMemory::Memzero( &Obj, sizeof( FParticleEmitterReplayFrame ) );
	}

	// Emitter type
	Ar << Obj.EmitterType;

	// Original emitter index
	Ar << Obj.OriginalEmitterIndex;

	if( Ar.IsLoading() )
	{
		switch( Obj.EmitterType )
		{
			case DET_Sprite:
				Obj.FrameState = new FDynamicSpriteEmitterReplayData();
				break;

			case DET_Mesh:
				Obj.FrameState = new FDynamicMeshEmitterReplayData();
				break;

			case DET_Beam2:
				Obj.FrameState = new FDynamicBeam2EmitterReplayData();
				break;

			case DET_Ribbon:
				Obj.FrameState = new FDynamicRibbonEmitterReplayData();
				break;

			case DET_AnimTrail:
				Obj.FrameState = new FDynamicTrailsEmitterReplayData();
				break;

			default:
				{
					// @todo: Support other particle types
					Obj.FrameState = NULL;
				}
				break;
		}
	}

	if( Obj.FrameState != NULL )
	{
		// Serialize this emitter frame state
		Obj.FrameState->Serialize( Ar );
	}

	return Ar;
}

AEmitterCameraLensEffectBase::AEmitterCameraLensEffectBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
		.DoNotCreateDefaultSubobject(TEXT("Sprite"))
		.DoNotCreateDefaultSubobject(TEXT("ArrowComponent0"))
	)
{
	InitialLifeSpan = 10.0f;
	BaseFOV = 80.0f;
	bDestroyOnSystemFinish = true;

	// default transform is a 180 yaw to flip the system around to face the camera
	// and 90 units pushed out
	// (we assume it by default that the effect was authored facing down the +X, due to legacy reasons)
	RelativeTransform = FTransform( 
		FRotator(0.f, 180.f, 0.f),
		FVector(90.f, 0.f, 0.f)
		);

	GetParticleSystemComponent()->bOnlyOwnerSee = true;
	GetParticleSystemComponent()->SecondsBeforeInactive = 0.0f;

	// this property is deprecated, give it the sentinel value to indicate it doesn't need to be migrated
	DistFromCamera_DEPRECATED = TNumericLimits<float>::Max();
	bResetWhenRetriggered = false;
}


FTransform AEmitterCameraLensEffectBase::GetAttachedEmitterTransform(AEmitterCameraLensEffectBase const* Emitter, const FVector& CamLoc, const FRotator& CamRot, float CamFOVDeg)
{
	if (Emitter)
	{
		// adjust for FOV
		// base dist uses BaseFOV which is set on the indiv camera lens effect class
		FTransform RelativeTransformAdjustedForFOV = Emitter->RelativeTransform;
		FVector AdjustedRelativeLoc = RelativeTransformAdjustedForFOV.GetLocation();
		AdjustedRelativeLoc.X *= FMath::Tan(Emitter->BaseFOV*0.5f*PI / 180.f) / FMath::Tan(CamFOVDeg*0.5f*PI / 180.f);
		RelativeTransformAdjustedForFOV.SetLocation(AdjustedRelativeLoc);

		FTransform const CameraToWorld(CamRot, CamLoc);

		// RelativeTransform is "effect to camera"
		FTransform const EffectToWorld = RelativeTransformAdjustedForFOV * CameraToWorld;

		return EffectToWorld;
	}

	return FTransform::Identity;
}

void AEmitterCameraLensEffectBase::UpdateLocation(const FVector& CamLoc, const FRotator& CamRot, float CamFOVDeg)
{
	FTransform const EffectToWorld = GetAttachedEmitterTransform(this, CamLoc, CamRot, CamFOVDeg);
	SetActorTransform(EffectToWorld);
}

void AEmitterCameraLensEffectBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BaseCamera != NULL)
	{
		BaseCamera->RemoveCameraLensEffect(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AEmitterCameraLensEffectBase::RegisterCamera(APlayerCameraManager* C)
{
	BaseCamera = C;
}

void AEmitterCameraLensEffectBase::NotifyRetriggered() 
{
	UParticleSystemComponent* const PSC = GetParticleSystemComponent();
	if (PSC && (PSC->bWasDeactivated || bResetWhenRetriggered))
	{
		PSC->Activate(bResetWhenRetriggered);
	}
}

void AEmitterCameraLensEffectBase::PostInitializeComponents()
{
	LLM_SCOPE(ELLMTag::Particles);

	GetParticleSystemComponent()->SetDepthPriorityGroup(SDPG_Foreground);
	Super::PostInitializeComponents();
	ActivateLensEffect();
}

void AEmitterCameraLensEffectBase::PostLoad()
{
	LLM_SCOPE(ELLMTag::Particles);

	Super::PostLoad();

	// using TNumericLimits<float>::Max() as a sentinel value to indicate this deprecated data has been 
	// migrated to the new format
	if (DistFromCamera_DEPRECATED != TNumericLimits<float>::Max())
	{
		// copy old data into the new transform
		FVector Loc = RelativeTransform.GetLocation();
		Loc.X = DistFromCamera_DEPRECATED;
		RelativeTransform.SetLocation(Loc);

		// don't copy again (just in case this gets saved, which is shouldn't)
		DistFromCamera_DEPRECATED = TNumericLimits<float>::Max();
	}
}

void AEmitterCameraLensEffectBase::ActivateLensEffect()
{
	// only play the camera effect on clients
	UWorld const* const World = GetWorld();
	check(World);
	if( !IsNetMode(NM_DedicatedServer) )
	{
		UParticleSystem* PSToActuallySpawn = PS_CameraEffect;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		AGameState* GameState = World->GetGameState<AGameState>();
		if(GameState && !GameState->ShouldShowGore() )
		{
			PSToActuallySpawn = PS_CameraEffectNonExtremeContent_DEPRECATED;
		}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		if( PSToActuallySpawn != NULL )
		{
			SetTemplate( PS_CameraEffect );
		}
	}
}

void AEmitterCameraLensEffectBase::DeactivateLensEffect()
{
	UParticleSystemComponent* const PSC = GetParticleSystemComponent();
	if (PSC)
	{
		PSC->DeactivateSystem();
	}
}

bool AEmitterCameraLensEffectBase::IsLooping() const
{
	if ((PS_CameraEffect != nullptr) && PS_CameraEffect->IsLooping())
	{
		return true;
	}

	if ((PS_CameraEffectNonExtremeContent_DEPRECATED != nullptr) && PS_CameraEffectNonExtremeContent_DEPRECATED->IsLooping())
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void FParticleResetContext::AddTemplate(UParticleSystem* Template)
{
	check(Template);
	SystemsToReset.AddUnique(Template);
}

void FParticleResetContext::AddTemplate(UParticleModule* Module)
{
	check(Module);
	UParticleSystem* Template = Module->GetTypedOuter<UParticleSystem>();
	check(Template);
	SystemsToReset.Add(Template);
}

void FParticleResetContext::AddTemplate(UParticleEmitter* Emitter)
{
	check(Emitter);
	UParticleSystem* Template = Emitter->GetTypedOuter<UParticleSystem>();
	check(Template);
	SystemsToReset.Add(Template);
}

FParticleResetContext::~FParticleResetContext()
{
	for (TObjectIterator<UParticleSystemComponent> PSCIt; PSCIt; ++PSCIt)
	{
		UParticleSystemComponent* PSC = *PSCIt;
		check(PSC);

		if (SystemsToReset.Contains(PSC->Template))
		{
			PSC->ResetNextTick();
		}
	}
}

#undef LOCTEXT_NAMESPACE

