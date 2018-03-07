// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsEngine/PhysicsSettings.h"
#include "GameFramework/MovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/Package.h"

UPhysicsSettings::UPhysicsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultGravityZ(-980.f)
	, DefaultTerminalVelocity(4000.f)
	, DefaultFluidFriction(0.3f)
	, SimulateScratchMemorySize(262144)
	, RagdollAggregateThreshold(4)
	, TriangleMeshTriangleMinAreaThreshold(5.0f)
	, bEnableAsyncScene(false)
	, bEnableShapeSharing(false)
	, bEnablePCM(true)
	, bEnableStabilization(false)
	, bWarnMissingLocks(true)
	, bEnable2DPhysics(false)
	, LockedAxis_DEPRECATED(ESettingsLockedAxis::Invalid)
	, BounceThresholdVelocity(200.f)
	, MaxAngularVelocity(3600)	//10 revolutions per second
	, ContactOffsetMultiplier(0.02f)
	, MinContactOffset(2.f)
	, MaxContactOffset(8.f)
	, bSimulateSkeletalMeshOnDedicatedServer(true)
	, DefaultShapeComplexity((ECollisionTraceFlag)-1)
	, bDefaultHasComplexCollision_DEPRECATED(true)
	, bSuppressFaceRemapTable(false)
	, bDisableActiveActors(false)
	, bEnableEnhancedDeterminism(false)
	, MaxPhysicsDeltaTime(1.f / 30.f)
	, bSubstepping(false)
	, bSubsteppingAsync(false)
	, MaxSubstepDeltaTime(1.f / 60.f)
	, MaxSubsteps(6)
	, SyncSceneSmoothingFactor(0.0f)
	, AsyncSceneSmoothingFactor(0.99f)
	, InitialAverageFrameRate(1.f / 60.f)
	, PhysXTreeRebuildRate(10)
{
	SectionName = TEXT("Physics");
}

void UPhysicsSettings::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITOR
	LoadSurfaceType();
#endif

	if (LockedAxis_DEPRECATED == static_cast<ESettingsLockedAxis::Type>(-1))
	{
		LockedAxis_DEPRECATED = ESettingsLockedAxis::Invalid;
	}

	if (LockedAxis_DEPRECATED != ESettingsLockedAxis::Invalid)
	{
		if (LockedAxis_DEPRECATED == ESettingsLockedAxis::None)
		{
			DefaultDegreesOfFreedom = ESettingsDOF::Full3D;
		}
		else if (LockedAxis_DEPRECATED == ESettingsLockedAxis::X)
		{
			DefaultDegreesOfFreedom = ESettingsDOF::YZPlane;
		}
		else if (LockedAxis_DEPRECATED == ESettingsLockedAxis::Y)
		{
			DefaultDegreesOfFreedom = ESettingsDOF::XZPlane;
		}
		else if (LockedAxis_DEPRECATED == ESettingsLockedAxis::Z)
		{
			DefaultDegreesOfFreedom = ESettingsDOF::XYPlane;
		}

		LockedAxis_DEPRECATED = ESettingsLockedAxis::Invalid;
	}

	if(DefaultShapeComplexity == TEnumAsByte<ECollisionTraceFlag>(-1))
	{
		DefaultShapeComplexity = bDefaultHasComplexCollision_DEPRECATED ? CTF_UseSimpleAndComplex : CTF_UseSimpleAsComplex;
	}
}

#if WITH_EDITOR
bool UPhysicsSettings::CanEditChange(const UProperty* Property) const
{
	bool bIsEditable = Super::CanEditChange(Property);
	if(bIsEditable && Property != NULL)
	{
		const FName Name = Property->GetFName();
		if(Name == TEXT("MaxPhysicsDeltaTime") || Name == TEXT("SyncSceneSmoothingFactor") || Name == TEXT("AsyncSceneSmoothingFactor") || Name == TEXT("InitialAverageFrameRate"))
		{
			bIsEditable = !bSubstepping;
		}
	}

	return bIsEditable;
}

void UPhysicsSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UPhysicsSettings, FrictionCombineMode) || PropertyName == GET_MEMBER_NAME_CHECKED(UPhysicsSettings, RestitutionCombineMode))
	{
		UPhysicalMaterial::RebuildPhysicalMaterials();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UPhysicsSettings, DefaultDegreesOfFreedom))
	{
		UMovementComponent::PhysicsLockedAxisSettingChanged();
	}
}

void UPhysicsSettings::LoadSurfaceType()
{
	// read "SurfaceType" defines and set meta data for the enum
	// find the enum
	UEnum * Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPhysicalSurface"), true);
	// we need this Enum
	check(Enum);

	const FString KeyName = TEXT("DisplayName");
	const FString HiddenMeta = TEXT("Hidden");
	const FString UnusedDisplayName = TEXT("Unused");

	// remainders, set to be unused
	for(int32 EnumIndex=1; EnumIndex<Enum->NumEnums(); ++EnumIndex)
	{
		// if meta data isn't set yet, set name to "Unused" until we fix property window to handle this
		// make sure all hide and set unused first
		// if not hidden yet
		if(!Enum->HasMetaData(*HiddenMeta, EnumIndex))
		{
			Enum->SetMetaData(*HiddenMeta, TEXT(""), EnumIndex);
			Enum->SetMetaData(*KeyName, *UnusedDisplayName, EnumIndex);
		}
	}

	for(auto Iter=PhysicalSurfaces.CreateConstIterator(); Iter; ++Iter)
	{
		// @todo only for editor
		Enum->SetMetaData(*KeyName, *Iter->Name.ToString(), Iter->Type);
		// also need to remove "Hidden"
		Enum->RemoveMetaData(*HiddenMeta, Iter->Type);
	}
}
#endif	// WITH_EDITOR
