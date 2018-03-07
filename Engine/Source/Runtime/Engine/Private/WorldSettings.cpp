// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameFramework/WorldSettings.h"
#include "Misc/MessageDialog.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineDefines.h"
#include "EngineStats.h"
#include "Engine/World.h"
#include "SceneInterface.h"
#include "GameFramework/DefaultPhysicsVolume.h"
#include "EngineUtils.h"
#include "Engine/AssetUserData.h"
#include "Engine/WorldComposition.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameNetworkManager.h"
#include "AudioDevice.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "Particles/ParticleEventManager.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "ReleaseObjectVersion.h"
#include "SceneManagement.h"

#if WITH_EDITOR
#include "Editor.h"
#endif 

#define LOCTEXT_NAMESPACE "ErrorChecking"

// @todo vreditor urgent: Temporary hack to allow world-to-meters to be set before
// input is polled for motion controller devices each frame.
ENGINE_API float GNewWorldToMetersScale = 0.0f;

AWorldSettings::AWorldSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("Sprite")))
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UClass> DmgType_Environmental_Object;
		FConstructorStatics()
			: DmgType_Environmental_Object(TEXT("/Engine/EngineDamageTypes/DmgTypeBP_Environmental.DmgTypeBP_Environmental_C"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bEnableWorldBoundsChecks = true;
	bEnableNavigationSystem = true;
	bEnableAISystem = true;
	bEnableWorldComposition = false;
	bEnableWorldOriginRebasing = false;
#if WITH_EDITORONLY_DATA	
	bEnableHierarchicalLODSystem = false;

 	FHierarchicalSimplification LODBaseSetup;
	HierarchicalLODSetup.Add(LODBaseSetup);
	NumHLODLevels = HierarchicalLODSetup.Num();
#endif

	KillZ = -HALF_WORLD_MAX1;
	KillZDamageType = ConstructorStatics.DmgType_Environmental_Object.Object;

	WorldToMeters = 100.f;
	MonoCullingDistance = 750.f;

	DefaultPhysicsVolumeClass = ADefaultPhysicsVolume::StaticClass();
	GameNetworkManagerClass = AGameNetworkManager::StaticClass();
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	TimeDilation = 1.0f;
	MatineeTimeDilation = 1.0f;
	DemoPlayTimeDilation = 1.0f;
	PackedLightAndShadowMapTextureSize = 1024;
	bHidden = false;

	DefaultColorScale = FVector(1.0f, 1.0f, 1.0f);
	DefaultMaxDistanceFieldOcclusionDistance = 600;
	GlobalDistanceFieldViewDistance = 20000;
	DynamicIndirectShadowsSelfShadowingIntensity = .8f;
	bPlaceCellsOnlyAlongCameraTracks = false;
	VisibilityCellSize = 200;
	VisibilityAggressiveness = VIS_LeastAggressive;

#if WITH_EDITORONLY_DATA
	bActorLabelEditable = false;
#endif // WITH_EDITORONLY_DATA
}

void AWorldSettings::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	// create the emitter pool
	// we only need to do this for the persistent level's WorldSettings as sublevel actors will have their WorldSettings set to it on association
	if (GetNetMode() != NM_DedicatedServer && IsInPersistentLevel())
	{
		UWorld* World = GetWorld();
		check(World);		

		// only create once - 
		if (World->MyParticleEventManager == NULL && !GEngine->ParticleEventManagerClassPath.IsEmpty())
		{
			UObject* Object = StaticLoadObject(UClass::StaticClass(), NULL, *GEngine->ParticleEventManagerClassPath, NULL, LOAD_NoWarn, NULL);
			if (Object != NULL)
			{
				TSubclassOf<AParticleEventManager> ParticleEventManagerClass = Cast<UClass>(Object);
				if (ParticleEventManagerClass != NULL)
				{
					FActorSpawnParameters SpawnParameters;
					SpawnParameters.Owner = this;
					SpawnParameters.Instigator = Instigator;
					SpawnParameters.ObjectFlags |= RF_Transient;	// We never want to save particle event managers into a map
					World->MyParticleEventManager = World->SpawnActor<AParticleEventManager>(ParticleEventManagerClass, SpawnParameters);
				}
			}
		}
	}
}

void AWorldSettings::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GEngine->IsConsoleBuild())
	{
		GEngine->bUseConsoleInput = true;
	}
}

void AWorldSettings::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	UWorld* World = GetWorld();
	if (FAudioDevice* AudioDevice = World->GetAudioDevice())
	{
		AudioDevice->SetDefaultAudioSettings(World, DefaultReverbSettings, DefaultAmbientZoneSettings);
	}
}

float AWorldSettings::GetGravityZ() const
{
	if (!bWorldGravitySet)
	{
		// try to initialize cached value
		AWorldSettings* const MutableThis = const_cast<AWorldSettings*>(this);
		MutableThis->WorldGravityZ = bGlobalGravitySet ? GlobalGravityZ : UPhysicsSettings::Get()->DefaultGravityZ;	//allows us to override DefaultGravityZ
	}

	return WorldGravityZ;
}

void AWorldSettings::OnRep_WorldGravityZ()
{
	bWorldGravitySet = true;
}

float AWorldSettings::FixupDeltaSeconds(float DeltaSeconds, float RealDeltaSeconds)
{
	// DeltaSeconds is assumed to be fully dilated at this time, so we will dilate the clamp range as well
	float const Dilation = GetEffectiveTimeDilation();
	float const MinFrameTime = MinUndilatedFrameTime * Dilation;
	float const MaxFrameTime = MaxUndilatedFrameTime * Dilation;

	// clamp frame time according to desired limits
	return FMath::Clamp(DeltaSeconds, MinFrameTime, MaxFrameTime);	
}

float AWorldSettings::SetTimeDilation(float NewTimeDilation)
{
	TimeDilation = FMath::Clamp(NewTimeDilation, MinGlobalTimeDilation, MaxGlobalTimeDilation);
	return TimeDilation;
}

void AWorldSettings::NotifyBeginPlay()
{
	UWorld* World = GetWorld();
	if (!World->bBegunPlay)
	{
		for (FActorIterator It(World); It; ++It)
		{
			SCOPE_CYCLE_COUNTER(STAT_ActorBeginPlay);
			It->DispatchBeginPlay();
		}
		World->bBegunPlay = true;
	}
}

void AWorldSettings::NotifyMatchStarted()
{
	UWorld* World = GetWorld();
	World->bMatchStarted = true;
}

void AWorldSettings::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( AWorldSettings, Pauser );
	DOREPLIFETIME( AWorldSettings, TimeDilation );
	DOREPLIFETIME( AWorldSettings, MatineeTimeDilation );
	DOREPLIFETIME( AWorldSettings, WorldGravityZ );
	DOREPLIFETIME( AWorldSettings, bHighPriorityLoading );
}

void AWorldSettings::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);

	if (Ar.UE4Ver() < VER_UE4_ADD_OVERRIDE_GRAVITY_FLAG)
	{
		//before we had override flag we would use GlobalGravityZ != 0
		if(GlobalGravityZ != 0.0f)
		{
			bGlobalGravitySet = true;
		}
	}
#if WITH_EDITOR	
	if (Ar.CustomVer(FReleaseObjectVersion::GUID) < FReleaseObjectVersion::ConvertHLODScreenSize)
	{
		for (FHierarchicalSimplification& Setup : HierarchicalLODSetup)
		{
			const float OldScreenSize = Setup.TransitionScreenSize;

			const float HalfFOV = PI * 0.25f;
			const float ScreenWidth = 1920.0f;
			const float ScreenHeight = 1080.0f;
			const FPerspectiveMatrix ProjMatrix(HalfFOV, ScreenWidth, ScreenHeight, 1.0f);

			const float DummySphereRadius = 16.0f;
			const float ScreenArea = OldScreenSize * (ScreenWidth * ScreenHeight);
			const float ScreenRadius = FMath::Sqrt(ScreenArea / PI);
			const float ScreenDistance = FMath::Max(ScreenWidth / 2.0f * ProjMatrix.M[0][0], ScreenHeight / 2.0f * ProjMatrix.M[1][1]) * DummySphereRadius / ScreenRadius;

			Setup.TransitionScreenSize = ComputeBoundsScreenSize(FVector::ZeroVector, DummySphereRadius, FVector(0.0f, 0.0f, ScreenDistance), ProjMatrix);
		}
	}
#endif
}

void AWorldSettings::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* AWorldSettings::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void AWorldSettings::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

void AWorldSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	for (FHierarchicalSimplification& Entry : HierarchicalLODSetup)
	{
		Entry.ProxySetting.PostLoadDeprecated();
		Entry.MergeSetting.LODSelectionType = EMeshLODSelectionType::CalculateLOD;
	}

	SetIsTemporarilyHiddenInEditor(true);
#endif// WITH_EDITOR
}


#if WITH_EDITOR

void AWorldSettings::CheckForErrors()
{
	Super::CheckForErrors();

	UWorld* World = GetWorld();
	if ( World->GetWorldSettings() != this )
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_DuplicateLevelInfo", "Duplicate level info" ) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::DuplicateLevelInfo));
	}

	int32 NumLightingScenariosEnabled = 0;

	for (int32 LevelIndex = 0; LevelIndex < World->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = World->GetLevels()[LevelIndex];

		if (Level->bIsLightingScenario && Level->bIsVisible)
		{
			NumLightingScenariosEnabled++;
		}
	}

	if( World->NumLightingUnbuiltObjects > 0 && NumLightingScenariosEnabled <= 1 )
	{
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_RebuildLighting", "Maps need lighting rebuilt" ) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::RebuildLighting));
	}
}

bool AWorldSettings::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (InProperty->GetOuter()
			&& InProperty->GetOuter()->GetName() == TEXT("LightmassWorldInfoSettings"))
		{
			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, bGenerateAmbientOcclusionMaterialMask)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, DirectIlluminationOcclusionFraction)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, IndirectIlluminationOcclusionFraction)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, OcclusionExponent)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, FullyOccludedSamplesFraction)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, MaxOcclusionDistance)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, bVisualizeAmbientOcclusion))
			{
				return LightmassSettings.bUseAmbientOcclusion;
			}

			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, VolumetricLightmapDetailCellSize)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, VolumetricLightmapMaximumBrickMemoryMb))
			{
				return LightmassSettings.VolumeLightingMethod == VLM_VolumetricLightmap;
			}

			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, VolumeLightSamplePlacementScale))
			{
				return LightmassSettings.VolumeLightingMethod == VLM_SparseVolumeLightingSamples;
			}

			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, EnvironmentColor))
			{
				return LightmassSettings.EnvironmentIntensity > 0;
			}
		}

		// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
		if (InProperty->GetOuter()
			&& InProperty->GetOuter()->GetName() == TEXT("NVVolumetricLightingProperties"))
		{
			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FNVVolumetricLightingProperties, TemporalFactor)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FNVVolumetricLightingProperties, FilterThreshold))
			{
				return (VolumetricLightingProperties.FilterMode == EFilterMode::TEMPORAL);
			}
		}
		// NVCHANGE_END: Nvidia Volumetric Lighting


	}

	return Super::CanEditChange(InProperty);
}

void AWorldSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FName MemberPropertyName = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName();

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(AWorldSettings, DefaultReverbSettings) || MemberPropertyName == GET_MEMBER_NAME_CHECKED(AWorldSettings, DefaultAmbientZoneSettings))
	{
		UWorld* World = GetWorld();
		if (FAudioDevice* AudioDevice = World->GetAudioDevice())
		{
			AudioDevice->SetDefaultAudioSettings(World, DefaultReverbSettings, DefaultAmbientZoneSettings);
		}
	}
}

void AWorldSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (PropertyThatChanged->GetFName()==GET_MEMBER_NAME_CHECKED(AWorldSettings,bForceNoPrecomputedLighting) && bForceNoPrecomputedLighting)
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("bForceNoPrecomputedLightingIsEnabled", "bForceNoPrecomputedLighting is now enabled, build lighting once to propagate the change (will remove existing precomputed lighting data)."));
		}

		else if (PropertyThatChanged->GetFName()==GET_MEMBER_NAME_CHECKED(AWorldSettings,bEnableWorldComposition))
		{
			if (UWorldComposition::EnableWorldCompositionEvent.IsBound())
			{
				bEnableWorldComposition = UWorldComposition::EnableWorldCompositionEvent.Execute(GetWorld(), bEnableWorldComposition);
			}
			else
			{
				bEnableWorldComposition = false;
			}
		}
	}

	LightmassSettings.NumIndirectLightingBounces = FMath::Clamp(LightmassSettings.NumIndirectLightingBounces, 0, 100);
	LightmassSettings.NumSkyLightingBounces = FMath::Clamp(LightmassSettings.NumSkyLightingBounces, 0, 100);
	LightmassSettings.IndirectLightingSmoothness = FMath::Clamp(LightmassSettings.IndirectLightingSmoothness, .25f, 10.0f);
	LightmassSettings.VolumeLightSamplePlacementScale = FMath::Clamp(LightmassSettings.VolumeLightSamplePlacementScale, .1f, 100.0f);
	LightmassSettings.VolumetricLightmapDetailCellSize = FMath::Clamp(LightmassSettings.VolumetricLightmapDetailCellSize, 1.0f, 10000.0f);
	LightmassSettings.IndirectLightingQuality = FMath::Clamp(LightmassSettings.IndirectLightingQuality, .1f, 100.0f);
	LightmassSettings.StaticLightingLevelScale = FMath::Clamp(LightmassSettings.StaticLightingLevelScale, .001f, 1000.0f);
	LightmassSettings.EmissiveBoost = FMath::Max(LightmassSettings.EmissiveBoost, 0.0f);
	LightmassSettings.DiffuseBoost = FMath::Max(LightmassSettings.DiffuseBoost, 0.0f);
	LightmassSettings.DirectIlluminationOcclusionFraction = FMath::Clamp(LightmassSettings.DirectIlluminationOcclusionFraction, 0.0f, 1.0f);
	LightmassSettings.IndirectIlluminationOcclusionFraction = FMath::Clamp(LightmassSettings.IndirectIlluminationOcclusionFraction, 0.0f, 1.0f);
	LightmassSettings.OcclusionExponent = FMath::Max(LightmassSettings.OcclusionExponent, 0.0f);
	LightmassSettings.FullyOccludedSamplesFraction = FMath::Clamp(LightmassSettings.FullyOccludedSamplesFraction, 0.0f, 1.0f);
	LightmassSettings.MaxOcclusionDistance = FMath::Max(LightmassSettings.MaxOcclusionDistance, 0.0f);
	LightmassSettings.EnvironmentIntensity = FMath::Max(LightmassSettings.EnvironmentIntensity, 0.0f);

	// Ensure texture size is power of two between 512 and 4096.
	PackedLightAndShadowMapTextureSize = FMath::Clamp<uint32>( FMath::RoundUpToPowerOfTwo( PackedLightAndShadowMapTextureSize ), 512, 4096 );

	if (PropertyThatChanged != nullptr && GetWorld() != nullptr && GetWorld()->PersistentLevel->GetWorldSettings() == this)
	{
		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(FHierarchicalSimplification,TransitionScreenSize))
		{
			GEditor->BroadcastHLODTransitionScreenSizeChanged();
		}

		else if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(AWorldSettings,HierarchicalLODSetup))
		{
			GEditor->BroadcastHLODLevelsArrayChanged();
			NumHLODLevels = HierarchicalLODSetup.Num();			
		}
	}

	if (PropertyThatChanged != nullptr && GetWorld() != nullptr && GetWorld()->Scene)
	{
		GetWorld()->Scene->UpdateSceneSettings(this);

		// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
#if WITH_NVVOLUMETRICLIGHTING
		GetWorld()->Scene->UpdateVolumetricLightingSettings(this);
#endif
		// NVCHANGE_END: Nvidia Volumetric Lighting

	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
