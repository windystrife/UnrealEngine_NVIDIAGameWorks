// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Movement.cpp: Character movement implementation

=============================================================================*/

#include "GameFramework/CharacterMovementComponent.h"
#include "EngineStats.h"
#include "Components/PrimitiveComponent.h"
#include "AI/Navigation/NavigationSystem.h"
#include "UObject/Package.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PhysicsVolume.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/NetDriver.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/Canvas.h"

// @todo this is here only due to circular dependency to AIModule. To be removed
#include "Navigation/PathFollowingComponent.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "AI/Navigation/AvoidanceManager.h"
#include "Components/BrushComponent.h"

#include "Engine/DemoNetDriver.h"
#include "Engine/NetworkObjectList.h"

#include "Net/PerfCountersHelpers.h"


DEFINE_LOG_CATEGORY_STATIC(LogCharacterMovement, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogNavMeshMovement, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogCharacterNetSmoothing, Log, All);

/**
 * Character stats
 */
DECLARE_CYCLE_STAT(TEXT("Char Tick"), STAT_CharacterMovementTick, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char NonSimulated Time"), STAT_CharacterMovementNonSimulated, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char Simulated Time"), STAT_CharacterMovementSimulated, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char PerformMovement"), STAT_CharacterMovementPerformMovement, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char ReplicateMoveToServer"), STAT_CharacterMovementReplicateMoveToServer, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char CallServerMove"), STAT_CharacterMovementCallServerMove, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char RootMotionSource Calculate"), STAT_CharacterMovementRootMotionSourceCalculate, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char RootMotionSource Apply"), STAT_CharacterMovementRootMotionSourceApply, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char ClientUpdatePositionAfterServerUpdate"), STAT_CharacterMovementClientUpdatePositionAfterServerUpdate, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char CombineNetMove"), STAT_CharacterMovementCombineNetMove, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char NetSmoothCorrection"), STAT_CharacterMovementSmoothCorrection, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char SmoothClientPosition"), STAT_CharacterMovementSmoothClientPosition, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char SmoothClientPosition_Interp"), STAT_CharacterMovementSmoothClientPosition_Interp, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char SmoothClientPosition_Visual"), STAT_CharacterMovementSmoothClientPosition_Visual, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char Physics Interation"), STAT_CharPhysicsInteraction, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char StepUp"), STAT_CharStepUp, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char FindFloor"), STAT_CharFindFloor, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char AdjustFloorHeight"), STAT_CharAdjustFloorHeight, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char Update Acceleration"), STAT_CharUpdateAcceleration, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char MoveUpdateDelegate"), STAT_CharMoveUpdateDelegate, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char PhysWalking"), STAT_CharPhysWalking, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char PhysFalling"), STAT_CharPhysFalling, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char PhysNavWalking"), STAT_CharPhysNavWalking, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char NavProjectPoint"), STAT_CharNavProjectPoint, STATGROUP_Character);
DECLARE_CYCLE_STAT(TEXT("Char NavProjectLocation"), STAT_CharNavProjectLocation, STATGROUP_Character);

// MAGIC NUMBERS
const float MAX_STEP_SIDE_Z = 0.08f;	// maximum z value for the normal on the vertical side of steps
const float SWIMBOBSPEED = -80.f;
const float VERTICAL_SLOPE_NORMAL_Z = 0.001f; // Slope is vertical if Abs(Normal.Z) <= this threshold. Accounts for precision problems that sometimes angle normals slightly off horizontal for vertical surface.

const float UCharacterMovementComponent::MIN_TICK_TIME = 1e-6f;
const float UCharacterMovementComponent::MIN_FLOOR_DIST = 1.9f;
const float UCharacterMovementComponent::MAX_FLOOR_DIST = 2.4f;
const float UCharacterMovementComponent::BRAKE_TO_STOP_VELOCITY = 10.f;
const float UCharacterMovementComponent::SWEEP_EDGE_REJECT_DISTANCE = 0.15f;

// CVars
namespace CharacterMovementCVars
{
	// Listen server smoothing
	static int32 NetEnableListenServerSmoothing = 1;
	FAutoConsoleVariableRef CVarNetEnableListenServerSmoothing(
		TEXT("p.NetEnableListenServerSmoothing"),
		NetEnableListenServerSmoothing,
		TEXT("Whether to enable mesh smoothing on listen servers for the local view of remote clients.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);

	// Logging when character is stuck. Off by default in shipping.
#if UE_BUILD_SHIPPING
	static float StuckWarningPeriod = -1.f;
#else
	static float StuckWarningPeriod = 1.f;
#endif

	FAutoConsoleVariableRef CVarStuckWarningPeriod(
		TEXT("p.CharacterStuckWarningPeriod"),
		StuckWarningPeriod,
		TEXT("How often (in seconds) we are allowed to log a message about being stuck in geometry.\n")
		TEXT("<0: Disable, >=0: Enable and log this often, in seconds."),
		ECVF_Default);

	static int32 NetEnableMoveCombining = 1;
	FAutoConsoleVariableRef CVarNetEnableMoveCombining(
		TEXT("p.NetEnableMoveCombining"),
		NetEnableMoveCombining,
		TEXT("Whether to enable move combining on the client to reduce bandwidth by combining similar moves.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);

	static int32 ReplayUseInterpolation = 1;
	FAutoConsoleVariableRef CVarReplayUseInterpolation(
		TEXT( "p.ReplayUseInterpolation" ),
		ReplayUseInterpolation,
		TEXT( "" ),
		ECVF_Default);

	static int32 FixReplayOverSampling = 1;
	FAutoConsoleVariableRef CVarFixReplayOverSampling(
		TEXT( "p.FixReplayOverSampling" ),
		FixReplayOverSampling,
		TEXT( "If 1, remove invalid replay samples that can occur due to oversampling (sampling at higher rate than physics is being ticked)" ),
		ECVF_Default);

#if !UE_BUILD_SHIPPING

	static int32 NetShowCorrections = 0;
	FAutoConsoleVariableRef CVarNetShowCorrections(
		TEXT("p.NetShowCorrections"),
		NetShowCorrections,
		TEXT("Whether to draw client position corrections (red is incorrect, green is corrected).\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Cheat);

	static float NetCorrectionLifetime = 4.f;
	FAutoConsoleVariableRef CVarNetCorrectionLifetime(
		TEXT("p.NetCorrectionLifetime"),
		NetCorrectionLifetime,
		TEXT("How long a visualized network correction persists.\n")
		TEXT("Time in seconds each visualized network correction persists."),
		ECVF_Cheat);

#endif // !UI_BUILD_SHIPPING


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	static float NetForceClientAdjustmentPercent = 0.f;
	FAutoConsoleVariableRef CVarNetForceClientAdjustmentPercent(
		TEXT("p.NetForceClientAdjustmentPercent"),
		NetForceClientAdjustmentPercent,
		TEXT("Percent of ServerCheckClientError checks to return true regardless of actual error.\n")
		TEXT("Useful for testing client correction code.\n")
		TEXT("<=0: Disable, 0.05: 5% of checks will return failed, 1.0: Always send client adjustments"),
		ECVF_Cheat);

	static int32 VisualizeMovement = 0;
	FAutoConsoleVariableRef CVarVisualizeMovement(
		TEXT("p.VisualizeMovement"),
		VisualizeMovement,
		TEXT("Whether to draw in-world debug information for character movement.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Cheat);

	static int32 NetVisualizeSimulatedCorrections = 0;
	FAutoConsoleVariableRef CVarNetVisualizeSimulatedCorrections(
		TEXT("p.NetVisualizeSimulatedCorrections"),
		NetVisualizeSimulatedCorrections,
		TEXT("")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Cheat);

	static int32 DebugTimeDiscrepancy = 0;
	FAutoConsoleVariableRef CVarDebugTimeDiscrepancy(
		TEXT("p.DebugTimeDiscrepancy"),
		DebugTimeDiscrepancy,
		TEXT("Whether to log detailed Movement Time Discrepancy values for testing")
		TEXT("0: Disable, 1: Enable Detection logging, 2: Enable Detection and Resolution logging"),
		ECVF_Cheat);
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}


void FFindFloorResult::SetFromSweep(const FHitResult& InHit, const float InSweepFloorDist, const bool bIsWalkableFloor)
{
	bBlockingHit = InHit.IsValidBlockingHit();
	bWalkableFloor = bIsWalkableFloor;
	bLineTrace = false;
	FloorDist = InSweepFloorDist;
	LineDist = 0.f;
	HitResult = InHit;
}

void FFindFloorResult::SetFromLineTrace(const FHitResult& InHit, const float InSweepFloorDist, const float InLineDist, const bool bIsWalkableFloor)
{
	// We require a sweep that hit if we are going to use a line result.
	check(HitResult.bBlockingHit);
	if (HitResult.bBlockingHit && InHit.bBlockingHit)
	{
		// Override most of the sweep result with the line result, but save some values
		FHitResult OldHit(HitResult);
		HitResult = InHit;

		// Restore some of the old values. We want the new normals and hit actor, however.
		HitResult.Time = OldHit.Time;
		HitResult.ImpactPoint = OldHit.ImpactPoint;
		HitResult.Location = OldHit.Location;
		HitResult.TraceStart = OldHit.TraceStart;
		HitResult.TraceEnd = OldHit.TraceEnd;

		bLineTrace = true;
		FloorDist = InSweepFloorDist;
		LineDist = InLineDist;
		bWalkableFloor = bIsWalkableFloor;
	}
}

void FCharacterMovementComponentPostPhysicsTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FActorComponentTickFunction::ExecuteTickHelper(Target, /*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime)
	{
		Target->PostPhysicsTickComponent(DilatedTime, *this);
	});
}

FString FCharacterMovementComponentPostPhysicsTickFunction::DiagnosticMessage()
{
	return Target->GetFullName() + TEXT("[UCharacterMovementComponent::PreClothTick]");
}

UCharacterMovementComponent::UCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PostPhysicsTickFunction.bCanEverTick = true;
	PostPhysicsTickFunction.bStartWithTickEnabled = false;
	PostPhysicsTickFunction.TickGroup = TG_PostPhysics;

	bApplyGravityWhileJumping = true;

	GravityScale = 1.f;
	GroundFriction = 8.0f;
	JumpZVelocity = 420.0f;
	JumpOffJumpZFactor = 0.5f;
	RotationRate = FRotator(0.f, 360.0f, 0.0f);
	SetWalkableFloorZ(0.71f);

	MaxStepHeight = 45.0f;
	PerchRadiusThreshold = 0.0f;
	PerchAdditionalHeight = 40.f;

	MaxFlySpeed = 600.0f;
	MaxWalkSpeed = 600.0f;
	MaxSwimSpeed = 300.0f;
	MaxCustomMovementSpeed = MaxWalkSpeed;
	
	MaxSimulationTimeStep = 0.05f;
	MaxSimulationIterations = 8;

	MaxDepenetrationWithGeometry = 500.f;
	MaxDepenetrationWithGeometryAsProxy = 100.f;
	MaxDepenetrationWithPawn = 100.f;
	MaxDepenetrationWithPawnAsProxy = 2.f;

	// Set to match EVectorQuantization::RoundTwoDecimals
	NetProxyShrinkRadius = 0.01f;
	NetProxyShrinkHalfHeight = 0.01f;

	NetworkSimulatedSmoothLocationTime = 0.100f;
	NetworkSimulatedSmoothRotationTime = 0.033f;
	ListenServerNetworkSimulatedSmoothLocationTime = 0.040f;
	ListenServerNetworkSimulatedSmoothRotationTime = 0.033f;
	NetworkMaxSmoothUpdateDistance = 256.f;
	NetworkNoSmoothUpdateDistance = 384.f;
	NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;

	CrouchedSpeedMultiplier_DEPRECATED = 0.5f;
	MaxWalkSpeedCrouched = MaxWalkSpeed * CrouchedSpeedMultiplier_DEPRECATED;
	MaxOutOfWaterStepHeight = 40.0f;
	OutofWaterZ = 420.0f;
	AirControl = 0.05f;
	AirControlBoostMultiplier = 2.f;
	AirControlBoostVelocityThreshold = 25.f;
	FallingLateralFriction = 0.f;
	MaxAcceleration = 2048.0f;
	BrakingFrictionFactor = 2.0f; // Historical value, 1 would be more appropriate.
	BrakingDecelerationWalking = MaxAcceleration;
	BrakingDecelerationFalling = 0.f;
	BrakingDecelerationFlying = 0.f;
	BrakingDecelerationSwimming = 0.f;
	LedgeCheckThreshold = 4.0f;
	JumpOutOfWaterPitch = 11.25f;
	UpperImpactNormalScale_DEPRECATED = 0.5f;
	
	Mass = 100.0f;
	bJustTeleported = true;
	CrouchedHalfHeight = 40.0f;
	Buoyancy = 1.0f;
	LastUpdateRotation = FQuat::Identity;
	LastUpdateVelocity = FVector::ZeroVector;
	PendingImpulseToApply = FVector::ZeroVector;
	PendingLaunchVelocity = FVector::ZeroVector;
	DefaultWaterMovementMode = MOVE_Swimming;
	DefaultLandMovementMode = MOVE_Walking;
	GroundMovementMode = MOVE_Walking;
	bForceNextFloorCheck = true;
	bForceBraking_DEPRECATED = false;
	bShrinkProxyCapsule = true;
	bCanWalkOffLedges = true;
	bCanWalkOffLedgesWhenCrouching = false;
	bNetworkSmoothingComplete = true; // Initially true until we get a net update, so we don't try to smooth to an uninitialized value.
	bWantsToLeaveNavWalking = false;
	bIsNavWalkingOnServer = false;
	bSweepWhileNavWalking = true;
	bNeedsSweepWhileWalkingUpdate = false;

	bEnablePhysicsInteraction = true;
	StandingDownwardForceScale = 1.0f;
	InitialPushForceFactor = 500.0f;
	PushForceFactor = 750000.0f;
	PushForcePointZOffsetFactor = -0.75f;
	bPushForceUsingZOffset = false;
	bPushForceScaledToMass = false;
	bScalePushForceToVelocity = true;
	
	TouchForceFactor = 1.0f;
	bTouchForceScaledToMass = true;
	MinTouchForce = -1.0f;
	MaxTouchForce = 250.0f;
	RepulsionForce = 2.5f;

	bAllowPhysicsRotationDuringAnimRootMotion = false; // Old default behavior.
	bUseControllerDesiredRotation = false;

	bUseSeparateBrakingFriction = false; // Old default behavior.

	bMaintainHorizontalGroundVelocity = true;
	bImpartBaseVelocityX = true;
	bImpartBaseVelocityY = true;
	bImpartBaseVelocityZ = true;
	bImpartBaseAngularVelocity = true;
	bIgnoreClientMovementErrorChecksAndCorrection = false;
	bAlwaysCheckFloor = true;

	// default character can jump, walk, and swim
	NavAgentProps.bCanJump = true;
	NavAgentProps.bCanWalk = true;
	NavAgentProps.bCanSwim = true;
	ResetMoveState();

	ClientPredictionData = NULL;
	ServerPredictionData = NULL;

	// This should be greater than tolerated player timeout * 2.
	MinTimeBetweenTimeStampResets = 4.f * 60.f; 

	bEnableScopedMovementUpdates = true;

	bRequestedMoveUseAcceleration = true;
	bUseRVOAvoidance = false;
	bUseRVOPostProcess = false;
	AvoidanceLockVelocity = FVector::ZeroVector;
	AvoidanceLockTimer = 0.0f;
	AvoidanceGroup.bGroup0 = true;
	GroupsToAvoid.Packed = 0xFFFFFFFF;
	GroupsToIgnore.Packed = 0;
	AvoidanceConsiderationRadius = 500.0f;

	OldBaseQuat = FQuat::Identity;
	OldBaseLocation = FVector::ZeroVector;

	NavMeshProjectionInterval = 0.1f;
	NavMeshProjectionInterpSpeed = 12.f;
	NavMeshProjectionHeightScaleUp = 0.67f;
	NavMeshProjectionHeightScaleDown = 1.0f;
	NavWalkingFloorDistTolerance = 10.0f;
}

void UCharacterMovementComponent::PostLoad()
{
	Super::PostLoad();

	const int32 LinkerUE4Ver = GetLinkerUE4Version();

	if (LinkerUE4Ver < VER_UE4_CHARACTER_MOVEMENT_DECELERATION)
	{
		BrakingDecelerationWalking = MaxAcceleration;
	}

	if (LinkerUE4Ver < VER_UE4_CHARACTER_BRAKING_REFACTOR)
	{
		// This bool used to apply walking braking in flying and swimming modes.
		if (bForceBraking_DEPRECATED)
		{
			BrakingDecelerationFlying = BrakingDecelerationWalking;
			BrakingDecelerationSwimming = BrakingDecelerationWalking;
		}
	}

	if (LinkerUE4Ver < VER_UE4_CHARACTER_MOVEMENT_WALKABLE_FLOOR_REFACTOR)
	{
		// Compute the walkable floor angle, since we have never done so yet.
		UCharacterMovementComponent::SetWalkableFloorZ(WalkableFloorZ);
	}

	if (LinkerUE4Ver < VER_UE4_DEPRECATED_MOVEMENTCOMPONENT_MODIFIED_SPEEDS)
	{
		MaxWalkSpeedCrouched = MaxWalkSpeed * CrouchedSpeedMultiplier_DEPRECATED;
		MaxCustomMovementSpeed = MaxWalkSpeed;
	}

	CharacterOwner = Cast<ACharacter>(PawnOwner);
}


#if WITH_EDITOR
void UCharacterMovementComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const UProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UCharacterMovementComponent, WalkableFloorAngle))
	{
		// Compute WalkableFloorZ from the Angle.
		SetWalkableFloorAngle(WalkableFloorAngle);
	}
}
#endif // WITH_EDITOR


void UCharacterMovementComponent::OnRegister()
{
	const ENetMode NetMode = GetNetMode();
	if (bUseRVOAvoidance && NetMode == NM_Client)
	{
		bUseRVOAvoidance = false;
	}

	Super::OnRegister();

#if WITH_EDITOR
	// Compute WalkableFloorZ from the WalkableFloorAngle.
	// This is only to respond to changes propagated by PostEditChangeProperty, so it's only done in the editor.
	SetWalkableFloorAngle(WalkableFloorAngle);
#endif

	// Force linear smoothing for replays.
	const UWorld* MyWorld = GetWorld();
	const bool IsReplay = (MyWorld && MyWorld->DemoNetDriver && MyWorld->DemoNetDriver->ServerConnection);
	if (IsReplay)
	{
		if (CharacterMovementCVars::ReplayUseInterpolation == 1)
		{
			NetworkSmoothingMode = ENetworkSmoothingMode::Replay;
		}
		else
		{
			NetworkSmoothingMode = ENetworkSmoothingMode::Linear;
		}
	}
	else if (NetMode == NM_ListenServer)
	{
		// Linear smoothing works on listen servers, but makes a lot less sense under the typical high update rate.
		if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
		{
			NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
		}
	}
}


void UCharacterMovementComponent::BeginDestroy()
{
	if (ClientPredictionData)
	{
		delete ClientPredictionData;
		ClientPredictionData = NULL;
	}
	
	if (ServerPredictionData)
	{
		delete ServerPredictionData;
		ServerPredictionData = NULL;
	}
	
	Super::BeginDestroy();
}

void UCharacterMovementComponent::Deactivate()
{
	bStopMovementAbortPaths = false; // Mirrors StopMovementKeepPathing(), because Super calls StopMovement() and we want that handled differently.
	Super::Deactivate();
	if (!IsActive())
	{
		ClearAccumulatedForces();
		if (CharacterOwner)
		{
			CharacterOwner->ClearJumpInput();
		}
	}
	bStopMovementAbortPaths = true;
}


void UCharacterMovementComponent::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	if (NewUpdatedComponent)
	{
		const ACharacter* NewCharacterOwner = Cast<ACharacter>(NewUpdatedComponent->GetOwner());
		if (NewCharacterOwner == NULL)
		{
			UE_LOG(LogCharacterMovement, Error, TEXT("%s owned by %s must update a component owned by a Character"), *GetName(), *GetNameSafe(NewUpdatedComponent->GetOwner()));
			return;
		}

		// check that UpdatedComponent is a Capsule
		if (Cast<UCapsuleComponent>(NewUpdatedComponent) == NULL)
		{
			UE_LOG(LogCharacterMovement, Error, TEXT("%s owned by %s must update a capsule component"), *GetName(), *GetNameSafe(NewUpdatedComponent->GetOwner()));
			return;
		}
	}

	if ( bMovementInProgress )
	{
		// failsafe to avoid crashes in CharacterMovement. 
		bDeferUpdateMoveComponent = true;
		DeferredUpdatedMoveComponent = NewUpdatedComponent;
		return;
	}
	bDeferUpdateMoveComponent = false;
	DeferredUpdatedMoveComponent = NULL;

	USceneComponent* OldUpdatedComponent = UpdatedComponent;
	UPrimitiveComponent* OldPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
	if (IsValid(OldPrimitive) && OldPrimitive->OnComponentBeginOverlap.IsBound())
	{
		OldPrimitive->OnComponentBeginOverlap.RemoveDynamic(this, &UCharacterMovementComponent::CapsuleTouched);
	}
	
	Super::SetUpdatedComponent(NewUpdatedComponent);
	CharacterOwner = Cast<ACharacter>(PawnOwner);

	if (UpdatedComponent != OldUpdatedComponent)
	{
		ClearAccumulatedForces();
	}

	if (UpdatedComponent == NULL)
	{
		StopActiveMovement();
	}

	const bool bValidUpdatedPrimitive = IsValid(UpdatedPrimitive);

	if (bValidUpdatedPrimitive && bEnablePhysicsInteraction)
	{
		UpdatedPrimitive->OnComponentBeginOverlap.AddUniqueDynamic(this, &UCharacterMovementComponent::CapsuleTouched);
	}

	if (bNeedsSweepWhileWalkingUpdate)
	{
		bSweepWhileNavWalking = bValidUpdatedPrimitive ? UpdatedPrimitive->bGenerateOverlapEvents : false;
		bNeedsSweepWhileWalkingUpdate = false;
	}

	if (bUseRVOAvoidance && IsValid(NewUpdatedComponent))
	{
		UAvoidanceManager* AvoidanceManager = GetWorld()->GetAvoidanceManager();
		if (AvoidanceManager)
		{
			AvoidanceManager->RegisterMovementComponent(this, AvoidanceWeight);
		}
	}
}

bool UCharacterMovementComponent::HasValidData() const
{
	bool bIsValid = UpdatedComponent && IsValid(CharacterOwner);
#if ENABLE_NAN_DIAGNOSTIC
	if (bIsValid)
	{
		// NaN-checking updates
		if (Velocity.ContainsNaN())
		{
			logOrEnsureNanError(TEXT("UCharacterMovementComponent::HasValidData() detected NaN/INF for (%s) in Velocity:\n%s"), *GetPathNameSafe(this), *Velocity.ToString());
			UCharacterMovementComponent* MutableThis = const_cast<UCharacterMovementComponent*>(this);
			MutableThis->Velocity = FVector::ZeroVector;
		}
		if (!UpdatedComponent->GetComponentTransform().IsValid())
		{
			logOrEnsureNanError(TEXT("UCharacterMovementComponent::HasValidData() detected NaN/INF for (%s) in UpdatedComponent->ComponentTransform:\n%s"), *GetPathNameSafe(this), *UpdatedComponent->GetComponentTransform().ToHumanReadableString());
		}
		if (UpdatedComponent->GetComponentRotation().ContainsNaN())
		{
			logOrEnsureNanError(TEXT("UCharacterMovementComponent::HasValidData() detected NaN/INF for (%s) in UpdatedComponent->GetComponentRotation():\n%s"), *GetPathNameSafe(this), *UpdatedComponent->GetComponentRotation().ToString());
		}
	}
#endif
	return bIsValid;
}

FCollisionShape UCharacterMovementComponent::GetPawnCapsuleCollisionShape(const EShrinkCapsuleExtent ShrinkMode, const float CustomShrinkAmount) const
{
	FVector Extent = GetPawnCapsuleExtent(ShrinkMode, CustomShrinkAmount);
	return FCollisionShape::MakeCapsule(Extent);
}

FVector UCharacterMovementComponent::GetPawnCapsuleExtent(const EShrinkCapsuleExtent ShrinkMode, const float CustomShrinkAmount) const
{
	check(CharacterOwner);

	float Radius, HalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
	FVector CapsuleExtent(Radius, Radius, HalfHeight);

	float RadiusEpsilon = 0.f;
	float HeightEpsilon = 0.f;

	switch(ShrinkMode)
	{
	case SHRINK_None:
		return CapsuleExtent;

	case SHRINK_RadiusCustom:
		RadiusEpsilon = CustomShrinkAmount;
		break;

	case SHRINK_HeightCustom:
		HeightEpsilon = CustomShrinkAmount;
		break;
		
	case SHRINK_AllCustom:
		RadiusEpsilon = CustomShrinkAmount;
		HeightEpsilon = CustomShrinkAmount;
		break;

	default:
		UE_LOG(LogCharacterMovement, Warning, TEXT("Unknown EShrinkCapsuleExtent in UCharacterMovementComponent::GetCapsuleExtent"));
		break;
	}

	// Don't shrink to zero extent.
	const float MinExtent = KINDA_SMALL_NUMBER * 10.f;
	CapsuleExtent.X = FMath::Max(CapsuleExtent.X - RadiusEpsilon, MinExtent);
	CapsuleExtent.Y = CapsuleExtent.X;
	CapsuleExtent.Z = FMath::Max(CapsuleExtent.Z - HeightEpsilon, MinExtent);

	return CapsuleExtent;
}


bool UCharacterMovementComponent::DoJump(bool bReplayingMoves)
{
	if ( CharacterOwner && CharacterOwner->CanJump() )
	{
		// Don't jump if we can't move up/down.
		if (!bConstrainToPlane || FMath::Abs(PlaneConstraintNormal.Z) != 1.f)
		{
			Velocity.Z = JumpZVelocity;
			SetMovementMode(MOVE_Falling);
			return true;
		}
	}
	
	return false;
}

FVector UCharacterMovementComponent::GetImpartedMovementBaseVelocity() const
{
	FVector Result = FVector::ZeroVector;
	if (CharacterOwner)
	{
		UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
		if (MovementBaseUtility::IsDynamicBase(MovementBase))
		{
			FVector BaseVelocity = MovementBaseUtility::GetMovementBaseVelocity(MovementBase, CharacterOwner->GetBasedMovement().BoneName);
			
			if (bImpartBaseAngularVelocity)
			{
				const FVector CharacterBasePosition = (UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
				const FVector BaseTangentialVel = MovementBaseUtility::GetMovementBaseTangentialVelocity(MovementBase, CharacterOwner->GetBasedMovement().BoneName, CharacterBasePosition);
				BaseVelocity += BaseTangentialVel;
			}

			if (bImpartBaseVelocityX)
			{
				Result.X = BaseVelocity.X;
			}
			if (bImpartBaseVelocityY)
			{
				Result.Y = BaseVelocity.Y;
			}
			if (bImpartBaseVelocityZ)
			{
				Result.Z = BaseVelocity.Z;
			}
		}
	}
	
	return Result;
}

void UCharacterMovementComponent::Launch(FVector const& LaunchVel)
{
	if ((MovementMode != MOVE_None) && IsActive() && HasValidData())
	{
		PendingLaunchVelocity = LaunchVel;
	}
}

bool UCharacterMovementComponent::HandlePendingLaunch()
{
	if (!PendingLaunchVelocity.IsZero() && HasValidData())
	{
		Velocity = PendingLaunchVelocity;
		SetMovementMode(MOVE_Falling);
		PendingLaunchVelocity = FVector::ZeroVector;
		return true;
	}

	return false;
}

void UCharacterMovementComponent::JumpOff(AActor* MovementBaseActor)
{
	if ( !bPerformingJumpOff )
	{
		bPerformingJumpOff = true;
		if ( CharacterOwner )
		{
			const float MaxSpeed = GetMaxSpeed() * 0.85f;
			Velocity += MaxSpeed * GetBestDirectionOffActor(MovementBaseActor);
			if ( Velocity.Size2D() > MaxSpeed )
			{
				Velocity = MaxSpeed * Velocity.GetSafeNormal();
			}
			Velocity.Z = JumpOffJumpZFactor * JumpZVelocity;
			SetMovementMode(MOVE_Falling);
		}
		bPerformingJumpOff = false;
	}
}

FVector UCharacterMovementComponent::GetBestDirectionOffActor(AActor* BaseActor) const
{
	// By default, just pick a random direction.  Derived character classes can choose to do more complex calculations,
	// such as finding the shortest distance to move in based on the BaseActor's Bounding Volume.
	const float RandAngle = FMath::DegreesToRadians(GetNetworkSafeRandomAngleDegrees());
	return FVector(FMath::Cos(RandAngle), FMath::Sin(RandAngle), 0.5f).GetSafeNormal();
}

float UCharacterMovementComponent::GetNetworkSafeRandomAngleDegrees() const
{
	float Angle = FMath::SRand() * 360.f;

	if (!IsNetMode(NM_Standalone))
	{
		// Networked game
		// Get a timestamp that is relatively close between client and server (within ping).
		FNetworkPredictionData_Server_Character const* ServerData = (HasPredictionData_Server() ? GetPredictionData_Server_Character() : NULL);
		FNetworkPredictionData_Client_Character const* ClientData = (HasPredictionData_Client() ? GetPredictionData_Client_Character() : NULL);

		float TimeStamp = Angle;
		if (ServerData)
		{
			TimeStamp = ServerData->CurrentClientTimeStamp;
		}
		else if (ClientData)
		{
			TimeStamp = ClientData->CurrentTimeStamp;
		}
		
		// Convert to degrees with a faster period.
		const float PeriodMult = 8.0f;
		Angle = TimeStamp * PeriodMult;
		Angle = FMath::Fmod(Angle, 360.f);
	}

	return Angle;
}


void UCharacterMovementComponent::SetDefaultMovementMode()
{
	// check for water volume
	if (CanEverSwim() && IsInWater())
	{
		SetMovementMode(DefaultWaterMovementMode);
	}
	else if ( !CharacterOwner || MovementMode != DefaultLandMovementMode )
	{
		const float SavedVelocityZ = Velocity.Z;
		SetMovementMode(DefaultLandMovementMode);

		// Avoid 1-frame delay if trying to walk but walking fails at this location.
		if (MovementMode == MOVE_Walking && GetMovementBase() == NULL)
		{
			Velocity.Z = SavedVelocityZ; // Prevent temporary walking state from zeroing Z velocity.
			SetMovementMode(MOVE_Falling);
		}
	}
}

void UCharacterMovementComponent::SetGroundMovementMode(EMovementMode NewGroundMovementMode)
{
	// Enforce restriction that it's either Walking or NavWalking.
	if (NewGroundMovementMode != MOVE_Walking && NewGroundMovementMode != MOVE_NavWalking)
	{
		return;
	}

	// Set new value
	GroundMovementMode = NewGroundMovementMode;

	// Possibly change movement modes if already on ground and choosing the other ground mode.
	const bool bOnGround = (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking);
	if (bOnGround && MovementMode != NewGroundMovementMode)
	{
		SetMovementMode(NewGroundMovementMode);
	}
}

void UCharacterMovementComponent::SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode)
{
	if (NewMovementMode != MOVE_Custom)
	{
		NewCustomMode = 0;
	}

	// If trying to use NavWalking but there is no navmesh, use walking instead.
	if (NewMovementMode == MOVE_NavWalking)
	{
		if (GetNavData() == nullptr)
		{
			NewMovementMode = MOVE_Walking;
		}
	}

	// Do nothing if nothing is changing.
	if (MovementMode == NewMovementMode)
	{
		// Allow changes in custom sub-mode.
		if ((NewMovementMode != MOVE_Custom) || (NewCustomMode == CustomMovementMode))
		{
			return;
		}
	}

	const EMovementMode PrevMovementMode = MovementMode;
	const uint8 PrevCustomMode = CustomMovementMode;

	MovementMode = NewMovementMode;
	CustomMovementMode = NewCustomMode;

	// We allow setting movement mode before we have a component to update, in case this happens at startup.
	if (!HasValidData())
	{
		return;
	}
	
	// Handle change in movement mode
	OnMovementModeChanged(PrevMovementMode, PrevCustomMode);

	// @todo UE4 do we need to disable ragdoll physics here? Should this function do nothing if in ragdoll?
}


void UCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (!HasValidData())
	{
		return;
	}

	// Update collision settings if needed
	if (MovementMode == MOVE_NavWalking)
	{
		SetNavWalkingPhysics(true);
		GroundMovementMode = MovementMode;
		// Walking uses only XY velocity
		Velocity.Z = 0.f;
	}
	else if (PreviousMovementMode == MOVE_NavWalking)
	{
		if (MovementMode == DefaultLandMovementMode || IsWalking())
		{
			const bool bSucceeded = TryToLeaveNavWalking();
			if (!bSucceeded)
			{
				return;
			}
		}
		else
		{
			SetNavWalkingPhysics(false);
		}
	}

	// React to changes in the movement mode.
	if (MovementMode == MOVE_Walking)
	{
		// Walking uses only XY velocity, and must be on a walkable floor, with a Base.
		Velocity.Z = 0.f;
		bCrouchMaintainsBaseLocation = true;
		GroundMovementMode = MovementMode;

		// make sure we update our new floor/base on initial entry of the walking physics
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
		AdjustFloorHeight();
		SetBaseFromFloor(CurrentFloor);
	}
	else
	{
		CurrentFloor.Clear();
		bCrouchMaintainsBaseLocation = false;

		if (MovementMode == MOVE_Falling)
		{
			Velocity += GetImpartedMovementBaseVelocity();
			CharacterOwner->Falling();
		}

		SetBase(NULL);

		if (MovementMode == MOVE_None)
		{
			// Kill velocity and clear queued up events
			StopMovementKeepPathing();
			CharacterOwner->ClearJumpInput();
			ClearAccumulatedForces();
		}
	}

	if (MovementMode == MOVE_Falling && PreviousMovementMode != MOVE_Falling && PathFollowingComp.IsValid())
	{
		PathFollowingComp->OnStartedFalling();
	}

	CharacterOwner->OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	ensure(GroundMovementMode == MOVE_Walking || GroundMovementMode == MOVE_NavWalking);
};


namespace PackedMovementModeConstants
{
	const uint32 GroundShift = FMath::CeilLogTwo(MOVE_MAX);
	const uint8 CustomModeThr = 2 * (1 << GroundShift);
	const uint8 GroundMask = (1 << GroundShift) - 1;
}

uint8 UCharacterMovementComponent::PackNetworkMovementMode() const
{
	if (MovementMode != MOVE_Custom)
	{
		ensure(GroundMovementMode == MOVE_Walking || GroundMovementMode == MOVE_NavWalking);
		const uint8 GroundModeBit = (GroundMovementMode == MOVE_Walking ? 0 : 1);
		return uint8(MovementMode.GetValue()) | (GroundModeBit << PackedMovementModeConstants::GroundShift);
	}
	else
	{
		return CustomMovementMode + PackedMovementModeConstants::CustomModeThr;
	}
}


void UCharacterMovementComponent::UnpackNetworkMovementMode(const uint8 ReceivedMode, TEnumAsByte<EMovementMode>& OutMode, uint8& OutCustomMode, TEnumAsByte<EMovementMode>& OutGroundMode) const
{
	if (ReceivedMode < PackedMovementModeConstants::CustomModeThr)
	{
		OutMode = TEnumAsByte<EMovementMode>(ReceivedMode & PackedMovementModeConstants::GroundMask);
		OutCustomMode = 0;
		const uint8 GroundModeBit = (ReceivedMode >> PackedMovementModeConstants::GroundShift);
		OutGroundMode = TEnumAsByte<EMovementMode>(GroundModeBit == 0 ? MOVE_Walking : MOVE_NavWalking);
	}
	else
	{
		OutMode = MOVE_Custom;
		OutCustomMode = ReceivedMode - PackedMovementModeConstants::CustomModeThr;
		OutGroundMode = MOVE_Walking;
	}
}


void UCharacterMovementComponent::ApplyNetworkMovementMode(const uint8 ReceivedMode)
{
	TEnumAsByte<EMovementMode> NetMovementMode(MOVE_None);
	TEnumAsByte<EMovementMode> NetGroundMode(MOVE_None);
	uint8 NetCustomMode(0);
	UnpackNetworkMovementMode(ReceivedMode, NetMovementMode, NetCustomMode, NetGroundMode);
	ensure(NetGroundMode == MOVE_Walking || NetGroundMode == MOVE_NavWalking);

	// set additional flag, GroundMovementMode will be overwritten by SetMovementMode to match actual mode on client side
	bIsNavWalkingOnServer = (NetGroundMode == MOVE_NavWalking);

	GroundMovementMode = NetGroundMode;
	SetMovementMode(NetMovementMode, NetCustomMode);
}

void UCharacterMovementComponent::PerformAirControlForPathFollowing(FVector Direction, float ZDiff)
{
	// use air control if low grav or above destination and falling towards it
	if ( CharacterOwner && Velocity.Z < 0.f && (ZDiff < 0.f || GetGravityZ() > 0.9f * GetWorld()->GetDefaultGravityZ()))
	{
		if ( ZDiff < 0.f )
		{
			if ( (Velocity.X == 0.f) && (Velocity.Y == 0.f) )
			{
				Acceleration = FVector::ZeroVector;
			}
			else
			{
				float Dist2D = Direction.Size2D();
				//Direction.Z = 0.f;
				Acceleration = Direction.GetSafeNormal() * GetMaxAcceleration();

				if ( (Dist2D < 0.5f * FMath::Abs(Direction.Z)) && ((Velocity | Direction) > 0.5f*FMath::Square(Dist2D)) )
				{
					Acceleration *= -1.f;
				}

				if ( Dist2D < 1.5f*CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() )
				{
					Velocity.X = 0.f;
					Velocity.Y = 0.f;
					Acceleration = FVector::ZeroVector;
				}
				else if ( (Velocity | Direction) < 0.f )
				{
					float M = FMath::Max(0.f, 0.2f - GetWorld()->DeltaTimeSeconds);
					Velocity.X *= M;
					Velocity.Y *= M;
				}
			}
		}
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static void DrawCircle( const UWorld* InWorld, const FVector& Base, const FVector& X, const FVector& Y, const FColor& Color, float Radius, int32 NumSides, bool bPersistentLines, float LifeTime, uint8 DepthPriority=0, float Thickness=0.0f )
{
	const float	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;

	for ( int32 SideIndex = 0; SideIndex < NumSides; SideIndex++ )
	{
		const FVector Vertex = Base + ( X * FMath::Cos( AngleDelta * ( SideIndex + 1 ) ) + Y * FMath::Sin( AngleDelta * ( SideIndex + 1 ) ) ) * Radius;
		DrawDebugLine( InWorld, LastVertex, Vertex, Color, bPersistentLines, LifeTime, DepthPriority, Thickness );
		LastVertex = Vertex;
	}
}
#endif

void UCharacterMovementComponent::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);

	if (Archive.IsLoading() && Archive.UE4Ver() < VER_UE4_ADDED_SWEEP_WHILE_WALKING_FLAG)
	{
		// We need to update the bSweepWhileNavWalking flag to match the previous behavior.
		// Since UpdatedComponent is transient, we'll have to wait until we're registered.
		bNeedsSweepWhileWalkingUpdate = true;
	}
}

void UCharacterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	SCOPED_NAMED_EVENT(UCharacterMovementComponent_TickComponent, FColor::Yellow);
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovement);
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementTick);

	const FVector InputVector = ConsumeInputVector();
	if (!HasValidData() || ShouldSkipUpdate(DeltaTime))
	{
		return;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Super tick may destroy/invalidate CharacterOwner or UpdatedComponent, so we need to re-check.
	if (!HasValidData())
	{
		return;
	}

	// See if we fell out of the world.
	const bool bIsSimulatingPhysics = UpdatedComponent->IsSimulatingPhysics();
	if (CharacterOwner->Role == ROLE_Authority && (!bCheatFlying || bIsSimulatingPhysics) && !CharacterOwner->CheckStillInWorld())
	{
		return;
	}

	// We don't update if simulating physics (eg ragdolls).
	if (bIsSimulatingPhysics)
	{
		// Update camera to ensure client gets updates even when physics move him far away from point where simulation started
		if (CharacterOwner->Role == ROLE_AutonomousProxy && IsNetMode(NM_Client))
		{
			APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
			APlayerCameraManager* PlayerCameraManager = (PC ? PC->PlayerCameraManager : NULL);
			if (PlayerCameraManager != NULL && PlayerCameraManager->bUseClientSideCameraUpdates)
			{
				PlayerCameraManager->bShouldSendClientSideCameraUpdate = true;
			}
		}

		ClearAccumulatedForces();
		return;
	}

	AvoidanceLockTimer -= DeltaTime;

	if (CharacterOwner->Role > ROLE_SimulatedProxy)
	{
		SCOPE_CYCLE_COUNTER(STAT_CharacterMovementNonSimulated);

		// If we are a client we might have received an update from the server.
		const bool bIsClient = (CharacterOwner->Role == ROLE_AutonomousProxy && IsNetMode(NM_Client));
		if (bIsClient)
		{
			ClientUpdatePositionAfterServerUpdate();
		}

		// Allow root motion to move characters that have no controller.
		if( CharacterOwner->IsLocallyControlled() || (!CharacterOwner->Controller && bRunPhysicsWithNoController) || (!CharacterOwner->Controller && CharacterOwner->IsPlayingRootMotion()) )
		{
			{
				SCOPE_CYCLE_COUNTER(STAT_CharUpdateAcceleration);

				// We need to check the jump state before adjusting input acceleration, to minimize latency
				// and to make sure acceleration respects our potentially new falling state.
				CharacterOwner->CheckJumpInput(DeltaTime);

				// apply input to acceleration
				Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(InputVector));
				AnalogInputModifier = ComputeAnalogInputModifier();
			}

			if (CharacterOwner->Role == ROLE_Authority)
			{
				PerformMovement(DeltaTime);
			}
			else if (bIsClient)
			{
				ReplicateMoveToServer(DeltaTime, Acceleration);
			}
		}
		else if (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy)
		{
			// Server ticking for remote client.
			// Between net updates from the client we need to update position if based on another object,
			// otherwise the object will move on intermediate frames and we won't follow it.
			MaybeUpdateBasedMovement(DeltaTime);
			MaybeSaveBaseLocation();

			// Smooth on listen server for local view of remote clients. We may receive updates at a rate different than our own tick rate.
			if (CharacterMovementCVars::NetEnableListenServerSmoothing && !bNetworkSmoothingComplete && IsNetMode(NM_ListenServer))
			{
				SmoothClientPosition(DeltaTime);
			}
		}
	}
	else if (CharacterOwner->Role == ROLE_SimulatedProxy)
	{
		if (bShrinkProxyCapsule)
		{
			AdjustProxyCapsuleSize();
		}
		SimulatedTick(DeltaTime);
	}

	if (bUseRVOAvoidance)
	{
		UpdateDefaultAvoidance();
	}

	if (bEnablePhysicsInteraction)
	{
		SCOPE_CYCLE_COUNTER(STAT_CharPhysicsInteraction);
		ApplyDownwardForce(DeltaTime);
		ApplyRepulsionForce(DeltaTime);
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bVisualizeMovement = CharacterMovementCVars::VisualizeMovement > 0;
	if (bVisualizeMovement)
	{
		VisualizeMovement();
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

}

void UCharacterMovementComponent::PostPhysicsTickComponent(float DeltaTime, FCharacterMovementComponentPostPhysicsTickFunction& ThisTickFunction)
{
	if (bDeferUpdateBasedMovement)
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);
		UpdateBasedMovement(DeltaTime);
		SaveBaseLocation();
		bDeferUpdateBasedMovement = false;
	}
}

void UCharacterMovementComponent::AdjustProxyCapsuleSize()
{
	if (bShrinkProxyCapsule && CharacterOwner && CharacterOwner->Role == ROLE_SimulatedProxy)
	{
		bShrinkProxyCapsule = false;

		float ShrinkRadius = FMath::Max(0.f, NetProxyShrinkRadius);
		float ShrinkHalfHeight = FMath::Max(0.f, NetProxyShrinkHalfHeight);

		if (ShrinkRadius == 0.f && ShrinkHalfHeight == 0.f)
		{
			return;
		}

		float Radius, HalfHeight;
		CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleSize(Radius, HalfHeight);
		const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();

		if (ComponentScale <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float NewRadius = FMath::Max(0.f, Radius - ShrinkRadius / ComponentScale);
		const float NewHalfHeight = FMath::Max(0.f, HalfHeight - ShrinkHalfHeight / ComponentScale);

		if (NewRadius == 0.f || NewHalfHeight == 0.f)
		{
			UE_LOG(LogCharacterMovement, Warning, TEXT("Invalid attempt to shrink Proxy capsule for %s to zero dimension!"), *CharacterOwner->GetName());
			return;
		}

		UE_LOG(LogCharacterMovement, Verbose, TEXT("Shrinking capsule for %s from (r=%.3f, h=%.3f) to (r=%.3f, h=%.3f)"), *CharacterOwner->GetName(),
			Radius * ComponentScale, HalfHeight * ComponentScale, NewRadius * ComponentScale, NewHalfHeight * ComponentScale);

		CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(NewRadius, NewHalfHeight, true);
	}	
}


void UCharacterMovementComponent::SimulatedTick(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementSimulated);
	checkSlow(CharacterOwner != nullptr);

	if (NetworkSmoothingMode == ENetworkSmoothingMode::Replay)
	{
		const FVector OldLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldVelocity = Velocity;

		// Interpolate between appropriate samples
		{
			SCOPE_CYCLE_COUNTER( STAT_CharacterMovementSmoothClientPosition );
			SmoothClientPosition( DeltaSeconds );
		}

		// Update replicated movement mode
		ApplyNetworkMovementMode( GetCharacterOwner()->GetReplicatedMovementMode() );

		UpdateComponentVelocity();
		bJustTeleported = false;

		CharacterOwner->RootMotionRepMoves.Empty();
		CurrentRootMotion.Clear();
		CharacterOwner->SavedRootMotion.Clear();

		// Note: we do not call the Super implementation, that runs prediction.
		// We do still need to call these though
		OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
		CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);

		LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
		LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
		LastUpdateVelocity = Velocity;

		//TickCharacterPose( DeltaSeconds );
		return;
	}

	// If we are playing a RootMotion AnimMontage.
	if (CharacterOwner->IsPlayingNetworkedRootMotionMontage())
	{
		bWasSimulatingRootMotion = true;
		UE_LOG(LogRootMotion, Verbose, TEXT("UCharacterMovementComponent::SimulatedTick"));

		// Tick animations before physics.
		if( CharacterOwner->GetMesh() )
		{
			TickCharacterPose(DeltaSeconds);

			// Make sure animation didn't trigger an event that destroyed us
			if (!HasValidData())
			{
				return;
			}
		}

		if( RootMotionParams.bHasRootMotion )
		{
			const FQuat OldRotationQuat = UpdatedComponent->GetComponentQuat();
			const FVector OldLocation = UpdatedComponent->GetComponentLocation();
			SimulateRootMotion(DeltaSeconds, RootMotionParams.GetRootMotionTransform());

#if !(UE_BUILD_SHIPPING)
			// debug
			if (false)
			{
				const FRotator OldRotation = OldRotationQuat.Rotator();
				const FRotator NewRotation = UpdatedComponent->GetComponentRotation();
				const FVector NewLocation = UpdatedComponent->GetComponentLocation();
				DrawDebugCoordinateSystem(GetWorld(), CharacterOwner->GetMesh()->GetComponentLocation() + FVector(0,0,1), NewRotation, 50.f, false);
				DrawDebugLine(GetWorld(), OldLocation, NewLocation, FColor::Red, true, 10.f);

				UE_LOG(LogRootMotion, Log,  TEXT("UCharacterMovementComponent::SimulatedTick DeltaMovement Translation: %s, Rotation: %s, MovementBase: %s"),
					*(NewLocation - OldLocation).ToCompactString(), *(NewRotation - OldRotation).GetNormalized().ToCompactString(), *GetNameSafe(CharacterOwner->GetMovementBase()) );
			}
#endif // !(UE_BUILD_SHIPPING)
		}

		// then, once our position is up to date with our animation, 
		// handle position correction if we have any pending updates received from the server.
		if( CharacterOwner && (CharacterOwner->RootMotionRepMoves.Num() > 0) )
		{
			CharacterOwner->SimulatedRootMotionPositionFixup(DeltaSeconds);
		}
	}
	else if (CurrentRootMotion.HasActiveRootMotionSources())
	{
		// We have root motion sources and possibly animated root motion
		bWasSimulatingRootMotion = true;
		UE_LOG(LogRootMotion, Verbose, TEXT("UCharacterMovementComponent::SimulatedTick"));

		// If we have RootMotionRepMoves, find the most recent important one and set position/rotation to it
		bool bCorrectedToServer = false;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FQuat OldRotation = UpdatedComponent->GetComponentQuat();
		if( CharacterOwner->RootMotionRepMoves.Num() > 0 )
		{
			// Move Actor back to position of that buffered move. (server replicated position).
			const FSimulatedRootMotionReplicatedMove& RootMotionRepMove = CharacterOwner->RootMotionRepMoves.Last();
			if( CharacterOwner->RestoreReplicatedMove(RootMotionRepMove) )
			{
				bCorrectedToServer = true;
			}
			Acceleration = RootMotionRepMove.RootMotion.Acceleration;

			CharacterOwner->PostNetReceiveVelocity(RootMotionRepMove.RootMotion.LinearVelocity);
			LastUpdateVelocity = RootMotionRepMove.RootMotion.LinearVelocity;

			// Set root motion states to that of repped in state
			CurrentRootMotion.UpdateStateFrom(RootMotionRepMove.RootMotion.AuthoritativeRootMotion, true);

			// Clear out existing RootMotionRepMoves since we've consumed the most recent
			UE_LOG(LogRootMotion, Log,  TEXT("\tClearing old moves in SimulatedTick (%d)"), CharacterOwner->RootMotionRepMoves.Num());
			CharacterOwner->RootMotionRepMoves.Reset();
		}

		// Perform movement
		PerformMovement(DeltaSeconds);

		// After movement correction, smooth out error in position if any.
		if( bCorrectedToServer )
		{
			SmoothCorrection(OldLocation, OldRotation, UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat());
		}
	}
	// Not playing RootMotion AnimMontage
	else
	{
		// if we were simulating root motion, we've been ignoring regular ReplicatedMovement updates.
		// If we're not simulating root motion anymore, force us to sync our movement properties.
		// (Root Motion could leave Velocity out of sync w/ ReplicatedMovement)
		if( bWasSimulatingRootMotion )
		{
			bWasSimulatingRootMotion = false;
			CharacterOwner->RootMotionRepMoves.Empty();
			CharacterOwner->OnRep_ReplicatedMovement();
			CharacterOwner->OnRep_ReplicatedBasedMovement();
			ApplyNetworkMovementMode(GetCharacterOwner()->GetReplicatedMovementMode());
		}

		// Avoid moving the mesh during movement if SmoothClientPosition will take care of it.
		// TODO: also for root motion stuff above?
		const FScopedPreventAttachedComponentMove PreventMeshMovement(bNetworkSmoothingComplete ? nullptr : CharacterOwner->GetMesh());

		if (CharacterOwner->bReplicateMovement)
		{
			if (CharacterOwner->IsMatineeControlled() || CharacterOwner->IsPlayingRootMotion())
			{
				PerformMovement(DeltaSeconds);
			}
			else
			{
				SimulateMovement(DeltaSeconds);
			}
		}
	}

	// Smooth mesh location after moving the capsule above.
	if (!bNetworkSmoothingComplete)
	{
		SCOPE_CYCLE_COUNTER(STAT_CharacterMovementSmoothClientPosition);
		SmoothClientPosition(DeltaSeconds);
	}
	else
	{
		UE_LOG(LogCharacterNetSmoothing, Verbose, TEXT("Skipping network smoothing for %s."), *GetNameSafe(CharacterOwner));
	}
}

void UCharacterMovementComponent::SimulateRootMotion(float DeltaSeconds, const FTransform& LocalRootMotionTransform)
{
	if( CharacterOwner && CharacterOwner->GetMesh() && (DeltaSeconds > 0.f) )
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

		// Convert Local Space Root Motion to world space. Do it right before used by physics to make sure we use up to date transforms, as translation is relative to rotation.
		const FTransform WorldSpaceRootMotionTransform = CharacterOwner->GetMesh()->ConvertLocalRootMotionToWorld(LocalRootMotionTransform);
		RootMotionParams.Set( WorldSpaceRootMotionTransform );

		// Compute root motion velocity to be used by physics
		AnimRootMotionVelocity = CalcAnimRootMotionVelocity(WorldSpaceRootMotionTransform.GetTranslation(), DeltaSeconds, Velocity);
		Velocity = ConstrainAnimRootMotionVelocity(AnimRootMotionVelocity, Velocity);

		// Update replicated movement mode.
		if (bNetworkMovementModeChanged)
		{
			bNetworkMovementModeChanged = false;
			ApplyNetworkMovementMode(CharacterOwner->GetReplicatedMovementMode());
		}

		StartNewPhysics(DeltaSeconds, 0);
		// fixme laurent - simulate movement seems to have step up issues? investigate as that would be cheaper to use.
		// 		SimulateMovement(DeltaSeconds);

		// Apply Root Motion rotation after movement is complete.
		const FQuat RootMotionRotationQuat = WorldSpaceRootMotionTransform.GetRotation();
		if( !RootMotionRotationQuat.IsIdentity() )
		{
			const FQuat NewActorRotationQuat = RootMotionRotationQuat * UpdatedComponent->GetComponentQuat();
			MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true);
		}
	}

	// Root Motion has been used, clear
	RootMotionParams.Clear();
}


// TODO: Deprecated, remove.
FVector UCharacterMovementComponent::CalcRootMotionVelocity(const FVector& RootMotionDeltaMove, float DeltaSeconds, const FVector& CurrentVelocity) const
{
	return CalcAnimRootMotionVelocity(RootMotionDeltaMove, DeltaSeconds, CurrentVelocity);
}


FVector UCharacterMovementComponent::CalcAnimRootMotionVelocity(const FVector& RootMotionDeltaMove, float DeltaSeconds, const FVector& CurrentVelocity) const
{
	if (ensure(DeltaSeconds > 0.f))
	{
		FVector RootMotionVelocity = RootMotionDeltaMove / DeltaSeconds;
		return RootMotionVelocity;
	}
	else
	{
		return CurrentVelocity;
	}
}


FVector UCharacterMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
	FVector Result = RootMotionVelocity;

	// Do not override Velocity.Z if in falling physics, we want to keep the effect of gravity.
	if (IsFalling())
	{
		Result.Z = CurrentVelocity.Z;
	}

	return Result;
}

void UCharacterMovementComponent::SimulateMovement(float DeltaSeconds)
{
	if (!HasValidData() || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	const bool bIsSimulatedProxy = (CharacterOwner->Role == ROLE_SimulatedProxy);

	// Workaround for replication not being updated initially
	if (bIsSimulatedProxy &&
		CharacterOwner->ReplicatedMovement.Location.IsZero() &&
		CharacterOwner->ReplicatedMovement.Rotation.IsZero() &&
		CharacterOwner->ReplicatedMovement.LinearVelocity.IsZero())
	{
		return;
	}

	// If base is not resolved on the client, we should not try to simulate at all
	if (CharacterOwner->GetReplicatedBasedMovement().IsBaseUnresolved())
	{
		UE_LOG(LogCharacterMovement, Verbose, TEXT("Base for simulated character '%s' is not resolved on client, skipping SimulateMovement"), *CharacterOwner->GetName());
		return;
	}

	FVector OldVelocity;
	FVector OldLocation;

	// Scoped updates can improve performance of multiple MoveComponent calls.
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

		if (bIsSimulatedProxy)
		{
			// Handle network changes
			if (bNetworkUpdateReceived)
			{
				bNetworkUpdateReceived = false;
				if (bNetworkMovementModeChanged)
				{
					bNetworkMovementModeChanged = false;
					ApplyNetworkMovementMode(CharacterOwner->GetReplicatedMovementMode());
				}
				else if (bJustTeleported)
				{
					// Make sure floor is current. We will continue using the replicated base, if there was one.
					bJustTeleported = false;
					UpdateFloorFromAdjustment();
				}
			}
		}

		if (MovementMode == MOVE_None)
		{
			ClearAccumulatedForces();
			return;
		}

		//TODO: Also ApplyAccumulatedForces()?
		HandlePendingLaunch();
		ClearAccumulatedForces();

		Acceleration = Velocity.GetSafeNormal();	// Not currently used for simulated movement
		AnalogInputModifier = 1.0f;				// Not currently used for simulated movement

		MaybeUpdateBasedMovement(DeltaSeconds);

		// simulated pawns predict location
		OldVelocity = Velocity;
		OldLocation = UpdatedComponent->GetComponentLocation();
		FStepDownResult StepDownResult;
		MoveSmooth(Velocity, DeltaSeconds, &StepDownResult);

		// consume path following requested velocity
		bHasRequestedVelocity = false;

		// find floor and check if falling
		if (IsMovingOnGround() || MovementMode == MOVE_Falling)
		{
			const bool bSimGravityDisabled = (CharacterOwner->bSimGravityDisabled && bIsSimulatedProxy);
			if (StepDownResult.bComputedFloor)
			{
				CurrentFloor = StepDownResult.FloorResult;
			}
			else if (Velocity.Z <= 0.f)
			{
				FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, Velocity.IsZero(), NULL);
			}
			else
			{
				CurrentFloor.Clear();
			}

			if (!CurrentFloor.IsWalkableFloor())
			{
				if (!bSimGravityDisabled)
				{
					// No floor, must fall.
					Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaSeconds);
				}
				SetMovementMode(MOVE_Falling);
			}
			else
			{
				// Walkable floor
				if (IsMovingOnGround())
				{
					AdjustFloorHeight();
					SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
				}
				else if (MovementMode == MOVE_Falling)
				{
					if (CurrentFloor.FloorDist <= MIN_FLOOR_DIST || (bSimGravityDisabled && CurrentFloor.FloorDist <= MAX_FLOOR_DIST))
					{
						// Landed
						SetPostLandedPhysics(CurrentFloor.HitResult);
					}
					else
					{
						if (!bSimGravityDisabled)
						{
							// Continue falling.
							Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaSeconds);
						}
						CurrentFloor.Clear();
					}
				}
			}
		}

		OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	} // End scoped movement update

	// Call custom post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);

	MaybeSaveBaseLocation();
	UpdateComponentVelocity();
	bJustTeleported = false;

	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
	LastUpdateVelocity = Velocity;
}

UPrimitiveComponent* UCharacterMovementComponent::GetMovementBase() const
{
	return CharacterOwner ? CharacterOwner->GetMovementBase() : NULL;
}

void UCharacterMovementComponent::SetBase( UPrimitiveComponent* NewBase, FName BoneName, bool bNotifyActor )
{
	// prevent from changing Base while server is NavWalking (no Base in that mode), so both sides are in sync
	// otherwise it will cause problems with position smoothing

	if (CharacterOwner && !bIsNavWalkingOnServer)
	{
		CharacterOwner->SetBase(NewBase, NewBase ? BoneName : NAME_None, bNotifyActor);
	}
}

void UCharacterMovementComponent::SetBaseFromFloor(const FFindFloorResult& FloorResult)
{
	if (FloorResult.IsWalkableFloor())
	{
		SetBase(FloorResult.HitResult.GetComponent(), FloorResult.HitResult.BoneName);
	}
	else
	{
		SetBase(nullptr);
	}
}

void UCharacterMovementComponent::MaybeUpdateBasedMovement(float DeltaSeconds)
{
	bDeferUpdateBasedMovement = false;

	UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
	if (MovementBaseUtility::UseRelativeLocation(MovementBase))
	{
		const bool bBaseIsSimulatingPhysics = MovementBase->IsSimulatingPhysics();
		
		// Temporarily disabling deferred tick on skeletal mesh components that sim physics.
		// We need to be consistent on when we read the bone locations for those, and while this reads
		// the wrong location, the relative changes (which is what we care about) will be accurate.
		const bool bAllowDefer = (bBaseIsSimulatingPhysics && !Cast<USkeletalMeshComponent>(MovementBase));
		
		if (!bBaseIsSimulatingPhysics || !bAllowDefer)
		{
			bDeferUpdateBasedMovement = false;
			UpdateBasedMovement(DeltaSeconds);
			// If previously simulated, go back to using normal tick dependencies.
			if (PostPhysicsTickFunction.IsTickFunctionEnabled())
			{
				PostPhysicsTickFunction.SetTickFunctionEnable(false);
				MovementBaseUtility::AddTickDependency(PrimaryComponentTick, MovementBase);
			}
		}
		else
		{
			// defer movement base update until after physics
			bDeferUpdateBasedMovement = true;
			// If previously not simulating, remove tick dependencies and use post physics tick function.
			if (!PostPhysicsTickFunction.IsTickFunctionEnabled())
			{
				PostPhysicsTickFunction.SetTickFunctionEnable(true);
				MovementBaseUtility::RemoveTickDependency(PrimaryComponentTick, MovementBase);
			}
		}
	}
}

void UCharacterMovementComponent::MaybeSaveBaseLocation()
{
	if (!bDeferUpdateBasedMovement)
	{
		SaveBaseLocation();
	}
}

// @todo UE4 - handle lift moving up and down through encroachment
void UCharacterMovementComponent::UpdateBasedMovement(float DeltaSeconds)
{
	if (!HasValidData())
	{
		return;
	}

	const UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
	if (!MovementBaseUtility::UseRelativeLocation(MovementBase))
	{
		return;
	}

	if (!IsValid(MovementBase) || !IsValid(MovementBase->GetOwner()))
	{
		SetBase(NULL);
		return;
	}

	// Ignore collision with bases during these movements.
	TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, MoveComponentFlags | MOVECOMP_IgnoreBases);

	FQuat DeltaQuat = FQuat::Identity;
	FVector DeltaPosition = FVector::ZeroVector;

	FQuat NewBaseQuat;
	FVector NewBaseLocation;
	if (!MovementBaseUtility::GetMovementBaseTransform(MovementBase, CharacterOwner->GetBasedMovement().BoneName, NewBaseLocation, NewBaseQuat))
	{
		return;
	}

	// Find change in rotation
	const bool bRotationChanged = !OldBaseQuat.Equals(NewBaseQuat, 1e-8f);
	if (bRotationChanged)
	{
		DeltaQuat = NewBaseQuat * OldBaseQuat.Inverse();
	}

	// only if base moved
	if (bRotationChanged || (OldBaseLocation != NewBaseLocation))
	{
		// Calculate new transform matrix of base actor (ignoring scale).
		const FQuatRotationTranslationMatrix OldLocalToWorld(OldBaseQuat, OldBaseLocation);
		const FQuatRotationTranslationMatrix NewLocalToWorld(NewBaseQuat, NewBaseLocation);

		if( CharacterOwner->IsMatineeControlled() )
		{
			FRotationTranslationMatrix HardRelMatrix(CharacterOwner->GetBasedMovement().Rotation, CharacterOwner->GetBasedMovement().Location);
			const FMatrix NewWorldTM = HardRelMatrix * NewLocalToWorld;
			const FQuat NewWorldRot = bIgnoreBaseRotation ? UpdatedComponent->GetComponentQuat() : NewWorldTM.ToQuat();
			MoveUpdatedComponent( NewWorldTM.GetOrigin() - UpdatedComponent->GetComponentLocation(), NewWorldRot, true );
		}
		else
		{
			FQuat FinalQuat = UpdatedComponent->GetComponentQuat();
			
			if (bRotationChanged && !bIgnoreBaseRotation)
			{
				// Apply change in rotation and pipe through FaceRotation to maintain axis restrictions
				const FQuat PawnOldQuat = UpdatedComponent->GetComponentQuat();
				const FQuat TargetQuat = DeltaQuat * FinalQuat;
				FRotator TargetRotator(TargetQuat);
				CharacterOwner->FaceRotation(TargetRotator, 0.f);
				FinalQuat = UpdatedComponent->GetComponentQuat();

				if (PawnOldQuat.Equals(FinalQuat, 1e-6f))
				{
					// Nothing changed. This means we probably are using another rotation mechanism (bOrientToMovement etc). We should still follow the base object.
					// @todo: This assumes only Yaw is used, currently a valid assumption. This is the only reason FaceRotation() is used above really, aside from being a virtual hook.
					if (bOrientRotationToMovement || (bUseControllerDesiredRotation && CharacterOwner->Controller))
					{
						TargetRotator.Pitch = 0.f;
						TargetRotator.Roll = 0.f;
						MoveUpdatedComponent(FVector::ZeroVector, TargetRotator, false);
						FinalQuat = UpdatedComponent->GetComponentQuat();
					}
				}

				// Pipe through ControlRotation, to affect camera.
				if (CharacterOwner->Controller)
				{
					const FQuat PawnDeltaRotation = FinalQuat * PawnOldQuat.Inverse();
					FRotator FinalRotation = FinalQuat.Rotator();
					UpdateBasedRotation(FinalRotation, PawnDeltaRotation.Rotator());
					FinalQuat = UpdatedComponent->GetComponentQuat();
				}
			}

			// We need to offset the base of the character here, not its origin, so offset by half height
			float HalfHeight, Radius;
			CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);

			FVector const BaseOffset(0.0f, 0.0f, HalfHeight);
			FVector const LocalBasePos = OldLocalToWorld.InverseTransformPosition(UpdatedComponent->GetComponentLocation() - BaseOffset);
			FVector const NewWorldPos = ConstrainLocationToPlane(NewLocalToWorld.TransformPosition(LocalBasePos) + BaseOffset);
			DeltaPosition = ConstrainDirectionToPlane(NewWorldPos - UpdatedComponent->GetComponentLocation());

			// move attached actor
			if (bFastAttachedMove)
			{
				// we're trusting no other obstacle can prevent the move here
				UpdatedComponent->SetWorldLocationAndRotation(NewWorldPos, FinalQuat, false);
			}
			else
			{
				// hack - transforms between local and world space introducing slight error FIXMESTEVE - discuss with engine team: just skip the transforms if no rotation?
				FVector BaseMoveDelta = NewBaseLocation - OldBaseLocation;
				if (!bRotationChanged && (BaseMoveDelta.X == 0.f) && (BaseMoveDelta.Y == 0.f))
				{
					DeltaPosition.X = 0.f;
					DeltaPosition.Y = 0.f;
				}

				FHitResult MoveOnBaseHit(1.f);
				const FVector OldLocation = UpdatedComponent->GetComponentLocation();
				MoveUpdatedComponent(DeltaPosition, FinalQuat, true, &MoveOnBaseHit);
				if ((UpdatedComponent->GetComponentLocation() - (OldLocation + DeltaPosition)).IsNearlyZero() == false)
				{
					OnUnableToFollowBaseMove(DeltaPosition, OldLocation, MoveOnBaseHit);
				}
			}
		}

		if (MovementBase->IsSimulatingPhysics() && CharacterOwner->GetMesh())
		{
			CharacterOwner->GetMesh()->ApplyDeltaToAllPhysicsTransforms(DeltaPosition, DeltaQuat);
		}
	}
}


void UCharacterMovementComponent::OnUnableToFollowBaseMove(const FVector& DeltaPosition, const FVector& OldLocation, const FHitResult& MoveOnBaseHit)
{
	// no default implementation, left for subclasses to override.
}


void UCharacterMovementComponent::UpdateBasedRotation(FRotator& FinalRotation, const FRotator& ReducedRotation)
{
	AController* Controller = CharacterOwner ? CharacterOwner->Controller : NULL;
	float ControllerRoll = 0.f;
	if( Controller && !bIgnoreBaseRotation )
	{
		FRotator const ControllerRot = Controller->GetControlRotation();
		ControllerRoll = ControllerRot.Roll;
		Controller->SetControlRotation(ControllerRot + ReducedRotation);
	}

	// Remove roll
	FinalRotation.Roll = 0.f;
	if( Controller )
	{
		FinalRotation.Roll = UpdatedComponent->GetComponentRotation().Roll;
		FRotator NewRotation = Controller->GetControlRotation();
		NewRotation.Roll = ControllerRoll;
		Controller->SetControlRotation(NewRotation);
	}
}

void UCharacterMovementComponent::DisableMovement()
{
	if (CharacterOwner)
	{
		SetMovementMode(MOVE_None);
	}
	else
	{
		MovementMode = MOVE_None;
		CustomMovementMode = 0;
	}
}

void UCharacterMovementComponent::PerformMovement(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementPerformMovement);

	if (!HasValidData())
	{
		return;
	}
	
	// no movement if we can't move, or if currently doing physical simulation on UpdatedComponent
	if (MovementMode == MOVE_None || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
	{
		if (!CharacterOwner->bClientUpdating && CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh() && !CharacterOwner->bServerMoveIgnoreRootMotion)
		{
			// Consume root motion
			TickCharacterPose(DeltaSeconds);
			RootMotionParams.Clear();
			CurrentRootMotion.Clear();
		}
		// Clear pending physics forces
		ClearAccumulatedForces();
		return;
	}

	// Force floor update if we've moved outside of CharacterMovement since last update.
	bForceNextFloorCheck |= (IsMovingOnGround() && UpdatedComponent->GetComponentLocation() != LastUpdateLocation);

	// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened since last update.
	if( CurrentRootMotion.HasAdditiveVelocity() )
	{
		const FVector Adjustment = (Velocity - LastUpdateVelocity);
		CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
		{
			if (!Adjustment.IsNearlyZero())
			{
				FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement HasAdditiveVelocity LastUpdateVelocityAdjustment LastPreAdditiveVelocity(%s) Adjustment(%s)"),
					*CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString(), *Adjustment.ToCompactString());
				RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
			}
		}
#endif
	}

	FVector OldVelocity;
	FVector OldLocation;

	// Scoped updates can improve performance of multiple MoveComponent calls.
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

		MaybeUpdateBasedMovement(DeltaSeconds);

		// Clean up invalid RootMotion Sources.
		// This includes RootMotion sources that ended naturally.
		// They might want to perform a clamp on velocity or an override, 
		// so we want this to happen before ApplyAccumulatedForces and HandlePendingLaunch as to not clobber these.
		const bool bHasRootMotionSources = HasRootMotionSources();
		if (bHasRootMotionSources && !CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
		{
			SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceCalculate);

			const FVector VelocityBeforeCleanup = Velocity;
			CurrentRootMotion.CleanUpInvalidRootMotion(DeltaSeconds, *CharacterOwner, *this);

#if ROOT_MOTION_DEBUG
			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
			{
				if (Velocity != VelocityBeforeCleanup)
				{
					const FVector Adjustment = Velocity - VelocityBeforeCleanup;
					FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement CleanUpInvalidRootMotion Velocity(%s) VelocityBeforeCleanup(%s) Adjustment(%s)"),
						*Velocity.ToCompactString(), *VelocityBeforeCleanup.ToCompactString(), *Adjustment.ToCompactString());
					RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
				}
			}
#endif
		}

		OldVelocity = Velocity;
		OldLocation = UpdatedComponent->GetComponentLocation();

		ApplyAccumulatedForces(DeltaSeconds);

		// Update the character state before we do our movement
		UpdateCharacterStateBeforeMovement();

		if (MovementMode == MOVE_NavWalking && bWantsToLeaveNavWalking)
		{
			TryToLeaveNavWalking();
		}

		// Character::LaunchCharacter() has been deferred until now.
		HandlePendingLaunch();
		ClearAccumulatedForces();

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
		{
			if (OldVelocity != Velocity)
			{
				const FVector Adjustment = Velocity - OldVelocity;
				FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement ApplyAccumulatedForces+HandlePendingLaunch Velocity(%s) OldVelocity(%s) Adjustment(%s)"),
					*Velocity.ToCompactString(), *OldVelocity.ToCompactString(), *Adjustment.ToCompactString());
				RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
			}
		}
#endif

		// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened due to ApplyAccumulatedForces/HandlePendingLaunch
		if( CurrentRootMotion.HasAdditiveVelocity() )
		{
			const FVector Adjustment = (Velocity - OldVelocity);
			CurrentRootMotion.LastPreAdditiveVelocity += Adjustment;

#if ROOT_MOTION_DEBUG
			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
			{
				if (!Adjustment.IsNearlyZero())
				{
					FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement HasAdditiveVelocity AccumulatedForces LastPreAdditiveVelocity(%s) Adjustment(%s)"),
						*CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString(), *Adjustment.ToCompactString());
					RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
				}
			}
#endif
		}

		// Prepare Root Motion (generate/accumulate from root motion sources to be used later)
		if (bHasRootMotionSources && !CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
		{
			// Animation root motion - If using animation RootMotion, tick animations before running physics.
			if( CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh() )
			{
				TickCharacterPose(DeltaSeconds);

				// Make sure animation didn't trigger an event that destroyed us
				if (!HasValidData())
				{
					return;
				}

				// For local human clients, save off root motion data so it can be used by movement networking code.
				if( CharacterOwner->IsLocallyControlled() && (CharacterOwner->Role == ROLE_AutonomousProxy) && CharacterOwner->IsPlayingNetworkedRootMotionMontage() )
				{
					CharacterOwner->ClientRootMotionParams = RootMotionParams;
				}
			}

			// Generates root motion to be used this frame from sources other than animation
			{
				SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceCalculate);
				CurrentRootMotion.PrepareRootMotion(DeltaSeconds, *CharacterOwner, *this, true);
			}

			// For local human clients, save off root motion data so it can be used by movement networking code.
			if( CharacterOwner->IsLocallyControlled() && (CharacterOwner->Role == ROLE_AutonomousProxy) )
			{
				CharacterOwner->SavedRootMotion = CurrentRootMotion;
			}
		}

		// Apply Root Motion to Velocity
		if( CurrentRootMotion.HasOverrideVelocity() || HasAnimRootMotion() )
		{
			// Animation root motion overrides Velocity and currently doesn't allow any other root motion sources
			if( HasAnimRootMotion() )
			{
				// Convert to world space (animation root motion is always local)
				USkeletalMeshComponent * SkelMeshComp = CharacterOwner->GetMesh();
				if( SkelMeshComp )
				{
					// Convert Local Space Root Motion to world space. Do it right before used by physics to make sure we use up to date transforms, as translation is relative to rotation.
					RootMotionParams.Set( SkelMeshComp->ConvertLocalRootMotionToWorld(RootMotionParams.GetRootMotionTransform()) );
				}

				// Then turn root motion to velocity to be used by various physics modes.
				if( DeltaSeconds > 0.f )
				{
					AnimRootMotionVelocity = CalcAnimRootMotionVelocity(RootMotionParams.GetRootMotionTransform().GetTranslation(), DeltaSeconds, Velocity);
					Velocity = ConstrainAnimRootMotionVelocity(AnimRootMotionVelocity, Velocity);
				}
				
				UE_LOG(LogRootMotion, Log,  TEXT("PerformMovement WorldSpaceRootMotion Translation: %s, Rotation: %s, Actor Facing: %s, Velocity: %s")
					, *RootMotionParams.GetRootMotionTransform().GetTranslation().ToCompactString()
					, *RootMotionParams.GetRootMotionTransform().GetRotation().Rotator().ToCompactString()
					, *CharacterOwner->GetActorForwardVector().ToCompactString()
					, *Velocity.ToCompactString()
					);
			}
			else
			{
				// We don't have animation root motion so we apply other sources
				if( DeltaSeconds > 0.f )
				{
					SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceApply);

					const FVector VelocityBeforeOverride = Velocity;
					FVector NewVelocity = Velocity;
					CurrentRootMotion.AccumulateOverrideRootMotionVelocity(DeltaSeconds, *CharacterOwner, *this, NewVelocity);
					Velocity = NewVelocity;

#if ROOT_MOTION_DEBUG
					if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
					{
						if (VelocityBeforeOverride != Velocity)
						{
							FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement AccumulateOverrideRootMotionVelocity Velocity(%s) VelocityBeforeOverride(%s)"),
								*Velocity.ToCompactString(), *VelocityBeforeOverride.ToCompactString());
							RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
						}
					}
#endif
				}
			}
		}

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
		{
			FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement Velocity(%s) OldVelocity(%s)"),
				*Velocity.ToCompactString(), *OldVelocity.ToCompactString());
			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		}
#endif

		// NaN tracking
		checkCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("UCharacterMovementComponent::PerformMovement: Velocity contains NaN (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

		// Clear jump input now, to allow movement events to trigger it for next update.
		CharacterOwner->ClearJumpInput();

		// change position
		StartNewPhysics(DeltaSeconds, 0);

		if (!HasValidData())
		{
			return;
		}

		// Update character state based on change from movement
		UpdateCharacterStateAfterMovement();

		if ((bAllowPhysicsRotationDuringAnimRootMotion || !HasAnimRootMotion()) && !CharacterOwner->IsMatineeControlled())
		{
			PhysicsRotation(DeltaSeconds);
		}

		// Apply Root Motion rotation after movement is complete.
		if( HasAnimRootMotion() )
		{
			const FQuat OldActorRotationQuat = UpdatedComponent->GetComponentQuat();
			const FQuat RootMotionRotationQuat = RootMotionParams.GetRootMotionTransform().GetRotation();
			if( !RootMotionRotationQuat.IsIdentity() )
			{
				const FQuat NewActorRotationQuat = RootMotionRotationQuat * OldActorRotationQuat;
				MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true);
			}

#if !(UE_BUILD_SHIPPING)
			// debug
			if (false)
			{
				const FRotator OldActorRotation = OldActorRotationQuat.Rotator();
				const FVector ResultingLocation = UpdatedComponent->GetComponentLocation();
				const FRotator ResultingRotation = UpdatedComponent->GetComponentRotation();

				// Show current position
				DrawDebugCoordinateSystem(GetWorld(), CharacterOwner->GetMesh()->GetComponentLocation() + FVector(0,0,1), ResultingRotation, 50.f, false);

				// Show resulting delta move.
				DrawDebugLine(GetWorld(), OldLocation, ResultingLocation, FColor::Red, true, 10.f);

				// Log details.
				UE_LOG(LogRootMotion, Warning,  TEXT("PerformMovement Resulting DeltaMove Translation: %s, Rotation: %s, MovementBase: %s"),
					*(ResultingLocation - OldLocation).ToCompactString(), *(ResultingRotation - OldActorRotation).GetNormalized().ToCompactString(), *GetNameSafe(CharacterOwner->GetMovementBase()) );

				const FVector RMTranslation = RootMotionParams.GetRootMotionTransform().GetTranslation();
				const FRotator RMRotation = RootMotionParams.GetRootMotionTransform().GetRotation().Rotator();
				UE_LOG(LogRootMotion, Warning,  TEXT("PerformMovement Resulting DeltaError Translation: %s, Rotation: %s"),
					*(ResultingLocation - OldLocation - RMTranslation).ToCompactString(), *(ResultingRotation - OldActorRotation - RMRotation).GetNormalized().ToCompactString() );
			}
#endif // !(UE_BUILD_SHIPPING)

			// Root Motion has been used, clear
			RootMotionParams.Clear();
		}

		// consume path following requested velocity
		bHasRequestedVelocity = false;

		OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	} // End scoped movement update

	// Call external post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);

	MaybeSaveBaseLocation();
	UpdateComponentVelocity();

	const bool bHasAuthority = CharacterOwner && CharacterOwner->HasAuthority();

	// If we move we want to avoid a long delay before replication catches up to notice this change, especially if it's throttling our rate.
	if (bHasAuthority && UNetDriver::IsAdaptiveNetUpdateFrequencyEnabled() && UpdatedComponent)
	{
		const UWorld* MyWorld = GetWorld();
		if (MyWorld)
		{
			UNetDriver* NetDriver = MyWorld->GetNetDriver();
			if (NetDriver && NetDriver->IsServer())
			{
				FNetworkObjectInfo* NetActor = NetDriver->GetNetworkObjectInfo(CharacterOwner);
				
				if (NetActor && MyWorld->GetTimeSeconds() <= NetActor->NextUpdateTime && NetDriver->IsNetworkActorUpdateFrequencyThrottled(*NetActor))
				{
					if (ShouldCancelAdaptiveReplication())
					{
						NetDriver->CancelAdaptiveReplication(*NetActor);
					}
				}
			}
		}
	}

	const FVector NewLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	const FQuat NewRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;

	if (bHasAuthority && UpdatedComponent && !IsNetMode(NM_Client))
	{
		const bool bLocationChanged = (NewLocation != LastUpdateLocation);
		const bool bRotationChanged = (NewRotation != LastUpdateRotation);
		if (bLocationChanged || bRotationChanged)
		{
			const UWorld* MyWorld = GetWorld();
			ServerLastTransformUpdateTimeStamp = MyWorld ? MyWorld->GetTimeSeconds() : 0.f;
		}
	}

	LastUpdateLocation = NewLocation;
	LastUpdateRotation = NewRotation;
	LastUpdateVelocity = Velocity;
}


bool UCharacterMovementComponent::ShouldCancelAdaptiveReplication() const
{
	// Update sooner if important properties changed.
	const bool bVelocityChanged = (Velocity != LastUpdateVelocity);
	const bool bLocationChanged = (UpdatedComponent->GetComponentLocation() != LastUpdateLocation);
	const bool bRotationChanged = (UpdatedComponent->GetComponentQuat() != LastUpdateRotation);

	return (bVelocityChanged || bLocationChanged || bRotationChanged);
}


void UCharacterMovementComponent::CallMovementUpdateDelegate(float DeltaTime, const FVector& OldLocation, const FVector& OldVelocity)
{
	SCOPE_CYCLE_COUNTER(STAT_CharMoveUpdateDelegate);

	// Update component velocity in case events want to read it
	UpdateComponentVelocity();

	// Delegate (for blueprints)
	if (CharacterOwner)
	{
		CharacterOwner->OnCharacterMovementUpdated.Broadcast(DeltaTime, OldLocation, OldVelocity);
	}
}


void UCharacterMovementComponent::OnMovementUpdated(float DeltaTime, const FVector& OldLocation, const FVector& OldVelocity)
{
	// empty base implementation, intended for derived classes to override.
}


void UCharacterMovementComponent::SaveBaseLocation()
{
	if (!HasValidData())
	{
		return;
	}

	const UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
	if (MovementBaseUtility::UseRelativeLocation(MovementBase) && !CharacterOwner->IsMatineeControlled())
	{
		// Read transforms into OldBaseLocation, OldBaseQuat
		MovementBaseUtility::GetMovementBaseTransform(MovementBase, CharacterOwner->GetBasedMovement().BoneName, OldBaseLocation, OldBaseQuat);

		// Location
		const FVector RelativeLocation = UpdatedComponent->GetComponentLocation() - OldBaseLocation;

		// Rotation
		if (bIgnoreBaseRotation)
		{
			// Absolute rotation
			CharacterOwner->SaveRelativeBasedMovement(RelativeLocation, UpdatedComponent->GetComponentRotation(), false);
		}
		else
		{
			// Relative rotation
			const FRotator RelativeRotation = (FQuatRotationMatrix(UpdatedComponent->GetComponentQuat()) * FQuatRotationMatrix(OldBaseQuat).GetTransposed()).Rotator();
			CharacterOwner->SaveRelativeBasedMovement(RelativeLocation, RelativeRotation, true);
		}
	}
}


bool UCharacterMovementComponent::CanCrouchInCurrentState() const
{
	if (!CanEverCrouch())
	{
		return false;
	}

	return IsFalling() || IsMovingOnGround();
}


void UCharacterMovementComponent::Crouch(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!CanCrouchInCurrentState())
	{
		return;
	}

	// See if collision is already at desired size.
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == CrouchedHalfHeight)
	{
		if (!bClientSimulation)
		{
			CharacterOwner->bIsCrouched = true;
		}
		CharacterOwner->OnStartCrouch( 0.f, 0.f );
		return;
	}

	if (bClientSimulation && CharacterOwner->Role == ROLE_SimulatedProxy)
	{
		// restore collision size before crouching
		ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
		CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		bShrinkProxyCapsule = true;
	}

	// Change collision size to crouching dimensions
	const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float OldUnscaledRadius = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	// Height is not allowed to be smaller than radius.
	const float ClampedCrouchedHalfHeight = FMath::Max3(0.f, OldUnscaledRadius, CrouchedHalfHeight);
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(OldUnscaledRadius, ClampedCrouchedHalfHeight);
	float HalfHeightAdjust = (OldUnscaledHalfHeight - ClampedCrouchedHalfHeight);
	float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	if( !bClientSimulation )
	{
		// Crouching to a larger height? (this is rare)
		if (ClampedCrouchedHalfHeight > OldUnscaledHalfHeight)
		{
			FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CrouchTrace), false, CharacterOwner);
			FCollisionResponseParams ResponseParam;
			InitCollisionParams(CapsuleParams, ResponseParam);
			const bool bEncroached = GetWorld()->OverlapBlockingTestByChannel(UpdatedComponent->GetComponentLocation() - FVector(0.f,0.f,ScaledHalfHeightAdjust), FQuat::Identity,
				UpdatedComponent->GetCollisionObjectType(), GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleParams, ResponseParam);

			// If encroached, cancel
			if( bEncroached )
			{
				CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(OldUnscaledRadius, OldUnscaledHalfHeight);
				return;
			}
		}

		if (bCrouchMaintainsBaseLocation)
		{
			// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
			UpdatedComponent->MoveComponent(FVector(0.f, 0.f, -ScaledHalfHeightAdjust), UpdatedComponent->GetComponentQuat(), true, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
		}

		CharacterOwner->bIsCrouched = true;
	}

	bForceNextFloorCheck = true;

	// OnStartCrouch takes the change from the Default size, not the current one (though they are usually the same).
	const float MeshAdjust = ScaledHalfHeightAdjust;
	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	HalfHeightAdjust = (DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - ClampedCrouchedHalfHeight);
	ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	AdjustProxyCapsuleSize();
	CharacterOwner->OnStartCrouch( HalfHeightAdjust, ScaledHalfHeightAdjust );

	// Don't smooth this change in mesh position
	if (bClientSimulation && CharacterOwner->Role == ROLE_SimulatedProxy)
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData && ClientData->MeshTranslationOffset.Z != 0.f)
		{
			ClientData->MeshTranslationOffset -= FVector(0.f, 0.f, MeshAdjust);
			ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		}
	}
}


void UCharacterMovementComponent::UnCrouch(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();

	// See if collision is already at desired size.
	if( CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() )
	{
		if (!bClientSimulation)
		{
			CharacterOwner->bIsCrouched = false;
		}
		CharacterOwner->OnEndCrouch( 0.f, 0.f );
		return;
	}

	const float CurrentCrouchedHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float HalfHeightAdjust = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - OldUnscaledHalfHeight;
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
	const FVector PawnLocation = UpdatedComponent->GetComponentLocation();

	// Grow to uncrouched size.
	check(CharacterOwner->GetCapsuleComponent());

	if( !bClientSimulation )
	{
		// Try to stay in place and see if the larger capsule fits. We use a slightly taller capsule to avoid penetration.
		const float SweepInflation = KINDA_SMALL_NUMBER * 10.f;
		FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CrouchTrace), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(CapsuleParams, ResponseParam);

		// Compensate for the difference between current capsule size and standing size
		const FCollisionShape StandingCapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_HeightCustom, -SweepInflation - ScaledHalfHeightAdjust); // Shrink by negative amount, so actually grow it.
		const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
		bool bEncroached = true;

		if (!bCrouchMaintainsBaseLocation)
		{
			// Expand in place
			bEncroached = GetWorld()->OverlapBlockingTestByChannel(PawnLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
		
			if (bEncroached)
			{
				// Try adjusting capsule position to see if we can avoid encroachment.
				if (ScaledHalfHeightAdjust > 0.f)
				{
					// Shrink to a short capsule, sweep down to base to find where that would hit something, and then try to stand up from there.
					float PawnRadius, PawnHalfHeight;
					CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
					const float ShrinkHalfHeight = PawnHalfHeight - PawnRadius;
					const float TraceDist = PawnHalfHeight - ShrinkHalfHeight;
					const FVector Down = FVector(0.f, 0.f, -TraceDist);

					FHitResult Hit(1.f);
					const FCollisionShape ShortCapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_HeightCustom, ShrinkHalfHeight);
					const bool bBlockingHit = GetWorld()->SweepSingleByChannel(Hit, PawnLocation, PawnLocation + Down, FQuat::Identity, CollisionChannel, ShortCapsuleShape, CapsuleParams);
					if (Hit.bStartPenetrating)
					{
						bEncroached = true;
					}
					else
					{
						// Compute where the base of the sweep ended up, and see if we can stand there
						const float DistanceToBase = (Hit.Time * TraceDist) + ShortCapsuleShape.Capsule.HalfHeight;
						const FVector NewLoc = FVector(PawnLocation.X, PawnLocation.Y, PawnLocation.Z - DistanceToBase + PawnHalfHeight + SweepInflation + MIN_FLOOR_DIST / 2.f);
						bEncroached = GetWorld()->OverlapBlockingTestByChannel(NewLoc, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
						if (!bEncroached)
						{
							// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
							UpdatedComponent->MoveComponent(NewLoc - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
						}
					}
				}
			}
		}
		else
		{
			// Expand while keeping base location the same.
			FVector StandingLocation = PawnLocation + FVector(0.f, 0.f, StandingCapsuleShape.GetCapsuleHalfHeight() - CurrentCrouchedHalfHeight);
			bEncroached = GetWorld()->OverlapBlockingTestByChannel(StandingLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);

			if (bEncroached)
			{
				if (IsMovingOnGround())
				{
					// Something might be just barely overhead, try moving down closer to the floor to avoid it.
					const float MinFloorDist = KINDA_SMALL_NUMBER * 10.f;
					if (CurrentFloor.bBlockingHit && CurrentFloor.FloorDist > MinFloorDist)
					{
						StandingLocation.Z -= CurrentFloor.FloorDist - MinFloorDist;
						bEncroached = GetWorld()->OverlapBlockingTestByChannel(StandingLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
					}
				}				
			}

			if (!bEncroached)
			{
				// Commit the change in location.
				UpdatedComponent->MoveComponent(StandingLocation - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
				bForceNextFloorCheck = true;
			}
		}

		// If still encroached then abort.
		if (bEncroached)
		{
			return;
		}

		CharacterOwner->bIsCrouched = false;
	}	
	else
	{
		bShrinkProxyCapsule = true;
	}

	// Now call SetCapsuleSize() to cause touch/untouch events and actually grow the capsule
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), true);

	const float MeshAdjust = ScaledHalfHeightAdjust;
	AdjustProxyCapsuleSize();
	CharacterOwner->OnEndCrouch( HalfHeightAdjust, ScaledHalfHeightAdjust );

	// Don't smooth this change in mesh position
	if (bClientSimulation && CharacterOwner->Role == ROLE_SimulatedProxy)
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData && ClientData->MeshTranslationOffset.Z != 0.f)
		{
			ClientData->MeshTranslationOffset += FVector(0.f, 0.f, MeshAdjust);
			ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		}
	}
}

void UCharacterMovementComponent::UpdateCharacterStateBeforeMovement()
{
	// Check for a change in crouch state. Players toggle crouch by changing bWantsToCrouch.
	const bool bAllowedToCrouch = CanCrouchInCurrentState();
	if ((!bAllowedToCrouch || !bWantsToCrouch) && IsCrouching())
	{
		UnCrouch(false);
	}
	else if (bWantsToCrouch && bAllowedToCrouch && !IsCrouching())
	{
		Crouch(false);
	}
}

void UCharacterMovementComponent::UpdateCharacterStateAfterMovement()
{
	// Uncrouch if no longer allowed to be crouched
	if (IsCrouching() && !CanCrouchInCurrentState())
	{
		UnCrouch(false);
	}
}

void UCharacterMovementComponent::StartNewPhysics(float deltaTime, int32 Iterations)
{
	if ((deltaTime < MIN_TICK_TIME) || (Iterations >= MaxSimulationIterations) || !HasValidData())
	{
		return;
	}

	if (UpdatedComponent->IsSimulatingPhysics())
	{
		UE_LOG(LogCharacterMovement, Log, TEXT("UCharacterMovementComponent::StartNewPhysics: UpdateComponent (%s) is simulating physics - aborting."), *UpdatedComponent->GetPathName());
		return;
	}

	const bool bSavedMovementInProgress = bMovementInProgress;
	bMovementInProgress = true;

	switch ( MovementMode )
	{
	case MOVE_None:
		break;
	case MOVE_Walking:
		PhysWalking(deltaTime, Iterations);
		break;
	case MOVE_NavWalking:
		PhysNavWalking(deltaTime, Iterations);
		break;
	case MOVE_Falling:
		PhysFalling(deltaTime, Iterations);
		break;
	case MOVE_Flying:
		PhysFlying(deltaTime, Iterations);
		break;
	case MOVE_Swimming:
		PhysSwimming(deltaTime, Iterations);
		break;
	case MOVE_Custom:
		PhysCustom(deltaTime, Iterations);
		break;
	default:
		UE_LOG(LogCharacterMovement, Warning, TEXT("%s has unsupported movement mode %d"), *CharacterOwner->GetName(), int32(MovementMode));
		SetMovementMode(MOVE_None);
		break;
	}

	bMovementInProgress = bSavedMovementInProgress;
	if ( bDeferUpdateMoveComponent )
	{
		SetUpdatedComponent(DeferredUpdatedMoveComponent);
	}
}

float UCharacterMovementComponent::GetGravityZ() const
{
	return Super::GetGravityZ() * GravityScale;
}

float UCharacterMovementComponent::GetMaxSpeed() const
{
	switch(MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		return IsCrouching() ? MaxWalkSpeedCrouched : MaxWalkSpeed;
	case MOVE_Falling:
		return MaxWalkSpeed;
	case MOVE_Swimming:
		return MaxSwimSpeed;
	case MOVE_Flying:
		return MaxFlySpeed;
	case MOVE_Custom:
		return MaxCustomMovementSpeed;
	case MOVE_None:
	default:
		return 0.f;
	}
}

float UCharacterMovementComponent::GetMinAnalogSpeed() const
{
	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
	case MOVE_Falling:
		return MinAnalogWalkSpeed;
	default:
		return 0.f;
	}
}

FVector UCharacterMovementComponent::GetPenetrationAdjustment(const FHitResult& Hit) const
{
	FVector Result = Super::GetPenetrationAdjustment(Hit);

	if (CharacterOwner)
	{
		const bool bIsProxy = (CharacterOwner->Role == ROLE_SimulatedProxy);
		float MaxDistance = bIsProxy ? MaxDepenetrationWithGeometryAsProxy : MaxDepenetrationWithGeometry;
		const AActor* HitActor = Hit.GetActor();
		if (Cast<APawn>(HitActor))
		{
			MaxDistance = bIsProxy ? MaxDepenetrationWithPawnAsProxy : MaxDepenetrationWithPawn;
		}

		Result = Result.GetClampedToMaxSize(MaxDistance);
	}

	return Result;
}


bool UCharacterMovementComponent::ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation)
{
	// If movement occurs, mark that we teleported, so we don't incorrectly adjust velocity based on a potentially very different movement than our movement direction.
	bJustTeleported |= Super::ResolvePenetrationImpl(Adjustment, Hit, NewRotation);
	return bJustTeleported;
}


float UCharacterMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact)
{
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	FVector Normal(InNormal);
	if (IsMovingOnGround())
	{
		// We don't want to be pushed up an unwalkable surface.
		if (Normal.Z > 0.f)
		{
			if (!IsWalkable(Hit))
			{
				Normal = Normal.GetSafeNormal2D();
			}
		}
		else if (Normal.Z < -KINDA_SMALL_NUMBER)
		{
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				const FVector FloorNormal = CurrentFloor.HitResult.Normal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (FloorNormal.Z < 1.f - DELTA);
				if (bFloorOpposedToMovement)
				{
					Normal = FloorNormal;
				}
				
				Normal = Normal.GetSafeNormal2D();
			}
		}
	}

	return Super::SlideAlongSurface(Delta, Time, Normal, Hit, bHandleImpact);
}


void UCharacterMovementComponent::TwoWallAdjust(FVector& Delta, const FHitResult& Hit, const FVector& OldHitNormal) const
{
	const FVector InDelta = Delta;
	Super::TwoWallAdjust(Delta, Hit, OldHitNormal);

	if (IsMovingOnGround())
	{
		// Allow slides up walkable surfaces, but not unwalkable ones (treat those as vertical barriers).
		if (Delta.Z > 0.f)
		{
			if ((Hit.Normal.Z >= WalkableFloorZ || IsWalkable(Hit)) && Hit.Normal.Z > KINDA_SMALL_NUMBER)
			{
				// Maintain horizontal velocity
				const float Time = (1.f - Hit.Time);
				const FVector ScaledDelta = Delta.GetSafeNormal() * InDelta.Size();
				Delta = FVector(InDelta.X, InDelta.Y, ScaledDelta.Z / Hit.Normal.Z) * Time;
			}
			else
			{
				Delta.Z = 0.f;
			}
		}
		else if (Delta.Z < 0.f)
		{
			// Don't push down into the floor.
			if (CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				Delta.Z = 0.f;
			}
		}
	}
}


FVector UCharacterMovementComponent::ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit) const
{
	FVector Result = Super::ComputeSlideVector(Delta, Time, Normal, Hit);

	// prevent boosting up slopes
	if (IsFalling())
	{
		Result = HandleSlopeBoosting(Result, Delta, Time, Normal, Hit);
	}

	return Result;
}


FVector UCharacterMovementComponent::HandleSlopeBoosting(const FVector& SlideResult, const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit) const
{
	FVector Result = SlideResult;

	if (Result.Z > 0.f)
	{
		// Don't move any higher than we originally intended.
		const float ZLimit = Delta.Z * Time;
		if (Result.Z - ZLimit > KINDA_SMALL_NUMBER)
		{
			if (ZLimit > 0.f)
			{
				// Rescale the entire vector (not just the Z component) otherwise we change the direction and likely head right back into the impact.
				const float UpPercent = ZLimit / Result.Z;
				Result *= UpPercent;
			}
			else
			{
				// We were heading down but were going to deflect upwards. Just make the deflection horizontal.
				Result = FVector::ZeroVector;
			}

			// Make remaining portion of original result horizontal and parallel to impact normal.
			const FVector RemainderXY = (SlideResult - Result) * FVector(1.f, 1.f, 0.f);
			const FVector NormalXY = Normal.GetSafeNormal2D();
			const FVector Adjust = Super::ComputeSlideVector(RemainderXY, 1.f, NormalXY, Hit);
			Result += Adjust;
		}
	}

	return Result;
}

FVector UCharacterMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
	FVector Result = InitialVelocity;

	if (!Gravity.IsZero() && (bApplyGravityWhileJumping || !(CharacterOwner && CharacterOwner->IsJumpProvidingForce())))
	{
		// Apply gravity.
		Result += Gravity * DeltaTime;

		const FVector GravityDir = Gravity.GetSafeNormal();
		const float TerminalLimit = FMath::Abs(GetPhysicsVolume()->TerminalVelocity);

		// Don't exceed terminal velocity.
		if ((Result | GravityDir) > TerminalLimit)
		{
			Result = FVector::PointPlaneProject(Result, FVector::ZeroVector, GravityDir) + GravityDir * TerminalLimit;
		}
	}

	return Result;
}


float UCharacterMovementComponent::ImmersionDepth() const
{
	float depth = 0.f;

	if ( CharacterOwner && GetPhysicsVolume()->bWaterVolume )
	{
		const float CollisionHalfHeight = CharacterOwner->GetSimpleCollisionHalfHeight();

		if ( (CollisionHalfHeight == 0.f) || (Buoyancy == 0.f) )
		{
			depth = 1.f;
		}
		else
		{
			UBrushComponent* VolumeBrushComp = GetPhysicsVolume()->GetBrushComponent();
			FHitResult Hit(1.f);
			if ( VolumeBrushComp )
			{
				const FVector TraceStart = UpdatedComponent->GetComponentLocation() + FVector(0.f,0.f,CollisionHalfHeight);
				const FVector TraceEnd = UpdatedComponent->GetComponentLocation() - FVector(0.f,0.f,CollisionHalfHeight);

				FCollisionQueryParams NewTraceParams(SCENE_QUERY_STAT(ImmersionDepth), true);
				VolumeBrushComp->LineTraceComponent( Hit, TraceStart, TraceEnd, NewTraceParams );
			}

			depth = (Hit.Time == 1.f) ? 1.f : (1.f - Hit.Time);
		}
	}
	return depth;
}

bool UCharacterMovementComponent::IsFlying() const
{
	return (MovementMode == MOVE_Flying) && UpdatedComponent;
}

bool UCharacterMovementComponent::IsMovingOnGround() const
{
	return ((MovementMode == MOVE_Walking) || (MovementMode == MOVE_NavWalking)) && UpdatedComponent;
}

bool UCharacterMovementComponent::IsFalling() const
{
	return (MovementMode == MOVE_Falling) && UpdatedComponent;
}

bool UCharacterMovementComponent::IsSwimming() const
{
	return (MovementMode == MOVE_Swimming) && UpdatedComponent;
}

bool UCharacterMovementComponent::IsCrouching() const
{
	return CharacterOwner && CharacterOwner->bIsCrouched;
}

void UCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Do not update velocity when using root motion or when SimulatedProxy - SimulatedProxy are repped their Velocity
	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->Role == ROLE_SimulatedProxy))
	{
		return;
	}

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = GetMaxAcceleration();
	float MaxSpeed = GetMaxSpeed();
	
	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;
	if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		RequestedAcceleration = RequestedAcceleration.GetClampedToMaxSize(MaxAccel);
		bZeroRequestedAcceleration = false;
	}

	if (bForceMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else 
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
		}

		AnalogInputModifier = 1.f;
	}

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	MaxSpeed = FMath::Max3(RequestedSpeed, MaxSpeed * AnalogInputModifier, GetMinAnalogSpeed());

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);
	
	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);
	
		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}

	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}

	// Apply acceleration
	const float NewMaxSpeed = (IsExceedingMaxSpeed(MaxSpeed)) ? Velocity.Size() : MaxSpeed;
	Velocity += Acceleration * DeltaTime;
	Velocity += RequestedAcceleration * DeltaTime;
	Velocity = Velocity.GetClampedToMaxSize(NewMaxSpeed);

	if (bUseRVOAvoidance)
	{
		CalcAvoidanceVelocity(DeltaTime);
	}
}


bool UCharacterMovementComponent::ApplyRequestedMove(float DeltaTime, float MaxAccel, float MaxSpeed, float Friction, float BrakingDeceleration, FVector& OutAcceleration, float& OutRequestedSpeed)
{
	if (bHasRequestedVelocity)
	{
		const float RequestedSpeedSquared = RequestedVelocity.SizeSquared();
		if (RequestedSpeedSquared < KINDA_SMALL_NUMBER)
		{
			return false;
		}

		// Compute requested speed from path following
		float RequestedSpeed = FMath::Sqrt(RequestedSpeedSquared);
		const FVector RequestedMoveDir = RequestedVelocity / RequestedSpeed;
		RequestedSpeed = (bRequestedMoveWithMaxSpeed ? MaxSpeed : FMath::Min(MaxSpeed, RequestedSpeed));
		
		// Compute actual requested velocity
		const FVector MoveVelocity = RequestedMoveDir * RequestedSpeed;
		
		// Compute acceleration. Use MaxAccel to limit speed increase, 1% buffer.
		FVector NewAcceleration = FVector::ZeroVector;
		const float CurrentSpeedSq = Velocity.SizeSquared();
		if (bRequestedMoveUseAcceleration && CurrentSpeedSq < FMath::Square(RequestedSpeed * 1.01f))
		{
			// Turn in the same manner as with input acceleration.
			const float VelSize = FMath::Sqrt(CurrentSpeedSq);
			Velocity = Velocity - (Velocity - RequestedMoveDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);

			// How much do we need to accelerate to get to the new velocity?
			NewAcceleration = ((MoveVelocity - Velocity) / DeltaTime);
			NewAcceleration = NewAcceleration.GetClampedToMaxSize(MaxAccel);
		}
		else
		{
			// Just set velocity directly.
			// If decelerating we do so instantly, so we don't slide through the destination if we can't brake fast enough.
			Velocity = MoveVelocity;
		}

		// Copy to out params
		OutRequestedSpeed = RequestedSpeed;
		OutAcceleration = NewAcceleration;
		return true;
	}

	return false;
}

void UCharacterMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	if (MoveVelocity.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (IsFalling())
	{
		const FVector FallVelocity = MoveVelocity.GetClampedToMaxSize(GetMaxSpeed());
		PerformAirControlForPathFollowing(FallVelocity, FallVelocity.Z);
		return;
	}

	RequestedVelocity = MoveVelocity;
	bHasRequestedVelocity = true;
	bRequestedMoveWithMaxSpeed = bForceMaxSpeed;

	if (IsMovingOnGround())
	{
		RequestedVelocity.Z = 0.0f;
	}
}

void UCharacterMovementComponent::RequestPathMove(const FVector& MoveInput)
{
	FVector AdjustedMoveInput(MoveInput);

	// preserve magnitude when moving on ground/falling and requested input has Z component
	// see ConstrainInputAcceleration for details
	if (MoveInput.Z != 0.f && (IsMovingOnGround() || IsFalling()))
	{
		const float Mag = MoveInput.Size();
		AdjustedMoveInput = MoveInput.GetSafeNormal2D() * Mag;
	}
	
	Super::RequestPathMove(AdjustedMoveInput);
}

bool UCharacterMovementComponent::CanStartPathFollowing() const
{
	if (!HasValidData() || HasAnimRootMotion())
	{
		return false;
	}

	if (CharacterOwner)
	{
		if (CharacterOwner->GetRootComponent() && CharacterOwner->GetRootComponent()->IsSimulatingPhysics())
		{
			return false;
		}
		else if (CharacterOwner->IsMatineeControlled())
		{
			return false;
		}
	}

	return Super::CanStartPathFollowing();
}

bool UCharacterMovementComponent::CanStopPathFollowing() const
{
	return !IsFalling();
}

float UCharacterMovementComponent::GetPathFollowingBrakingDistance(float MaxSpeed) const
{
	if (bUseFixedBrakingDistanceForPaths)
	{
		return FixedPathBrakingDistance;
	}

	const float BrakingDeceleration = FMath::Abs(GetMaxBrakingDeceleration());

	// character won't be able to stop with negative or nearly zero deceleration, use MaxSpeed for path length calculations
	const float BrakingDistance = (BrakingDeceleration < SMALL_NUMBER) ? MaxSpeed : (FMath::Square(MaxSpeed) / (2.f * BrakingDeceleration));
	return BrakingDistance;
}

void UCharacterMovementComponent::CalcAvoidanceVelocity(float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_AI_ObstacleAvoidance);

	UAvoidanceManager* AvoidanceManager = GetWorld()->GetAvoidanceManager();
	if (AvoidanceWeight >= 1.0f || AvoidanceManager == NULL || GetCharacterOwner() == NULL)
	{
		return;
	}

	if (GetCharacterOwner()->Role != ROLE_Authority)
	{
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bShowDebug = AvoidanceManager->IsDebugEnabled(AvoidanceUID);
#endif

	//Adjust velocity only if we're in "Walking" mode. We should also check if we're dazed, being knocked around, maybe off-navmesh, etc.
	UCapsuleComponent *OurCapsule = GetCharacterOwner()->GetCapsuleComponent();
	if (!Velocity.IsZero() && IsMovingOnGround() && OurCapsule)
	{
		//See if we're doing a locked avoidance move already, and if so, skip the testing and just do the move.
		if (AvoidanceLockTimer > 0.0f)
		{
			Velocity = AvoidanceLockVelocity;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (bShowDebug)
			{
				DrawDebugLine(GetWorld(), GetActorFeetLocation(), GetActorFeetLocation() + Velocity, FColor::Blue, true, 0.5f, SDPG_MAX);
			}
#endif
		}
		else
		{
			FVector NewVelocity = AvoidanceManager->GetAvoidanceVelocityForComponent(this);
			if (bUseRVOPostProcess)
			{
				PostProcessAvoidanceVelocity(NewVelocity);
			}

			if (!NewVelocity.Equals(Velocity))		//Really want to branch hint that this will probably not pass
			{
				//Had to divert course, lock this avoidance move in for a short time. This will make us a VO, so unlocked others will know to avoid us.
				Velocity = NewVelocity;
				SetAvoidanceVelocityLock(AvoidanceManager, AvoidanceManager->LockTimeAfterAvoid);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (bShowDebug)
				{
					DrawDebugLine(GetWorld(), GetActorFeetLocation(), GetActorFeetLocation() + Velocity, FColor::Red, true, 0.05f, SDPG_MAX, 10.0f);
				}
#endif
			}
			else
			{
				//Although we didn't divert course, our velocity for this frame is decided. We will not reciprocate anything further, so treat as a VO for the remainder of this frame.
				SetAvoidanceVelocityLock(AvoidanceManager, AvoidanceManager->LockTimeAfterClean);	//10 ms of lock time should be adequate.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (bShowDebug)
				{
					//DrawDebugLine(GetWorld(), GetActorLocation(), GetActorLocation() + Velocity, FColor::Green, true, 0.05f, SDPG_MAX, 10.0f);
				}
#endif
			}
		}
		//RickH - We might do better to do this later in our update
		AvoidanceManager->UpdateRVO(this);

		bWasAvoidanceUpdated = true;
	}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	else if (bShowDebug)
	{
		DrawDebugLine(GetWorld(), GetActorFeetLocation(), GetActorFeetLocation() + Velocity, FColor::Yellow, true, 0.05f, SDPG_MAX);
	}

	if (bShowDebug)
	{
		FVector UpLine(0,0,500);
		DrawDebugLine(GetWorld(), GetActorFeetLocation(), GetActorFeetLocation() + UpLine, (AvoidanceLockTimer > 0.01f) ? FColor::Red : FColor::Blue, true, 0.05f, SDPG_MAX, 5.0f);
	}
#endif
}

void UCharacterMovementComponent::PostProcessAvoidanceVelocity(FVector& NewVelocity)
{
	// empty in base class
}

void UCharacterMovementComponent::UpdateDefaultAvoidance()
{
	if (!bUseRVOAvoidance)
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_AI_ObstacleAvoidance);

	UAvoidanceManager* AvoidanceManager = GetWorld()->GetAvoidanceManager();
	if (AvoidanceManager && !bWasAvoidanceUpdated)
	{
		if (UCapsuleComponent *OurCapsule = GetCharacterOwner()->GetCapsuleComponent())
		{
			AvoidanceManager->UpdateRVO(this);

			//Consider this a clean move because we didn't even try to avoid.
			SetAvoidanceVelocityLock(AvoidanceManager, AvoidanceManager->LockTimeAfterClean);
		}
	}

	bWasAvoidanceUpdated = false;		//Reset for next frame
}

void UCharacterMovementComponent::SetRVOAvoidanceUID(int32 UID)
{
	AvoidanceUID = UID;
}

int32 UCharacterMovementComponent::GetRVOAvoidanceUID()
{
	return AvoidanceUID;
}

void UCharacterMovementComponent::SetRVOAvoidanceWeight(float Weight)
{
	AvoidanceWeight = Weight;
}

float UCharacterMovementComponent::GetRVOAvoidanceWeight()
{
	return AvoidanceWeight;
}

FVector UCharacterMovementComponent::GetRVOAvoidanceOrigin()
{
	return GetActorFeetLocation();
}

float UCharacterMovementComponent::GetRVOAvoidanceRadius()
{
	UCapsuleComponent* CapsuleComp = GetCharacterOwner()->GetCapsuleComponent();
	return CapsuleComp ? CapsuleComp->GetScaledCapsuleRadius() : 0.0f;
}

float UCharacterMovementComponent::GetRVOAvoidanceConsiderationRadius()
{
	return AvoidanceConsiderationRadius;
}

float UCharacterMovementComponent::GetRVOAvoidanceHeight()
{
	UCapsuleComponent* CapsuleComp = GetCharacterOwner()->GetCapsuleComponent();
	return CapsuleComp ? CapsuleComp->GetScaledCapsuleHalfHeight() : 0.0f;
}

FVector UCharacterMovementComponent::GetVelocityForRVOConsideration()
{
	return Velocity;
}

int32 UCharacterMovementComponent::GetAvoidanceGroupMask()
{
	return AvoidanceGroup.Packed;
}

int32 UCharacterMovementComponent::GetGroupsToAvoidMask()
{
	return GroupsToAvoid.Packed;
}

int32 UCharacterMovementComponent::GetGroupsToIgnoreMask()
{
	return GroupsToIgnore.Packed;
}

void UCharacterMovementComponent::SetAvoidanceVelocityLock(class UAvoidanceManager* Avoidance, float Duration)
{
	Avoidance->OverrideToMaxWeight(AvoidanceUID, Duration);
	AvoidanceLockVelocity = Velocity;
	AvoidanceLockTimer = Duration;
}

void UCharacterMovementComponent::NotifyBumpedPawn(APawn* BumpedPawn)
{
	Super::NotifyBumpedPawn(BumpedPawn);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UAvoidanceManager* Avoidance = GetWorld()->GetAvoidanceManager();
	const bool bShowDebug = Avoidance && Avoidance->IsDebugEnabled(AvoidanceUID);
	if (bShowDebug)
	{
		DrawDebugLine(GetWorld(), GetActorFeetLocation(), GetActorFeetLocation() + FVector(0,0,500), (AvoidanceLockTimer > 0) ? FColor(255,64,64) : FColor(64,64,255), true, 2.0f, SDPG_MAX, 20.0f);
	}
#endif

	// Unlock avoidance move. This mostly happens when two pawns who are locked into avoidance moves collide with each other.
	AvoidanceLockTimer = 0.0f;
}

float UCharacterMovementComponent::GetMaxJumpHeight() const
{
	const float Gravity = GetGravityZ();
	if (FMath::Abs(Gravity) > KINDA_SMALL_NUMBER)
	{
		return FMath::Square(JumpZVelocity) / (-2.f * Gravity);
	}
	else
	{
		return 0.f;
	}
}

float UCharacterMovementComponent::GetMaxJumpHeightWithJumpTime() const
{
	const float MaxJumpHeight = GetMaxJumpHeight();

	if (CharacterOwner)
	{
		// When bApplyGravityWhileJumping is true, the actual max height will be lower than this.
		// However, it will also be dependent on framerate (and substep iterations) so just return this
		// to avoid expensive calculations.

		// This can be imagined as the character being displaced to some height, then jumping from that height.
		return (CharacterOwner->JumpMaxHoldTime * JumpZVelocity) + MaxJumpHeight;
	}

	return MaxJumpHeight;
}

// TODO: deprecated, remove.
float UCharacterMovementComponent::GetModifiedMaxAcceleration() const
{
	// Allow calling old deprecated function to maintain old behavior until it is removed.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return CharacterOwner ? MaxAcceleration * GetMaxSpeedModifier() : 0.f;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

// TODO: deprecated, remove.
float UCharacterMovementComponent::K2_GetModifiedMaxAcceleration() const
{
	// Allow calling old deprecated function to maintain old behavior until it is removed.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GetModifiedMaxAcceleration();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}


float UCharacterMovementComponent::GetMaxAcceleration() const
{
	return MaxAcceleration;
}

float UCharacterMovementComponent::GetMaxBrakingDeceleration() const
{
	switch (MovementMode)
	{
		case MOVE_Walking:
		case MOVE_NavWalking:
			return BrakingDecelerationWalking;
		case MOVE_Falling:
			return BrakingDecelerationFalling;
		case MOVE_Swimming:
			return BrakingDecelerationSwimming;
		case MOVE_Flying:
			return BrakingDecelerationFlying;
		case MOVE_Custom:
			return 0.f;
		case MOVE_None:
		default:
			return 0.f;
	}
}

FVector UCharacterMovementComponent::GetCurrentAcceleration() const
{
	return Acceleration;
}

void UCharacterMovementComponent::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (Velocity.IsZero() || !HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const float FrictionFactor = FMath::Max(0.f, BrakingFrictionFactor);
	Friction = FMath::Max(0.f, Friction * FrictionFactor);
	BrakingDeceleration = FMath::Max(0.f, BrakingDeceleration);
	const bool bZeroFriction = (Friction == 0.f);
	const bool bZeroBraking = (BrakingDeceleration == 0.f);

	if (bZeroFriction && bZeroBraking)
	{
		return;
	}

	const FVector OldVel = Velocity;

	// subdivide braking to get reasonably consistent results at lower frame rates
	// (important for packet loss situations w/ networking)
	float RemainingTime = DeltaTime;
	const float MaxTimeStep = (1.0f / 33.0f);

	// Decelerate to brake to a stop
	const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * Velocity.GetSafeNormal()));
	while( RemainingTime >= MIN_TICK_TIME )
	{
		// Zero friction uses constant deceleration, so no need for iteration.
		const float dt = ((RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
		RemainingTime -= dt;

		// apply friction and braking
		Velocity = Velocity + ((-Friction) * Velocity + RevAccel) * dt ; 
		
		// Don't reverse direction
		if ((Velocity | OldVel) <= 0.f)
		{
			Velocity = FVector::ZeroVector;
			return;
		}
	}

	// Clamp to zero if nearly zero, or if below min threshold and braking.
	const float VSizeSq = Velocity.SizeSquared();
	if (VSizeSq <= KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(BRAKE_TO_STOP_VELOCITY)))
	{
		Velocity = FVector::ZeroVector;
	}
}

void UCharacterMovementComponent::PhysFlying(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{
		if( bCheatFlying && Acceleration.IsZero() )
		{
			Velocity = FVector::ZeroVector;
		}
		const float Friction = 0.5f * GetPhysicsVolume()->FluidFriction;
		CalcVelocity(deltaTime, Friction, true, GetMaxBrakingDeceleration());
	}

	ApplyRootMotionToVelocity(deltaTime);

	Iterations++;
	bJustTeleported = false;

	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.f)
	{
		const FVector GravDir = FVector(0.f, 0.f, -1.f);
		const FVector VelDir = Velocity.GetSafeNormal();
		const float UpDown = GravDir | VelDir;

		bool bSteppedUp = false;
		if ((FMath::Abs(Hit.ImpactNormal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) && CanStepUp(Hit))
		{
			float stepZ = UpdatedComponent->GetComponentLocation().Z;
			bSteppedUp = StepUp(GravDir, Adjusted * (1.f - Hit.Time), Hit);
			if (bSteppedUp)
			{
				OldLocation.Z = UpdatedComponent->GetComponentLocation().Z + (OldLocation.Z - stepZ);
			}
		}

		if (!bSteppedUp)
		{
			//adjust and try again
			HandleImpact(Hit, deltaTime, Adjusted);
			SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
		}
	}

	if( !bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}
}

void UCharacterMovementComponent::RestorePreAdditiveRootMotionVelocity()
{
	// Restore last frame's pre-additive Velocity if we had additive applied 
	// so that we're not adding more additive velocity than intended
	if( CurrentRootMotion.bIsAdditiveVelocityApplied )
	{
#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
		{
			FString AdjustedDebugString = FString::Printf(TEXT("RestorePreAdditiveRootMotionVelocity Velocity(%s) LastPreAdditiveVelocity(%s)"), 
				*Velocity.ToCompactString(), *CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString());
			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		}
#endif

		Velocity = CurrentRootMotion.LastPreAdditiveVelocity;
		CurrentRootMotion.bIsAdditiveVelocityApplied = false;
	}
}

void UCharacterMovementComponent::ApplyRootMotionToVelocity(float deltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceApply);

	// Animation root motion is distinct from root motion sources right now and takes precedence
	if( HasAnimRootMotion() && deltaTime > 0.f )
	{
		Velocity = ConstrainAnimRootMotionVelocity(AnimRootMotionVelocity, Velocity);
		return;
	}

	const FVector OldVelocity = Velocity;

	bool bAppliedRootMotion = false;

	// Apply override velocity
	if( CurrentRootMotion.HasOverrideVelocity() )
	{
		CurrentRootMotion.AccumulateOverrideRootMotionVelocity(deltaTime, *CharacterOwner, *this, Velocity);
		bAppliedRootMotion = true;

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
		{
			FString AdjustedDebugString = FString::Printf(TEXT("ApplyRootMotionToVelocity HasOverrideVelocity Velocity(%s)"),
				*Velocity.ToCompactString());
			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		}
#endif
	}

	// Next apply additive root motion
	if( CurrentRootMotion.HasAdditiveVelocity() )
	{
		CurrentRootMotion.LastPreAdditiveVelocity = Velocity; // Save off pre-additive Velocity for restoration next tick
		CurrentRootMotion.AccumulateAdditiveRootMotionVelocity(deltaTime, *CharacterOwner, *this, Velocity);
		CurrentRootMotion.bIsAdditiveVelocityApplied = true; // Remember that we have it applied
		bAppliedRootMotion = true;

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
		{
			FString AdjustedDebugString = FString::Printf(TEXT("ApplyRootMotionToVelocity HasAdditiveVelocity Velocity(%s) LastPreAdditiveVelocity(%s)"),
				*Velocity.ToCompactString(), *CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString());
			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		}
#endif
	}

	// Switch to Falling if we have vertical velocity from root motion so we can lift off the ground
	const FVector AppliedVelocityDelta = Velocity - OldVelocity;
	if( bAppliedRootMotion && AppliedVelocityDelta.Z != 0.f && IsMovingOnGround() )
	{
		float LiftoffBound;
		if( CurrentRootMotion.LastAccumulatedSettings.HasFlag(ERootMotionSourceSettingsFlags::UseSensitiveLiftoffCheck) )
		{
			// Sensitive bounds - "any positive force"
			LiftoffBound = SMALL_NUMBER;
		}
		else
		{
			// Default bounds - the amount of force gravity is applying this tick
			LiftoffBound = FMath::Max(GetGravityZ() * deltaTime, SMALL_NUMBER);
		}

		if( AppliedVelocityDelta.Z > LiftoffBound )
		{
			SetMovementMode(MOVE_Falling);
		}
	}
}

void UCharacterMovementComponent::HandleSwimmingWallHit(const FHitResult& Hit, float DeltaTime)
{
}

void UCharacterMovementComponent::PhysSwimming(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	float NetFluidFriction  = 0.f;
	float Depth = ImmersionDepth();
	float NetBuoyancy = Buoyancy * Depth;
	float OriginalAccelZ = Acceleration.Z;
	bool bLimitedUpAccel = false;

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (Velocity.Z > 0.33f * MaxSwimSpeed) && (NetBuoyancy != 0.f))
	{
		//damp positive Z out of water
		Velocity.Z = FMath::Max(0.33f * MaxSwimSpeed, Velocity.Z * Depth*Depth);
	}
	else if (Depth < 0.65f)
	{
		bLimitedUpAccel = (Acceleration.Z > 0.f);
		Acceleration.Z = FMath::Min(0.1f, Acceleration.Z);
	}

	Iterations++;
	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	bJustTeleported = false;
	if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{
		const float Friction = 0.5f * GetPhysicsVolume()->FluidFriction * Depth;
		CalcVelocity(deltaTime, Friction, true, GetMaxBrakingDeceleration());
		Velocity.Z += GetGravityZ() * deltaTime * (1.f - NetBuoyancy);
	}

	ApplyRootMotionToVelocity(deltaTime);

	FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	float remainingTime = deltaTime * Swim(Adjusted, Hit);

	//may have left water - if so, script might have set new physics mode
	if ( !IsSwimming() )
	{
		StartNewPhysics(remainingTime, Iterations);
		return;
	}

	if ( Hit.Time < 1.f && CharacterOwner)
	{
		HandleSwimmingWallHit(Hit, deltaTime);
		if (bLimitedUpAccel && (Velocity.Z >= 0.f))
		{
			// allow upward velocity at surface if against obstacle
			Velocity.Z += OriginalAccelZ * deltaTime;
			Adjusted = Velocity * (1.f - Hit.Time)*deltaTime;
			Swim(Adjusted, Hit);
			if (!IsSwimming())
			{
				StartNewPhysics(remainingTime, Iterations);
				return;
			}
		}

		const FVector GravDir = FVector(0.f,0.f,-1.f);
		const FVector VelDir = Velocity.GetSafeNormal();
		const float UpDown = GravDir | VelDir;

		bool bSteppedUp = false;
		if( (FMath::Abs(Hit.ImpactNormal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) && CanStepUp(Hit))
		{
			float stepZ = UpdatedComponent->GetComponentLocation().Z;
			const FVector RealVelocity = Velocity;
			Velocity.Z = 1.f;	// HACK: since will be moving up, in case pawn leaves the water
			bSteppedUp = StepUp(GravDir, Adjusted * (1.f - Hit.Time), Hit);
			if (bSteppedUp)
			{
				//may have left water - if so, script might have set new physics mode
				if (!IsSwimming())
				{
					StartNewPhysics(remainingTime, Iterations);
					return;
				}
				OldLocation.Z = UpdatedComponent->GetComponentLocation().Z + (OldLocation.Z - stepZ);
			}
			Velocity = RealVelocity;
		}

		if (!bSteppedUp)
		{
			//adjust and try again
			HandleImpact(Hit, deltaTime, Adjusted);
			SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
		}
	}

	if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && !bJustTeleported && ((deltaTime - remainingTime) > KINDA_SMALL_NUMBER) && CharacterOwner )
	{
		bool bWaterJump = !GetPhysicsVolume()->bWaterVolume;
		float velZ = Velocity.Z;
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / (deltaTime - remainingTime);
		if (bWaterJump)
		{
			Velocity.Z = velZ;
		}
	}

	if ( !GetPhysicsVolume()->bWaterVolume && IsSwimming() )
	{
		SetMovementMode(MOVE_Falling); //in case script didn't change it (w/ zone change)
	}

	//may have left water - if so, script might have set new physics mode
	if ( !IsSwimming() )
	{
		StartNewPhysics(remainingTime, Iterations);
	}
}


void UCharacterMovementComponent::StartSwimming(FVector OldLocation, FVector OldVelocity, float timeTick, float remainingTime, int32 Iterations)
{
	if (remainingTime < MIN_TICK_TIME || timeTick < MIN_TICK_TIME)
	{
		return;
	}

	if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && !bJustTeleported )
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation)/timeTick; //actual average velocity
		Velocity = 2.f*Velocity - OldVelocity; //end velocity has 2* accel of avg
		Velocity = Velocity.GetClampedToMaxSize(GetPhysicsVolume()->TerminalVelocity);
	}
	const FVector End = FindWaterLine(UpdatedComponent->GetComponentLocation(), OldLocation);
	float waterTime = 0.f;
	if (End != UpdatedComponent->GetComponentLocation())
	{	
		const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size();
		if (ActualDist > KINDA_SMALL_NUMBER)
		{
			waterTime = timeTick * (End - UpdatedComponent->GetComponentLocation()).Size() / ActualDist;
			remainingTime += waterTime;
		}
		MoveUpdatedComponent(End - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true);
	}
	if ( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (Velocity.Z > 2.f*SWIMBOBSPEED) && (Velocity.Z < 0.f)) //allow for falling out of water
	{
		Velocity.Z = SWIMBOBSPEED - Velocity.Size2D() * 0.7f; //smooth bobbing
	}
	if ( (remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) )
	{
		PhysSwimming(remainingTime, Iterations);
	}
}

float UCharacterMovementComponent::Swim(FVector Delta, FHitResult& Hit)
{
	FVector Start = UpdatedComponent->GetComponentLocation();
	float airTime = 0.f;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if ( !GetPhysicsVolume()->bWaterVolume ) //then left water
	{
		const FVector End = FindWaterLine(Start,UpdatedComponent->GetComponentLocation());
		const float DesiredDist = Delta.Size();
		if (End != UpdatedComponent->GetComponentLocation() && DesiredDist > KINDA_SMALL_NUMBER)
		{
			airTime = (End - UpdatedComponent->GetComponentLocation()).Size() / DesiredDist;
			if ( ((UpdatedComponent->GetComponentLocation() - Start) | (End - UpdatedComponent->GetComponentLocation())) > 0.f )
			{
				airTime = 0.f;
			}
			SafeMoveUpdatedComponent(End - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true, Hit);
		}
	}
	return airTime;
}

FVector UCharacterMovementComponent::FindWaterLine(FVector InWater, FVector OutofWater)
{
	FVector Result = OutofWater;

	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByChannel(Hits, OutofWater, InWater, UpdatedComponent->GetCollisionObjectType(), FCollisionQueryParams(SCENE_QUERY_STAT(FindWaterLine), true, CharacterOwner));

	for( int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++ )
	{
		const FHitResult& Check = Hits[HitIdx];
		if ( !CharacterOwner->IsOwnedBy(Check.GetActor()) && !Check.Component.Get()->IsWorldGeometry() )
		{
			APhysicsVolume *W = Cast<APhysicsVolume>(Check.GetActor());
			if ( W && W->bWaterVolume )
			{
				FVector Dir = (InWater - OutofWater).GetSafeNormal();
				Result = Check.Location;
				if ( W == GetPhysicsVolume() )
					Result += 0.1f * Dir;
				else
					Result -= 0.1f * Dir;
				break;
			}
		}
	}

	return Result;
}

void UCharacterMovementComponent::NotifyJumpApex() 
{
	if( CharacterOwner )
	{
		CharacterOwner->NotifyJumpApex();
	}
}


FVector UCharacterMovementComponent::GetFallingLateralAcceleration(float DeltaTime)
{
	// No acceleration in Z
	FVector FallAcceleration = FVector(Acceleration.X, Acceleration.Y, 0.f);

	// bound acceleration, falling object has minimal ability to impact acceleration
	if (!HasAnimRootMotion() && FallAcceleration.SizeSquared2D() > 0.f)
	{
		FallAcceleration = GetAirControl(DeltaTime, AirControl, FallAcceleration);
		FallAcceleration = FallAcceleration.GetClampedToMaxSize(GetMaxAcceleration());
	}

	return FallAcceleration;
}


FVector UCharacterMovementComponent::GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	// Boost
	if (TickAirControl != 0.f)
	{
		TickAirControl = BoostAirControl(DeltaTime, TickAirControl, FallAcceleration);
	}

	return TickAirControl * FallAcceleration;
}


float UCharacterMovementComponent::BoostAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	// Allow a burst of initial acceleration
	if (AirControlBoostMultiplier > 0.f && Velocity.SizeSquared2D() < FMath::Square(AirControlBoostVelocityThreshold))
	{
		TickAirControl = FMath::Min(1.f, AirControlBoostMultiplier * TickAirControl);
	}

	return TickAirControl;
}


void UCharacterMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_CharPhysFalling);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	FVector FallAcceleration = GetFallingLateralAcceleration(deltaTime);
	FallAcceleration.Z = 0.f;
	const bool bHasAirControl = (FallAcceleration.SizeSquared2D() > 0.f);

	float remainingTime = deltaTime;
	while( (remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) )
	{
		Iterations++;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;
		
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
		bJustTeleported = false;

		RestorePreAdditiveRootMotionVelocity();

		FVector OldVelocity = Velocity;
		FVector VelocityNoAirControl = Velocity;

		// Apply input
		if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
		{
			const float MaxDecel = GetMaxBrakingDeceleration();
			// Compute VelocityNoAirControl
			if (bHasAirControl)
			{
				// Find velocity *without* acceleration.
				TGuardValue<FVector> RestoreAcceleration(Acceleration, FVector::ZeroVector);
				TGuardValue<FVector> RestoreVelocity(Velocity, Velocity);
				Velocity.Z = 0.f;
				CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
				VelocityNoAirControl = FVector(Velocity.X, Velocity.Y, OldVelocity.Z);
			}

			// Compute Velocity
			{
				// Acceleration = FallAcceleration for CalcVelocity(), but we restore it after using it.
				TGuardValue<FVector> RestoreAcceleration(Acceleration, FallAcceleration);
				Velocity.Z = 0.f;
				CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
				Velocity.Z = OldVelocity.Z;
			}

			// Just copy Velocity to VelocityNoAirControl if they are the same (ie no acceleration).
			if (!bHasAirControl)
			{
				VelocityNoAirControl = Velocity;
			}
		}

		// Apply gravity
		const FVector Gravity(0.f, 0.f, GetGravityZ());
		Velocity = NewFallVelocity(Velocity, Gravity, timeTick);
		VelocityNoAirControl = NewFallVelocity(VelocityNoAirControl, Gravity, timeTick);
		const FVector AirControlAccel = (Velocity - VelocityNoAirControl) / timeTick;

		ApplyRootMotionToVelocity(timeTick);

		if( bNotifyApex && CharacterOwner->Controller && (Velocity.Z <= 0.f) )
		{
			// Just passed jump apex since now going down
			bNotifyApex = false;
			NotifyJumpApex();
		}


		// Move
		FHitResult Hit(1.f);
		FVector Adjusted = 0.5f*(OldVelocity + Velocity) * timeTick;
		SafeMoveUpdatedComponent( Adjusted, PawnRotation, true, Hit);
		
		if (!HasValidData())
		{
			return;
		}
		
		float LastMoveTimeSlice = timeTick;
		float subTimeTickRemaining = timeTick * (1.f - Hit.Time);
		
		if ( IsSwimming() ) //just entered water
		{
			remainingTime += subTimeTickRemaining;
			StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
			return;
		}
		else if ( Hit.bBlockingHit )
		{
			if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
			{
				remainingTime += subTimeTickRemaining;
				ProcessLanded(Hit, remainingTime, Iterations);
				return;
			}
			else
			{
				// Compute impact deflection based on final velocity, not integration step.
				// This allows us to compute a new velocity from the deflected vector, and ensures the full gravity effect is included in the slide result.
				Adjusted = Velocity * timeTick;

				// See if we can convert a normally invalid landing spot (based on the hit result) to a usable one.
				if (!Hit.bStartPenetrating && ShouldCheckForValidLandingSpot(timeTick, Adjusted, Hit))
				{
					const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
					FFindFloorResult FloorResult;
					FindFloor(PawnLocation, FloorResult, false);
					if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
					{
						remainingTime += subTimeTickRemaining;
						ProcessLanded(FloorResult.HitResult, remainingTime, Iterations);
						return;
					}
				}

				HandleImpact(Hit, LastMoveTimeSlice, Adjusted);
				
				// If we've changed physics mode, abort.
				if (!HasValidData() || !IsFalling())
				{
					return;
				}

				// Limit air control based on what we hit.
				// We moved to the impact point using air control, but may want to deflect from there based on a limited air control acceleration.
				if (bHasAirControl)
				{
					const bool bCheckLandingSpot = false; // we already checked above.
					const FVector AirControlDeltaV = LimitAirControl(LastMoveTimeSlice, AirControlAccel, Hit, bCheckLandingSpot) * LastMoveTimeSlice;
					Adjusted = (VelocityNoAirControl + AirControlDeltaV) * LastMoveTimeSlice;
				}

				const FVector OldHitNormal = Hit.Normal;
				const FVector OldHitImpactNormal = Hit.ImpactNormal;				
				FVector Delta = ComputeSlideVector(Adjusted, 1.f - Hit.Time, OldHitNormal, Hit);

				// Compute velocity after deflection (only gravity component for RootMotion)
				if (subTimeTickRemaining > KINDA_SMALL_NUMBER && !bJustTeleported)
				{
					const FVector NewVelocity = (Delta / subTimeTickRemaining);
					Velocity = HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
				}

				if (subTimeTickRemaining > KINDA_SMALL_NUMBER && (Delta | Adjusted) > 0.f)
				{
					// Move in deflected direction.
					SafeMoveUpdatedComponent( Delta, PawnRotation, true, Hit);
					
					if (Hit.bBlockingHit)
					{
						// hit second wall
						LastMoveTimeSlice = subTimeTickRemaining;
						subTimeTickRemaining = subTimeTickRemaining * (1.f - Hit.Time);

						if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
						{
							remainingTime += subTimeTickRemaining;
							ProcessLanded(Hit, remainingTime, Iterations);
							return;
						}

						HandleImpact(Hit, LastMoveTimeSlice, Delta);

						// If we've changed physics mode, abort.
						if (!HasValidData() || !IsFalling())
						{
							return;
						}

						// Act as if there was no air control on the last move when computing new deflection.
						if (bHasAirControl && Hit.Normal.Z > VERTICAL_SLOPE_NORMAL_Z)
						{
							const FVector LastMoveNoAirControl = VelocityNoAirControl * LastMoveTimeSlice;
							Delta = ComputeSlideVector(LastMoveNoAirControl, 1.f, OldHitNormal, Hit);
						}

						FVector PreTwoWallDelta = Delta;
						TwoWallAdjust(Delta, Hit, OldHitNormal);

						// Limit air control, but allow a slide along the second wall.
						if (bHasAirControl)
						{
							const bool bCheckLandingSpot = false; // we already checked above.
							const FVector AirControlDeltaV = LimitAirControl(subTimeTickRemaining, AirControlAccel, Hit, bCheckLandingSpot) * subTimeTickRemaining;

							// Only allow if not back in to first wall
							if (FVector::DotProduct(AirControlDeltaV, OldHitNormal) > 0.f)
							{
								Delta += (AirControlDeltaV * subTimeTickRemaining);
							}
						}

						// Compute velocity after deflection (only gravity component for RootMotion)
						if (subTimeTickRemaining > KINDA_SMALL_NUMBER && !bJustTeleported)
						{
							const FVector NewVelocity = (Delta / subTimeTickRemaining);
							Velocity = HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
						}

						// bDitch=true means that pawn is straddling two slopes, neither of which he can stand on
						bool bDitch = ( (OldHitImpactNormal.Z > 0.f) && (Hit.ImpactNormal.Z > 0.f) && (FMath::Abs(Delta.Z) <= KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f) );
						SafeMoveUpdatedComponent( Delta, PawnRotation, true, Hit);
						if ( Hit.Time == 0.f )
						{
							// if we are stuck then try to side step
							FVector SideDelta = (OldHitNormal + Hit.ImpactNormal).GetSafeNormal2D();
							if ( SideDelta.IsNearlyZero() )
							{
								SideDelta = FVector(OldHitNormal.Y, -OldHitNormal.X, 0).GetSafeNormal();
							}
							SafeMoveUpdatedComponent( SideDelta, PawnRotation, true, Hit);
						}
							
						if ( bDitch || IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit) || Hit.Time == 0.f  )
						{
							remainingTime = 0.f;
							ProcessLanded(Hit, remainingTime, Iterations);
							return;
						}
						else if (GetPerchRadiusThreshold() > 0.f && Hit.Time == 1.f && OldHitImpactNormal.Z >= WalkableFloorZ)
						{
							// We might be in a virtual 'ditch' within our perch radius. This is rare.
							const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
							const float ZMovedDist = FMath::Abs(PawnLocation.Z - OldLocation.Z);
							const float MovedDist2DSq = (PawnLocation - OldLocation).SizeSquared2D();
							if (ZMovedDist <= 0.2f * timeTick && MovedDist2DSq <= 4.f * timeTick)
							{
								Velocity.X += 0.25f * GetMaxSpeed() * (FMath::FRand() - 0.5f);
								Velocity.Y += 0.25f * GetMaxSpeed() * (FMath::FRand() - 0.5f);
								Velocity.Z = FMath::Max<float>(JumpZVelocity * 0.25f, 1.f);
								Delta = Velocity * timeTick;
								SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
							}
						}
					}
				}
			}
		}

		if (Velocity.SizeSquared2D() <= KINDA_SMALL_NUMBER * 10.f)
		{
			Velocity.X = 0.f;
			Velocity.Y = 0.f;
		}
	}
}

FVector UCharacterMovementComponent::LimitAirControl(float DeltaTime, const FVector& FallAcceleration, const FHitResult& HitResult, bool bCheckForValidLandingSpot)
{
	FVector Result(FallAcceleration);

	if (HitResult.IsValidBlockingHit() && HitResult.Normal.Z > VERTICAL_SLOPE_NORMAL_Z)
	{
		if (!bCheckForValidLandingSpot || !IsValidLandingSpot(HitResult.Location, HitResult))
		{
			// If acceleration is into the wall, limit contribution.
			if (FVector::DotProduct(FallAcceleration, HitResult.Normal) < 0.f)
			{
				// Allow movement parallel to the wall, but not into it because that may push us up.
				const FVector Normal2D = HitResult.Normal.GetSafeNormal2D();
				Result = FVector::VectorPlaneProject(FallAcceleration, Normal2D);
			}
		}
	}
	else if (HitResult.bStartPenetrating)
	{
		// Allow movement out of penetration.
		return (FVector::DotProduct(Result, HitResult.Normal) > 0.f ? Result : FVector::ZeroVector);
	}

	return Result;
}

bool UCharacterMovementComponent::CheckLedgeDirection(const FVector& OldLocation, const FVector& SideStep, const FVector& GravDir) const
{
	const FVector SideDest = OldLocation + SideStep;
	FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CheckLedgeDirection), false, CharacterOwner);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(CapsuleParams, ResponseParam);
	const FCollisionShape CapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_None);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
	FHitResult Result(1.f);
	GetWorld()->SweepSingleByChannel(Result, OldLocation, SideDest, FQuat::Identity, CollisionChannel, CapsuleShape, CapsuleParams, ResponseParam);

	if ( !Result.bBlockingHit || IsWalkable(Result) )
	{
		if ( !Result.bBlockingHit )
		{
			GetWorld()->SweepSingleByChannel(Result, SideDest, SideDest + GravDir * (MaxStepHeight + LedgeCheckThreshold), FQuat::Identity, CollisionChannel, CapsuleShape, CapsuleParams, ResponseParam);
		}
		if ( (Result.Time < 1.f) && IsWalkable(Result) )
		{
			return true;
		}
	}
	return false;
}


FVector UCharacterMovementComponent::GetLedgeMove(const FVector& OldLocation, const FVector& Delta, const FVector& GravDir) const
{
	if (!HasValidData() || Delta.IsZero())
	{
		return FVector::ZeroVector;
	}

	FVector SideDir(Delta.Y, -1.f * Delta.X, 0.f);
		
	// try left
	if ( CheckLedgeDirection(OldLocation, SideDir, GravDir) )
	{
		return SideDir;
	}

	// try right
	SideDir *= -1.f;
	if ( CheckLedgeDirection(OldLocation, SideDir, GravDir) )
	{
		return SideDir;
	}
	
	return FVector::ZeroVector;
}


bool UCharacterMovementComponent::CanWalkOffLedges() const
{
	if (!bCanWalkOffLedgesWhenCrouching && IsCrouching())
	{
		return false;
	}	

	return bCanWalkOffLedges;
}

bool UCharacterMovementComponent::CheckFall(const FFindFloorResult& OldFloor, const FHitResult& Hit, const FVector& Delta, const FVector& OldLocation, float remainingTime, float timeTick, int32 Iterations, bool bMustJump)
{
	if (!HasValidData())
	{
		return false;
	}

	if (bMustJump || CanWalkOffLedges())
	{
		CharacterOwner->OnWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
		if (IsMovingOnGround())
		{
			// If still walking, then fall. If not, assume the user set a different mode they want to keep.
			StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation);
		}
		return true;
	}
	return false;
}

void UCharacterMovementComponent::StartFalling(int32 Iterations, float remainingTime, float timeTick, const FVector& Delta, const FVector& subLoc)
{
	// start falling 
	const float DesiredDist = Delta.Size();
	const float ActualDist = (UpdatedComponent->GetComponentLocation() - subLoc).Size2D();
	remainingTime = (DesiredDist < KINDA_SMALL_NUMBER) 
					? 0.f
					: remainingTime + timeTick * (1.f - FMath::Min(1.f,ActualDist/DesiredDist));

	if ( IsMovingOnGround() )
	{
		// This is to catch cases where the first frame of PIE is executed, and the
		// level is not yet visible. In those cases, the player will fall out of the
		// world... So, don't set MOVE_Falling straight away.
		if ( !GIsEditor || (GetWorld()->HasBegunPlay() && (GetWorld()->GetTimeSeconds() >= 1.f)) )
		{
			SetMovementMode(MOVE_Falling); //default behavior if script didn't change physics
		}
		else
		{
			// Make sure that the floor check code continues processing during this delay.
			bForceNextFloorCheck = true;
		}
	}
	StartNewPhysics(remainingTime,Iterations);
}


void UCharacterMovementComponent::RevertMove(const FVector& OldLocation, UPrimitiveComponent* OldBase, const FVector& PreviousBaseLocation, const FFindFloorResult& OldFloor, bool bFailMove)
{
	//UE_LOG(LogCharacterMovement, Log, TEXT("RevertMove from %f %f %f to %f %f %f"), CharacterOwner->Location.X, CharacterOwner->Location.Y, CharacterOwner->Location.Z, OldLocation.X, OldLocation.Y, OldLocation.Z);
	UpdatedComponent->SetWorldLocation(OldLocation, false);
	
	//UE_LOG(LogCharacterMovement, Log, TEXT("Now at %f %f %f"), CharacterOwner->Location.X, CharacterOwner->Location.Y, CharacterOwner->Location.Z);
	bJustTeleported = false;
	// if our previous base couldn't have moved or changed in any physics-affecting way, restore it
	if (IsValid(OldBase) && 
		(!MovementBaseUtility::IsDynamicBase(OldBase) ||
		 (OldBase->Mobility == EComponentMobility::Static) ||
		 (OldBase->GetComponentLocation() == PreviousBaseLocation)
		)
	   )
	{
		CurrentFloor = OldFloor;
		SetBase(OldBase, OldFloor.HitResult.BoneName);
	}
	else
	{
		SetBase(NULL);
	}

	if ( bFailMove )
	{
		// end movement now
		Velocity = FVector::ZeroVector;
		Acceleration = FVector::ZeroVector;
		//UE_LOG(LogCharacterMovement, Log, TEXT("%s FAILMOVE RevertMove"), *CharacterOwner->GetName());
	}
}


FVector UCharacterMovementComponent::ComputeGroundMovementDelta(const FVector& Delta, const FHitResult& RampHit, const bool bHitFromLineTrace) const
{
	const FVector FloorNormal = RampHit.ImpactNormal;
	const FVector ContactNormal = RampHit.Normal;

	if (FloorNormal.Z < (1.f - KINDA_SMALL_NUMBER) && FloorNormal.Z > KINDA_SMALL_NUMBER && ContactNormal.Z > KINDA_SMALL_NUMBER && !bHitFromLineTrace && IsWalkable(RampHit))
	{
		// Compute a vector that moves parallel to the surface, by projecting the horizontal movement direction onto the ramp.
		const float FloorDotDelta = (FloorNormal | Delta);
		FVector RampMovement(Delta.X, Delta.Y, -FloorDotDelta / FloorNormal.Z);
		
		if (bMaintainHorizontalGroundVelocity)
		{
			return RampMovement;
		}
		else
		{
			return RampMovement.GetSafeNormal() * Delta.Size();
		}
	}

	return Delta;
}

void UCharacterMovementComponent::OnCharacterStuckInGeometry(const FHitResult* Hit)
{
	if (CharacterMovementCVars::StuckWarningPeriod >= 0)
	{
		UWorld* MyWorld = GetWorld();
		const float RealTimeSeconds = MyWorld->GetRealTimeSeconds();
		if ((RealTimeSeconds - LastStuckWarningTime) >= CharacterMovementCVars::StuckWarningPeriod)
		{
			LastStuckWarningTime = RealTimeSeconds;
			if (Hit == nullptr)
			{
				UE_LOG(LogCharacterMovement, Log, TEXT("%s is stuck and failed to move! (%d other events since notify)"), *CharacterOwner->GetName(), StuckWarningCountSinceNotify);
			}
			else
			{
				UE_LOG(LogCharacterMovement, Log, TEXT("%s is stuck and failed to move! Velocity: X=%3.2f Y=%3.2f Z=%3.2f Location: X=%3.2f Y=%3.2f Z=%3.2f Normal: X=%3.2f Y=%3.2f Z=%3.2f PenetrationDepth:%.3f Actor:%s Component:%s BoneName:%s (%d other events since notify)"),
					   *GetNameSafe(CharacterOwner),
					   Velocity.X, Velocity.Y, Velocity.Z,
					   Hit->Location.X, Hit->Location.Y, Hit->Location.Z,
					   Hit->Normal.X, Hit->Normal.Y, Hit->Normal.Z,
					   Hit->PenetrationDepth,
					   *GetNameSafe(Hit->GetActor()),
					   *GetNameSafe(Hit->GetComponent()),
					   Hit->BoneName.IsValid() ? *Hit->BoneName.ToString() : TEXT("None"),
					   StuckWarningCountSinceNotify
					   );
			}
			StuckWarningCountSinceNotify = 0;
		}
		else
		{
			StuckWarningCountSinceNotify += 1;
		}
	}

	// Don't update velocity based on our (failed) change in position this update since we're stuck.
	bJustTeleported = true;
}

void UCharacterMovementComponent::MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult)
{
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	// Move along the current floor
	const FVector Delta = FVector(InVelocity.X, InVelocity.Y, 0.f) * DeltaSeconds;
	FHitResult Hit(1.f);
	FVector RampVector = ComputeGroundMovementDelta(Delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
	SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);
	float LastMoveTimeSlice = DeltaSeconds;
	
	if (Hit.bStartPenetrating)
	{
		// Allow this hit to be used as an impact we can deflect off, otherwise we do nothing the rest of the update and appear to hitch.
		HandleImpact(Hit);
		SlideAlongSurface(Delta, 1.f, Hit.Normal, Hit, true);

		if (Hit.bStartPenetrating)
		{
			OnCharacterStuckInGeometry(&Hit);
		}
	}
	else if (Hit.IsValidBlockingHit())
	{
		// We impacted something (most likely another ramp, but possibly a barrier).
		float PercentTimeApplied = Hit.Time;
		if ((Hit.Time > 0.f) && (Hit.Normal.Z > KINDA_SMALL_NUMBER) && IsWalkable(Hit))
		{
			// Another walkable ramp.
			const float InitialPercentRemaining = 1.f - PercentTimeApplied;
			RampVector = ComputeGroundMovementDelta(Delta * InitialPercentRemaining, Hit, false);
			LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
			SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);

			const float SecondHitPercent = Hit.Time * InitialPercentRemaining;
			PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
		}

		if (Hit.IsValidBlockingHit())
		{
			if (CanStepUp(Hit) || (CharacterOwner->GetMovementBase() != NULL && CharacterOwner->GetMovementBase()->GetOwner() == Hit.GetActor()))
			{
				// hit a barrier, try to step up
				const FVector GravDir(0.f, 0.f, -1.f);
				if (!StepUp(GravDir, Delta * (1.f - PercentTimeApplied), Hit, OutStepDownResult))
				{
					UE_LOG(LogCharacterMovement, Verbose, TEXT("- StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					HandleImpact(Hit, LastMoveTimeSlice, RampVector);
					SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
				}
				else
				{
					// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
					UE_LOG(LogCharacterMovement, Verbose, TEXT("+ StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					bJustTeleported |= !bMaintainHorizontalGroundVelocity;
				}
			}
			else if ( Hit.Component.IsValid() && !Hit.Component.Get()->CanCharacterStepUp(CharacterOwner) )
			{
				HandleImpact(Hit, LastMoveTimeSlice, RampVector);
				SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
			}
		}
	}
}


void UCharacterMovementComponent::MaintainHorizontalGroundVelocity()
{
	if (Velocity.Z != 0.f)
	{
		if (bMaintainHorizontalGroundVelocity)
		{
			// Ramp movement already maintained the velocity, so we just want to remove the vertical component.
			Velocity.Z = 0.f;
		}
		else
		{
			// Rescale velocity to be horizontal but maintain magnitude of last update.
			Velocity = Velocity.GetSafeNormal2D() * Velocity.Size();
		}
	}
}


void UCharacterMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_CharPhysWalking);


	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->Role != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		SetMovementMode(MOVE_Walking);
		return;
	}

	checkCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN before Iteration (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
	
	bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float remainingTime = deltaTime;

	// Perform the move
	while ( (remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) && CharacterOwner && (CharacterOwner->Controller || bRunPhysicsWithNoController || HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity() || (CharacterOwner->Role == ROLE_SimulatedProxy)) )
	{
		Iterations++;
		bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		// Save current values
		UPrimitiveComponent * const OldBase = GetMovementBase();
		const FVector PreviousBaseLocation = (OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FFindFloorResult OldFloor = CurrentFloor;

		RestorePreAdditiveRootMotionVelocity();

		// Ensure velocity is horizontal.
		MaintainHorizontalGroundVelocity();
		const FVector OldVelocity = Velocity;
		Acceleration.Z = 0.f;

		// Apply acceleration
		if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
		{
			CalcVelocity(timeTick, GroundFriction, false, GetMaxBrakingDeceleration());
			checkCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
		}
		
		ApplyRootMotionToVelocity(timeTick);
		checkCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after Root Motion application (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

		if( IsFalling() )
		{
			// Root motion could have put us into Falling.
			// No movement has taken place this movement tick so we pass on full time/past iteration count
			StartNewPhysics(remainingTime+timeTick, Iterations-1);
			return;
		}

		// Compute move parameters
		const FVector MoveVelocity = Velocity;
		const FVector Delta = timeTick * MoveVelocity;
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownResult StepDownResult;

		if ( bZeroDelta )
		{
			remainingTime = 0.f;
		}
		else
		{
			// try to move forward
			MoveAlongFloor(MoveVelocity, timeTick, &StepDownResult);

			if ( IsFalling() )
			{
				// pawn decided to jump up
				const float DesiredDist = Delta.Size();
				if (DesiredDist > KINDA_SMALL_NUMBER)
				{
					const float ActualDist = (UpdatedComponent->GetComponentLocation() - OldLocation).Size2D();
					remainingTime += timeTick * (1.f - FMath::Min(1.f,ActualDist/DesiredDist));
				}
				StartNewPhysics(remainingTime,Iterations);
				return;
			}
			else if ( IsSwimming() ) //just entered water
			{
				StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		// Update floor.
		// StepUp might have already done it for us.
		if (StepDownResult.bComputedFloor)
		{
			CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, bZeroDelta, NULL);
		}

		// check for ledges here
		const bool bCheckLedges = !CanWalkOffLedges();
		if ( bCheckLedges && !CurrentFloor.IsWalkableFloor() )
		{
			// calculate possible alternate movement
			const FVector GravDir = FVector(0.f,0.f,-1.f);
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta, GravDir);
			if ( !NewDelta.IsZero() )
			{
				// first revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, false);

				// avoid repeated ledge moves if the first one fails
				bTriedLedgeMove = true;

				// Try new movement direction
				Velocity = NewDelta/timeTick;
				remainingTime += timeTick;
				continue;
			}
			else
			{
				// see if it is OK to jump
				// @todo collision : only thing that can be problem is that oldbase has world collision on
				bool bMustJump = bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ( (bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump) )
				{
					return;
				}
				bCheckedFall = true;

				// revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
				remainingTime = 0.f;
				break;
			}
		}
		else
		{
			// Validate the floor check
			if (CurrentFloor.IsWalkableFloor())
			{
				if (ShouldCatchAir(OldFloor, CurrentFloor))
				{
					CharacterOwner->OnWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
					if (IsMovingOnGround())
					{
						// If still walking, then fall. If not, assume the user set a different mode they want to keep.
						StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation);
					}
					return;
				}

				AdjustFloorHeight();
				SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
			}
			else if (CurrentFloor.HitResult.bStartPenetrating && remainingTime <= 0.f)
			{
				// The floor check failed because it started in penetration
				// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
				FHitResult Hit(CurrentFloor.HitResult);
				Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, MAX_FLOOR_DIST);
				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
				bForceNextFloorCheck = true;
			}

			// check if just entered water
			if ( IsSwimming() )
			{
				StartSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}

			// See if we need to start falling.
			if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
			{
				const bool bMustJump = bJustTeleported || bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump) )
				{
					return;
				}
				bCheckedFall = true;
			}
		}


		// Allow overlap events and such to change physics state and velocity
		if (IsMovingOnGround())
		{
			// Make velocity reflect actual move
			if( !bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && timeTick >= MIN_TICK_TIME)
			{
				// TODO-RootMotionSource: Allow this to happen during partial override Velocity, but only set allowed axes?
				Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / timeTick;
			}
		}

		// If we didn't move at all this iteration then abort (since future iterations will also be stuck).
		if (UpdatedComponent->GetComponentLocation() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}	
	}

	if (IsMovingOnGround())
	{
		MaintainHorizontalGroundVelocity();
	}
}

void UCharacterMovementComponent::PhysNavWalking(float deltaTime, int32 Iterations)
{
	SCOPE_CYCLE_COUNTER(STAT_CharPhysNavWalking);

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if ((!CharacterOwner || !CharacterOwner->Controller) && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	// Ensure velocity is horizontal.
	MaintainHorizontalGroundVelocity();
	checkCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysNavWalking: Velocity contains NaN before CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

	//bound acceleration
	Acceleration.Z = 0.f;
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		CalcVelocity(deltaTime, GroundFriction, false, GetMaxBrakingDeceleration());
		checkCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysNavWalking: Velocity contains NaN after CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
	}

	ApplyRootMotionToVelocity(deltaTime);

	if( IsFalling() )
	{
		// Root motion could have put us into Falling
		StartNewPhysics(deltaTime, Iterations);
		return;
	}

	Iterations++;

	FVector DesiredMove = Velocity;
	DesiredMove.Z = 0.f;

	const FVector OldLocation = GetActorFeetLocation();
	const FVector DeltaMove = DesiredMove * deltaTime;

	FVector AdjustedDest = OldLocation + DeltaMove;
	FNavLocation DestNavLocation;

	bool bSameNavLocation = false;
	if (CachedNavLocation.NodeRef != INVALID_NAVNODEREF)
	{
		if (bProjectNavMeshWalking)
		{
			const float DistSq2D = (OldLocation - CachedNavLocation.Location).SizeSquared2D();
			const float DistZ = FMath::Abs(OldLocation.Z - CachedNavLocation.Location.Z);

			const float TotalCapsuleHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;
			const float ProjectionScale = (OldLocation.Z > CachedNavLocation.Location.Z) ? NavMeshProjectionHeightScaleUp : NavMeshProjectionHeightScaleDown;
			const float DistZThr = TotalCapsuleHeight * FMath::Max(0.f, ProjectionScale);

			bSameNavLocation = (DistSq2D <= KINDA_SMALL_NUMBER) && (DistZ < DistZThr);
		}
		else
		{
			bSameNavLocation = CachedNavLocation.Location.Equals(OldLocation);
		}
	}
		

	if (DeltaMove.IsNearlyZero() && bSameNavLocation)
	{
		DestNavLocation = CachedNavLocation;
		UE_LOG(LogNavMeshMovement, VeryVerbose, TEXT("%s using cached navmesh location! (bProjectNavMeshWalking = %d)"), *GetNameSafe(CharacterOwner), bProjectNavMeshWalking);
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_CharNavProjectPoint);

		// Start the trace from the Z location of the last valid trace.
		// Otherwise if we are projecting our location to the underlying geometry and it's far above or below the navmesh,
		// we'll follow that geometry's plane out of range of valid navigation.
		if (bSameNavLocation && bProjectNavMeshWalking)
		{
			AdjustedDest.Z = CachedNavLocation.Location.Z;
		}

		// Find the point on the NavMesh
		const bool bHasNavigationData = FindNavFloor(AdjustedDest, DestNavLocation);
		if (!bHasNavigationData)
		{
			SetMovementMode(MOVE_Walking);
			return;
		}

		CachedNavLocation = DestNavLocation;
	}

	if (DestNavLocation.NodeRef != INVALID_NAVNODEREF)
	{
		FVector NewLocation(AdjustedDest.X, AdjustedDest.Y, DestNavLocation.Location.Z);
		if (bProjectNavMeshWalking)
		{
			SCOPE_CYCLE_COUNTER(STAT_CharNavProjectLocation);
			const float TotalCapsuleHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f;
			const float UpOffset = TotalCapsuleHeight * FMath::Max(0.f, NavMeshProjectionHeightScaleUp);
			const float DownOffset = TotalCapsuleHeight * FMath::Max(0.f, NavMeshProjectionHeightScaleDown);
			NewLocation = ProjectLocationFromNavMesh(deltaTime, OldLocation, NewLocation, UpOffset, DownOffset);
		}

		FVector AdjustedDelta = NewLocation - OldLocation;

		if (!AdjustedDelta.IsNearlyZero())
		{
			FHitResult HitResult;
			SafeMoveUpdatedComponent(AdjustedDelta, UpdatedComponent->GetComponentQuat(), bSweepWhileNavWalking, HitResult);
		}

		// Update velocity to reflect actual move
		if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasVelocity())
		{
			Velocity = (GetActorFeetLocation() - OldLocation) / deltaTime;
			MaintainHorizontalGroundVelocity();
		}

		bJustTeleported = false;
	}
	else
	{
		StartFalling(Iterations, deltaTime, deltaTime, DeltaMove, OldLocation);
	}
}

bool UCharacterMovementComponent::FindNavFloor(const FVector& TestLocation, FNavLocation& NavFloorLocation) const
{
	const ANavigationData* NavData = GetNavData();
	if (NavData == nullptr)
	{
		return false;
	}

	INavAgentInterface* MyNavAgent = CastChecked<INavAgentInterface>(CharacterOwner);
	float SearchRadius = 0.0f;
	float SearchHeight = 100.0f;
	if (MyNavAgent)
	{
		const FNavAgentProperties& AgentProps = MyNavAgent->GetNavAgentPropertiesRef();
		SearchRadius = AgentProps.AgentRadius * 2.0f;
		SearchHeight = AgentProps.AgentHeight * AgentProps.NavWalkingSearchHeightScale;
	}

	return NavData->ProjectPoint(TestLocation, NavFloorLocation, FVector(SearchRadius, SearchRadius, SearchHeight));
}

FVector UCharacterMovementComponent::ProjectLocationFromNavMesh(float DeltaSeconds, const FVector& CurrentFeetLocation, const FVector& TargetNavLocation, float UpOffset, float DownOffset)
{
	SCOPE_CYCLE_COUNTER(STAT_CharNavProjectLocation);

	FVector NewLocation = TargetNavLocation;

	const float ZOffset = -(DownOffset + UpOffset);
	if (ZOffset > -SMALL_NUMBER)
	{
		return NewLocation;
	}

	const FVector TraceStart = FVector(TargetNavLocation.X, TargetNavLocation.Y, TargetNavLocation.Z + UpOffset);
	const FVector TraceEnd   = FVector(TargetNavLocation.X, TargetNavLocation.Y, TargetNavLocation.Z - DownOffset);

	// We can skip this trace if we are checking at the same location as the last trace (ie, we haven't moved).
	const bool bCachedLocationStillValid = (CachedProjectedNavMeshHitResult.bBlockingHit &&
											CachedProjectedNavMeshHitResult.TraceStart == TraceStart &&
											CachedProjectedNavMeshHitResult.TraceEnd == TraceEnd);

	NavMeshProjectionTimer -= DeltaSeconds;
	if (NavMeshProjectionTimer <= 0.0f)
	{
		if (!bCachedLocationStillValid || bAlwaysCheckFloor)
		{
			UE_LOG(LogNavMeshMovement, VeryVerbose, TEXT("ProjectLocationFromNavMesh(): %s interval: %.3f velocity: %s"), *GetNameSafe(CharacterOwner), NavMeshProjectionInterval, *Velocity.ToString());

			FHitResult HitResult;
			FindBestNavMeshLocation(TraceStart, TraceEnd, CurrentFeetLocation, TargetNavLocation, HitResult);

			// discard result if we were already inside something			
			if (HitResult.bStartPenetrating || !HitResult.bBlockingHit)
			{
				CachedProjectedNavMeshHitResult.Reset();
			}
			else
			{
				CachedProjectedNavMeshHitResult = HitResult;
			}
		}
		else
		{
			UE_LOG(LogNavMeshMovement, VeryVerbose, TEXT("ProjectLocationFromNavMesh(): %s interval: %.3f velocity: %s [SKIP TRACE]"), *GetNameSafe(CharacterOwner), NavMeshProjectionInterval, *Velocity.ToString());
		}

		// Wrap around to maintain same relative offset to tick time changes.
		// Prevents large framerate spikes from aligning multiple characters to the same frame (if they start staggered, they will now remain staggered).
		float ModTime = 0.f;
		if (NavMeshProjectionInterval > SMALL_NUMBER)
		{
			ModTime = FMath::Fmod(-NavMeshProjectionTimer, NavMeshProjectionInterval);
		}

		NavMeshProjectionTimer = NavMeshProjectionInterval - ModTime;
	}
	
	// Project to last plane we found.
	if (CachedProjectedNavMeshHitResult.bBlockingHit)
	{
		if (bCachedLocationStillValid && FMath::IsNearlyEqual(CurrentFeetLocation.Z, CachedProjectedNavMeshHitResult.ImpactPoint.Z, 0.01f))
		{
			// Already at destination.
			NewLocation.Z = CurrentFeetLocation.Z;
		}
		else
		{
			//const FVector ProjectedPoint = FMath::LinePlaneIntersection(TraceStart, TraceEnd, CachedProjectedNavMeshHitResult.ImpactPoint, CachedProjectedNavMeshHitResult.Normal);
			//float ProjectedZ = ProjectedPoint.Z;

			// Optimized assuming we only care about Z coordinate of result.
			const FVector& PlaneOrigin = CachedProjectedNavMeshHitResult.ImpactPoint;
			const FVector& PlaneNormal = CachedProjectedNavMeshHitResult.Normal;
			float ProjectedZ = TraceStart.Z + ZOffset * (((PlaneOrigin - TraceStart)|PlaneNormal) / (ZOffset * PlaneNormal.Z));

			// Limit to not be too far above or below NavMesh location
			ProjectedZ = FMath::Clamp(ProjectedZ, TraceEnd.Z, TraceStart.Z);

			// Interp for smoother updates (less "pop" when trace hits something new). 0 interp speed is instant.
			const float InterpSpeed = FMath::Max(0.f, NavMeshProjectionInterpSpeed);
			ProjectedZ = FMath::FInterpTo(CurrentFeetLocation.Z, ProjectedZ, DeltaSeconds, InterpSpeed);
			ProjectedZ = FMath::Clamp(ProjectedZ, TraceEnd.Z, TraceStart.Z);

			// Final result
			NewLocation.Z = ProjectedZ;
		}
	}

	return NewLocation;
}

void UCharacterMovementComponent::FindBestNavMeshLocation(const FVector& TraceStart, const FVector& TraceEnd, const FVector& CurrentFeetLocation, const FVector& TargetNavLocation, FHitResult& OutHitResult) const
{
	// raycast to underlying mesh to allow us to more closely follow geometry
	// we use static objects here as a best approximation to accept only objects that
	// influence navmesh generation
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ProjectLocation), false);

	// blocked by world static and optionally world dynamic
	FCollisionResponseParams ResponseParams(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Overlap);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldDynamic, bProjectNavMeshOnBothWorldChannels ? ECR_Overlap : ECR_Ignore);

	TArray<FHitResult> MultiTraceHits;
	GetWorld()->LineTraceMultiByChannel(MultiTraceHits, TraceStart, TraceEnd, ECC_WorldStatic, Params, ResponseParams);

	struct FCompareFHitResultNavMeshTrace
	{
		explicit FCompareFHitResultNavMeshTrace(const FVector& inSourceLocation) : SourceLocation(inSourceLocation)
		{
		}

		FORCEINLINE bool operator()(const FHitResult& A, const FHitResult& B) const
		{
			const float ADistSqr = (SourceLocation - A.ImpactPoint).SizeSquared();
			const float BDistSqr = (SourceLocation - B.ImpactPoint).SizeSquared();

			return (ADistSqr < BDistSqr);
		}

		const FVector& SourceLocation;
	};

	struct FRemoveNotBlockingResponseNavMeshTrace
	{
		FRemoveNotBlockingResponseNavMeshTrace(bool bInCheckOnlyWorldStatic) : bCheckOnlyWorldStatic(bInCheckOnlyWorldStatic) {}

		FORCEINLINE bool operator()(const FHitResult& TestHit) const
		{
			UPrimitiveComponent* PrimComp = TestHit.GetComponent();
			const bool bBlockOnWorldStatic = PrimComp && (PrimComp->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block);
			const bool bBlockOnWorldDynamic = PrimComp && (PrimComp->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Block);

			return !bBlockOnWorldStatic && (!bBlockOnWorldDynamic || bCheckOnlyWorldStatic);
		}

		bool bCheckOnlyWorldStatic;
	};

	MultiTraceHits.RemoveAllSwap(FRemoveNotBlockingResponseNavMeshTrace(!bProjectNavMeshOnBothWorldChannels), /*bAllowShrinking*/false);
	if (MultiTraceHits.Num() > 0)
	{
		// Sort the hits by the closest to our origin.
		MultiTraceHits.Sort(FCompareFHitResultNavMeshTrace(TargetNavLocation));

		// Cache the closest hit and treat it as a blocking hit (we used an overlap to get all the world static hits so we could sort them ourselves)
		OutHitResult = MultiTraceHits[0];
		OutHitResult.bBlockingHit = true;
	}
}

const ANavigationData* UCharacterMovementComponent::GetNavData() const
{
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys == nullptr || !HasValidData())
	{
		return nullptr;
	}

	const ANavigationData* NavData = nullptr;
	INavAgentInterface* MyNavAgent = CastChecked<INavAgentInterface>(CharacterOwner);
	if (MyNavAgent)
	{
		const FNavAgentProperties& AgentProps = MyNavAgent->GetNavAgentPropertiesRef();
		NavData = NavSys->GetNavDataForProps(AgentProps);
	}
	if (NavData == nullptr)
	{
		NavData = NavSys->GetMainNavData();
	}

	// Only RecastNavMesh supported
	const ARecastNavMesh* NavMeshData = Cast<const ARecastNavMesh>(NavData);
	if (NavMeshData == nullptr)
	{
		return nullptr;
	}

	return NavData;
}


void UCharacterMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (CharacterOwner)
	{
		CharacterOwner->K2_UpdateCustomMovement(deltaTime);
	}
}


bool UCharacterMovementComponent::ShouldCatchAir(const FFindFloorResult& OldFloor, const FFindFloorResult& NewFloor)
{
	return false;
}

void UCharacterMovementComponent::AdjustFloorHeight()
{
	SCOPE_CYCLE_COUNTER(STAT_CharAdjustFloorHeight);

	// If we have a floor check that hasn't hit anything, don't adjust height.
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	float OldFloorDist = CurrentFloor.FloorDist;
	if (CurrentFloor.bLineTrace)
	{
		if (OldFloorDist < MIN_FLOOR_DIST && CurrentFloor.LineDist >= MIN_FLOOR_DIST)
		{
			// This would cause us to scale unwalkable walls
			UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("Adjust floor height aborting due to line trace with small floor distance (line: %.2f, sweep: %.2f)"), CurrentFloor.LineDist, CurrentFloor.FloorDist);
			return;
		}
		else
		{
			// Falling back to a line trace means the sweep was unwalkable (or in penetration). Use the line distance for the vertical adjustment.
			OldFloorDist = CurrentFloor.LineDist;
		}
	}

	// Move up or down to maintain floor height.
	if (OldFloorDist < MIN_FLOOR_DIST || OldFloorDist > MAX_FLOOR_DIST)
	{
		FHitResult AdjustHit(1.f);
		const float InitialZ = UpdatedComponent->GetComponentLocation().Z;
		const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;
		const float MoveDist = AvgFloorDist - OldFloorDist;
		SafeMoveUpdatedComponent( FVector(0.f,0.f,MoveDist), UpdatedComponent->GetComponentQuat(), true, AdjustHit );
		UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("Adjust floor height %.3f (Hit = %d)"), MoveDist, AdjustHit.bBlockingHit);

		if (!AdjustHit.IsValidBlockingHit())
		{
			CurrentFloor.FloorDist += MoveDist;
		}
		else if (MoveDist > 0.f)
		{
			const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;
			CurrentFloor.FloorDist += CurrentZ - InitialZ;
		}
		else
		{
			checkSlow(MoveDist < 0.f);
			const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;
			CurrentFloor.FloorDist = CurrentZ - AdjustHit.Location.Z;
			if (IsWalkable(AdjustHit))
			{
				CurrentFloor.SetFromSweep(AdjustHit, CurrentFloor.FloorDist, true);
			}
		}

		// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
		// Also avoid it if we moved out of penetration
		bJustTeleported |= !bMaintainHorizontalGroundVelocity || (OldFloorDist < 0.f);
		
		// If something caused us to adjust our height (especially a depentration) we should ensure another check next frame or we will keep a stale result.
		bForceNextFloorCheck = true;
	}
}


void UCharacterMovementComponent::StopActiveMovement() 
{ 
	Super::StopActiveMovement();

	Acceleration = FVector::ZeroVector; 
	bHasRequestedVelocity = false;
	RequestedVelocity = FVector::ZeroVector;
}

void UCharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if( CharacterOwner && CharacterOwner->ShouldNotifyLanded(Hit) )
	{
		CharacterOwner->Landed(Hit);
	}
	if( IsFalling() )
	{
		if (GroundMovementMode == MOVE_NavWalking)
		{
			// verify navmesh projection and current floor
			// otherwise movement will be stuck in infinite loop:
			// navwalking -> (no navmesh) -> falling -> (standing on something) -> navwalking -> ....

			const FVector TestLocation = GetActorFeetLocation();
			FNavLocation NavLocation;

			const bool bHasNavigationData = FindNavFloor(TestLocation, NavLocation);
			if (!bHasNavigationData || NavLocation.NodeRef == INVALID_NAVNODEREF)
			{
				GroundMovementMode = MOVE_Walking;
				UE_LOG(LogNavMeshMovement, Verbose, TEXT("ProcessLanded(): %s tried to go to NavWalking but couldn't find NavMesh! Using Walking instead."), *GetNameSafe(CharacterOwner));
			}
		}

		SetPostLandedPhysics(Hit);
	}
	if (PathFollowingComp.IsValid())
	{
		PathFollowingComp->OnLanded();
	}

	StartNewPhysics(remainingTime, Iterations);
}

void UCharacterMovementComponent::SetPostLandedPhysics(const FHitResult& Hit)
{
	if( CharacterOwner )
	{
		if (CanEverSwim() && IsInWater())
		{
			SetMovementMode(MOVE_Swimming);
		}
		else
		{
			const FVector PreImpactAccel = Acceleration + (IsFalling() ? FVector(0.f, 0.f, GetGravityZ()) : FVector::ZeroVector);
			const FVector PreImpactVelocity = Velocity;

			if (DefaultLandMovementMode == MOVE_Walking ||
				DefaultLandMovementMode == MOVE_NavWalking ||
				DefaultLandMovementMode == MOVE_Falling)
			{
				SetMovementMode(GroundMovementMode);
			}
			else
			{
				SetDefaultMovementMode();
			}
			
			ApplyImpactPhysicsForces(Hit, PreImpactAccel, PreImpactVelocity);
		}
	}
}

void UCharacterMovementComponent::SetNavWalkingPhysics(bool bEnable)
{
	if (UpdatedPrimitive)
	{
		if (bEnable)
		{
			UpdatedPrimitive->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
			UpdatedPrimitive->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
			CachedProjectedNavMeshHitResult.Reset();

			// Stagger timed updates so many different characters spawned at the same time don't update on the same frame.
			// Initially we want an immediate update though, so set time to a negative randomized range.
			NavMeshProjectionTimer = (NavMeshProjectionInterval > 0.f) ? FMath::FRandRange(-NavMeshProjectionInterval, 0.f) : 0.f;
		}
		else
		{
			UPrimitiveComponent* DefaultCapsule = nullptr;
			if (CharacterOwner && CharacterOwner->GetCapsuleComponent() == UpdatedComponent)
			{
				ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
				DefaultCapsule = DefaultCharacter ? DefaultCharacter->GetCapsuleComponent() : nullptr;
			}

			if (DefaultCapsule)
			{
				UpdatedPrimitive->SetCollisionResponseToChannel(ECC_WorldStatic, DefaultCapsule->GetCollisionResponseToChannel(ECC_WorldStatic));
				UpdatedPrimitive->SetCollisionResponseToChannel(ECC_WorldDynamic, DefaultCapsule->GetCollisionResponseToChannel(ECC_WorldDynamic));
			}
			else
			{
				UE_LOG(LogCharacterMovement, Warning, TEXT("Can't revert NavWalking collision settings for %s.%s"),
					*GetNameSafe(CharacterOwner), *GetNameSafe(UpdatedComponent));
			}
		}
	}
}

bool UCharacterMovementComponent::TryToLeaveNavWalking()
{
	SetNavWalkingPhysics(false);

	bool bSucceeded = true;
	if (CharacterOwner)
	{
		FVector CollisionFreeLocation = UpdatedComponent->GetComponentLocation();
		bSucceeded = GetWorld()->FindTeleportSpot(CharacterOwner, CollisionFreeLocation, UpdatedComponent->GetComponentRotation());
		if (bSucceeded)
		{
			CharacterOwner->SetActorLocation(CollisionFreeLocation);
		}
		else
		{
			SetNavWalkingPhysics(true);
		}
	}

	if (MovementMode == MOVE_NavWalking && bSucceeded)
	{
		SetMovementMode(DefaultLandMovementMode != MOVE_NavWalking ? DefaultLandMovementMode.GetValue() : MOVE_Walking);
	}
	else if (MovementMode != MOVE_NavWalking && !bSucceeded)
	{
		SetMovementMode(MOVE_NavWalking);
	}

	bWantsToLeaveNavWalking = !bSucceeded;
	return bSucceeded;
}

void UCharacterMovementComponent::OnTeleported()
{
	if (!HasValidData())
	{
		return;
	}

	bJustTeleported = true;

	// Find floor at current location
	UpdateFloorFromAdjustment();

	// Validate it. We don't want to pop down to walking mode from very high off the ground, but we'd like to keep walking if possible.
	UPrimitiveComponent* OldBase = CharacterOwner->GetMovementBase();
	UPrimitiveComponent* NewBase = NULL;
	
	if (OldBase && CurrentFloor.IsWalkableFloor() && CurrentFloor.FloorDist <= MAX_FLOOR_DIST && Velocity.Z <= 0.f)
	{
		// Close enough to land or just keep walking.
		NewBase = CurrentFloor.HitResult.Component.Get();
	}
	else
	{
		CurrentFloor.Clear();
	}

	const bool bWasFalling = (MovementMode == MOVE_Falling);
	const bool bWasSwimming = (MovementMode == DefaultWaterMovementMode) || (MovementMode == MOVE_Swimming);

	if (CanEverSwim() && IsInWater())
	{
		if (!bWasSwimming)
		{
			SetMovementMode(DefaultWaterMovementMode);
		}
	}
	else if (!CurrentFloor.IsWalkableFloor() || (OldBase && !NewBase))
	{
		if (!bWasFalling && MovementMode != MOVE_Flying && MovementMode != MOVE_Custom)
		{
			SetMovementMode(MOVE_Falling);
		}
	}
	else if (NewBase)
	{
		if (bWasSwimming)
		{
			SetMovementMode(DefaultLandMovementMode);
		}
		else if (bWasFalling)
		{
			ProcessLanded(CurrentFloor.HitResult, 0.f, 0);
		}
	}

	MaybeSaveBaseLocation();
}

float GetAxisDeltaRotation(float InAxisRotationRate, float DeltaTime)
{
	return (InAxisRotationRate >= 0.f) ? (InAxisRotationRate * DeltaTime) : 360.f;
}

FRotator UCharacterMovementComponent::GetDeltaRotation(float DeltaTime) const
{
	return FRotator(GetAxisDeltaRotation(RotationRate.Pitch, DeltaTime), GetAxisDeltaRotation(RotationRate.Yaw, DeltaTime), GetAxisDeltaRotation(RotationRate.Roll, DeltaTime));
}

FRotator UCharacterMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const
{
	if (Acceleration.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		// AI path following request can orient us in that direction (it's effectively an acceleration)
		if (bHasRequestedVelocity && RequestedVelocity.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			return RequestedVelocity.GetSafeNormal().Rotation();
		}

		// Don't change rotation if there is no acceleration.
		return CurrentRotation;
	}

	// Rotate toward direction of acceleration.
	return Acceleration.GetSafeNormal().Rotation();
}

bool UCharacterMovementComponent::ShouldRemainVertical() const
{
	// Always remain vertical when walking or falling.
	return IsMovingOnGround() || IsFalling();
}

void UCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (!(bOrientRotationToMovement || bUseControllerDesiredRotation))
	{
		return;
	}

	if (!HasValidData() || (!CharacterOwner->Controller && !bRunPhysicsWithNoController))
	{
		return;
	}

	FRotator CurrentRotation = UpdatedComponent->GetComponentRotation(); // Normalized
	CurrentRotation.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): CurrentRotation"));

	FRotator DeltaRot = GetDeltaRotation(DeltaTime);
	DeltaRot.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): GetDeltaRotation"));

	FRotator DesiredRotation = CurrentRotation;
	if (bOrientRotationToMovement)
	{
		DesiredRotation = ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRot);
	}
	else if (CharacterOwner->Controller && bUseControllerDesiredRotation)
	{
		DesiredRotation = CharacterOwner->Controller->GetDesiredRotation();
	}
	else
	{
		return;
	}

	if (ShouldRemainVertical())
	{
		DesiredRotation.Pitch = 0.f;
		DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
		DesiredRotation.Roll = 0.f;
	}
	else
	{
		DesiredRotation.Normalize();
	}
	
	// Accumulate a desired new rotation.
	const float AngleTolerance = 1e-3f;

	if (!CurrentRotation.Equals(DesiredRotation, AngleTolerance))
	{
		// PITCH
		if (!FMath::IsNearlyEqual(CurrentRotation.Pitch, DesiredRotation.Pitch, AngleTolerance))
		{
			DesiredRotation.Pitch = FMath::FixedTurn(CurrentRotation.Pitch, DesiredRotation.Pitch, DeltaRot.Pitch);
		}

		// YAW
		if (!FMath::IsNearlyEqual(CurrentRotation.Yaw, DesiredRotation.Yaw, AngleTolerance))
		{
			DesiredRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, DesiredRotation.Yaw, DeltaRot.Yaw);
		}

		// ROLL
		if (!FMath::IsNearlyEqual(CurrentRotation.Roll, DesiredRotation.Roll, AngleTolerance))
		{
			DesiredRotation.Roll = FMath::FixedTurn(CurrentRotation.Roll, DesiredRotation.Roll, DeltaRot.Roll);
		}

		// Set the new rotation.
		DesiredRotation.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): DesiredRotation"));
		MoveUpdatedComponent( FVector::ZeroVector, DesiredRotation, true );
	}
}


void UCharacterMovementComponent::PhysicsVolumeChanged( APhysicsVolume* NewVolume )
{
	if (!HasValidData())
	{
		return;
	}
	if ( NewVolume && NewVolume->bWaterVolume )
	{
		// just entered water
		if ( !CanEverSwim() )
		{
			// AI needs to stop any current moves
			if (PathFollowingComp.IsValid())
			{
				PathFollowingComp->AbortMove(*this, FPathFollowingResultFlags::MovementStop);
			}			
		}
		else if ( !IsSwimming() )
		{
			SetMovementMode(MOVE_Swimming);
		}
	}
	else if ( IsSwimming() )
	{
		// just left the water - check if should jump out
		SetMovementMode(MOVE_Falling);
		FVector JumpDir(0.f);
		FVector WallNormal(0.f);
		if ( Acceleration.Z > 0.f && ShouldJumpOutOfWater(JumpDir)
			&& ((JumpDir | Acceleration) > 0.f) && CheckWaterJump(JumpDir, WallNormal) ) 
		{
			JumpOutOfWater(WallNormal);
			Velocity.Z = OutofWaterZ; //set here so physics uses this for remainder of tick
		}
	}
}


bool UCharacterMovementComponent::ShouldJumpOutOfWater(FVector& JumpDir)
{
	AController* OwnerController = CharacterOwner->GetController();
	if (OwnerController)
	{
		const FRotator ControllerRot = OwnerController->GetControlRotation();
		if ( (Velocity.Z > 0.0f) && (ControllerRot.Pitch > JumpOutOfWaterPitch) )
		{
			// if Pawn is going up and looking up, then make him jump
			JumpDir = ControllerRot.Vector();
			return true;
		}
	}
	
	return false;
}


void UCharacterMovementComponent::JumpOutOfWater(FVector WallNormal) {}


bool UCharacterMovementComponent::CheckWaterJump(FVector CheckPoint, FVector& WallNormal)
{
	if (!HasValidData())
	{
		return false;
	}
	// check if there is a wall directly in front of the swimming pawn
	CheckPoint.Z = 0.f;
	FVector CheckNorm = CheckPoint.GetSafeNormal();
	float PawnCapsuleRadius, PawnCapsuleHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnCapsuleRadius, PawnCapsuleHalfHeight);
	CheckPoint = UpdatedComponent->GetComponentLocation() + 1.2f * PawnCapsuleRadius * CheckNorm;
	FVector Extent(PawnCapsuleRadius, PawnCapsuleRadius, PawnCapsuleHalfHeight);
	FHitResult HitInfo(1.f);
	FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CheckWaterJump), false, CharacterOwner);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(CapsuleParams, ResponseParam);
	FCollisionShape CapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_None);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
	bool bHit = GetWorld()->SweepSingleByChannel( HitInfo, UpdatedComponent->GetComponentLocation(), CheckPoint, FQuat::Identity, CollisionChannel, CapsuleShape, CapsuleParams, ResponseParam);
	
	if ( bHit && !Cast<APawn>(HitInfo.GetActor()) )
	{
		// hit a wall - check if it is low enough
		WallNormal = -1.f * HitInfo.ImpactNormal;
		FVector Start = UpdatedComponent->GetComponentLocation();
		Start.Z += MaxOutOfWaterStepHeight;
		CheckPoint = Start + 3.2f * PawnCapsuleRadius * WallNormal;
		FCollisionQueryParams LineParams(SCENE_QUERY_STAT(CheckWaterJump), true, CharacterOwner);
		FCollisionResponseParams LineResponseParam;
		InitCollisionParams(LineParams, LineResponseParam);
		bHit = GetWorld()->LineTraceSingleByChannel( HitInfo, Start, CheckPoint, CollisionChannel, LineParams, LineResponseParam );
		// if no high obstruction, or it's a valid floor, then pawn can jump out of water
		return !bHit || IsWalkable(HitInfo);
	}
	return false;
}

void UCharacterMovementComponent::AddImpulse( FVector Impulse, bool bVelocityChange )
{
	if (!Impulse.IsZero() && (MovementMode != MOVE_None) && IsActive() && HasValidData())
	{
		// handle scaling by mass
		FVector FinalImpulse = Impulse;
		if ( !bVelocityChange )
		{
			if (Mass > SMALL_NUMBER)
			{
				FinalImpulse = FinalImpulse / Mass;
			}
			else
			{
				UE_LOG(LogCharacterMovement, Warning, TEXT("Attempt to apply impulse to zero or negative Mass in CharacterMovement"));
			}
		}

		PendingImpulseToApply += FinalImpulse;
	}
}

void UCharacterMovementComponent::AddForce( FVector Force )
{
	if (!Force.IsZero() && (MovementMode != MOVE_None) && IsActive() && HasValidData())
	{
		if (Mass > SMALL_NUMBER)
		{
			PendingForceToApply += Force / Mass;
		}
		else
		{
			UE_LOG(LogCharacterMovement, Warning, TEXT("Attempt to apply force to zero or negative Mass in CharacterMovement"));
		}
	}
}

void UCharacterMovementComponent::MoveSmooth(const FVector& InVelocity, const float DeltaSeconds, FStepDownResult* OutStepDownResult)
{
	if (!HasValidData())
	{
		return;
	}

	// Custom movement mode.
	// Custom movement may need an update even if there is zero velocity.
	if (MovementMode == MOVE_Custom)
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);
		PhysCustom(DeltaSeconds, 0);
		return;
	}

	FVector Delta = InVelocity * DeltaSeconds;
	if (Delta.IsZero())
	{
		return;
	}

	FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

	if (IsMovingOnGround())
	{
		MoveAlongFloor(InVelocity, DeltaSeconds, OutStepDownResult);
	}
	else
	{
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	
		if (Hit.IsValidBlockingHit())
		{
			bool bSteppedUp = false;

			if (IsFlying())
			{
				if (CanStepUp(Hit))
				{
					OutStepDownResult = NULL; // No need for a floor when not walking.
					if (FMath::Abs(Hit.ImpactNormal.Z) < 0.2f)
					{
						const FVector GravDir = FVector(0.f,0.f,-1.f);
						const FVector DesiredDir = Delta.GetSafeNormal();
						const float UpDown = GravDir | DesiredDir;
						if ((UpDown < 0.5f) && (UpDown > -0.2f))
						{
							bSteppedUp = StepUp(GravDir, Delta * (1.f - Hit.Time), Hit, OutStepDownResult);
						}			
					}
				}
			}
				
			// If StepUp failed, try sliding.
			if (!bSteppedUp)
			{
				SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, false);
			}
		}
	}
}


bool UCharacterMovementComponent::IsWalkable(const FHitResult& Hit) const
{
	if (!Hit.IsValidBlockingHit())
	{
		// No hit, or starting in penetration
		return false;
	}

	// Never walk up vertical surfaces.
	if (Hit.ImpactNormal.Z < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	float TestWalkableZ = WalkableFloorZ;

	// See if this component overrides the walkable floor z.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (HitComponent)
	{
		const FWalkableSlopeOverride& SlopeOverride = HitComponent->GetWalkableSlopeOverride();
		TestWalkableZ = SlopeOverride.ModifyWalkableFloorZ(TestWalkableZ);
	}

	// Can't walk on this surface if it is too steep.
	if (Hit.ImpactNormal.Z < TestWalkableZ)
	{
		return false;
	}

	return true;
}

void UCharacterMovementComponent::SetWalkableFloorAngle(float InWalkableFloorAngle)
{
	WalkableFloorAngle = FMath::Clamp(InWalkableFloorAngle, 0.f, 90.0f);
	WalkableFloorZ = FMath::Cos(FMath::DegreesToRadians(WalkableFloorAngle));
}

void UCharacterMovementComponent::SetWalkableFloorZ(float InWalkableFloorZ)
{
	WalkableFloorZ = FMath::Clamp(InWalkableFloorZ, 0.f, 1.f);
	WalkableFloorAngle = FMath::RadiansToDegrees(FMath::Acos(WalkableFloorZ));
}

float UCharacterMovementComponent::K2_GetWalkableFloorAngle() const
{
	return GetWalkableFloorAngle();
}

float UCharacterMovementComponent::K2_GetWalkableFloorZ() const
{
	return GetWalkableFloorZ();
}



bool UCharacterMovementComponent::IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, const float CapsuleRadius) const
{
	const float DistFromCenterSq = (TestImpactPoint - CapsuleLocation).SizeSquared2D();
	const float ReducedRadiusSq = FMath::Square(FMath::Max(SWEEP_EDGE_REJECT_DISTANCE + KINDA_SMALL_NUMBER, CapsuleRadius - SWEEP_EDGE_REJECT_DISTANCE));
	return DistFromCenterSq < ReducedRadiusSq;
}


void UCharacterMovementComponent::ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance, FFindFloorResult& OutFloorResult, float SweepRadius, const FHitResult* DownwardSweepResult) const
{
	OutFloorResult.Clear();

	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	bool bSkipSweep = false;
	if (DownwardSweepResult != NULL && DownwardSweepResult->IsValidBlockingHit())
	{
		// Only if the supplied sweep was vertical and downward.
		if ((DownwardSweepResult->TraceStart.Z > DownwardSweepResult->TraceEnd.Z) &&
			(DownwardSweepResult->TraceStart - DownwardSweepResult->TraceEnd).SizeSquared2D() <= KINDA_SMALL_NUMBER)
		{
			// Reject hits that are barely on the cusp of the radius of the capsule
			if (IsWithinEdgeTolerance(DownwardSweepResult->Location, DownwardSweepResult->ImpactPoint, PawnRadius))
			{
				// Don't try a redundant sweep, regardless of whether this sweep is usable.
				bSkipSweep = true;

				const bool bIsWalkable = IsWalkable(*DownwardSweepResult);
				const float FloorDist = (CapsuleLocation.Z - DownwardSweepResult->Location.Z);
				OutFloorResult.SetFromSweep(*DownwardSweepResult, FloorDist, bIsWalkable);
				
				if (bIsWalkable)
				{
					// Use the supplied downward sweep as the floor hit result.			
					return;
				}
			}
		}
	}

	// We require the sweep distance to be >= the line distance, otherwise the HitResult can't be interpreted as the sweep result.
	if (SweepDistance < LineDistance)
	{
		ensure(SweepDistance >= LineDistance);
		return;
	}

	bool bBlockingHit = false;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComputeFloorDist), false, CharacterOwner);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

	// Sweep test
	if (!bSkipSweep && SweepDistance > 0.f && SweepRadius > 0.f)
	{
		// Use a shorter height to avoid sweeps giving weird results if we start on a surface.
		// This also allows us to adjust out of penetrations.
		const float ShrinkScale = 0.9f;
		const float ShrinkScaleOverlap = 0.1f;
		float ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScale);
		float TraceDist = SweepDistance + ShrinkHeight;
		FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(SweepRadius, PawnHalfHeight - ShrinkHeight);

		FHitResult Hit(1.f);
		bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation + FVector(0.f,0.f,-TraceDist), CollisionChannel, CapsuleShape, QueryParams, ResponseParam);

		if (bBlockingHit)
		{
			// Reject hits adjacent to us, we only care about hits on the bottom portion of our capsule.
			// Check 2D distance to impact point, reject if within a tolerance from radius.
			if (Hit.bStartPenetrating || !IsWithinEdgeTolerance(CapsuleLocation, Hit.ImpactPoint, CapsuleShape.Capsule.Radius))
			{
				// Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object.
				// Capsule must not be nearly zero or the trace will fall back to a line trace from the start point and have the wrong length.
				CapsuleShape.Capsule.Radius = FMath::Max(0.f, CapsuleShape.Capsule.Radius - SWEEP_EDGE_REJECT_DISTANCE - KINDA_SMALL_NUMBER);
				if (!CapsuleShape.IsNearlyZero())
				{
					ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScaleOverlap);
					TraceDist = SweepDistance + ShrinkHeight;
					CapsuleShape.Capsule.HalfHeight = FMath::Max(PawnHalfHeight - ShrinkHeight, CapsuleShape.Capsule.Radius);
					Hit.Reset(1.f, false);

					bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation + FVector(0.f,0.f,-TraceDist), CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
				}
			}

			// Reduce hit distance by ShrinkHeight because we shrank the capsule for the trace.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
			const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

			OutFloorResult.SetFromSweep(Hit, SweepResult, false);
			if (Hit.IsValidBlockingHit() && IsWalkable(Hit))
			{		
				if (SweepResult <= SweepDistance)
				{
					// Hit within test distance.
					OutFloorResult.bWalkableFloor = true;
					return;
				}
			}
		}
	}

	// Since we require a longer sweep than line trace, we don't want to run the line trace if the sweep missed everything.
	// We do however want to try a line trace if the sweep was stuck in penetration.
	if (!OutFloorResult.bBlockingHit && !OutFloorResult.HitResult.bStartPenetrating)
	{
		OutFloorResult.FloorDist = SweepDistance;
		return;
	}

	// Line trace
	if (LineDistance > 0.f)
	{
		const float ShrinkHeight = PawnHalfHeight;
		const FVector LineTraceStart = CapsuleLocation;	
		const float TraceDist = LineDistance + ShrinkHeight;
		const FVector Down = FVector(0.f, 0.f, -TraceDist);
		QueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace);

		FHitResult Hit(1.f);
		bBlockingHit = GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down, CollisionChannel, QueryParams, ResponseParam);

		if (bBlockingHit)
		{
			if (Hit.Time > 0.f)
			{
				// Reduce hit distance by ShrinkHeight because we started the trace higher than the base.
				// We allow negative distances here, because this allows us to pull out of penetrations.
				const float MaxPenetrationAdjust = FMath::Max(MAX_FLOOR_DIST, PawnRadius);
				const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

				OutFloorResult.bBlockingHit = true;
				if (LineResult <= LineDistance && IsWalkable(Hit))
				{
					OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
					return;
				}
			}
		}
	}
	
	// No hits were acceptable.
	OutFloorResult.bWalkableFloor = false;
	OutFloorResult.FloorDist = SweepDistance;
}


void UCharacterMovementComponent::FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bZeroDelta, const FHitResult* DownwardSweepResult) const
{
	SCOPE_CYCLE_COUNTER(STAT_CharFindFloor);

	// No collision, no floor...
	if (!HasValidData() || !UpdatedComponent->IsQueryCollisionEnabled())
	{
		OutFloorResult.Clear();
		return;
	}

	check(CharacterOwner->GetCapsuleComponent());

	// Increase height check slightly if walking, to prevent floor height adjustment from later invalidating the floor result.
	const float HeightCheckAdjust = (IsMovingOnGround() ? MAX_FLOOR_DIST + KINDA_SMALL_NUMBER : -MAX_FLOOR_DIST);

	float FloorSweepTraceDist = FMath::Max(MAX_FLOOR_DIST, MaxStepHeight + HeightCheckAdjust);
	float FloorLineTraceDist = FloorSweepTraceDist;
	bool bNeedToValidateFloor = true;
	
	// Sweep floor
	if (FloorLineTraceDist > 0.f || FloorSweepTraceDist > 0.f)
	{
		UCharacterMovementComponent* MutableThis = const_cast<UCharacterMovementComponent*>(this);

		if ( bAlwaysCheckFloor || !bZeroDelta || bForceNextFloorCheck || bJustTeleported )
		{
			MutableThis->bForceNextFloorCheck = false;
			ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius(), DownwardSweepResult);
		}
		else
		{
			// Force floor check if base has collision disabled or if it does not block us.
			UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
			const AActor* BaseActor = MovementBase ? MovementBase->GetOwner() : NULL;
			const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

			if (MovementBase != NULL)
			{
				MutableThis->bForceNextFloorCheck = !MovementBase->IsQueryCollisionEnabled()
				|| MovementBase->GetCollisionResponseToChannel(CollisionChannel) != ECR_Block
				|| MovementBaseUtility::IsDynamicBase(MovementBase);
			}

			const bool IsActorBasePendingKill = BaseActor && BaseActor->IsPendingKill();

			if ( !bForceNextFloorCheck && !IsActorBasePendingKill && MovementBase )
			{
				//UE_LOG(LogCharacterMovement, Log, TEXT("%s SKIP check for floor"), *CharacterOwner->GetName());
				OutFloorResult = CurrentFloor;
				bNeedToValidateFloor = false;
			}
			else
			{
				MutableThis->bForceNextFloorCheck = false;
				ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius(), DownwardSweepResult);
			}
		}
	}

	// OutFloorResult.HitResult is now the result of the vertical floor check.
	// See if we should try to "perch" at this location.
	if (bNeedToValidateFloor && OutFloorResult.bBlockingHit && !OutFloorResult.bLineTrace)
	{
		const bool bCheckRadius = true;
		if (ShouldComputePerchResult(OutFloorResult.HitResult, bCheckRadius))
		{
			float MaxPerchFloorDist = FMath::Max(MAX_FLOOR_DIST, MaxStepHeight + HeightCheckAdjust);
			if (IsMovingOnGround())
			{
				MaxPerchFloorDist += FMath::Max(0.f, PerchAdditionalHeight);
			}

			FFindFloorResult PerchFloorResult;
			if (ComputePerchResult(GetValidPerchRadius(), OutFloorResult.HitResult, MaxPerchFloorDist, PerchFloorResult))
			{
				// Don't allow the floor distance adjustment to push us up too high, or we will move beyond the perch distance and fall next time.
				const float AvgFloorDist = (MIN_FLOOR_DIST + MAX_FLOOR_DIST) * 0.5f;
				const float MoveUpDist = (AvgFloorDist - OutFloorResult.FloorDist);
				if (MoveUpDist + PerchFloorResult.FloorDist >= MaxPerchFloorDist)
				{
					OutFloorResult.FloorDist = AvgFloorDist;
				}

				// If the regular capsule is on an unwalkable surface but the perched one would allow us to stand, override the normal to be one that is walkable.
				if (!OutFloorResult.bWalkableFloor)
				{
					OutFloorResult.SetFromLineTrace(PerchFloorResult.HitResult, OutFloorResult.FloorDist, FMath::Min(PerchFloorResult.FloorDist, PerchFloorResult.LineDist), true);
				}
			}
			else
			{
				// We had no floor (or an invalid one because it was unwalkable), and couldn't perch here, so invalidate floor (which will cause us to start falling).
				OutFloorResult.bWalkableFloor = false;
			}
		}
	}
}


void UCharacterMovementComponent::K2_FindFloor(FVector CapsuleLocation, FFindFloorResult& FloorResult) const
{
	FindFloor(CapsuleLocation, FloorResult, false);
}

void UCharacterMovementComponent::K2_ComputeFloorDist(FVector CapsuleLocation, float LineDistance, float SweepDistance, float SweepRadius, FFindFloorResult& FloorResult) const
{
	if (HasValidData())
	{
		SweepDistance = FMath::Max(SweepDistance, 0.f);
		LineDistance = FMath::Clamp(LineDistance, 0.f, SweepDistance);
		SweepRadius = FMath::Max(SweepRadius, 0.f);

		ComputeFloorDist(CapsuleLocation, LineDistance, SweepDistance, FloorResult, SweepRadius);
	}
}


bool UCharacterMovementComponent::FloorSweepTest(
	FHitResult& OutHit,
	const FVector& Start,
	const FVector& End,
	ECollisionChannel TraceChannel,
	const struct FCollisionShape& CollisionShape,
	const struct FCollisionQueryParams& Params,
	const struct FCollisionResponseParams& ResponseParam
	) const
{
	bool bBlockingHit = false;

	if (!bUseFlatBaseForFloorChecks)
	{
		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel, CollisionShape, Params, ResponseParam);
	}
	else
	{
		// Test with a box that is enclosed by the capsule.
		const float CapsuleRadius = CollisionShape.GetCapsuleRadius();
		const float CapsuleHeight = CollisionShape.GetCapsuleHalfHeight();
		const FCollisionShape BoxShape = FCollisionShape::MakeBox(FVector(CapsuleRadius * 0.707f, CapsuleRadius * 0.707f, CapsuleHeight));

		// First test with the box rotated so the corners are along the major axes (ie rotated 45 degrees).
		bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat(FVector(0.f, 0.f, -1.f), PI * 0.25f), TraceChannel, BoxShape, Params, ResponseParam);

		if (!bBlockingHit)
		{
			// Test again with the same box, not rotated.
			OutHit.Reset(1.f, false);
			bBlockingHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel, BoxShape, Params, ResponseParam);
		}
	}

	return bBlockingHit;
}


bool UCharacterMovementComponent::IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit) const
{
	if (!Hit.bBlockingHit)
	{
		return false;
	}

	// Skip some checks if penetrating. Penetration will be handled by the FindFloor call (using a smaller capsule)
	if (!Hit.bStartPenetrating)
	{
		// Reject unwalkable floor normals.
		if (!IsWalkable(Hit))
		{
			return false;
		}

		float PawnRadius, PawnHalfHeight;
		CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

		// Reject hits that are above our lower hemisphere (can happen when sliding down a vertical surface).
		const float LowerHemisphereZ = Hit.Location.Z - PawnHalfHeight + PawnRadius;
		if (Hit.ImpactPoint.Z >= LowerHemisphereZ)
		{
			return false;
		}

		// Reject hits that are barely on the cusp of the radius of the capsule
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			return false;
		}
	}
	else
	{
		// Penetrating
		if (Hit.Normal.Z < KINDA_SMALL_NUMBER)
		{
			// Normal is nearly horizontal or downward, that's a penetration adjustment next to a vertical or overhanging wall. Don't pop to the floor.
			return false;
		}
	}

	FFindFloorResult FloorResult;
	FindFloor(CapsuleLocation, FloorResult, false, &Hit);

	if (!FloorResult.IsWalkableFloor())
	{
		return false;
	}

	return true;
}


bool UCharacterMovementComponent::ShouldCheckForValidLandingSpot(float DeltaTime, const FVector& Delta, const FHitResult& Hit) const
{
	// See if we hit an edge of a surface on the lower portion of the capsule.
	// In this case the normal will not equal the impact normal, and a downward sweep may find a walkable surface on top of the edge.
	if (Hit.Normal.Z > KINDA_SMALL_NUMBER && !Hit.Normal.Equals(Hit.ImpactNormal))
	{
		const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
		if (IsWithinEdgeTolerance(PawnLocation, Hit.ImpactPoint, CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius()))
		{						
			return true;
		}
	}

	return false;
}


float UCharacterMovementComponent::GetPerchRadiusThreshold() const
{
	// Don't allow negative values.
	return FMath::Max(0.f, PerchRadiusThreshold);
}


float UCharacterMovementComponent::GetValidPerchRadius() const
{
	if (CharacterOwner)
	{
		const float PawnRadius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
		return FMath::Clamp(PawnRadius - GetPerchRadiusThreshold(), 0.1f, PawnRadius);
	}
	return 0.f;
}


bool UCharacterMovementComponent::ShouldComputePerchResult(const FHitResult& InHit, bool bCheckRadius) const
{
	if (!InHit.IsValidBlockingHit())
	{
		return false;
	}

	// Don't try to perch if the edge radius is very small.
	if (GetPerchRadiusThreshold() <= SWEEP_EDGE_REJECT_DISTANCE)
	{
		return false;
	}

	if (bCheckRadius)
	{
		const float DistFromCenterSq = (InHit.ImpactPoint - InHit.Location).SizeSquared2D();
		const float StandOnEdgeRadius = GetValidPerchRadius();
		if (DistFromCenterSq <= FMath::Square(StandOnEdgeRadius))
		{
			// Already within perch radius.
			return false;
		}
	}
	
	return true;
}


bool UCharacterMovementComponent::ComputePerchResult(const float TestRadius, const FHitResult& InHit, const float InMaxFloorDist, FFindFloorResult& OutPerchFloorResult) const
{
	if (InMaxFloorDist <= 0.f)
	{
		return 0.f;
	}

	// Sweep further than actual requested distance, because a reduced capsule radius means we could miss some hits that the normal radius would contact.
	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	const float InHitAboveBase = FMath::Max(0.f, InHit.ImpactPoint.Z - (InHit.Location.Z - PawnHalfHeight));
	const float PerchLineDist = FMath::Max(0.f, InMaxFloorDist - InHitAboveBase);
	const float PerchSweepDist = FMath::Max(0.f, InMaxFloorDist);

	const float ActualSweepDist = PerchSweepDist + PawnRadius;
	ComputeFloorDist(InHit.Location, PerchLineDist, ActualSweepDist, OutPerchFloorResult, TestRadius);

	if (!OutPerchFloorResult.IsWalkableFloor())
	{
		return false;
	}
	else if (InHitAboveBase + OutPerchFloorResult.FloorDist > InMaxFloorDist)
	{
		// Hit something past max distance
		OutPerchFloorResult.bWalkableFloor = false;
		return false;
	}

	return true;
}


bool UCharacterMovementComponent::CanStepUp(const FHitResult& Hit) const
{
	if (!Hit.IsValidBlockingHit() || !HasValidData() || MovementMode == MOVE_Falling)
	{
		return false;
	}

	// No component for "fake" hits when we are on a known good base.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (!HitComponent)
	{
		return true;
	}

	if (!HitComponent->CanCharacterStepUp(CharacterOwner))
	{
		return false;
	}

	// No actor for "fake" hits when we are on a known good base.
	const AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		 return true;
	}

	if (!HitActor->CanBeBaseForCharacter(CharacterOwner))
	{
		return false;
	}

	return true;
}


bool UCharacterMovementComponent::StepUp(const FVector& GravDir, const FVector& Delta, const FHitResult &InHit, FStepDownResult* OutStepDownResult)
{
	SCOPE_CYCLE_COUNTER(STAT_CharStepUp);

	if (!CanStepUp(InHit) || MaxStepHeight <= 0.f)
	{
		return false;
	}

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	float PawnRadius, PawnHalfHeight;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	// Don't bother stepping up if top of capsule is hitting something.
	const float InitialImpactZ = InHit.ImpactPoint.Z;
	if (InitialImpactZ > OldLocation.Z + (PawnHalfHeight - PawnRadius))
	{
		return false;
	}

	if (GravDir.IsZero())
	{
		return false;
	}

	// Gravity should be a normalized direction
	ensure(GravDir.IsNormalized());

	float StepTravelUpHeight = MaxStepHeight;
	float StepTravelDownHeight = StepTravelUpHeight;
	const float StepSideZ = -1.f * FVector::DotProduct(InHit.ImpactNormal, GravDir);
	float PawnInitialFloorBaseZ = OldLocation.Z - PawnHalfHeight;
	float PawnFloorPointZ = PawnInitialFloorBaseZ;

	if (IsMovingOnGround() && CurrentFloor.IsWalkableFloor())
	{
		// Since we float a variable amount off the floor, we need to enforce max step height off the actual point of impact with the floor.
		const float FloorDist = FMath::Max(0.f, CurrentFloor.GetDistanceToFloor());
		PawnInitialFloorBaseZ -= FloorDist;
		StepTravelUpHeight = FMath::Max(StepTravelUpHeight - FloorDist, 0.f);
		StepTravelDownHeight = (MaxStepHeight + MAX_FLOOR_DIST*2.f);

		const bool bHitVerticalFace = !IsWithinEdgeTolerance(InHit.Location, InHit.ImpactPoint, PawnRadius);
		if (!CurrentFloor.bLineTrace && !bHitVerticalFace)
		{
			PawnFloorPointZ = CurrentFloor.HitResult.ImpactPoint.Z;
		}
		else
		{
			// Base floor point is the base of the capsule moved down by how far we are hovering over the surface we are hitting.
			PawnFloorPointZ -= CurrentFloor.FloorDist;
		}
	}

	// Don't step up if the impact is below us, accounting for distance from floor.
	if (InitialImpactZ <= PawnInitialFloorBaseZ)
	{
		return false;
	}

	// Scope our movement updates, and do not apply them until all intermediate moves are completed.
	FScopedMovementUpdate ScopedStepUpMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	// step up - treat as vertical wall
	FHitResult SweepUpHit(1.f);
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	MoveUpdatedComponent(-GravDir * StepTravelUpHeight, PawnRotation, true, &SweepUpHit);

	if (SweepUpHit.bStartPenetrating)
	{
		// Undo movement
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	// step fwd
	FHitResult Hit(1.f);
	MoveUpdatedComponent( Delta, PawnRotation, true, &Hit);

	// Check result of forward movement
	if (Hit.bBlockingHit)
	{
		if (Hit.bStartPenetrating)
		{
			// Undo movement
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If we hit something above us and also something ahead of us, we should notify about the upward hit as well.
		// The forward hit will be handled later (in the bSteppedOver case below).
		// In the case of hitting something above but not forward, we are not blocked from moving so we don't need the notification.
		if (SweepUpHit.bBlockingHit && Hit.bBlockingHit)
		{
			HandleImpact(SweepUpHit);
		}

		// pawn ran into a wall
		HandleImpact(Hit);
		if (IsFalling())
		{
			return true;
		}

		// adjust and try again
		const float ForwardHitTime = Hit.Time;
		const float ForwardSlideAmount = SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		
		if (IsFalling())
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If both the forward hit and the deflection got us nowhere, there is no point in this step up.
		if (ForwardHitTime == 0.f && ForwardSlideAmount == 0.f)
		{
			ScopedStepUpMovement.RevertMove();
			return false;
		}
	}
	
	// Step down
	MoveUpdatedComponent(GravDir * StepTravelDownHeight, UpdatedComponent->GetComponentQuat(), true, &Hit);

	// If step down was initially penetrating abort the step up
	if (Hit.bStartPenetrating)
	{
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	FStepDownResult StepDownResult;
	if (Hit.IsValidBlockingHit())
	{	
		// See if this step sequence would have allowed us to travel higher than our max step height allows.
		const float DeltaZ = Hit.ImpactPoint.Z - PawnFloorPointZ;
		if (DeltaZ > MaxStepHeight)
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (too high Height %.3f) up from floor base %f to %f"), DeltaZ, PawnInitialFloorBaseZ, NewLocation.Z);
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Reject unwalkable surface normals here.
		if (!IsWalkable(Hit))
		{
			// Reject if normal opposes movement direction
			const bool bNormalTowardsMe = (Delta | Hit.ImpactNormal) < 0.f;
			if (bNormalTowardsMe)
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s opposed to movement)"), *Hit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}

			// Also reject if we would end up being higher than our starting location by stepping down.
			// It's fine to step down onto an unwalkable normal below us, we will just slide off. Rejecting those moves would prevent us from being able to walk off the edge.
			if (Hit.Location.Z > OldLocation.Z)
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s above old position)"), *Hit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}
		}

		// Reject moves where the downward sweep hit something very close to the edge of the capsule. This maintains consistency with FindFloor as well.
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (outside edge tolerance)"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Don't step up onto invalid surfaces if traveling higher.
		if (DeltaZ > 0.f && !CanStepUp(Hit))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (up onto surface with !CanStepUp())"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// See if we can validate the floor as a result of this step down. In almost all cases this should succeed, and we can avoid computing the floor outside this method.
		if (OutStepDownResult != NULL)
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), StepDownResult.FloorResult, false, &Hit);

			// Reject unwalkable normals if we end up higher than our initial height.
			// It's fine to walk down onto an unwalkable surface, don't reject those moves.
			if (Hit.Location.Z > OldLocation.Z)
			{
				// We should reject the floor result if we are trying to step up an actual step where we are not able to perch (this is rare).
				// In those cases we should instead abort the step up and try to slide along the stair.
				if (!StepDownResult.FloorResult.bBlockingHit && StepSideZ < MAX_STEP_SIDE_Z)
				{
					ScopedStepUpMovement.RevertMove();
					return false;
				}
			}

			StepDownResult.bComputedFloor = true;
		}
	}
	
	// Copy step down result.
	if (OutStepDownResult != NULL)
	{
		*OutStepDownResult = StepDownResult;
	}

	// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
	bJustTeleported |= !bMaintainHorizontalGroundVelocity;

	return true;
}

void UCharacterMovementComponent::HandleImpact(const FHitResult& Impact, float TimeSlice, const FVector& MoveDelta)
{
	if (CharacterOwner)
	{
		CharacterOwner->MoveBlockedBy(Impact);
	}

	if (PathFollowingComp.IsValid())
	{	// Also notify path following!
		PathFollowingComp->OnMoveBlockedBy(Impact);
	}

	APawn* OtherPawn = Cast<APawn>(Impact.GetActor());
	if (OtherPawn)
	{
		NotifyBumpedPawn(OtherPawn);
	}

	if (bEnablePhysicsInteraction)
	{
		const FVector ForceAccel = Acceleration + (IsFalling() ? FVector(0.f, 0.f, GetGravityZ()) : FVector::ZeroVector);
		ApplyImpactPhysicsForces(Impact, ForceAccel, Velocity);
	}
}

void UCharacterMovementComponent::ApplyImpactPhysicsForces(const FHitResult& Impact, const FVector& ImpactAcceleration, const FVector& ImpactVelocity)
{
	if (bEnablePhysicsInteraction && Impact.bBlockingHit )
	{
		if (UPrimitiveComponent* ImpactComponent = Impact.GetComponent())
		{
			FBodyInstance* BI = ImpactComponent->GetBodyInstance(Impact.BoneName);
			if(BI != nullptr && BI->IsInstanceSimulatingPhysics())
			{
				FVector ForcePoint = Impact.ImpactPoint;

				const float BodyMass = FMath::Max(BI->GetBodyMass(), 1.0f);

				if(bPushForceUsingZOffset)
				{
					FBox Bounds = BI->GetBodyBounds();

					FVector Center, Extents;
					Bounds.GetCenterAndExtents(Center, Extents);

					if (!Extents.IsNearlyZero())
					{
						ForcePoint.Z = Center.Z + Extents.Z * PushForcePointZOffsetFactor;
					}
				}

				FVector Force = Impact.ImpactNormal * -1.0f;

				float PushForceModificator = 1.0f;

				const FVector ComponentVelocity = ImpactComponent->GetPhysicsLinearVelocity();
				const FVector VirtualVelocity = ImpactAcceleration.IsZero() ? ImpactVelocity : ImpactAcceleration.GetSafeNormal() * GetMaxSpeed();

				float Dot = 0.0f;

				if (bScalePushForceToVelocity && !ComponentVelocity.IsNearlyZero())
				{			
					Dot = ComponentVelocity | VirtualVelocity;

					if (Dot > 0.0f && Dot < 1.0f)
					{
						PushForceModificator *= Dot;
					}
				}

				if (bPushForceScaledToMass)
				{
					PushForceModificator *= BodyMass;
				}

				Force *= PushForceModificator;

				if (ComponentVelocity.IsNearlyZero())
				{
					Force *= InitialPushForceFactor;
					ImpactComponent->AddImpulseAtLocation(Force, ForcePoint, Impact.BoneName);
				}
				else
				{
					Force *= PushForceFactor;
					ImpactComponent->AddForceAtLocation(Force, ForcePoint, Impact.BoneName);
				}
			}
		}
	}
}

FString UCharacterMovementComponent::GetMovementName() const
{
	if( CharacterOwner )
	{
		if ( CharacterOwner->GetRootComponent() && CharacterOwner->GetRootComponent()->IsSimulatingPhysics() )
		{
			return TEXT("Rigid Body");
		}
		else if ( CharacterOwner->IsMatineeControlled() )
		{
			return TEXT("Matinee");
		}
	}

	// Using character movement
	switch( MovementMode )
	{
		case MOVE_None:				return TEXT("NULL"); break;
		case MOVE_Walking:			return TEXT("Walking"); break;
		case MOVE_NavWalking:		return TEXT("NavWalking"); break;
		case MOVE_Falling:			return TEXT("Falling"); break;
		case MOVE_Swimming:			return TEXT("Swimming"); break;
		case MOVE_Flying:			return TEXT("Flying"); break;
		case MOVE_Custom:			return TEXT("Custom"); break;
		default:					break;
	}
	return TEXT("Unknown");
}

void UCharacterMovementComponent::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	if ( CharacterOwner == NULL )
	{
		return;
	}

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.SetDrawColor(FColor::White);
	FString T = FString::Printf(TEXT("CHARACTER MOVEMENT Floor %s Crouched %i"), *CurrentFloor.HitResult.ImpactNormal.ToString(), IsCrouching());
	DisplayDebugManager.DrawString(T);

	T = FString::Printf(TEXT("Updated Component: %s"), *UpdatedComponent->GetName());
	DisplayDebugManager.DrawString(T);

	T = FString::Printf(TEXT("Acceleration: %s"), *Acceleration.ToCompactString());
	DisplayDebugManager.DrawString(T);

	T = FString::Printf(TEXT("bForceMaxAccel: %i"), bForceMaxAccel);
	DisplayDebugManager.DrawString(T);

	T = FString::Printf(TEXT("RootMotionSources: %d active"), CurrentRootMotion.RootMotionSources.Num());
	DisplayDebugManager.DrawString(T);

	APhysicsVolume * PhysicsVolume = GetPhysicsVolume();

	const UPrimitiveComponent* BaseComponent = CharacterOwner->GetMovementBase();
	const AActor* BaseActor = BaseComponent ? BaseComponent->GetOwner() : NULL;

	T = FString::Printf(TEXT("%s In physicsvolume %s on base %s component %s gravity %f"), *GetMovementName(), (PhysicsVolume ? *PhysicsVolume->GetName() : TEXT("None")),
		(BaseActor ? *BaseActor->GetName() : TEXT("None")), (BaseComponent ? *BaseComponent->GetName() : TEXT("None")), GetGravityZ());
	DisplayDebugManager.DrawString(T);
}

void UCharacterMovementComponent::VisualizeMovement() const
{
	if (CharacterOwner == nullptr)
	{
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const FVector TopOfCapsule = GetActorLocation() + FVector(0.f, 0.f, CharacterOwner->GetSimpleCollisionHalfHeight());
	float HeightOffset = 0.f;

	// Position
	{
		const FColor DebugColor = FColor::White;
		const FVector DebugLocation = TopOfCapsule + FVector(0.f,0.f,HeightOffset);
		FString DebugText = FString::Printf(TEXT("Position: %s"), *GetActorLocation().ToCompactString());
		DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
	}

	// Velocity
	{
		const FColor DebugColor = FColor::Green;
		HeightOffset += 15.f;
		const FVector DebugLocation = TopOfCapsule + FVector(0.f,0.f,HeightOffset);
		DrawDebugDirectionalArrow(GetWorld(), DebugLocation, DebugLocation + Velocity, 
			100.f, DebugColor, false, -1.f, (uint8)'\000', 10.f);

		FString DebugText = FString::Printf(TEXT("Velocity: %s (Speed: %.2f)"), *Velocity.ToCompactString(), Velocity.Size());
		DrawDebugString(GetWorld(), DebugLocation + FVector(0.f,0.f,5.f), DebugText, nullptr, DebugColor, 0.f, true);
	}

	// Acceleration
	{
		const FColor DebugColor = FColor::Yellow;
		HeightOffset += 15.f;
		const float MaxAccelerationLineLength = 200.f;
		const float CurrentMaxAccel = GetMaxAcceleration();
		const float CurrentAccelAsPercentOfMaxAccel = CurrentMaxAccel > 0.f ? Acceleration.Size() / CurrentMaxAccel : 1.f;
		const FVector DebugLocation = TopOfCapsule + FVector(0.f,0.f,HeightOffset);
		DrawDebugDirectionalArrow(GetWorld(), DebugLocation, 
			DebugLocation + Acceleration.GetSafeNormal(SMALL_NUMBER) * CurrentAccelAsPercentOfMaxAccel * MaxAccelerationLineLength, 
			25.f, DebugColor, false, -1.f, (uint8)'\000', 8.f);

		FString DebugText = FString::Printf(TEXT("Acceleration: %s"), *Acceleration.ToCompactString());
		DrawDebugString(GetWorld(), DebugLocation + FVector(0.f,0.f,5.f), DebugText, nullptr, DebugColor, 0.f, true);
	}

	// Movement Mode
	{
		const FColor DebugColor = FColor::Blue;
		HeightOffset += 20.f;
		const FVector DebugLocation = TopOfCapsule + FVector(0.f,0.f,HeightOffset);
		FString DebugText = FString::Printf(TEXT("MovementMode: %s"), *GetMovementName());
		DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
	}

	// Root motion (additive)
	if (CurrentRootMotion.HasAdditiveVelocity())
	{
		const FColor DebugColor = FColor::Cyan;
		HeightOffset += 15.f;
		const FVector DebugLocation = TopOfCapsule + FVector(0.f,0.f,HeightOffset);

		FVector CurrentAdditiveVelocity(FVector::ZeroVector);
		CurrentRootMotion.AccumulateAdditiveRootMotionVelocity(0.f, *CharacterOwner, *this, CurrentAdditiveVelocity);

		DrawDebugDirectionalArrow(GetWorld(), DebugLocation, DebugLocation + CurrentAdditiveVelocity, 
			100.f, DebugColor, false, -1.f, (uint8)'\000', 10.f);

		FString DebugText = FString::Printf(TEXT("RootMotionAdditiveVelocity: %s (Speed: %.2f)"), 
			*CurrentAdditiveVelocity.ToCompactString(), CurrentAdditiveVelocity.Size());
		DrawDebugString(GetWorld(), DebugLocation + FVector(0.f,0.f,5.f), DebugText, nullptr, DebugColor, 0.f, true);
	}

	// Root motion (override)
	if (CurrentRootMotion.HasOverrideVelocity())
	{
		const FColor DebugColor = FColor::Green;
		HeightOffset += 15.f;
		const FVector DebugLocation = TopOfCapsule + FVector(0.f,0.f,HeightOffset);
		FString DebugText = FString::Printf(TEXT("Has Override RootMotion"));
		DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);
	}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UCharacterMovementComponent::ForceReplicationUpdate()
{
	if (HasPredictionData_Server())
	{
		GetPredictionData_Server_Character()->LastUpdateTime = GetWorld()->TimeSeconds - 10.f;
	}
}


FVector UCharacterMovementComponent::ConstrainInputAcceleration(const FVector& InputAcceleration) const
{
	// walking or falling pawns ignore up/down sliding
	if (InputAcceleration.Z != 0.f && (IsMovingOnGround() || IsFalling()))
	{
		return FVector(InputAcceleration.X, InputAcceleration.Y, 0.f);
	}

	return InputAcceleration;
}


FVector UCharacterMovementComponent::ScaleInputAcceleration(const FVector& InputAcceleration) const
{
	return GetMaxAcceleration() * InputAcceleration.GetClampedToMaxSize(1.0f);
}


FVector UCharacterMovementComponent::RoundAcceleration(FVector InAccel) const
{
	// Match FVector_NetQuantize10 (1 decimal place of precision).
	InAccel.X = FMath::RoundToFloat(InAccel.X * 10.f) / 10.f;
	InAccel.Y = FMath::RoundToFloat(InAccel.Y * 10.f) / 10.f;
	InAccel.Z = FMath::RoundToFloat(InAccel.Z * 10.f) / 10.f;
	return InAccel;
}


float UCharacterMovementComponent::ComputeAnalogInputModifier() const
{
	const float MaxAccel = GetMaxAcceleration();
	if (Acceleration.SizeSquared() > 0.f && MaxAccel > SMALL_NUMBER)
	{
		return FMath::Clamp(Acceleration.Size() / MaxAccel, 0.f, 1.f);
	}

	return 0.f;
}

float UCharacterMovementComponent::GetAnalogInputModifier() const
{
	return AnalogInputModifier;
}


float UCharacterMovementComponent::GetSimulationTimeStep(float RemainingTime, int32 Iterations) const
{
	if (RemainingTime > MaxSimulationTimeStep)
	{
		if (Iterations < MaxSimulationIterations)
		{
			// Subdivide moves to be no longer than MaxSimulationTimeStep seconds
			RemainingTime = FMath::Min(MaxSimulationTimeStep, RemainingTime * 0.5f);
		}
		else
		{
			// If this is the last iteration, just use all the remaining time. This is usually better than cutting things short, as the simulation won't move far enough otherwise.
			// Print a throttled warning.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			static uint32 s_WarningCount = 0;
			if ((s_WarningCount++ < 100) || (GFrameCounter & 15) == 0)
			{
				UE_LOG(LogCharacterMovement, Warning, TEXT("GetSimulationTimeStep() - Max iterations %d hit while remaining time %.6f > MaxSimulationTimeStep (%.3f) for '%s', movement '%s'"), MaxSimulationIterations, RemainingTime, MaxSimulationTimeStep, *GetNameSafe(CharacterOwner), *GetMovementName());
			}
#endif
		}
	}

	// no less than MIN_TICK_TIME (to avoid potential divide-by-zero during simulation).
	return FMath::Max(MIN_TICK_TIME, RemainingTime);
}

void UCharacterMovementComponent::SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation)
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementSmoothCorrection);
	if (!HasValidData())
	{
		return;
	}

	// We shouldn't be running this on a server that is not a listen server.
	checkSlow(GetNetMode() != NM_DedicatedServer);
	checkSlow(GetNetMode() != NM_Standalone);

	// Only client proxies or remote clients on a listen server should run this code.
	const bool bIsSimulatedProxy = (CharacterOwner->Role == ROLE_SimulatedProxy);
	const bool bIsRemoteAutoProxy = (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);
	ensure(bIsSimulatedProxy || bIsRemoteAutoProxy);

	// Getting a correction means new data, so smoothing needs to run.
	bNetworkSmoothingComplete = false;

	// Handle selected smoothing mode.
	if (NetworkSmoothingMode == ENetworkSmoothingMode::Replay)
	{
		// Replays use pure interpolation in this mode, all of the work is done in SmoothClientPosition_Interpolate
		return;
	}
	else if (NetworkSmoothingMode == ENetworkSmoothingMode::Disabled)
	{
		UpdatedComponent->SetWorldLocationAndRotation(NewLocation, NewRotation);
		bNetworkSmoothingComplete = true;
	}
	else if (FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character())
	{
		const UWorld* MyWorld = GetWorld();
		if (!ensure(MyWorld != nullptr))
		{
			return;
		}

		// The mesh doesn't move, but the capsule does so we have a new offset.
		FVector NewToOldVector = (OldLocation - NewLocation);
		if (bIsNavWalkingOnServer && FMath::Abs(NewToOldVector.Z) < NavWalkingFloorDistTolerance)
		{
			// ignore smoothing on Z axis
			// don't modify new location (local simulation result), since it's probably more accurate than server data
			// and shouldn't matter as long as difference is relatively small
			NewToOldVector.Z = 0;
		}

		const float DistSq = NewToOldVector.SizeSquared();
		if (DistSq > FMath::Square(ClientData->MaxSmoothNetUpdateDist))
		{
			ClientData->MeshTranslationOffset = (DistSq > FMath::Square(ClientData->NoSmoothNetUpdateDist)) 
				? FVector::ZeroVector 
				: ClientData->MeshTranslationOffset + ClientData->MaxSmoothNetUpdateDist * NewToOldVector.GetSafeNormal();
		}
		else
		{
			ClientData->MeshTranslationOffset = ClientData->MeshTranslationOffset + NewToOldVector;	
		}

		if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
		{
			ClientData->OriginalMeshTranslationOffset	= ClientData->MeshTranslationOffset;

			// Remember the current and target rotation, we're going to lerp between them
			ClientData->OriginalMeshRotationOffset		= OldRotation;
			ClientData->MeshRotationOffset				= OldRotation;
			ClientData->MeshRotationTarget				= NewRotation;

			// Move the capsule, but not the mesh.
			// Note: we don't change rotation, we lerp towards it in SmoothClientPosition.
			const FScopedPreventAttachedComponentMove PreventMeshMove(CharacterOwner->GetMesh());
			UpdatedComponent->SetWorldLocation(NewLocation);
		}
		else
		{
			// Calc rotation needed to keep current world rotation after UpdatedComponent moves.
			// Take difference between where we were rotated before, and where we're going
			ClientData->MeshRotationOffset = (NewRotation.Inverse() * OldRotation) * ClientData->MeshRotationOffset;
			ClientData->MeshRotationTarget = FQuat::Identity;

			const FScopedPreventAttachedComponentMove PreventMeshMove(CharacterOwner->GetMesh());
			UpdatedComponent->SetWorldLocationAndRotation(NewLocation, NewRotation);
		}
	
		//////////////////////////////////////////////////////////////////////////
		// Update smoothing timestamps

		// If running ahead, pull back slightly. This will cause the next delta to seem slightly longer, and cause us to lerp to it slightly slower.
		if (ClientData->SmoothingClientTimeStamp > ClientData->SmoothingServerTimeStamp)
		{
			const double OldClientTimeStamp = ClientData->SmoothingClientTimeStamp;
			ClientData->SmoothingClientTimeStamp = FMath::LerpStable(ClientData->SmoothingServerTimeStamp, OldClientTimeStamp, 0.5);

			UE_LOG(LogCharacterNetSmoothing, VeryVerbose, TEXT("SmoothCorrection: Pull back client from ClientTimeStamp: %.6f to %.6f, ServerTimeStamp: %.6f for %s"),
				OldClientTimeStamp, ClientData->SmoothingClientTimeStamp, ClientData->SmoothingServerTimeStamp, *GetNameSafe(CharacterOwner));
		}

		// Using server timestamp lets us know how much time actually elapsed, regardless of packet lag variance.
		double OldServerTimeStamp = ClientData->SmoothingServerTimeStamp;
		ClientData->SmoothingServerTimeStamp = (bIsSimulatedProxy ? CharacterOwner->GetReplicatedServerLastTransformUpdateTimeStamp() : ServerLastTransformUpdateTimeStamp);

		// Initial update has no delta.
		if (ClientData->LastCorrectionTime == 0)
		{
			ClientData->SmoothingClientTimeStamp = ClientData->SmoothingServerTimeStamp;
			OldServerTimeStamp = ClientData->SmoothingServerTimeStamp;
		}

		// Don't let the client fall too far behind or run ahead of new server time.
		const double ServerDeltaTime = ClientData->SmoothingServerTimeStamp - OldServerTimeStamp;
		const double MaxDelta = FMath::Clamp(ServerDeltaTime * 1.25, 0.0, ClientData->MaxMoveDeltaTime * 2.0);
		ClientData->SmoothingClientTimeStamp = FMath::Clamp(ClientData->SmoothingClientTimeStamp, ClientData->SmoothingServerTimeStamp - MaxDelta, ClientData->SmoothingServerTimeStamp);

		// Compute actual delta between new server timestamp and client simulation.
		ClientData->LastCorrectionDelta = ClientData->SmoothingServerTimeStamp - ClientData->SmoothingClientTimeStamp;
		ClientData->LastCorrectionTime = MyWorld->GetTimeSeconds();

		UE_LOG(LogCharacterNetSmoothing, VeryVerbose, TEXT("SmoothCorrection: WorldTime: %.6f, ServerTimeStamp: %.6f, ClientTimeStamp: %.6f, Delta: %.6f for %s"),
			MyWorld->GetTimeSeconds(), ClientData->SmoothingServerTimeStamp, ClientData->SmoothingClientTimeStamp, ClientData->LastCorrectionDelta, *GetNameSafe(CharacterOwner));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if ( CharacterMovementCVars::NetVisualizeSimulatedCorrections >= 2 )
		{
			const float Radius		= 4.0f;
			const bool	bPersist	= false;
			const float Lifetime	= 10.0f;
			const int32	Sides		= 8;
			const float ArrowSize	= 4.0f;

			const FVector SimulatedLocation = OldLocation;
			const FVector ServerLocation	= NewLocation + FVector( 0, 0, 0.5f );

			const FVector SmoothLocation	= CharacterOwner->GetMesh()->GetComponentLocation() - CharacterOwner->GetBaseTranslationOffset() + FVector( 0, 0, 1.0f );

			//DrawDebugCoordinateSystem( GetWorld(), ServerLocation + FVector( 0, 0, 300.0f ), UpdatedComponent->GetComponentRotation(), 45.0f, bPersist, Lifetime );

			// Draw simulated location
			DrawCircle( GetWorld(), SimulatedLocation, FVector( 1, 0, 0 ), FVector( 0, 1, 0 ), FColor( 255, 0, 0, 255 ), Radius, Sides, bPersist, Lifetime );

			// Draw server (corrected location)
			DrawCircle( GetWorld(), ServerLocation, FVector( 1, 0, 0 ), FVector( 0, 1, 0 ), FColor( 0, 255, 0, 255 ), Radius, Sides, bPersist, Lifetime );

			// Draw smooth simulated location
			FRotationMatrix SmoothMatrix( CharacterOwner->GetMesh()->GetComponentRotation() );
			DrawDebugDirectionalArrow( GetWorld(), SmoothLocation, SmoothLocation + SmoothMatrix.GetScaledAxis( EAxis::Y ) * 5, ArrowSize, FColor( 255, 255, 0, 255 ), bPersist, Lifetime );
			DrawCircle( GetWorld(), SmoothLocation, FVector( 1, 0, 0 ), FVector( 0, 1, 0 ), FColor( 0, 0, 255, 255 ), Radius, Sides, bPersist, Lifetime );

			if ( ClientData->LastServerLocation != FVector::ZeroVector )
			{
				// Arrow showing simulated line
				DrawDebugDirectionalArrow( GetWorld(), ClientData->LastServerLocation, SimulatedLocation, ArrowSize, FColor( 255, 0, 0, 255 ), bPersist, Lifetime );
				
				// Arrow showing server line
				DrawDebugDirectionalArrow( GetWorld(), ClientData->LastServerLocation, ServerLocation, ArrowSize, FColor( 0, 255, 0, 255 ), bPersist, Lifetime );
				
				// Arrow showing smooth location plot
				DrawDebugDirectionalArrow( GetWorld(), ClientData->LastSmoothLocation, SmoothLocation, ArrowSize, FColor( 0, 0, 255, 255 ), bPersist, Lifetime );

				// Line showing correction
				DrawDebugDirectionalArrow( GetWorld(), SimulatedLocation, ServerLocation, ArrowSize, FColor( 128, 0, 0, 255 ), bPersist, Lifetime );
	
				// Line showing smooth vector
				DrawDebugDirectionalArrow( GetWorld(), ServerLocation, SmoothLocation, ArrowSize, FColor( 0, 0, 128, 255 ), bPersist, Lifetime );
			}

			ClientData->LastServerLocation = ServerLocation;
			ClientData->LastSmoothLocation = SmoothLocation;
		}
#endif
	}
}

FArchive& operator<<( FArchive& Ar, FCharacterReplaySample& V )
{
	SerializePackedVector<10, 24>( V.Location, Ar );
	SerializePackedVector<10, 24>( V.Velocity, Ar );
	SerializePackedVector<10, 24>( V.Acceleration, Ar );
	V.Rotation.SerializeCompressed( Ar );
	Ar << V.RemoteViewPitch;

	//V.Rotation.SerializeCompressedShort( Ar );
	//Ar << V.Location << V.Velocity << V.Acceleration << V.Rotation;

	return Ar;
}

void UCharacterMovementComponent::SmoothClientPosition(float DeltaSeconds)
{
	if (!HasValidData() || NetworkSmoothingMode == ENetworkSmoothingMode::Disabled)
	{
		return;
	}

	// We shouldn't be running this on a server that is not a listen server.
	checkSlow(GetNetMode() != NM_DedicatedServer);
	checkSlow(GetNetMode() != NM_Standalone);

	// Only client proxies or remote clients on a listen server should run this code.
	const bool bIsSimulatedProxy = (CharacterOwner->Role == ROLE_SimulatedProxy);
	const bool bIsRemoteAutoProxy = (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);
	if (!ensure(bIsSimulatedProxy || bIsRemoteAutoProxy))
	{
		return;
	}

	SmoothClientPosition_Interpolate(DeltaSeconds);
	SmoothClientPosition_UpdateVisuals();
}


void UCharacterMovementComponent::SmoothClientPosition_Interpolate(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementSmoothClientPosition_Interp);
	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	if (ClientData)
	{
		if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
		{
			const UWorld* MyWorld = GetWorld();

			// Increment client position.
			ClientData->SmoothingClientTimeStamp += DeltaSeconds;

			float LerpPercent = 0.f;
			const float LerpLimit = 1.15f;
			const float TargetDelta = ClientData->LastCorrectionDelta;
			if (TargetDelta > SMALL_NUMBER)
			{
				// Don't let the client get too far ahead (happens on spikes). But we do want a buffer for variable network conditions.
				const float MaxClientTimeAheadPercent = 0.15f;
				const float MaxTimeAhead = TargetDelta * MaxClientTimeAheadPercent;
				ClientData->SmoothingClientTimeStamp = FMath::Min<float>(ClientData->SmoothingClientTimeStamp, ClientData->SmoothingServerTimeStamp + MaxTimeAhead);

				// Compute interpolation alpha based on our client position within the server delta. We should take TargetDelta seconds to reach alpha of 1.
				const float RemainingTime = ClientData->SmoothingServerTimeStamp - ClientData->SmoothingClientTimeStamp;
				const float CurrentSmoothTime = TargetDelta - RemainingTime;
				LerpPercent = FMath::Clamp(CurrentSmoothTime / TargetDelta, 0.0f, LerpLimit);

				UE_LOG(LogCharacterNetSmoothing, VeryVerbose, TEXT("Interpolate: WorldTime: %.6f, ServerTimeStamp: %.6f, ClientTimeStamp: %.6f, Elapsed: %.6f, Alpha: %.6f for %s"),
					MyWorld->GetTimeSeconds(), ClientData->SmoothingServerTimeStamp, ClientData->SmoothingClientTimeStamp, CurrentSmoothTime, LerpPercent, *GetNameSafe(CharacterOwner));
			}
			else
			{
				LerpPercent = 1.0f;
			}

			if (LerpPercent >= 1.0f - KINDA_SMALL_NUMBER)
			{
				if (Velocity.IsNearlyZero())
				{
					ClientData->MeshTranslationOffset = FVector::ZeroVector;
					ClientData->SmoothingClientTimeStamp = ClientData->SmoothingServerTimeStamp;
					bNetworkSmoothingComplete = true;
				}
				else
				{
					// Allow limited forward prediction.
					ClientData->MeshTranslationOffset = FMath::LerpStable(ClientData->OriginalMeshTranslationOffset, FVector::ZeroVector, LerpPercent);
					bNetworkSmoothingComplete = (LerpPercent >= LerpLimit);
				}

				ClientData->MeshRotationOffset = ClientData->MeshRotationTarget;
			}
			else
			{
				ClientData->MeshTranslationOffset = FMath::LerpStable(ClientData->OriginalMeshTranslationOffset, FVector::ZeroVector, LerpPercent);
				ClientData->MeshRotationOffset = FQuat::FastLerp(ClientData->OriginalMeshRotationOffset, ClientData->MeshRotationTarget, LerpPercent).GetNormalized();
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// Show lerp percent
			if ( CharacterMovementCVars::NetVisualizeSimulatedCorrections >= 1 )
			{
				const FColor DebugColor = FColor::White;
				const FVector DebugLocation = CharacterOwner->GetMesh()->GetComponentLocation() + FVector( 0.f, 0.f, 300.0f ) - CharacterOwner->GetBaseTranslationOffset();
				FString DebugText = FString::Printf( TEXT( "Lerp: %2.2f" ), LerpPercent );
				DrawDebugString( GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true );
			}
#endif
		}
		else if (NetworkSmoothingMode == ENetworkSmoothingMode::Exponential)
		{
			// Smooth interpolation of mesh translation to avoid popping of other client pawns unless under a low tick rate.
			// Faster interpolation if stopped.
			const float SmoothLocationTime = Velocity.IsZero() ? 0.5f*ClientData->SmoothNetUpdateTime : ClientData->SmoothNetUpdateTime;
			if (DeltaSeconds < SmoothLocationTime)
			{
				// Slowly decay translation offset
				ClientData->MeshTranslationOffset = (ClientData->MeshTranslationOffset * (1.f - DeltaSeconds / SmoothLocationTime));
			}
			else
			{
				ClientData->MeshTranslationOffset = FVector::ZeroVector;
			}

			// Smooth rotation
			const FQuat MeshRotationTarget = ClientData->MeshRotationTarget;
			if (DeltaSeconds < ClientData->SmoothNetUpdateRotationTime)
			{
				// Slowly decay rotation offset
				ClientData->MeshRotationOffset = FQuat::FastLerp(ClientData->MeshRotationOffset, MeshRotationTarget, DeltaSeconds / ClientData->SmoothNetUpdateRotationTime).GetNormalized();
			}
			else
			{
				ClientData->MeshRotationOffset = MeshRotationTarget;
			}

			// Check if lerp is complete
			if (ClientData->MeshTranslationOffset.IsNearlyZero(1e-2f) && ClientData->MeshRotationOffset.Equals(MeshRotationTarget, 1e-5f))
			{
				bNetworkSmoothingComplete = true;
				// Make sure to snap exactly to target values.
				ClientData->MeshTranslationOffset = FVector::ZeroVector;
				ClientData->MeshRotationOffset = MeshRotationTarget;
			}
		}
		else if ( NetworkSmoothingMode == ENetworkSmoothingMode::Replay )
		{
			const UWorld* MyWorld = GetWorld();

			if ( !MyWorld || !MyWorld->DemoNetDriver )
			{
				return;
			}

			const float CurrentTime = MyWorld->DemoNetDriver->DemoCurrentTime;

			// Remove old samples
			while ( ClientData->ReplaySamples.Num() > 0 )
			{
				if ( ClientData->ReplaySamples[0].Time > CurrentTime - 1.0f )
				{
					break;
				}

				ClientData->ReplaySamples.RemoveAt( 0 );
			}

			FReplayExternalDataArray* ExternalReplayData = MyWorld->DemoNetDriver->GetExternalDataArrayForObject( CharacterOwner );

			// Grab any samples available, deserialize them, then clear originals
			if ( ExternalReplayData && ExternalReplayData->Num() > 0 )
			{
				for ( int i = 0; i < ExternalReplayData->Num(); i++ )
				{
					FCharacterReplaySample ReplaySample;

					( *ExternalReplayData )[i].Reader << ReplaySample;

					ReplaySample.Time = ( *ExternalReplayData )[i].TimeSeconds;

					ClientData->ReplaySamples.Add( ReplaySample );
				}

				if ( CharacterMovementCVars::FixReplayOverSampling > 0 )
				{
					// Remove invalid replay samples that can occur due to oversampling (sampling at higher rate than physics is being ticked)
					// We detect this by finding samples that have the same location but have a velocity that says the character should be moving
					// If we don't do this, then characters will look like they are skipping around, which looks really bad
					for ( int i = 1; i < ClientData->ReplaySamples.Num(); i++ )
					{
						if ( ClientData->ReplaySamples[i].Location.Equals( ClientData->ReplaySamples[i - 1].Location, KINDA_SMALL_NUMBER ) )
						{
							if ( ClientData->ReplaySamples[i - 1].Velocity.SizeSquared() > FMath::Square( KINDA_SMALL_NUMBER ) && ClientData->ReplaySamples[i].Velocity.SizeSquared() > FMath::Square( KINDA_SMALL_NUMBER ) )
							{
								ClientData->ReplaySamples.RemoveAt( i );
								i--;
							}
						}
					}
				}

				ExternalReplayData->Empty();
			}

			bool bFoundSample = false;

			for ( int i = 0; i < ClientData->ReplaySamples.Num() - 1; i++ )
			{
				if ( CurrentTime >= ClientData->ReplaySamples[i].Time && CurrentTime <= ClientData->ReplaySamples[i + 1].Time )
				{
					const float EPSILON		= SMALL_NUMBER;
					const float Delta		= ( ClientData->ReplaySamples[i + 1].Time - ClientData->ReplaySamples[i].Time );
					const float LerpPercent = Delta > EPSILON ? FMath::Clamp<float>( ( float )( CurrentTime - ClientData->ReplaySamples[i].Time ) / Delta, 0.0f, 1.0f ) : 1.0f;

					const FCharacterReplaySample& ReplaySample1 = ClientData->ReplaySamples[i];
					const FCharacterReplaySample& ReplaySample2 = ClientData->ReplaySamples[i + 1];

					const FVector Location = FMath::Lerp( ReplaySample1.Location, ReplaySample2.Location, LerpPercent );
					const FQuat Rotation = FQuat::FastLerp( FQuat( ReplaySample1.Rotation ), FQuat( ReplaySample2.Rotation ), LerpPercent ).GetNormalized();
					Velocity = FMath::Lerp( ReplaySample1.Velocity, ReplaySample2.Velocity, LerpPercent );
					//Acceleration = FMath::Lerp( ClientData->ReplaySamples[i].Acceleration, ClientData->ReplaySamples[i + 1].Acceleration, LerpPercent );
					Acceleration = ClientData->ReplaySamples[i + 1].Acceleration;

					const FRotator Rotator1( FRotator::DecompressAxisFromByte( ReplaySample1.RemoteViewPitch ), 0.0f, 0.0f );
					const FRotator Rotator2( FRotator::DecompressAxisFromByte( ReplaySample2.RemoteViewPitch ), 0.0f, 0.0f );
					const FRotator FinalPitch = FQuat::FastLerp( FQuat( Rotator1 ), FQuat( Rotator2 ), LerpPercent ).GetNormalized().Rotator();
					CharacterOwner->RemoteViewPitch = FRotator::CompressAxisToByte( FinalPitch.Pitch );

					UpdateComponentVelocity();

					USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();

					if ( Mesh )
					{
						Mesh->RelativeLocation = CharacterOwner->GetBaseTranslationOffset();
						Mesh->RelativeRotation = CharacterOwner->GetBaseRotationOffset().Rotator();
					}

					ClientData->MeshTranslationOffset = Location;
					ClientData->MeshRotationOffset = Rotation;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					if ( CharacterMovementCVars::NetVisualizeSimulatedCorrections >= 1 )
					{
						const float Radius		= 4.0f;
						const int32	Sides		= 8;
						const float ArrowSize	= 4.0f;
						const FColor DebugColor = FColor::White;

						const FVector DebugLocation = CharacterOwner->GetMesh()->GetComponentLocation() + FVector( 0.f, 0.f, 300.0f ) - CharacterOwner->GetBaseTranslationOffset();

						FString DebugText = FString::Printf( TEXT( "Lerp: %2.2f, %i" ), LerpPercent, CharacterOwner->RemoteViewPitch );
						DrawDebugString( GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true );
						DrawDebugBox( GetWorld(), DebugLocation, FVector( 45, 45, 45 ), CharacterOwner->GetMesh()->GetComponentQuat(), FColor( 0, 255, 0 ) );

						DrawDebugDirectionalArrow( GetWorld(), DebugLocation, DebugLocation + Velocity, 20.0f, FColor( 255, 0, 0, 255 ) );
					}
#endif
					bFoundSample = true;
					break;
				}
			}

			if ( !bFoundSample )
			{
				int32 BestSample = -1;
				float BestTime = 0.0f;

				for ( int i = 0; i < ClientData->ReplaySamples.Num(); i++ )
				{
					const FCharacterReplaySample& ReplaySample = ClientData->ReplaySamples[i];

					if ( BestSample == -1 || FMath::Abs( BestTime - ReplaySample.Time ) < BestTime )
					{
						BestTime = ReplaySample.Time;
						BestSample = i;
					}
				}

				if ( BestSample != -1 )
				{
					const FCharacterReplaySample& ReplaySample = ClientData->ReplaySamples[BestSample];

					Velocity						= ReplaySample.Velocity;
					Acceleration					= ReplaySample.Acceleration;
					CharacterOwner->RemoteViewPitch = ReplaySample.RemoteViewPitch;

					UpdateComponentVelocity();

					USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();

					if ( Mesh )
					{
						Mesh->RelativeLocation = CharacterOwner->GetBaseTranslationOffset();
						Mesh->RelativeRotation = CharacterOwner->GetBaseRotationOffset().Rotator();
					}

					ClientData->MeshTranslationOffset	= ReplaySample.Location;
					ClientData->MeshRotationOffset		= ReplaySample.Rotation.Quaternion();
				}
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// Show future samples
			if ( CharacterMovementCVars::NetVisualizeSimulatedCorrections >= 1 )
			{
				const float Radius		= 4.0f;
				const int32	Sides		= 8;
				const float ArrowSize	= 4.0f;
				const FColor DebugColor = FColor::White;

				// Draw points ahead up to a few seconds
				for ( int i = 0; i < ClientData->ReplaySamples.Num(); i++ )
				{
					const bool bHasMorePoints = i < ClientData->ReplaySamples.Num() - 1;
					const bool bActiveSamples = ( bHasMorePoints && CurrentTime >= ClientData->ReplaySamples[i].Time && CurrentTime <= ClientData->ReplaySamples[i + 1].Time );

					if ( ClientData->ReplaySamples[i].Time >= CurrentTime || bActiveSamples )
					{
						//const FVector Adjust = FVector( 0.f, 0.f, 300.0f + i * 15.0f );
						const FVector Adjust = FVector( 0.f, 0.f, 300.0f );
						const FVector Location = ClientData->ReplaySamples[i].Location + Adjust;

						if ( bHasMorePoints )
						{
							const FVector NextLocation = ClientData->ReplaySamples[i + 1].Location + Adjust;
							DrawDebugDirectionalArrow( GetWorld(), Location, NextLocation, 4.0f, FColor( 0, 255, 0, 255 ) );
						}

						DrawCircle( GetWorld(), Location, FVector( 1, 0, 0 ), FVector( 0, 1, 0 ), FColor( 255, 0, 0, 255 ), Radius, Sides, false, 0.0f );

						if ( CharacterMovementCVars::NetVisualizeSimulatedCorrections >= 2 )
						{
							DrawDebugDirectionalArrow( GetWorld(), Location, Location + ClientData->ReplaySamples[i].Velocity, 20.0f, FColor( 255, 0, 0, 255 ) );
						}

						if ( CharacterMovementCVars::NetVisualizeSimulatedCorrections >= 3 )
						{
							DrawDebugDirectionalArrow( GetWorld(), Location, Location + ClientData->ReplaySamples[i].Acceleration, 20.0f, FColor( 255, 255, 255, 255 ) );
						}
					}
					
					if ( ClientData->ReplaySamples[i].Time - CurrentTime > 2.0f )
					{
						break;
					}
				}
			}
#endif

			bNetworkSmoothingComplete = false;
		}
		else
		{
			// Unhandled mode
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		//UE_LOG(LogCharacterNetSmoothing, VeryVerbose, TEXT("SmoothClientPosition_Interpolate %s: Translation: %s Rotation: %s"),
		//	*GetNameSafe(CharacterOwner), *ClientData->MeshTranslationOffset.ToString(), *ClientData->MeshRotationOffset.ToString());

		if ( CharacterMovementCVars::NetVisualizeSimulatedCorrections >= 1 && NetworkSmoothingMode != ENetworkSmoothingMode::Replay )
		{
			const FVector DebugLocation = CharacterOwner->GetMesh()->GetComponentLocation() + FVector( 0.f, 0.f, 300.0f ) - CharacterOwner->GetBaseTranslationOffset();
			DrawDebugBox( GetWorld(), DebugLocation, FVector( 45, 45, 45 ), CharacterOwner->GetMesh()->GetComponentQuat(), FColor( 0, 255, 0 ) );

			//DrawDebugCoordinateSystem( GetWorld(), UpdatedComponent->GetComponentLocation() + FVector( 0, 0, 300.0f ), UpdatedComponent->GetComponentRotation(), 45.0f );
			//DrawDebugBox( GetWorld(), UpdatedComponent->GetComponentLocation() + FVector( 0, 0, 300.0f ), FVector( 45, 45, 45 ), UpdatedComponent->GetComponentQuat(), FColor( 0, 255, 0 ) );

			if ( CharacterMovementCVars::NetVisualizeSimulatedCorrections >= 3 )
			{
				ClientData->SimulatedDebugDrawTime += DeltaSeconds;

				if ( ClientData->SimulatedDebugDrawTime >= 1.0f / 60.0f )
				{
					const float Radius		= 2.0f;
					const bool	bPersist	= false;
					const float Lifetime	= 10.0f;
					const int32	Sides		= 8;

					const FVector SmoothLocation	= CharacterOwner->GetMesh()->GetComponentLocation() - CharacterOwner->GetBaseTranslationOffset();
					const FVector SimulatedLocation	= UpdatedComponent->GetComponentLocation();

					DrawCircle( GetWorld(), SmoothLocation + FVector( 0, 0, 1.5f ), FVector( 1, 0, 0 ), FVector( 0, 1, 0 ), FColor( 0, 0, 255, 255 ), Radius, Sides, bPersist, Lifetime );
					DrawCircle( GetWorld(), SimulatedLocation + FVector( 0, 0, 2.0f ), FVector( 1, 0, 0 ), FVector( 0, 1, 0 ), FColor( 255, 0, 0, 255 ), Radius, Sides, bPersist, Lifetime );

					ClientData->SimulatedDebugDrawTime = 0.0f;
				}
			}
		}
#endif
	}
}

void UCharacterMovementComponent::SmoothClientPosition_UpdateVisuals()
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementSmoothClientPosition_Visual);
	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
	if (ClientData && Mesh && !Mesh->IsSimulatingPhysics())
	{
		if (NetworkSmoothingMode == ENetworkSmoothingMode::Linear)
		{
			// Adjust capsule rotation and mesh location. Optimized to trigger only one transform chain update.
			// If we know the rotation is changing that will update children, so it's sufficient to set RelativeLocation directly on the mesh.
			const FVector NewRelLocation = ClientData->MeshRotationOffset.UnrotateVector(ClientData->MeshTranslationOffset) + CharacterOwner->GetBaseTranslationOffset();
			if (!UpdatedComponent->GetComponentQuat().Equals(ClientData->MeshRotationOffset, SCENECOMPONENT_QUAT_TOLERANCE))
			{
				const FVector OldLocation = Mesh->RelativeLocation;
				const FRotator OldRotation = UpdatedComponent->RelativeRotation;
				Mesh->RelativeLocation = NewRelLocation;
				UpdatedComponent->SetWorldRotation(ClientData->MeshRotationOffset);

				// If we did not move from SetWorldRotation, we need to at least call SetRelativeLocation since we were relying on the UpdatedComponent to update the transform of the mesh
				if (UpdatedComponent->RelativeRotation == OldRotation)
				{
					Mesh->RelativeLocation = OldLocation;
					Mesh->SetRelativeLocation(NewRelLocation);
				}
			}
			else
			{
				Mesh->SetRelativeLocation(NewRelLocation);
			}
		}
		else if (NetworkSmoothingMode == ENetworkSmoothingMode::Exponential)
		{
			// Adjust mesh location and rotation
			const FVector NewRelTranslation = UpdatedComponent->GetComponentToWorld().InverseTransformVectorNoScale(ClientData->MeshTranslationOffset) + CharacterOwner->GetBaseTranslationOffset();
			const FQuat NewRelRotation = ClientData->MeshRotationOffset * CharacterOwner->GetBaseRotationOffset();
			Mesh->SetRelativeLocationAndRotation(NewRelTranslation, NewRelRotation);
		}
		else if ( NetworkSmoothingMode == ENetworkSmoothingMode::Replay )
		{
			if ( !UpdatedComponent->GetComponentQuat().Equals( ClientData->MeshRotationOffset, SCENECOMPONENT_QUAT_TOLERANCE ) || !UpdatedComponent->GetComponentLocation().Equals( ClientData->MeshTranslationOffset, KINDA_SMALL_NUMBER ) )
			{
				UpdatedComponent->SetWorldLocationAndRotation( ClientData->MeshTranslationOffset, ClientData->MeshRotationOffset );
			}
		}
		else
		{
			// Unhandled mode
		}
	}
}


bool UCharacterMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementClientUpdatePositionAfterServerUpdate);
	if (!HasValidData())
	{
		return false;
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	check(ClientData);

	if (!ClientData->bUpdatePosition)
	{
		return false;
	}

	if (bIgnoreClientMovementErrorChecksAndCorrection)
	{
#if !UE_BUILD_SHIPPING
		if (CharacterMovementCVars::NetShowCorrections != 0)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("*** Client: %s is set to ignore error checks and corrections with %d saved moves in queue."), *GetNameSafe(CharacterOwner), ClientData->SavedMoves.Num());
		}
#endif // !UE_BUILD_SHIPPING
		return false;
	}

	ClientData->bUpdatePosition = false;

	// Don't do any network position updates on things running PHYS_RigidBody
	if (CharacterOwner->GetRootComponent() && CharacterOwner->GetRootComponent()->IsSimulatingPhysics())
	{
		return false;
	}

	if (ClientData->SavedMoves.Num() == 0)
	{
		UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("ClientUpdatePositionAfterServerUpdate No saved moves to replay"), ClientData->SavedMoves.Num());

		// With no saved moves to resimulate, the move the server updated us with is the last move we've done, no resimulation needed.
		CharacterOwner->bClientResimulateRootMotion = false;
		if (CharacterOwner->bClientResimulateRootMotionSources)
		{
			// With no resimulation, we just update our current root motion to what the server sent us
			UE_LOG(LogRootMotion, VeryVerbose, TEXT("CurrentRootMotion getting updated to ServerUpdate state: %s"), *CharacterOwner->GetName());
			CurrentRootMotion.UpdateStateFrom(CharacterOwner->SavedRootMotion);
			CharacterOwner->bClientResimulateRootMotionSources = false;
		}

		return false;
	}

	// Save important values that might get affected by the replay.
	const float SavedAnalogInputModifier = AnalogInputModifier;
	const FRootMotionMovementParams BackupRootMotionParams = RootMotionParams; // For animation root motion
	const FRootMotionSourceGroup BackupRootMotion = CurrentRootMotion;
	const bool bRealJump = CharacterOwner->bPressedJump;
	const bool bRealCrouch = bWantsToCrouch;
	const bool bRealForceMaxAccel = bForceMaxAccel;
	CharacterOwner->bClientWasFalling = (MovementMode == MOVE_Falling);
	CharacterOwner->bClientUpdating = true;
	bForceNextFloorCheck = true;

	// Replay moves that have not yet been acked.
	UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("ClientUpdatePositionAfterServerUpdate Replaying %d Moves, starting at Timestamp %f"), ClientData->SavedMoves.Num(), ClientData->SavedMoves[0]->TimeStamp);
	for (int32 i=0; i<ClientData->SavedMoves.Num(); i++)
	{
		const FSavedMovePtr& CurrentMove = ClientData->SavedMoves[i];
		CurrentMove->PrepMoveFor(CharacterOwner);
		MoveAutonomous(CurrentMove->TimeStamp, CurrentMove->DeltaTime, CurrentMove->GetCompressedFlags(), CurrentMove->Acceleration);
		CurrentMove->PostUpdate(CharacterOwner, FSavedMove_Character::PostUpdate_Replay);
	}

	if (ClientData->PendingMove.IsValid())
	{
		ClientData->PendingMove->bForceNoCombine = true;
	}

	// Restore saved values.
	AnalogInputModifier = SavedAnalogInputModifier;
	RootMotionParams = BackupRootMotionParams;
	CurrentRootMotion = BackupRootMotion;
	if (CharacterOwner->bClientResimulateRootMotionSources)
	{
		// If we were resimulating root motion sources, it's because we had mismatched state
		// with the server - we just resimulated our SavedMoves and now need to restore
		// CurrentRootMotion with the latest "good state"
		UE_LOG(LogRootMotion, VeryVerbose, TEXT("CurrentRootMotion getting updated after ServerUpdate replays: %s"), *CharacterOwner->GetName());
		CurrentRootMotion.UpdateStateFrom(CharacterOwner->SavedRootMotion);
		CharacterOwner->bClientResimulateRootMotionSources = false;
	}
	CharacterOwner->SavedRootMotion.Clear();
	CharacterOwner->bClientResimulateRootMotion = false;
	CharacterOwner->bClientUpdating = false;
	CharacterOwner->bPressedJump = bRealJump;
	bWantsToCrouch = bRealCrouch;
	bForceMaxAccel = bRealForceMaxAccel;
	bForceNextFloorCheck = true;
	
	return (ClientData->SavedMoves.Num() > 0);
}


void UCharacterMovementComponent::ForcePositionUpdate(float DeltaTime)
{
	if (!HasValidData() || MovementMode == MOVE_None || UpdatedComponent->Mobility != EComponentMobility::Movable)
	{
		return;
	}

	check(CharacterOwner->Role == ROLE_Authority);
	check(CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);

	// TODO: smooth correction on listen server?
	PerformMovement(DeltaTime);
}


FNetworkPredictionData_Client* UCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UCharacterMovementComponent* MutableThis = const_cast<UCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Character(*this);
	}

	return ClientPredictionData;
}

FNetworkPredictionData_Server* UCharacterMovementComponent::GetPredictionData_Server() const
{
	if (ServerPredictionData == nullptr)
	{
		UCharacterMovementComponent* MutableThis = const_cast<UCharacterMovementComponent*>(this);
		MutableThis->ServerPredictionData = new FNetworkPredictionData_Server_Character(*this);
	}

	return ServerPredictionData;
}


FNetworkPredictionData_Client_Character* UCharacterMovementComponent::GetPredictionData_Client_Character() const
{
	// Should only be called on client or listen server (for remote clients) in network games
	checkSlow(CharacterOwner != NULL);
	checkSlow(CharacterOwner->Role < ROLE_Authority || (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy && GetNetMode() == NM_ListenServer));
	checkSlow(GetNetMode() == NM_Client || GetNetMode() == NM_ListenServer);

	if (ClientPredictionData == nullptr)
	{
		UCharacterMovementComponent* MutableThis = const_cast<UCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = static_cast<class FNetworkPredictionData_Client_Character*>(GetPredictionData_Client());
	}

	return ClientPredictionData;
}


FNetworkPredictionData_Server_Character* UCharacterMovementComponent::GetPredictionData_Server_Character() const
{
	// Should only be called on server in network games
	checkSlow(CharacterOwner != NULL);
	checkSlow(CharacterOwner->Role == ROLE_Authority);
	checkSlow(GetNetMode() < NM_Client);

	if (ServerPredictionData == nullptr)
	{
		UCharacterMovementComponent* MutableThis = const_cast<UCharacterMovementComponent*>(this);
		MutableThis->ServerPredictionData = static_cast<class FNetworkPredictionData_Server_Character*>(GetPredictionData_Server());
	}

	return ServerPredictionData;
}

bool UCharacterMovementComponent::HasPredictionData_Client() const
{
	return (ClientPredictionData != nullptr) && HasValidData();
}

bool UCharacterMovementComponent::HasPredictionData_Server() const
{
	return (ServerPredictionData != nullptr) && HasValidData();
}

void UCharacterMovementComponent::ResetPredictionData_Client()
{
	if (ClientPredictionData)
	{
		delete ClientPredictionData;
		ClientPredictionData = nullptr;
	}
}

void UCharacterMovementComponent::ResetPredictionData_Server()
{
	if (ServerPredictionData)
	{
		delete ServerPredictionData;
		ServerPredictionData = nullptr;
	}	
}

float FNetworkPredictionData_Client_Character::UpdateTimeStampAndDeltaTime(float DeltaTime, class ACharacter & CharacterOwner, class UCharacterMovementComponent & CharacterMovementComponent)
{
	// Reset TimeStamp regularly to combat float accuracy decreasing over time.
	if( CurrentTimeStamp > CharacterMovementComponent.MinTimeBetweenTimeStampResets )
	{
		UE_LOG(LogNetPlayerMovement, Log, TEXT("Resetting Client's TimeStamp %f"), CurrentTimeStamp);
		CurrentTimeStamp -= CharacterMovementComponent.MinTimeBetweenTimeStampResets;

		// Mark all buffered moves as having old time stamps, so we make sure to not resend them.
		// That would confuse the server.
		for(int32 MoveIndex=0; MoveIndex<SavedMoves.Num(); MoveIndex++)
		{
			const FSavedMovePtr& CurrentMove = SavedMoves[MoveIndex];
			SavedMoves[MoveIndex]->bOldTimeStampBeforeReset = true;
		}
		// Do LastAckedMove as well. No need to do PendingMove as that move is part of the SavedMoves array.
		if( LastAckedMove.IsValid() )
		{
			LastAckedMove->bOldTimeStampBeforeReset = true;
		}

		// Also apply the reset to any active root motions.
		CharacterMovementComponent.CurrentRootMotion.ApplyTimeStampReset(CharacterMovementComponent.MinTimeBetweenTimeStampResets);
	}

	// Update Current TimeStamp.
	CurrentTimeStamp += DeltaTime;
	float ClientDeltaTime = DeltaTime;

	// Server uses TimeStamps to derive DeltaTime which introduces some rounding errors.
	// Make sure we do the same, so MoveAutonomous uses the same inputs and is deterministic!!
	if( SavedMoves.Num() > 0 )
	{
		const FSavedMovePtr& PreviousMove = SavedMoves.Last();
		if( !PreviousMove->bOldTimeStampBeforeReset )
		{
			// How server will calculate its deltatime to update physics.
			const float ServerDeltaTime = CurrentTimeStamp - PreviousMove->TimeStamp;
			// Have client always use the Server's DeltaTime. Otherwise our physics simulation will differ and we'll trigger too many position corrections and increase our network traffic.
			ClientDeltaTime = ServerDeltaTime;
		}
	}

	return FMath::Min(ClientDeltaTime, MaxMoveDeltaTime * CharacterOwner.GetActorTimeDilation());
}

void UCharacterMovementComponent::ReplicateMoveToServer(float DeltaTime, const FVector& NewAcceleration)
{
	SCOPE_CYCLE_COUNTER(STAT_CharacterMovementReplicateMoveToServer);
	check(CharacterOwner != NULL);

	// Can only start sending moves if our controllers are synced up over the network, otherwise we flood the reliable buffer.
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (PC && PC->AcknowledgedPawn != CharacterOwner)
	{
		return;
	}

	// Bail out if our character's controller doesn't have a Player. This may be the case when the local player
	// has switched to another controller, such as a debug camera controller.
	if (PC && PC->Player == nullptr)
	{
		return;
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	if (!ClientData)
	{
		return;
	}
	
	// Update our delta time for physics simulation.
	DeltaTime = ClientData->UpdateTimeStampAndDeltaTime(DeltaTime, *CharacterOwner, *this);

	// Find the oldest (unacknowledged) important move (OldMove).
	// Don't include the last move because it may be combined with the next new move.
	// A saved move is interesting if it differs significantly from the last acknowledged move
	FSavedMovePtr OldMove = NULL;
	if( ClientData->LastAckedMove.IsValid() )
	{
		const int32 NumSavedMoves = ClientData->SavedMoves.Num();
		for (int32 i=0; i < NumSavedMoves-1; i++)
		{
			const FSavedMovePtr& CurrentMove = ClientData->SavedMoves[i];
			if (CurrentMove->IsImportantMove(ClientData->LastAckedMove))
			{
				OldMove = CurrentMove;
				break;
			}
		}
	}

	// Get a SavedMove object to store the movement in.
	FSavedMovePtr NewMove = ClientData->CreateSavedMove();
	if (NewMove.IsValid() == false)
	{
		return;
	}

	NewMove->SetMoveFor(CharacterOwner, DeltaTime, NewAcceleration, *ClientData);

	// see if the two moves could be combined
	// do not combine moves which have different TimeStamps (before and after reset).
	if( ClientData->PendingMove.IsValid() && !ClientData->PendingMove->bOldTimeStampBeforeReset && ClientData->PendingMove->CanCombineWith(NewMove, CharacterOwner, ClientData->MaxMoveDeltaTime * CharacterOwner->GetActorTimeDilation()))
	{
		SCOPE_CYCLE_COUNTER(STAT_CharacterMovementCombineNetMove);

		// Only combine and move back to the start location if we don't move back in to a spot that would make us collide with something new.
		const FVector OldStartLocation = ClientData->PendingMove->GetRevertedLocation();
		if (!OverlapTest(OldStartLocation, ClientData->PendingMove->StartRotation.Quaternion(), UpdatedComponent->GetCollisionObjectType(), GetPawnCapsuleCollisionShape(SHRINK_None), CharacterOwner))
		{
			FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, EScopedUpdate::DeferredUpdates);
			UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("CombineMove: add delta %f + %f and revert from %f %f to %f %f"), DeltaTime, ClientData->PendingMove->DeltaTime, UpdatedComponent->GetComponentLocation().X, UpdatedComponent->GetComponentLocation().Y, OldStartLocation.X, OldStartLocation.Y);
			
			// to combine move, first revert pawn position to PendingMove start position, before playing combined move on client
			const bool bNoCollisionCheck = true;
			UpdatedComponent->SetWorldLocationAndRotation(OldStartLocation, ClientData->PendingMove->StartRotation, false);
			Velocity = ClientData->PendingMove->StartVelocity;

			SetBase(ClientData->PendingMove->StartBase.Get(), ClientData->PendingMove->StartBoneName);
			CurrentFloor = ClientData->PendingMove->StartFloor;

			// Now that we have reverted to the old position, prepare a new move from that position,
			// using our current velocity, acceleration, and rotation, but applied over the combined time from the old and new move.

			NewMove->DeltaTime += ClientData->PendingMove->DeltaTime;
			
			if (PC)
			{
				// We reverted position to that at the start of the pending move (above), however some code paths expect rotation to be set correctly
				// before character movement occurs (via FaceRotation), so try that now. The bOrientRotationToMovement path happens later as part of PerformMovement() and PhysicsRotation().
				CharacterOwner->FaceRotation(PC->GetControlRotation(), NewMove->DeltaTime);
			}

			SaveBaseLocation();
			NewMove->SetInitialPosition(CharacterOwner);

			// Remove pending move from move list. It would have to be the last move on the list.
			if (ClientData->SavedMoves.Num() > 0 && ClientData->SavedMoves.Last() == ClientData->PendingMove)
			{
				const bool bAllowShrinking = false;
				ClientData->SavedMoves.Pop(bAllowShrinking);
			}
			ClientData->FreeMove(ClientData->PendingMove);
			ClientData->PendingMove = NULL;
		}
		else
		{
			//UE_LOG(LogNet, Log, TEXT("Not combining move, would collide at start location"));
		}
	}

	// Acceleration should match what we send to the server, plus any other restrictions the server also enforces (see MoveAutonomous).
	Acceleration = NewMove->Acceleration.GetClampedToMaxSize(GetMaxAcceleration());
	AnalogInputModifier = ComputeAnalogInputModifier(); // recompute since acceleration may have changed.

	// Perform the move locally
	CharacterOwner->ClientRootMotionParams.Clear();
	CharacterOwner->SavedRootMotion.Clear();
	PerformMovement(NewMove->DeltaTime);

	NewMove->PostUpdate(CharacterOwner, FSavedMove_Character::PostUpdate_Record);

	// Add NewMove to the list
	if (CharacterOwner->bReplicateMovement)
	{
		ClientData->SavedMoves.Push(NewMove);
		const UWorld* MyWorld = GetWorld();

		const bool bCanDelayMove = (CharacterMovementCVars::NetEnableMoveCombining != 0) && CanDelaySendingMove(NewMove);
		
		if (bCanDelayMove && ClientData->PendingMove.IsValid() == false)
		{
			// Decide whether to hold off on move
			const float NetMoveDelta = FMath::Clamp(GetClientNetSendDeltaTime(PC, ClientData, NewMove), 1.f/120.f, 1.f/15.f);

			if ((MyWorld->TimeSeconds - ClientData->ClientUpdateTime) * MyWorld->GetWorldSettings()->GetEffectiveTimeDilation() < NetMoveDelta)
			{
				// Delay sending this move.
				ClientData->PendingMove = NewMove;
				return;
			}
		}

		ClientData->ClientUpdateTime = MyWorld->TimeSeconds;

		UE_LOG(LogNetPlayerMovement, Verbose, TEXT("Client ReplicateMove Time %f Acceleration %s Position %s DeltaTime %f"),
			NewMove->TimeStamp, *NewMove->Acceleration.ToString(), *UpdatedComponent->GetComponentLocation().ToString(), DeltaTime);

		// Send move to server if this character is replicating movement
		{
			SCOPE_CYCLE_COUNTER(STAT_CharacterMovementCallServerMove);
			CallServerMove(NewMove.Get(), OldMove.Get());
		}
	}

	ClientData->PendingMove = NULL;
}

void UCharacterMovementComponent::CallServerMove
	(
	const class FSavedMove_Character* NewMove,
	const class FSavedMove_Character* OldMove
	)
{
	check(NewMove != NULL);

	// Compress rotation down to 5 bytes
	const uint32 ClientYawPitchINT = PackYawAndPitchTo32(NewMove->SavedControlRotation.Yaw, NewMove->SavedControlRotation.Pitch);
	const uint8 ClientRollBYTE = FRotator::CompressAxisToByte(NewMove->SavedControlRotation.Roll);

	// Determine if we send absolute or relative location
	UPrimitiveComponent* ClientMovementBase = NewMove->EndBase.Get();
	const FName ClientBaseBone = NewMove->EndBoneName;
	const FVector SendLocation = MovementBaseUtility::UseRelativeLocation(ClientMovementBase) ? NewMove->SavedRelativeLocation : NewMove->SavedLocation;

	// send old move if it exists
	if (OldMove)
	{
		ServerMoveOld(OldMove->TimeStamp, OldMove->Acceleration, OldMove->GetCompressedFlags());
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	if (ClientData->PendingMove.IsValid())
	{
		const uint32 OldClientYawPitchINT = PackYawAndPitchTo32(ClientData->PendingMove->SavedControlRotation.Yaw, ClientData->PendingMove->SavedControlRotation.Pitch);

		// If we delayed a move without root motion, and our new move has root motion, send these through a special function, so the server knows how to process them.
		if ((ClientData->PendingMove->RootMotionMontage == NULL) && (NewMove->RootMotionMontage != NULL))
		{
			// send two moves simultaneously
			ServerMoveDualHybridRootMotion
				(
				ClientData->PendingMove->TimeStamp,
				ClientData->PendingMove->Acceleration,
				ClientData->PendingMove->GetCompressedFlags(),
				OldClientYawPitchINT,
				NewMove->TimeStamp,
				NewMove->Acceleration,
				SendLocation,
				NewMove->GetCompressedFlags(),
				ClientRollBYTE,
				ClientYawPitchINT,
				ClientMovementBase,
				ClientBaseBone,
				NewMove->MovementMode
				);
		}
		else
		{
			// send two moves simultaneously
			ServerMoveDual
				(
				ClientData->PendingMove->TimeStamp,
				ClientData->PendingMove->Acceleration,
				ClientData->PendingMove->GetCompressedFlags(),
				OldClientYawPitchINT,
				NewMove->TimeStamp,
				NewMove->Acceleration,
				SendLocation,
				NewMove->GetCompressedFlags(),
				ClientRollBYTE,
				ClientYawPitchINT,
				ClientMovementBase,
				ClientBaseBone,
				NewMove->MovementMode
				);
		}
	}
	else
	{
		ServerMove
			(
			NewMove->TimeStamp,
			NewMove->Acceleration,
			SendLocation,
			NewMove->GetCompressedFlags(),
			ClientRollBYTE,
			ClientYawPitchINT,
			ClientMovementBase,
			ClientBaseBone,
			NewMove->MovementMode
			);
	}


	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	APlayerCameraManager* PlayerCameraManager = (PC ? PC->PlayerCameraManager : NULL);
	if (PlayerCameraManager != NULL && PlayerCameraManager->bUseClientSideCameraUpdates)
	{
		PlayerCameraManager->bShouldSendClientSideCameraUpdate = true;
	}
}



void UCharacterMovementComponent::ServerMoveOld_Implementation
	(
	float OldTimeStamp,
	FVector_NetQuantize10 OldAccel,
	uint8 OldMoveFlags
	)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if( !VerifyClientTimeStamp(OldTimeStamp, *ServerData) )
	{
		return;
	}

	UE_LOG(LogNetPlayerMovement, Log, TEXT("Recovered move from OldTimeStamp %f, DeltaTime: %f"), OldTimeStamp, OldTimeStamp - ServerData->CurrentClientTimeStamp);

	const float DeltaTime = ServerData->GetServerMoveDeltaTime(OldTimeStamp, CharacterOwner->GetActorTimeDilation());

	ServerData->CurrentClientTimeStamp = OldTimeStamp;
	ServerData->ServerTimeStamp = GetWorld()->GetTimeSeconds();
	ServerData->ServerTimeStampLastServerMove = ServerData->ServerTimeStamp;

	MoveAutonomous(OldTimeStamp, DeltaTime, OldMoveFlags, OldAccel);
}


void UCharacterMovementComponent::ServerMoveDual_Implementation(
	float TimeStamp0,
	FVector_NetQuantize10 InAccel0,
	uint8 PendingFlags,
	uint32 View0,
	float TimeStamp,
	FVector_NetQuantize10 InAccel,
	FVector_NetQuantize100 ClientLoc,
	uint8 NewFlags,
	uint8 ClientRoll,
	uint32 View,
	UPrimitiveComponent* ClientMovementBase,
	FName ClientBaseBone,
	uint8 ClientMovementMode)
{
	ServerMove_Implementation(TimeStamp0, InAccel0, FVector(1.f,2.f,3.f), PendingFlags, ClientRoll, View0, ClientMovementBase, ClientBaseBone, ClientMovementMode);
	ServerMove_Implementation(TimeStamp, InAccel, ClientLoc, NewFlags, ClientRoll, View, ClientMovementBase, ClientBaseBone, ClientMovementMode);
}

void UCharacterMovementComponent::ServerMoveDualHybridRootMotion_Implementation(
	float TimeStamp0,
	FVector_NetQuantize10 InAccel0,
	uint8 PendingFlags,
	uint32 View0,
	float TimeStamp,
	FVector_NetQuantize10 InAccel,
	FVector_NetQuantize100 ClientLoc,
	uint8 NewFlags,
	uint8 ClientRoll,
	uint32 View,
	UPrimitiveComponent* ClientMovementBase,
	FName ClientBaseBone,
	uint8 ClientMovementMode)
{
	// First move received didn't use root motion, process it as such.
	CharacterOwner->bServerMoveIgnoreRootMotion = CharacterOwner->IsPlayingNetworkedRootMotionMontage();
	ServerMove_Implementation(TimeStamp0, InAccel0, FVector(1.f, 2.f, 3.f), PendingFlags, ClientRoll, View0, ClientMovementBase, ClientBaseBone, ClientMovementMode);
	CharacterOwner->bServerMoveIgnoreRootMotion = false;

	ServerMove_Implementation(TimeStamp, InAccel, ClientLoc, NewFlags, ClientRoll, View, ClientMovementBase, ClientBaseBone, ClientMovementMode);
}

bool UCharacterMovementComponent::VerifyClientTimeStamp(float TimeStamp, FNetworkPredictionData_Server_Character& ServerData)
{
	bool bTimeStampResetDetected = false;
	const bool bIsValid = IsClientTimeStampValid(TimeStamp, ServerData, bTimeStampResetDetected);
	if (bIsValid)
	{
		if (bTimeStampResetDetected)
		{
			UE_LOG(LogNetPlayerMovement, Log, TEXT("TimeStamp reset detected. CurrentTimeStamp: %f, new TimeStamp: %f"), ServerData.CurrentClientTimeStamp, TimeStamp);
			OnClientTimeStampResetDetected();
			ServerData.CurrentClientTimeStamp -= MinTimeBetweenTimeStampResets;

			// Also apply the reset to any active root motions.
			CurrentRootMotion.ApplyTimeStampReset(MinTimeBetweenTimeStampResets);
		}
		else
		{
			UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("TimeStamp %f Accepted! CurrentTimeStamp: %f"), TimeStamp, ServerData.CurrentClientTimeStamp);
			ProcessClientTimeStampForTimeDiscrepancy(TimeStamp, ServerData);
		}
		return true;
	}
	else
	{
		if (bTimeStampResetDetected)
		{
			UE_LOG(LogNetPlayerMovement, Log, TEXT("TimeStamp expired. Before TimeStamp Reset. CurrentTimeStamp: %f, TimeStamp: %f"), ServerData.CurrentClientTimeStamp, TimeStamp);
		}
		else
		{
			UE_LOG(LogNetPlayerMovement, Log, TEXT("TimeStamp expired. %f, CurrentTimeStamp: %f"), TimeStamp, ServerData.CurrentClientTimeStamp);
		}
		return false;
	}
}

void UCharacterMovementComponent::ProcessClientTimeStampForTimeDiscrepancy(float ClientTimeStamp, FNetworkPredictionData_Server_Character& ServerData)
{
	// Should only be called on server in network games
	check(CharacterOwner != NULL);
	check(CharacterOwner->Role == ROLE_Authority);
	checkSlow(GetNetMode() < NM_Client);

	// Movement time discrepancy detection and resolution (potentially caused by client speed hacks, time manipulation)
	// Track client reported time deltas through ServerMove RPCs vs actual server time, when error accumulates enough
	// trigger prevention measures where client must "pay back" the time difference
	const bool bServerMoveHasOccurred = ServerData.ServerTimeStampLastServerMove != 0.f;
	AGameNetworkManager* GameNetworkManager = AGameNetworkManager::StaticClass()->GetDefaultObject<AGameNetworkManager>();
	if (GameNetworkManager != nullptr && GameNetworkManager->bMovementTimeDiscrepancyDetection && bServerMoveHasOccurred)
	{
		const float WorldTimeSeconds = GetWorld()->GetTimeSeconds();
		const float ServerDelta = (WorldTimeSeconds - ServerData.ServerTimeStampLastServerMove) * CharacterOwner->CustomTimeDilation;
		const float ClientDelta = ClientTimeStamp - ServerData.CurrentClientTimeStamp;
		const float ClientError = ClientDelta - ServerDelta; // Difference between how much time client has ticked since last move vs server

		// Accumulate raw total discrepancy, unfiltered/unbound (for tracking more long-term trends over the lifetime of the CharacterMovementComponent)
		ServerData.LifetimeRawTimeDiscrepancy += ClientError;

		//
		// 1. Determine total effective discrepancy 
		//
		// NewTimeDiscrepancy is bounded and has a DriftAllowance to limit momentary burst packet loss or 
		// low framerate from having significant impacts, which could cause needing multiple seconds worth of 
		// slow-down/speed-up even though it wasn't intentional time manipulation
		float NewTimeDiscrepancy = ServerData.TimeDiscrepancy + ClientError;
		{
			// Apply drift allowance - forgiving percent difference per time for error
			const float DriftAllowance = GameNetworkManager->MovementTimeDiscrepancyDriftAllowance;
			if (DriftAllowance > 0.f)
			{
				if (NewTimeDiscrepancy > 0.f)
				{
					NewTimeDiscrepancy = FMath::Max(NewTimeDiscrepancy - ServerDelta*DriftAllowance, 0.f); 
				}
				else
				{
					NewTimeDiscrepancy = FMath::Min(NewTimeDiscrepancy + ServerDelta*DriftAllowance, 0.f); 
				}
			}

			// Enforce bounds
			// Never go below MinTimeMargin - ClientError being negative means the client is BEHIND
			// the server (they are going slower).
			NewTimeDiscrepancy = FMath::Max(NewTimeDiscrepancy, GameNetworkManager->MovementTimeDiscrepancyMinTimeMargin); 
		}

		// Determine EffectiveClientError, which is error for the currently-being-processed move after 
		// drift allowances/clamping/resolution mode modifications.
		// We need to know how much the current move contributed towards actionable error so that we don't
		// count error that the server never allowed to impact movement to matter
		float EffectiveClientError = ClientError;
		{
			const float NewTimeDiscrepancyRaw = ServerData.TimeDiscrepancy + ClientError;
			if (NewTimeDiscrepancyRaw != 0.f)
			{
				EffectiveClientError = ClientError * (NewTimeDiscrepancy / NewTimeDiscrepancyRaw);
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Per-frame spew of time discrepancy-related values - useful for investigating state of time discrepancy tracking
		if (CharacterMovementCVars::DebugTimeDiscrepancy > 0)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("TimeDiscrepancyDetection: ClientError: %f, TimeDiscrepancy: %f, LifetimeRawTimeDiscrepancy: %f (Lifetime %f), Resolving: %d, ClientDelta: %f, ServerDelta: %f, ClientTimeStamp: %f"), 
				ClientError, ServerData.TimeDiscrepancy, ServerData.LifetimeRawTimeDiscrepancy, WorldTimeSeconds - ServerData.WorldCreationTime, ServerData.bResolvingTimeDiscrepancy, ClientDelta, ServerDelta, ClientTimeStamp);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

		//
		// 2. If we were in resolution mode, determine if we still need to be
		//
		ServerData.bResolvingTimeDiscrepancy = ServerData.bResolvingTimeDiscrepancy && (ServerData.TimeDiscrepancy > 0.f);

		//
		// 3. Determine if NewTimeDiscrepancy is significant enough to trigger detection, and if so, trigger resolution if enabled
		//
		if (!ServerData.bResolvingTimeDiscrepancy) 
		{
			if (NewTimeDiscrepancy > GameNetworkManager->MovementTimeDiscrepancyMaxTimeMargin)
			{
				// Time discrepancy detected - client timestamp ahead of where the server thinks it should be!

				// Trigger logic for resolving time discrepancies
				if (GameNetworkManager->bMovementTimeDiscrepancyResolution)
				{
					// Trigger Resolution
					ServerData.bResolvingTimeDiscrepancy = true;

					// Transfer calculated error to official TimeDiscrepancy value, which is the time that will be resolved down
					// in this and subsequent moves until it reaches 0 (meaning we equalize the error)
					// Don't include contribution to error for this move, since we are now going to be in resolution mode
					// and the expected client error (though it did help trigger resolution) won't be allowed
					// to increase error this frame
					ServerData.TimeDiscrepancy = (NewTimeDiscrepancy - EffectiveClientError);
				}
				else
				{
					// We're detecting discrepancy but not handling resolving that through movement component.
					// Clear time stamp error accumulated that triggered detection so we start fresh (maybe it was triggered
					// during severe hitches/packet loss/other non-goodness)
					ServerData.TimeDiscrepancy = 0.f;
				}

				// Project-specific resolution (reporting/recording/analytics)
				OnTimeDiscrepancyDetected(NewTimeDiscrepancy, ServerData.LifetimeRawTimeDiscrepancy, WorldTimeSeconds - ServerData.WorldCreationTime, ClientError);
			}
			else
			{
				// When not in resolution mode and still within error tolerances, accrue total discrepancy
				ServerData.TimeDiscrepancy = NewTimeDiscrepancy;
			}
		}

		//
		// 4. If we are actively resolving time discrepancy, we do so by altering the DeltaTime for the current ServerMove
		//
		if (ServerData.bResolvingTimeDiscrepancy)
		{
			// Optionally force client corrections during time discrepancy resolution
			// This is useful when default project movement error checking is lenient or ClientAuthorativePosition is enabled
			// to ensure time discrepancy resolution is enforced
			if (GameNetworkManager->bMovementTimeDiscrepancyForceCorrectionsDuringResolution)
			{
				ServerData.bForceClientUpdate = true;
			}

			// Movement time discrepancy resolution
			// When the server has detected a significant time difference between what the client ServerMove RPCs are reporting
			// and the actual time that has passed on the server (pointing to potential speed hacks/time manipulation by client),
			// we enter a resolution mode where the usual "base delta's off of client's reported timestamps" is clamped down
			// to the server delta since last movement update, so that during resolution we're not allowing further advantage.
			// Out of that ServerDelta-based move delta, we also need the client to "pay back" the time stolen from initial 
			// time discrepancy detection (held in TimeDiscrepancy) at a specified rate (AGameNetworkManager::TimeDiscrepancyResolutionRate) 
			// to equalize movement time passed on client and server before we can consider the discrepancy "resolved"
			const float ServerCurrentTimeStamp = WorldTimeSeconds;
			const float ServerDeltaSinceLastMovementUpdate = (ServerCurrentTimeStamp - ServerData.ServerTimeStamp) * CharacterOwner->CustomTimeDilation;
			const bool bIsFirstServerMoveThisServerTick = ServerDeltaSinceLastMovementUpdate > 0.f;

			// Restrict ServerMoves to server deltas during time discrepancy resolution 
			// (basing moves off of trusted server time, not client timestamp deltas)
			const float BaseDeltaTime = ServerData.GetBaseServerMoveDeltaTime(ClientTimeStamp, CharacterOwner->GetActorTimeDilation());

			if (!bIsFirstServerMoveThisServerTick)
			{
				// Accumulate client deltas for multiple ServerMoves per server tick so that the next server tick
				// can pay back the full amount of that tick and not be bounded by a single small Move delta
				ServerData.TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick += BaseDeltaTime;
			}

			float ServerBoundDeltaTime = FMath::Min(BaseDeltaTime + ServerData.TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick, ServerDeltaSinceLastMovementUpdate); 
			ServerBoundDeltaTime = FMath::Max(ServerBoundDeltaTime, 0.f); // No negative deltas allowed

			if (bIsFirstServerMoveThisServerTick)
			{
				// The first ServerMove for a server tick has used the accumulated client delta in the ServerBoundDeltaTime
				// calculation above, clear it out for next frame where we have multiple ServerMoves
				ServerData.TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick = 0.f;
			}

			// Calculate current move DeltaTime and PayBack time based on resolution rate
			const float ResolutionRate = FMath::Clamp(GameNetworkManager->MovementTimeDiscrepancyResolutionRate, 0.f, 1.f);
			float TimeToPayBack = FMath::Min(ServerBoundDeltaTime * ResolutionRate, ServerData.TimeDiscrepancy); // Make sure we only pay back the time we need to
			float DeltaTimeAfterPayback = ServerBoundDeltaTime - TimeToPayBack;

			// Adjust deltas so current move DeltaTime adheres to minimum tick time
			DeltaTimeAfterPayback = FMath::Max(DeltaTimeAfterPayback, UCharacterMovementComponent::MIN_TICK_TIME);
			TimeToPayBack = ServerBoundDeltaTime - DeltaTimeAfterPayback;

			// Output of resolution: an overridden delta time that will be picked up for this ServerMove, and removing the time
			// we paid back by overriding the DeltaTime to TimeDiscrepancy (time needing resolved)
			ServerData.TimeDiscrepancyResolutionMoveDeltaOverride = DeltaTimeAfterPayback;
			ServerData.TimeDiscrepancy -= TimeToPayBack;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Per-frame spew of time discrepancy resolution related values - useful for investigating state of time discrepancy tracking
		if ( CharacterMovementCVars::DebugTimeDiscrepancy > 1 )
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("TimeDiscrepancyResolution: DeltaOverride: %f, TimeToPayBack: %f, BaseDelta: %f, ServerDeltaSinceLastMovementUpdate: %f, TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick: %f"), 
				ServerData.TimeDiscrepancyResolutionMoveDeltaOverride, TimeToPayBack, BaseDeltaTime, ServerDeltaSinceLastMovementUpdate, ServerData.TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick);
		}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		}
	}
}

bool UCharacterMovementComponent::IsClientTimeStampValid(float TimeStamp, const FNetworkPredictionData_Server_Character& ServerData, bool& bTimeStampResetDetected) const
{
	// Very large deltas happen around a TimeStamp reset.
	const float DeltaTimeStamp = (TimeStamp - ServerData.CurrentClientTimeStamp);
	if( FMath::Abs(DeltaTimeStamp) > (MinTimeBetweenTimeStampResets * 0.5f) )
	{
		// Client is resetting TimeStamp to increase accuracy.
		bTimeStampResetDetected = true;
		if( DeltaTimeStamp < 0.f )
		{
			// TimeStamp accepted with reset
			return true;
		}
		else
		{
			// We already reset the TimeStamp, but we just got an old outdated move before the switch, not valid.
			return false;
		}
	}

	// If TimeStamp is in the past, move is outdated, not valid.
	if( TimeStamp <= ServerData.CurrentClientTimeStamp )
	{
		return false;
	}
	
	// TimeStamp valid.
	return true;
}

void UCharacterMovementComponent::OnClientTimeStampResetDetected()
{
}

void UCharacterMovementComponent::OnTimeDiscrepancyDetected(float CurrentTimeDiscrepancy, float LifetimeRawTimeDiscrepancy, float Lifetime, float CurrentMoveError)
{
	UE_LOG(LogNetPlayerMovement, Verbose, TEXT("Movement Time Discrepancy detected between client-reported time and server on character %s. CurrentTimeDiscrepancy: %f, LifetimeRawTimeDiscrepancy: %f, Lifetime: %f, CurrentMoveError %f"), 
		CharacterOwner ? *CharacterOwner->GetHumanReadableName() : TEXT("<UNKNOWN>"), 
		CurrentTimeDiscrepancy, 
		LifetimeRawTimeDiscrepancy, 
		Lifetime,
		CurrentMoveError);
}

void UCharacterMovementComponent::ServerMove_Implementation(
	float TimeStamp,
	FVector_NetQuantize10 InAccel,
	FVector_NetQuantize100 ClientLoc,
	uint8 MoveFlags,
	uint8 ClientRoll,
	uint32 View,
	UPrimitiveComponent* ClientMovementBase,
	FName ClientBaseBoneName,
	uint8 ClientMovementMode)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}	

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if( !VerifyClientTimeStamp(TimeStamp, *ServerData) )
	{
		return;
	}

	bool bServerReadyForClient = true;
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (PC)
	{
		bServerReadyForClient = PC->NotifyServerReceivedClientData(CharacterOwner, TimeStamp);
		if (!bServerReadyForClient)
		{
			InAccel = FVector::ZeroVector;
		}
	}

	// View components
	const uint16 ViewPitch = (View & 65535);
	const uint16 ViewYaw = (View >> 16);
	
	const FVector Accel = InAccel;
	// Save move parameters.
	const float DeltaTime = ServerData->GetServerMoveDeltaTime(TimeStamp, CharacterOwner->GetActorTimeDilation());

	ServerData->CurrentClientTimeStamp = TimeStamp;
	ServerData->ServerTimeStamp = GetWorld()->GetTimeSeconds();
	ServerData->ServerTimeStampLastServerMove = ServerData->ServerTimeStamp;
	FRotator ViewRot;
	ViewRot.Pitch = FRotator::DecompressAxisFromShort(ViewPitch);
	ViewRot.Yaw = FRotator::DecompressAxisFromShort(ViewYaw);
	ViewRot.Roll = FRotator::DecompressAxisFromByte(ClientRoll);

	if (PC)
	{
		PC->SetControlRotation(ViewRot);
	}

	if (!bServerReadyForClient)
	{
		return;
	}

	// Perform actual movement
	if ((GetWorld()->GetWorldSettings()->Pauser == NULL) && (DeltaTime > 0.f))
	{
		if (PC)
		{
			PC->UpdateRotation(DeltaTime);
		}

		MoveAutonomous(TimeStamp, DeltaTime, MoveFlags, Accel);
	}

	UE_LOG(LogNetPlayerMovement, Verbose, TEXT("ServerMove Time %f Acceleration %s Position %s DeltaTime %f"),
			TimeStamp, *Accel.ToString(), *UpdatedComponent->GetComponentLocation().ToString(), DeltaTime);

	ServerMoveHandleClientError(TimeStamp, DeltaTime, Accel, ClientLoc, ClientMovementBase, ClientBaseBoneName, ClientMovementMode);
}


void UCharacterMovementComponent::ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& RelativeClientLoc, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	if (RelativeClientLoc == FVector(1.f,2.f,3.f)) // first part of double servermove
	{
		return;
	}

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	// Don't prevent more recent updates from being sent if received this frame.
	// We're going to send out an update anyway, might as well be the most recent one.
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if( (ServerData->LastUpdateTime != GetWorld()->TimeSeconds) && GetDefault<AGameNetworkManager>()->WithinUpdateDelayBounds(PC, ServerData->LastUpdateTime))
	{
		return;
	}

	// Offset may be relative to base component
	FVector ClientLoc = RelativeClientLoc;
	if (MovementBaseUtility::UseRelativeLocation(ClientMovementBase))
	{
		FVector BaseLocation;
		FQuat BaseRotation;
		MovementBaseUtility::GetMovementBaseTransform(ClientMovementBase, ClientBaseBoneName, BaseLocation, BaseRotation);
		ClientLoc += BaseLocation;
	}

	// Compute the client error from the server's position
	// If client has accumulated a noticeable positional error, correct him.
	if (ServerData->bForceClientUpdate || ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientLoc, RelativeClientLoc, ClientMovementBase, ClientBaseBoneName, ClientMovementMode))
	{
		UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
		ServerData->PendingAdjustment.NewVel = Velocity;
		ServerData->PendingAdjustment.NewBase = MovementBase;
		ServerData->PendingAdjustment.NewBaseBoneName = CharacterOwner->GetBasedMovement().BoneName;
		ServerData->PendingAdjustment.NewLoc = FRepMovement::RebaseOntoZeroOrigin(UpdatedComponent->GetComponentLocation(), this);
		ServerData->PendingAdjustment.NewRot = UpdatedComponent->GetComponentRotation();

		ServerData->PendingAdjustment.bBaseRelativePosition = MovementBaseUtility::UseRelativeLocation(MovementBase);
		if (ServerData->PendingAdjustment.bBaseRelativePosition)
		{
			// Relative location
			ServerData->PendingAdjustment.NewLoc = CharacterOwner->GetBasedMovement().Location;
			
			// TODO: this could be a relative rotation, but all client corrections ignore rotation right now except the root motion one, which would need to be updated.
			//ServerData->PendingAdjustment.NewRot = CharacterOwner->GetBasedMovement().Rotation;
		}


#if !UE_BUILD_SHIPPING
		if (CharacterMovementCVars::NetShowCorrections != 0)
		{
			const FVector LocDiff = UpdatedComponent->GetComponentLocation() - ClientLoc;
			const FString BaseString = MovementBase ? MovementBase->GetPathName(MovementBase->GetOutermost()) : TEXT("None");
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("*** Server: Error for %s at Time=%.3f is %3.3f LocDiff(%s) ClientLoc(%s) ServerLoc(%s) Base: %s Bone: %s Accel(%s) Velocity(%s)"),
				*GetNameSafe(CharacterOwner), ClientTimeStamp, LocDiff.Size(), *LocDiff.ToString(), *ClientLoc.ToString(), *UpdatedComponent->GetComponentLocation().ToString(), *BaseString, *ServerData->PendingAdjustment.NewBaseBoneName.ToString(), *Accel.ToString(), *Velocity.ToString());
			const float DebugLifetime = CharacterMovementCVars::NetCorrectionLifetime;
			DrawDebugCapsule(GetWorld(), UpdatedComponent->GetComponentLocation(), CharacterOwner->GetSimpleCollisionHalfHeight(), CharacterOwner->GetSimpleCollisionRadius(), FQuat::Identity, FColor(100, 255, 100), true, DebugLifetime);
			DrawDebugCapsule(GetWorld(), ClientLoc                    , CharacterOwner->GetSimpleCollisionHalfHeight(), CharacterOwner->GetSimpleCollisionRadius(), FQuat::Identity, FColor(255, 100, 100), true, DebugLifetime);
		}
#endif

		ServerData->LastUpdateTime = GetWorld()->TimeSeconds;
		ServerData->PendingAdjustment.DeltaTime = DeltaTime;
		ServerData->PendingAdjustment.TimeStamp = ClientTimeStamp;
		ServerData->PendingAdjustment.bAckGoodMove = false;
		ServerData->PendingAdjustment.MovementMode = PackNetworkMovementMode();

		PerfCountersIncrement(TEXT("NumServerMoveCorrections"));
	}
	else
	{
		if (GetDefault<AGameNetworkManager>()->ClientAuthorativePosition)
		{
			const FVector LocDiff = UpdatedComponent->GetComponentLocation() - ClientLoc; //-V595
			if (!LocDiff.IsZero() || ClientMovementMode != PackNetworkMovementMode() || GetMovementBase() != ClientMovementBase || (CharacterOwner && CharacterOwner->GetBasedMovement().BoneName != ClientBaseBoneName))
			{
				// Just set the position. On subsequent moves we will resolve initially overlapping conditions.
				UpdatedComponent->SetWorldLocation(ClientLoc, false); //-V595

				// Trust the client's movement mode.
				ApplyNetworkMovementMode(ClientMovementMode);

				// Update base and floor at new location.
				SetBase(ClientMovementBase, ClientBaseBoneName);
				UpdateFloorFromAdjustment();

				// Even if base has not changed, we need to recompute the relative offsets (since we've moved).
				SaveBaseLocation();

				LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
				LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
				LastUpdateVelocity = Velocity;
			}
		}

		// acknowledge receipt of this successful servermove()
		ServerData->PendingAdjustment.TimeStamp = ClientTimeStamp;
		ServerData->PendingAdjustment.bAckGoodMove = true;
	}

	PerfCountersIncrement(TEXT("NumServerMoves"));

	ServerData->bForceClientUpdate = false;
}

bool UCharacterMovementComponent::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	// Check location difference against global setting
	if (!bIgnoreClientMovementErrorChecksAndCorrection)
	{
		const FVector LocDiff = UpdatedComponent->GetComponentLocation() - ClientWorldLocation;

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
		{
			FString AdjustedDebugString = FString::Printf(TEXT("ServerCheckClientError LocDiff(%.1f) ExceedsAllowablePositionError(%d) TimeStamp(%f)"),
				LocDiff.Size(), GetDefault<AGameNetworkManager>()->ExceedsAllowablePositionError(LocDiff), ClientTimeStamp);
			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		}
#endif
		if (GetDefault<AGameNetworkManager>()->ExceedsAllowablePositionError(LocDiff))
		{
			return true;
		}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (CharacterMovementCVars::NetForceClientAdjustmentPercent > SMALL_NUMBER)
		{
			if (FMath::SRand() < CharacterMovementCVars::NetForceClientAdjustmentPercent)
			{
				UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("** ServerCheckClientError forced by p.NetForceClientAdjustmentPercent"));
				return true;
			}
		}
#endif
	}
	else
	{
#if !UE_BUILD_SHIPPING
		if (CharacterMovementCVars::NetShowCorrections != 0)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("*** Server: %s is set to ignore error checks and corrections."), *GetNameSafe(CharacterOwner));
		}
#endif // !UE_BUILD_SHIPPING
	}

	// Check for disagreement in movement mode
	const uint8 CurrentPackedMovementMode = PackNetworkMovementMode();
	if (CurrentPackedMovementMode != ClientMovementMode)
	{
		return true;
	}

	return false;
}


bool UCharacterMovementComponent::ServerMove_Validate(float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, uint8 MoveFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	return true;
}

bool UCharacterMovementComponent::ServerMoveDual_Validate(float TimeStamp0, FVector_NetQuantize10 InAccel0, uint8 PendingFlags, uint32 View0, float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, uint8 NewFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	return true;
}

bool UCharacterMovementComponent::ServerMoveDualHybridRootMotion_Validate(float TimeStamp0, FVector_NetQuantize10 InAccel0, uint8 PendingFlags, uint32 View0, float TimeStamp, FVector_NetQuantize10 InAccel, FVector_NetQuantize100 ClientLoc, uint8 NewFlags, uint8 ClientRoll, uint32 View, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	return true;
}

bool UCharacterMovementComponent::ServerMoveOld_Validate(float OldTimeStamp, FVector_NetQuantize10 OldAccel, uint8 OldMoveFlags)
{
	return true;
}



void UCharacterMovementComponent::MoveAutonomous
	(
	float ClientTimeStamp,
	float DeltaTime,
	uint8 CompressedFlags,
	const FVector& NewAccel
	)
{
	if (!HasValidData())
	{
		return;
	}

	UpdateFromCompressedFlags(CompressedFlags);
	CharacterOwner->CheckJumpInput(DeltaTime);

	Acceleration = ConstrainInputAcceleration(NewAccel);
	Acceleration = Acceleration.GetClampedToMaxSize(GetMaxAcceleration());
	AnalogInputModifier = ComputeAnalogInputModifier();
	
	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FQuat OldRotation = UpdatedComponent->GetComponentQuat();

	PerformMovement(DeltaTime);

	// Check if data is valid as PerformMovement can mark character for pending kill
	if (!HasValidData())
	{
		return;
	}

	// If not playing root motion, tick animations after physics. We do this here to keep events, notifies, states and transitions in sync with client updates.
	if( CharacterOwner && !CharacterOwner->bClientUpdating && !CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh() )
	{
		TickCharacterPose(DeltaTime);
		// TODO: SaveBaseLocation() in case tick moves us?

		// Trigger Events right away, as we could be receiving multiple ServerMoves per frame.
		CharacterOwner->GetMesh()->ConditionallyDispatchQueuedAnimEvents();
	}

	if (CharacterOwner && UpdatedComponent)
	{
		// Smooth local view of remote clients on listen servers
		if (CharacterMovementCVars::NetEnableListenServerSmoothing &&
			CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy &&
			IsNetMode(NM_ListenServer))
		{
			SmoothCorrection(OldLocation, OldRotation, UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat());
		}
	}
}


void UCharacterMovementComponent::UpdateFloorFromAdjustment()
{
	if (!HasValidData())
	{
		return;
	}

	// If walking, try to update the cached floor so it is current. This is necessary for UpdateBasedMovement() and MoveAlongFloor() to work properly.
	// If base is now NULL, presumably we are no longer walking. If we had a valid floor but don't find one now, we'll likely start falling.
	if (CharacterOwner->GetMovementBase())
	{
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
	}
	else
	{
		CurrentFloor.Clear();
	}

	bForceNextFloorCheck = true;
}


void UCharacterMovementComponent::SendClientAdjustment()
{
	if (!HasValidData())
	{
		return;
	}

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if (ServerData->PendingAdjustment.TimeStamp <= 0.f)
	{
		return;
	}

	if (ServerData->PendingAdjustment.bAckGoodMove == true)
	{
		// just notify client this move was received
		ClientAckGoodMove(ServerData->PendingAdjustment.TimeStamp);
	}
	else
	{
		const bool bIsPlayingNetworkedRootMotionMontage = CharacterOwner->IsPlayingNetworkedRootMotionMontage();
		if( HasRootMotionSources() )
		{
			FRotator Rotation = ServerData->PendingAdjustment.NewRot.GetNormalized();
			FVector_NetQuantizeNormal CompressedRotation(Rotation.Pitch / 180.f, Rotation.Yaw / 180.f, Rotation.Roll / 180.f);
			ClientAdjustRootMotionSourcePosition
				(
				ServerData->PendingAdjustment.TimeStamp,
				CurrentRootMotion,
				bIsPlayingNetworkedRootMotionMontage,
				bIsPlayingNetworkedRootMotionMontage ? CharacterOwner->GetRootMotionAnimMontageInstance()->GetPosition() : -1.f,
				ServerData->PendingAdjustment.NewLoc,
				CompressedRotation,
				ServerData->PendingAdjustment.NewVel.Z,
				ServerData->PendingAdjustment.NewBase,
				ServerData->PendingAdjustment.NewBaseBoneName,
				ServerData->PendingAdjustment.NewBase != NULL,
				ServerData->PendingAdjustment.bBaseRelativePosition,
				PackNetworkMovementMode()
				);
		}
		else if( bIsPlayingNetworkedRootMotionMontage )
		{
			FRotator Rotation = ServerData->PendingAdjustment.NewRot.GetNormalized();
			FVector_NetQuantizeNormal CompressedRotation(Rotation.Pitch / 180.f, Rotation.Yaw / 180.f, Rotation.Roll / 180.f);
			ClientAdjustRootMotionPosition
				(
				ServerData->PendingAdjustment.TimeStamp,
				CharacterOwner->GetRootMotionAnimMontageInstance()->GetPosition(),
				ServerData->PendingAdjustment.NewLoc,
				CompressedRotation,
				ServerData->PendingAdjustment.NewVel.Z,
				ServerData->PendingAdjustment.NewBase,
				ServerData->PendingAdjustment.NewBaseBoneName,
				ServerData->PendingAdjustment.NewBase != NULL,
				ServerData->PendingAdjustment.bBaseRelativePosition,
				PackNetworkMovementMode()
				);
		}
		else if (ServerData->PendingAdjustment.NewVel.IsZero())
		{
			ClientVeryShortAdjustPosition
				(
				ServerData->PendingAdjustment.TimeStamp,
				ServerData->PendingAdjustment.NewLoc,
				ServerData->PendingAdjustment.NewBase,
				ServerData->PendingAdjustment.NewBaseBoneName,
				ServerData->PendingAdjustment.NewBase != NULL,
				ServerData->PendingAdjustment.bBaseRelativePosition,
				PackNetworkMovementMode()
				);
		}
		else
		{
			ClientAdjustPosition
				(
				ServerData->PendingAdjustment.TimeStamp,
				ServerData->PendingAdjustment.NewLoc,
				ServerData->PendingAdjustment.NewVel,
				ServerData->PendingAdjustment.NewBase,
				ServerData->PendingAdjustment.NewBaseBoneName,
				ServerData->PendingAdjustment.NewBase != NULL,
				ServerData->PendingAdjustment.bBaseRelativePosition,
				PackNetworkMovementMode()
				);
		}
	}

	ServerData->PendingAdjustment.TimeStamp = 0;
	ServerData->PendingAdjustment.bAckGoodMove = false;
	ServerData->bForceClientUpdate = false;
}


void UCharacterMovementComponent::ClientVeryShortAdjustPosition_Implementation
	(
	float TimeStamp,
	FVector NewLoc,
	UPrimitiveComponent* NewBase,
	FName NewBaseBoneName,
	bool bHasBase,
	bool bBaseRelativePosition,
	uint8 ServerMovementMode
	)
{
	if (HasValidData())
	{
		ClientAdjustPosition(TimeStamp, NewLoc, FVector::ZeroVector, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode);
	}
}


void UCharacterMovementComponent::ClientAdjustPosition_Implementation
	(
	float TimeStamp,
	FVector NewLocation,
	FVector NewVelocity,
	UPrimitiveComponent* NewBase,
	FName NewBaseBoneName,
	bool bHasBase,
	bool bBaseRelativePosition,
	uint8 ServerMovementMode
	)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}


	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	check(ClientData);
	
	// Make sure the base actor exists on this client.
	const bool bUnresolvedBase = bHasBase && (NewBase == NULL);
	if (bUnresolvedBase)
	{
		if (bBaseRelativePosition)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("ClientAdjustPosition_Implementation could not resolve the new relative movement base actor, ignoring server correction!"));
			return;
		}
		else
		{
			UE_LOG(LogNetPlayerMovement, Verbose, TEXT("ClientAdjustPosition_Implementation could not resolve the new absolute movement base actor, but WILL use the position!"));
		}
	}
	
	// Ack move if it has not expired.
	int32 MoveIndex = ClientData->GetSavedMoveIndex(TimeStamp);
	if( MoveIndex == INDEX_NONE )
	{
		if( ClientData->LastAckedMove.IsValid() )
		{
			UE_LOG(LogNetPlayerMovement, Log,  TEXT("ClientAdjustPosition_Implementation could not find Move for TimeStamp: %f, LastAckedTimeStamp: %f, CurrentTimeStamp: %f"), TimeStamp, ClientData->LastAckedMove->TimeStamp, ClientData->CurrentTimeStamp);
		}
		return;
	}
	ClientData->AckMove(MoveIndex);
	
	FVector WorldShiftedNewLocation;
	//  Received Location is relative to dynamic base
	if (bBaseRelativePosition)
	{
		FVector BaseLocation;
		FQuat BaseRotation;
		MovementBaseUtility::GetMovementBaseTransform(NewBase, NewBaseBoneName, BaseLocation, BaseRotation); // TODO: error handling if returns false		
		WorldShiftedNewLocation = NewLocation + BaseLocation;
	}
	else
	{
		WorldShiftedNewLocation = FRepMovement::RebaseOntoLocalOrigin(NewLocation, this);
	}


	// Trigger event
	OnClientCorrectionReceived(*ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode);

	// Trust the server's positioning.
	UpdatedComponent->SetWorldLocation(WorldShiftedNewLocation, false);
	Velocity = NewVelocity;

	// Trust the server's movement mode
	UPrimitiveComponent* PreviousBase = CharacterOwner->GetMovementBase();
	ApplyNetworkMovementMode(ServerMovementMode);

	// Set base component
	UPrimitiveComponent* FinalBase = NewBase;
	FName FinalBaseBoneName = NewBaseBoneName;
	if (bUnresolvedBase)
	{
		check(NewBase == NULL);
		check(!bBaseRelativePosition);
		
		// We had an unresolved base from the server
		// If walking, we'd like to continue walking if possible, to avoid falling for a frame, so try to find a base where we moved to.
		if (PreviousBase)
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false); //-V595
			if (CurrentFloor.IsWalkableFloor())
			{
				FinalBase = CurrentFloor.HitResult.Component.Get();
				FinalBaseBoneName = CurrentFloor.HitResult.BoneName;
			}
			else
			{
				FinalBase = nullptr;
				FinalBaseBoneName = NAME_None;
			}
		}
	}
	SetBase(FinalBase, FinalBaseBoneName);

	// Update floor at new location
	UpdateFloorFromAdjustment();
	bJustTeleported = true;

	// Even if base has not changed, we need to recompute the relative offsets (since we've moved).
	SaveBaseLocation();
	
	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
	LastUpdateVelocity = Velocity;

	UpdateComponentVelocity();
	ClientData->bUpdatePosition = true;
}


void UCharacterMovementComponent::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode)
{
#if !UE_BUILD_SHIPPING
	if (CharacterMovementCVars::NetShowCorrections != 0)
	{
		const FVector LocDiff = UpdatedComponent->GetComponentLocation() - NewLocation;
		const FString NewBaseString = NewBase ? NewBase->GetPathName(NewBase->GetOutermost()) : TEXT("None");
		UE_LOG(LogNetPlayerMovement, Warning, TEXT("*** Client: Error for %s at Time=%.3f is %3.3f LocDiff(%s) ClientLoc(%s) ServerLoc(%s) NewBase: %s NewBone: %s ClientVel(%s) ServerVel(%s) SavedMoves %d"),
			   *GetNameSafe(CharacterOwner), TimeStamp, LocDiff.Size(), *LocDiff.ToString(), *UpdatedComponent->GetComponentLocation().ToString(), *NewLocation.ToString(), *NewBaseString, *NewBaseBoneName.ToString(), *Velocity.ToString(), *NewVelocity.ToString(), ClientData.SavedMoves.Num());
		const float DebugLifetime = CharacterMovementCVars::NetCorrectionLifetime;
		DrawDebugCapsule(GetWorld(), UpdatedComponent->GetComponentLocation(), CharacterOwner->GetSimpleCollisionHalfHeight(), CharacterOwner->GetSimpleCollisionRadius(), FQuat::Identity, FColor(255, 100, 100), true, DebugLifetime);
		DrawDebugCapsule(GetWorld(), NewLocation, CharacterOwner->GetSimpleCollisionHalfHeight(), CharacterOwner->GetSimpleCollisionRadius(), FQuat::Identity, FColor(100, 255, 100), true, DebugLifetime);
	}
#endif //!UE_BUILD_SHIPPING

#if ROOT_MOTION_DEBUG
	if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
	{
		const FVector VelocityCorrection = NewVelocity - Velocity;
		FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement ClientAdjustPosition_Implementation Velocity(%s) OldVelocity(%s) Correction(%s) TimeStamp(%f)"),
													  *NewVelocity.ToCompactString(), *Velocity.ToCompactString(), *VelocityCorrection.ToCompactString(), TimeStamp);
		RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
	}
#endif
}


void UCharacterMovementComponent::ClientAdjustRootMotionPosition_Implementation(
	float TimeStamp,
	float ServerMontageTrackPosition,
	FVector ServerLoc,
	FVector_NetQuantizeNormal ServerRotation,
	float ServerVelZ,
	UPrimitiveComponent * ServerBase,
	FName ServerBaseBoneName,
	bool bHasBase,
	bool bBaseRelativePosition,
	uint8 ServerMovementMode)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}

	// Call ClientAdjustPosition first. This will Ack the move if it's not outdated.
	ClientAdjustPosition(TimeStamp, ServerLoc, FVector(0.f, 0.f, ServerVelZ), ServerBase, ServerBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode);
	
	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	check(ClientData);

	// If this adjustment wasn't acknowledged (because outdated), then abort.
	if( !ClientData->LastAckedMove.IsValid() || (ClientData->LastAckedMove->TimeStamp != TimeStamp) )
	{
		return;
	}

	// We're going to replay Root Motion. This is relative to the Pawn's rotation, so we need to reset that as well.
	FRotator DecompressedRot(ServerRotation.X * 180.f, ServerRotation.Y * 180.f, ServerRotation.Z * 180.f);
	CharacterOwner->SetActorRotation(DecompressedRot);
	const FVector ServerLocation(FRepMovement::RebaseOntoLocalOrigin(ServerLoc, UpdatedComponent));
	UE_LOG(LogRootMotion, Log,  TEXT("ClientAdjustRootMotionPosition_Implementation TimeStamp: %f, ServerMontageTrackPosition: %f, ServerLocation: %s, ServerRotation: %s, ServerVelZ: %f, ServerBase: %s"),
		TimeStamp, ServerMontageTrackPosition, *ServerLocation.ToCompactString(), *DecompressedRot.ToCompactString(), ServerVelZ, *GetNameSafe(ServerBase) );

	// DEBUG - get some insight on where errors came from
	if( false )
	{
		const FVector DeltaLocation = ServerLocation - ClientData->LastAckedMove->SavedLocation;
		const FRotator DeltaRotation = (DecompressedRot - ClientData->LastAckedMove->SavedRotation).GetNormalized();
		const float DeltaTrackPosition = (ServerMontageTrackPosition - ClientData->LastAckedMove->RootMotionTrackPosition);
		const float DeltaVelZ = (ServerVelZ - ClientData->LastAckedMove->SavedVelocity.Z);

		UE_LOG(LogRootMotion, Log,  TEXT("\tErrors DeltaLocation: %s, DeltaRotation: %s, DeltaTrackPosition: %f"),
			*DeltaLocation.ToCompactString(), *DeltaRotation.ToCompactString(), DeltaTrackPosition );
	}

	// Server disagrees with Client on the Root Motion AnimMontage Track position.
	if( CharacterOwner->bClientResimulateRootMotion || (ServerMontageTrackPosition != ClientData->LastAckedMove->RootMotionTrackPosition) )
	{
		// Not much we can do there unfortunately, just jump to server's track position.
		FAnimMontageInstance * RootMotionMontageInstance = CharacterOwner->GetRootMotionAnimMontageInstance();
		if (RootMotionMontageInstance && !RootMotionMontageInstance->IsRootMotionDisabled())
		{
			UE_LOG(LogRootMotion, Warning, TEXT("\tServer disagrees with Client's track position!! ServerTrackPosition: %f, ClientTrackPosition: %f, DeltaTrackPosition: %f. TimeStamp: %f, Character: %s, Montage: %s"),
					ServerMontageTrackPosition, ClientData->LastAckedMove->RootMotionTrackPosition, (ServerMontageTrackPosition - ClientData->LastAckedMove->RootMotionTrackPosition), TimeStamp, *GetNameSafe(CharacterOwner), *GetNameSafe(RootMotionMontageInstance->Montage));
	
			RootMotionMontageInstance->SetPosition(ServerMontageTrackPosition);
			CharacterOwner->bClientResimulateRootMotion = true;
		}
	}
}

void UCharacterMovementComponent::ClientAdjustRootMotionSourcePosition_Implementation(
	float TimeStamp,
	FRootMotionSourceGroup ServerRootMotion,
	bool bHasAnimRootMotion,
	float ServerMontageTrackPosition,
	FVector ServerLoc,
	FVector_NetQuantizeNormal ServerRotation,
	float ServerVelZ,
	UPrimitiveComponent * ServerBase,
	FName ServerBaseBoneName,
	bool bHasBase,
	bool bBaseRelativePosition,
	uint8 ServerMovementMode)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}

#if ROOT_MOTION_DEBUG
	if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
	{
		FString AdjustedDebugString = FString::Printf(TEXT("ClientAdjustRootMotionSourcePosition_Implementation TimeStamp(%f)"),
			TimeStamp);
		RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
	}
#endif

	// Call ClientAdjustPosition first. This will Ack the move if it's not outdated.
	ClientAdjustPosition(TimeStamp, ServerLoc, FVector(0.f, 0.f, ServerVelZ), ServerBase, ServerBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode);
	
	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	check(ClientData);

	// If this adjustment wasn't acknowledged (because outdated), then abort.
	if( !ClientData->LastAckedMove.IsValid() || (ClientData->LastAckedMove->TimeStamp != TimeStamp) )
	{
		return;
	}

	// We're going to replay Root Motion. This can be relative to the Pawn's rotation, so we need to reset that as well.
	FRotator DecompressedRot(ServerRotation.X * 180.f, ServerRotation.Y * 180.f, ServerRotation.Z * 180.f);
	CharacterOwner->SetActorRotation(DecompressedRot);
	const FVector ServerLocation(FRepMovement::RebaseOntoLocalOrigin(ServerLoc, UpdatedComponent));
	UE_LOG(LogRootMotion, Log,  TEXT("ClientAdjustRootMotionSourcePosition_Implementation TimeStamp: %f, NumRootMotionSources: %d, ServerLocation: %s, ServerRotation: %s, ServerVelZ: %f, ServerBase: %s"),
		TimeStamp, ServerRootMotion.RootMotionSources.Num(), *ServerLocation.ToCompactString(), *DecompressedRot.ToCompactString(), ServerVelZ, *GetNameSafe(ServerBase) );

	// Handle AnimRootMotion correction
	if (bHasAnimRootMotion)
	{
		// DEBUG - get some insight on where errors came from
		if( false )
		{
			const FVector DeltaLocation = ServerLocation - ClientData->LastAckedMove->SavedLocation;
			const FRotator DeltaRotation = (DecompressedRot - ClientData->LastAckedMove->SavedRotation).GetNormalized();
			const float DeltaTrackPosition = (ServerMontageTrackPosition - ClientData->LastAckedMove->RootMotionTrackPosition);
			const float DeltaVelZ = (ServerVelZ - ClientData->LastAckedMove->SavedVelocity.Z);

			UE_LOG(LogRootMotion, Log,  TEXT("\tErrors DeltaLocation: %s, DeltaRotation: %s, DeltaTrackPosition: %f"),
				*DeltaLocation.ToCompactString(), *DeltaRotation.ToCompactString(), DeltaTrackPosition );
		}

		// Server disagrees with Client on the Root Motion AnimMontage Track position.
		if( CharacterOwner->bClientResimulateRootMotion || (ServerMontageTrackPosition != ClientData->LastAckedMove->RootMotionTrackPosition) )
		{
			UE_LOG(LogRootMotion, Warning,  TEXT("\tServer disagrees with Client's track position!! ServerTrackPosition: %f, ClientTrackPosition: %f, DeltaTrackPosition: %f. TimeStamp: %f"),
				ServerMontageTrackPosition, ClientData->LastAckedMove->RootMotionTrackPosition, (ServerMontageTrackPosition - ClientData->LastAckedMove->RootMotionTrackPosition), TimeStamp);

			// Not much we can do there unfortunately, just jump to server's track position.
			FAnimMontageInstance * RootMotionMontageInstance = CharacterOwner->GetRootMotionAnimMontageInstance();
			if (RootMotionMontageInstance && !RootMotionMontageInstance->IsRootMotionDisabled())
			{
				RootMotionMontageInstance->SetPosition(ServerMontageTrackPosition);
				CharacterOwner->bClientResimulateRootMotion = true;
			}
		}
	}

	// First we need to convert Server IDs -> Local IDs in ServerRootMotion for comparison
	ConvertRootMotionServerIDsToLocalIDs(ClientData->LastAckedMove->SavedRootMotion, ServerRootMotion, TimeStamp);

	// Cull ServerRootMotion of any root motion sources that don't match ones we have in this move
	ServerRootMotion.CullInvalidSources();

	// Server disagrees with Client on Root Motion state.
	if( CharacterOwner->bClientResimulateRootMotionSources || (ServerRootMotion != ClientData->LastAckedMove->SavedRootMotion) )
	{
		if (!CharacterOwner->bClientResimulateRootMotionSources)
		{
			UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("ClientAdjustRootMotionSourcePosition called, server/LastAckedMove mismatch"));
		}

		CharacterOwner->SavedRootMotion = ServerRootMotion;
		CharacterOwner->bClientResimulateRootMotionSources = true;
	}
}

void UCharacterMovementComponent::ClientAckGoodMove_Implementation(float TimeStamp)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}

	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	check(ClientData);

#if ROOT_MOTION_DEBUG
	if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
	{
		FString AdjustedDebugString = FString::Printf(TEXT("ClientAckGoodMove_Implementation TimeStamp(%f)"),
			TimeStamp);
		RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
	}
#endif

	// Ack move if it has not expired.
	int32 MoveIndex = ClientData->GetSavedMoveIndex(TimeStamp);
	if( MoveIndex == INDEX_NONE )
	{
		if( ClientData->LastAckedMove.IsValid() )
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("ClientAckGoodMove_Implementation could not find Move for TimeStamp: %f, LastAckedTimeStamp: %f, CurrentTimeStamp: %f"), TimeStamp, ClientData->LastAckedMove->TimeStamp, ClientData->CurrentTimeStamp);
		}
		return;
	}
	ClientData->AckMove(MoveIndex);
}

void UCharacterMovementComponent::CapsuleTouched(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult )
{
	if (!bEnablePhysicsInteraction)
	{
		return;
	}

	if (OtherComp != NULL && OtherComp->IsAnySimulatingPhysics())
	{
		const FVector OtherLoc = OtherComp->GetComponentLocation();
		const FVector Loc = UpdatedComponent->GetComponentLocation();
		FVector ImpulseDir = FVector(OtherLoc.X - Loc.X, OtherLoc.Y - Loc.Y, 0.25f).GetSafeNormal();
		ImpulseDir = (ImpulseDir + Velocity.GetSafeNormal2D()) * 0.5f;
		ImpulseDir.Normalize();

		FName BoneName = NAME_None;
		if (OtherBodyIndex != INDEX_NONE)
		{
			BoneName = ((USkinnedMeshComponent*)OtherComp)->GetBoneName(OtherBodyIndex);
		}

		float TouchForceFactorModified = TouchForceFactor;

		if ( bTouchForceScaledToMass )
		{
			FBodyInstance* BI = OtherComp->GetBodyInstance(BoneName);
			TouchForceFactorModified *= BI ? BI->GetBodyMass() : 1.0f;
		}

		float ImpulseStrength = FMath::Clamp(Velocity.Size2D() * TouchForceFactorModified, 
			MinTouchForce > 0.0f ? MinTouchForce : -FLT_MAX, 
			MaxTouchForce > 0.0f ? MaxTouchForce : FLT_MAX);

		FVector Impulse = ImpulseDir * ImpulseStrength;

		OtherComp->AddImpulse(Impulse, BoneName);
	}
}

void UCharacterMovementComponent::SetAvoidanceGroup(int32 GroupFlags)
{
	AvoidanceGroup.SetFlagsDirectly(GroupFlags);
}

void UCharacterMovementComponent::SetAvoidanceGroupMask(const FNavAvoidanceMask& GroupMask)
{
	AvoidanceGroup.SetFlagsDirectly(GroupMask.Packed);
}

void UCharacterMovementComponent::SetGroupsToAvoid(int32 GroupFlags)
{
	GroupsToAvoid.SetFlagsDirectly(GroupFlags);
}

void UCharacterMovementComponent::SetGroupsToAvoidMask(const FNavAvoidanceMask& GroupMask)
{
	GroupsToAvoid.SetFlagsDirectly(GroupMask.Packed);
}

void UCharacterMovementComponent::SetGroupsToIgnore(int32 GroupFlags)
{
	GroupsToIgnore.SetFlagsDirectly(GroupFlags);
}

void UCharacterMovementComponent::SetGroupsToIgnoreMask(const FNavAvoidanceMask& GroupMask)
{
	GroupsToIgnore.SetFlagsDirectly(GroupMask.Packed);
}

void UCharacterMovementComponent::SetAvoidanceEnabled(bool bEnable)
{
	if (bUseRVOAvoidance != bEnable)
	{
		bUseRVOAvoidance = bEnable;

		// reset id, RegisterMovementComponent call is required to initialize update timers in avoidance manager
		AvoidanceUID = 0;

		// this is a safety check - it's possible to not have CharacterOwner at this point if this function gets
		// called too early
		ensure(GetCharacterOwner());
		if (GetCharacterOwner() != nullptr)
		{
			UAvoidanceManager* AvoidanceManager = GetWorld()->GetAvoidanceManager();
			if (AvoidanceManager && bEnable)
			{
				AvoidanceManager->RegisterMovementComponent(this, AvoidanceWeight);
			}
		}
	}
}

void UCharacterMovementComponent::ApplyDownwardForce(float DeltaSeconds)
{
	if (StandingDownwardForceScale != 0.0f && CurrentFloor.HitResult.IsValidBlockingHit())
	{
		UPrimitiveComponent* BaseComp = CurrentFloor.HitResult.GetComponent();
		const FVector Gravity = FVector(0.0f, 0.0f, GetGravityZ());

		if (BaseComp && BaseComp->IsAnySimulatingPhysics() && !Gravity.IsZero())
		{
			BaseComp->AddForceAtLocation(Gravity * Mass * StandingDownwardForceScale, CurrentFloor.HitResult.ImpactPoint, CurrentFloor.HitResult.BoneName);
		}
	}
}

void UCharacterMovementComponent::ApplyRepulsionForce(float DeltaSeconds)
{
	if (UpdatedPrimitive && RepulsionForce > 0.0f && CharacterOwner!=nullptr)
	{
		const TArray<FOverlapInfo>& Overlaps = UpdatedPrimitive->GetOverlapInfos();
		if (Overlaps.Num() > 0)
		{
			FCollisionQueryParams QueryParams (SCENE_QUERY_STAT(CMC_ApplyRepulsionForce));
			QueryParams.bReturnFaceIndex = false;
			QueryParams.bReturnPhysicalMaterial = false;

			float CapsuleRadius = 0.f;
			float CapsuleHalfHeight = 0.f;
			CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(CapsuleRadius, CapsuleHalfHeight);
			const float RepulsionForceRadius = CapsuleRadius * 1.2f;
			const float StopBodyDistance = 2.5f;
			const FVector MyLocation = UpdatedPrimitive->GetComponentLocation();

			for (int32 i=0; i < Overlaps.Num(); i++)
			{
				const FOverlapInfo& Overlap = Overlaps[i];

				UPrimitiveComponent* OverlapComp = Overlap.OverlapInfo.Component.Get();
				if (!OverlapComp || OverlapComp->Mobility < EComponentMobility::Movable)
				{ 
					continue; 
				}

				// Use the body instead of the component for cases where we have multi-body overlaps enabled
				FBodyInstance* OverlapBody = nullptr;
				const int32 OverlapBodyIndex = Overlap.GetBodyIndex();
				const USkeletalMeshComponent* SkelMeshForBody = (OverlapBodyIndex != INDEX_NONE) ? Cast<USkeletalMeshComponent>(OverlapComp) : nullptr;
				if (SkelMeshForBody != nullptr)
				{
					OverlapBody = SkelMeshForBody->Bodies.IsValidIndex(OverlapBodyIndex) ? SkelMeshForBody->Bodies[OverlapBodyIndex] : nullptr;
				}
				else
				{
					OverlapBody = OverlapComp->GetBodyInstance();
				}

				if (!OverlapBody)
				{
					UE_LOG(LogCharacterMovement, Warning, TEXT("%s could not find overlap body for body index %d"), *GetName(), OverlapBodyIndex);
					continue;
				}

				if (!OverlapBody->IsInstanceSimulatingPhysics())
				{
					continue;
				}

				FTransform BodyTransform = OverlapBody->GetUnrealWorldTransform();

				FVector BodyVelocity = OverlapBody->GetUnrealWorldVelocity();
				FVector BodyLocation = BodyTransform.GetLocation();

				// Trace to get the hit location on the capsule
				FHitResult Hit;
				bool bHasHit = UpdatedPrimitive->LineTraceComponent(Hit, BodyLocation,
																	FVector(MyLocation.X, MyLocation.Y, BodyLocation.Z),
																	QueryParams);

				FVector HitLoc = Hit.ImpactPoint;
				bool bIsPenetrating = Hit.bStartPenetrating || Hit.PenetrationDepth > StopBodyDistance;

				// If we didn't hit the capsule, we're inside the capsule
				if (!bHasHit) 
				{
					HitLoc = BodyLocation;
					bIsPenetrating = true;
				}

				const float DistanceNow = (HitLoc - BodyLocation).SizeSquared2D();
				const float DistanceLater = (HitLoc - (BodyLocation + BodyVelocity * DeltaSeconds)).SizeSquared2D();

				if (bHasHit && DistanceNow < StopBodyDistance && !bIsPenetrating)
				{
					OverlapBody->SetLinearVelocity(FVector(0.0f, 0.0f, 0.0f), false);
				}
				else if (DistanceLater <= DistanceNow || bIsPenetrating)
				{
					FVector ForceCenter = MyLocation;

					if (bHasHit)
					{
						ForceCenter.Z = HitLoc.Z;
					}
					else
					{
						ForceCenter.Z = FMath::Clamp(BodyLocation.Z, MyLocation.Z - CapsuleHalfHeight, MyLocation.Z + CapsuleHalfHeight);
					}

					OverlapBody->AddRadialForceToBody(ForceCenter, RepulsionForceRadius, RepulsionForce * Mass, ERadialImpulseFalloff::RIF_Constant);
				}
			}
		}
	}
}

void UCharacterMovementComponent::ApplyAccumulatedForces(float DeltaSeconds)
{
	if (PendingImpulseToApply.Z != 0.f || PendingForceToApply.Z != 0.f)
	{
		// check to see if applied momentum is enough to overcome gravity
		if ( IsMovingOnGround() && (PendingImpulseToApply.Z + (PendingForceToApply.Z * DeltaSeconds) + (GetGravityZ() * DeltaSeconds) > SMALL_NUMBER))
		{
			SetMovementMode(MOVE_Falling);
		}
	}

	Velocity += PendingImpulseToApply + (PendingForceToApply * DeltaSeconds);
	
	// Don't call ClearAccumulatedForces() because it could affect launch velocity
	PendingImpulseToApply = FVector::ZeroVector;
	PendingForceToApply = FVector::ZeroVector;
}

void UCharacterMovementComponent::ClearAccumulatedForces()
{
	PendingImpulseToApply = FVector::ZeroVector;
	PendingForceToApply = FVector::ZeroVector;
	PendingLaunchVelocity = FVector::ZeroVector;
}

void UCharacterMovementComponent::AddRadialForce(const FVector& Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff)
{
	FVector Delta = UpdatedComponent->GetComponentLocation() - Origin;
	const float DeltaMagnitude = Delta.Size();

	// Do nothing if outside radius
	if(DeltaMagnitude > Radius)
	{
		return;
	}

	Delta = Delta.GetSafeNormal();

	float ForceMagnitude = Strength;
	if (Falloff == RIF_Linear && Radius > 0.0f)
	{
		ForceMagnitude *= (1.0f - (DeltaMagnitude / Radius));
	}

	AddForce(Delta * ForceMagnitude);
}
 
void UCharacterMovementComponent::AddRadialImpulse(const FVector& Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, bool bVelChange)
{
	FVector Delta = UpdatedComponent->GetComponentLocation() - Origin;
	const float DeltaMagnitude = Delta.Size();

	// Do nothing if outside radius
	if(DeltaMagnitude > Radius)
	{
		return;
	}

	Delta = Delta.GetSafeNormal();

	float ImpulseMagnitude = Strength;
	if (Falloff == RIF_Linear && Radius > 0.0f)
	{
		ImpulseMagnitude *= (1.0f - (DeltaMagnitude / Radius));
	}

	AddImpulse(Delta * ImpulseMagnitude, bVelChange);
}

void UCharacterMovementComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);

	if (bRegister)
	{
		if (SetupActorComponentTickFunction(&PostPhysicsTickFunction))
		{
			PostPhysicsTickFunction.Target = this;
			PostPhysicsTickFunction.AddPrerequisite(this, this->PrimaryComponentTick);
		}
	}
	else
	{
		if(PostPhysicsTickFunction.IsTickFunctionRegistered())
		{
			PostPhysicsTickFunction.UnRegisterTickFunction();
		}
	}
}

void UCharacterMovementComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	OldBaseLocation += InOffset;
	LastUpdateLocation += InOffset;

	if (CharacterOwner != nullptr && CharacterOwner->Role == ROLE_AutonomousProxy)
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData != nullptr)
		{
			const int32 NumSavedMoves = ClientData->SavedMoves.Num();
			for (int32 i = 0; i < NumSavedMoves - 1; i++)
			{
				const FSavedMovePtr& CurrentMove = ClientData->SavedMoves[i];
				CurrentMove->StartLocation += InOffset;
				CurrentMove->SavedLocation += InOffset;
			}

			if (ClientData->PendingMove.IsValid())
			{
				ClientData->PendingMove->StartLocation += InOffset;
				ClientData->PendingMove->SavedLocation += InOffset;
			}

			for (int32 i = 0; i < ClientData->ReplaySamples.Num(); i++)
			{
				ClientData->ReplaySamples[i].Location += InOffset;
			}
		}
	}
}

void UCharacterMovementComponent::TickCharacterPose(float DeltaTime)
{
	check(CharacterOwner && CharacterOwner->GetMesh());
	USkeletalMeshComponent* CharacterMesh = CharacterOwner->GetMesh();

	// bAutonomousTickPose is set, we control TickPose from the Character's Movement and Networking updates, and bypass the Component's update.
	// (Or Simulating Root Motion for remote clients)
	CharacterMesh->bIsAutonomousTickPose = true;

	if (CharacterMesh->ShouldTickPose())
	{
		// Keep track of if we're playing root motion, just in case the root motion montage ends this frame.
		const bool bWasPlayingRootMotion = CharacterOwner->IsPlayingRootMotion();

		CharacterMesh->TickPose(DeltaTime, true);

		// Grab root motion now that we have ticked the pose
		if (CharacterOwner->IsPlayingRootMotion() || bWasPlayingRootMotion)
		{
			FRootMotionMovementParams RootMotion = CharacterMesh->ConsumeRootMotion();
			if (RootMotion.bHasRootMotion)
			{
				RootMotion.ScaleRootMotionTranslation(CharacterOwner->GetAnimRootMotionTranslationScale());
				RootMotionParams.Accumulate(RootMotion);
			}

#if !(UE_BUILD_SHIPPING)
			// Debugging
			{
				FAnimMontageInstance* RootMotionMontageInstance = CharacterOwner->GetRootMotionAnimMontageInstance();
				UE_LOG(LogRootMotion, Log, TEXT("UCharacterMovementComponent::TickCharacterPose Role: %s, RootMotionMontage: %s, MontagePos: %f, DeltaTime: %f, ExtractedRootMotion: %s, AccumulatedRootMotion: %s")
					, *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), CharacterOwner->Role)
					, *GetNameSafe(RootMotionMontageInstance ? RootMotionMontageInstance->Montage : NULL)
					, RootMotionMontageInstance ? RootMotionMontageInstance->GetPosition() : -1.f
					, DeltaTime
					, *RootMotion.GetRootMotionTransform().GetTranslation().ToCompactString()
					, *RootMotionParams.GetRootMotionTransform().GetTranslation().ToCompactString()
					);
			}
#endif // !(UE_BUILD_SHIPPING)
		}
	}

	CharacterMesh->bIsAutonomousTickPose = false;
}

/** 
*	Root Motion
*/

bool UCharacterMovementComponent::HasRootMotionSources() const
{
	return CurrentRootMotion.HasActiveRootMotionSources() || (CharacterOwner && CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh());
}

uint16 UCharacterMovementComponent::ApplyRootMotionSource(FRootMotionSource* SourcePtr)
{
	if (SourcePtr != nullptr)
	{
		// Set default StartTime if it hasn't been set manually
		if (!SourcePtr->IsStartTimeValid())
		{
			if (CharacterOwner)
			{
				if (CharacterOwner->Role == ROLE_AutonomousProxy)
				{
					// Autonomous defaults to local timestamp
					FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
					if (ClientData)
					{
						SourcePtr->StartTime = ClientData->CurrentTimeStamp;
					}
				}
				else if (CharacterOwner->Role == ROLE_Authority && !IsNetMode(NM_Client))
				{
					// Authority defaults to current client time stamp, meaning it'll start next tick if not corrected
					FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
					if (ServerData)
					{
						SourcePtr->StartTime = ServerData->CurrentClientTimeStamp;
					}
				}
			}
		}

		OnRootMotionSourceBeingApplied(SourcePtr);

		return CurrentRootMotion.ApplyRootMotionSource(SourcePtr);
	}
	else
	{
		checkf(false, TEXT("Passing nullptr into UCharacterMovementComponent::ApplyRootMotionSource"));
	}

	return (uint16)ERootMotionSourceID::Invalid;
}

void UCharacterMovementComponent::OnRootMotionSourceBeingApplied(const FRootMotionSource* Source)
{
}

TSharedPtr<FRootMotionSource> UCharacterMovementComponent::GetRootMotionSource(FName InstanceName)
{
	return CurrentRootMotion.GetRootMotionSource(InstanceName);
}

TSharedPtr<FRootMotionSource> UCharacterMovementComponent::GetRootMotionSourceByID(uint16 RootMotionSourceID)
{
	return CurrentRootMotion.GetRootMotionSourceByID(RootMotionSourceID);
}

void UCharacterMovementComponent::RemoveRootMotionSource(FName InstanceName)
{
	CurrentRootMotion.RemoveRootMotionSource(InstanceName);
}

void UCharacterMovementComponent::RemoveRootMotionSourceByID(uint16 RootMotionSourceID)
{
	CurrentRootMotion.RemoveRootMotionSourceByID(RootMotionSourceID);
}

void UCharacterMovementComponent::ConvertRootMotionServerIDsToLocalIDs(const FRootMotionSourceGroup& LocalRootMotionToMatchWith, FRootMotionSourceGroup& InOutServerRootMotion, float TimeStamp)
{
	// Remove out of date mappings, they can never be used again.
	for (int32 MappingIndex = 0; MappingIndex < RootMotionIDMappings.Num(); MappingIndex++)
	{
		if (RootMotionIDMappings[MappingIndex].IsStillValid(TimeStamp))
		{
			// MappingIndex is valid, remove anything before it.
			const int32 CutOffIndex = MappingIndex - 1;
			if (CutOffIndex >= 0)
			{
				// Most recent entries added last, so we can cull the top of the list.
				RootMotionIDMappings.RemoveAt(0, CutOffIndex + 1, false);
				break;
			}
		}
	}

	// Remove mappings that don't map to an active local root motion source.
	for (int32 MappingIndex = RootMotionIDMappings.Num()-1; MappingIndex>=0; MappingIndex--)
	{
		bool bFoundLocalSource = false;
		for (const TSharedPtr<FRootMotionSource>& LocalRootMotionSource : LocalRootMotionToMatchWith.RootMotionSources)
		{
			if (LocalRootMotionSource.IsValid() && (LocalRootMotionSource->LocalID == RootMotionIDMappings[MappingIndex].LocalID))
			{
				bFoundLocalSource = true;
				break;
			}
		}

		if (!bFoundLocalSource)
		{
			RootMotionIDMappings.RemoveAt(MappingIndex, 1, false);
		}
	}

	bool bDumpDebugInfo = false;

	// Root Motion Sources are applied independently on servers and clients.
	// FRootMotionSource::LocalID is an ID added when that Source is applied.
	// When we receive RootMotionSource data from the server, LocalIDs on that
	// RootMotion data are the server LocalIDs. When processing an FRootMotionSourceGroup
	// for use on clients, we want to map server LocalIDs to our LocalIDs.
	// We save off these mappings for quicker access and to save having to
	// "find best match" every time we receive server data.
	for (TSharedPtr<FRootMotionSource>& ServerRootMotionSource : InOutServerRootMotion.RootMotionSources)
	{
		if (ServerRootMotionSource.IsValid())
		{
			const uint16 ServerID = ServerRootMotionSource->LocalID;

			// Reset LocalID of replicated ServerRootMotionSource, and find a local match.
			ServerRootMotionSource->LocalID = (uint16)ERootMotionSourceID::Invalid;

			// See if we have any recent mappings that match this server ID
			// If we do, change it to that mapping and update the timestamp
			{
				bool bMappingFound = false;
				for (FRootMotionServerToLocalIDMapping& Mapping : RootMotionIDMappings)
				{
					if (ServerID == Mapping.ServerID)
					{
						ServerRootMotionSource->LocalID = Mapping.LocalID;
						Mapping.TimeStamp = TimeStamp;
						bMappingFound = true;
						break; // Found it, don't need to search any more mappings
					}
				}

				if (bMappingFound)
				{
					// We rely on this rule (Matches) being always true, so in non-shipping builds make sure it never breaks.
					for (const TSharedPtr<FRootMotionSource>& LocalRootMotionSource : LocalRootMotionToMatchWith.RootMotionSources)
					{
						if (LocalRootMotionSource.IsValid() && (LocalRootMotionSource->LocalID == ServerRootMotionSource->LocalID))
						{
							if (!LocalRootMotionSource->Matches(ServerRootMotionSource.Get()))
							{
								ensureMsgf(false,
									TEXT("Character(%s) Local RootMotionSource(%s) has the same LocalID(%d) as a non-matching ServerRootMotionSource(%s)!"),
									*GetNameSafe(CharacterOwner), *LocalRootMotionSource->ToSimpleString(), LocalRootMotionSource->LocalID, *ServerRootMotionSource->ToSimpleString());

								bDumpDebugInfo = true;
							}

							break;
						}
					}

					// We've found the correct LocalID, done with this one, process next ServerRootMotionSource
					continue;
				}
			}

			// If no mapping found, find match out of Local RootMotionSources that are not already mapped
			bool bMatchFound = false;
			for (const TSharedPtr<FRootMotionSource>& LocalRootMotionSource : LocalRootMotionToMatchWith.RootMotionSources)
			{
				if (LocalRootMotionSource.IsValid())
				{
					const uint16 LocalID = LocalRootMotionSource->LocalID;

					// Check if the LocalID is already mapped to a ServerID; if it's already "claimed",
					// it's not valid for being a match to our unmatched server source
					{
						bool bMappingFound = false;
						for (FRootMotionServerToLocalIDMapping& Mapping : RootMotionIDMappings)
						{
							if (LocalID == Mapping.LocalID)
							{
								bMappingFound = true;
								break; // Found it, don't need to search any more mappings
							}
						}

						if (bMappingFound)
						{
							continue; // We found a ServerID matching this LocalID, so we don't try to match this
						}
					}

					// This LocalRootMotionSource is a valid possible match to the ServerRootMotionSource
					if (LocalRootMotionSource->Matches(ServerRootMotionSource.Get()))
					{
						// We have a match!
						// Assign LocalID
						ServerRootMotionSource->LocalID = LocalID;

						// Add to Mapping
						{
							FRootMotionServerToLocalIDMapping NewMapping;
							NewMapping.LocalID = LocalID;
							NewMapping.ServerID = ServerID;
							NewMapping.TimeStamp = TimeStamp;

							RootMotionIDMappings.Add(NewMapping);
							bMatchFound = true;
							break; // Stop searching LocalRootMotionSources, we've found a match
						}
					}
				}
			} // loop through LocalRootMotionSources

			// if we don't find a match, set an invalid LocalID so that we know it's an invalid ID from the server
			// This doesn't mean it's a "bad" RootMotionSource; just that the Server sent a RootMotionSource
			// that we don't have in the current LocalRootMotion group we're searching. It's possible that next
			// frame the LocalRootMotionSource was added/will be added and from then on we'll match & correct from
			// the Server
			if (!bMatchFound)
			{
				ServerRootMotionSource->LocalID = (uint16)ERootMotionSourceID::Invalid;
			}
		}
	} // loop through ServerRootMotionSources

	if (bDumpDebugInfo)
	{
		UE_LOG(LogRootMotion, Warning, TEXT("Dumping current mappings:"));
		for (FRootMotionServerToLocalIDMapping& Mapping : RootMotionIDMappings)
		{
			UE_LOG(LogRootMotion, Warning, TEXT("- LocalID(%d) ServerID(%d)"), Mapping.LocalID, Mapping.ServerID);
		}

		UE_LOG(LogRootMotion, Warning, TEXT("Dumping local RootMotionSources:"));
		for (const TSharedPtr<FRootMotionSource>& LocalRootMotionSource : LocalRootMotionToMatchWith.RootMotionSources)
		{
			if (LocalRootMotionSource.IsValid())
			{
				UE_LOG(LogRootMotion, Warning, TEXT("- LocalRootMotionSource(%d)"), *LocalRootMotionSource->ToSimpleString());
			}
		}

		UE_LOG(LogRootMotion, Warning, TEXT("Dumping server RootMotionSources:"));
		for (TSharedPtr<FRootMotionSource>& ServerRootMotionSource : InOutServerRootMotion.RootMotionSources)
		{
			if (ServerRootMotionSource.IsValid())
			{
				UE_LOG(LogRootMotion, Warning, TEXT("- ServerRootMotionSource(%d)"), *ServerRootMotionSource->ToSimpleString());
			}
		}
	}
}


PRAGMA_DISABLE_DEPRECATION_WARNINGS // For deprecated members of FNetworkPredictionData_Client_Character

FNetworkPredictionData_Client_Character::FNetworkPredictionData_Client_Character(const UCharacterMovementComponent& ClientMovement)
	: ClientUpdateTime(0.f)
	, CurrentTimeStamp(0.f)
	, PendingMove(NULL)
	, LastAckedMove(NULL)
	, MaxFreeMoveCount(96)
	, MaxSavedMoveCount(96)
	, bUpdatePosition(false)
	, bSmoothNetUpdates(false) // Deprecated
	, OriginalMeshTranslationOffset(ForceInitToZero)
	, MeshTranslationOffset(ForceInitToZero)
	, OriginalMeshRotationOffset(FQuat::Identity)
	, MeshRotationOffset(FQuat::Identity)	
	, MeshRotationTarget(FQuat::Identity)
	, LastCorrectionDelta(0.f)
	, LastCorrectionTime(0.f)
	, SmoothingServerTimeStamp(0.f)
	, SmoothingClientTimeStamp(0.f)
	, CurrentSmoothTime(0.f) // Deprecated
	, bUseLinearSmoothing(false) // Deprecated
	, MaxSmoothNetUpdateDist(0.f)
	, NoSmoothNetUpdateDist(0.f)
	, SmoothNetUpdateTime(0.f)
	, SmoothNetUpdateRotationTime(0.f)
	, MaxResponseTime(0.125f) // Deprecated, use MaxMoveDeltaTime instead
	, MaxMoveDeltaTime(0.125f)
	, LastSmoothLocation(FVector::ZeroVector)
	, LastServerLocation(FVector::ZeroVector)
	, SimulatedDebugDrawTime(0.0f)
{
	MaxSmoothNetUpdateDist = ClientMovement.NetworkMaxSmoothUpdateDistance;
	NoSmoothNetUpdateDist = ClientMovement.NetworkNoSmoothUpdateDistance;

	const bool bIsListenServer = (ClientMovement.GetNetMode() == NM_ListenServer);
	SmoothNetUpdateTime = (bIsListenServer ? ClientMovement.ListenServerNetworkSimulatedSmoothLocationTime : ClientMovement.NetworkSimulatedSmoothLocationTime);
	SmoothNetUpdateRotationTime = (bIsListenServer ? ClientMovement.ListenServerNetworkSimulatedSmoothRotationTime : ClientMovement.NetworkSimulatedSmoothRotationTime);

	AGameNetworkManager* GameNetworkManager = AGameNetworkManager::StaticClass()->GetDefaultObject<AGameNetworkManager>();
	if (GameNetworkManager)
	{
		MaxMoveDeltaTime = GameNetworkManager->MaxMoveDeltaTime;
	}

	MaxResponseTime = MaxMoveDeltaTime; // MaxResponseTime is deprecated, use MaxMoveDeltaTime instead

	if (ClientMovement.GetOwnerRole() == ROLE_AutonomousProxy)
	{
		SavedMoves.Reserve(MaxSavedMoveCount);
		FreeMoves.Reserve(MaxFreeMoveCount);
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS // For deprecated members of FNetworkPredictionData_Client_Character


FNetworkPredictionData_Client_Character::~FNetworkPredictionData_Client_Character()
{
	SavedMoves.Empty();
	FreeMoves.Empty();
	PendingMove = NULL;
	LastAckedMove = NULL;
}


FSavedMovePtr FNetworkPredictionData_Client_Character::CreateSavedMove()
{
	if (SavedMoves.Num() >= MaxSavedMoveCount)
	{
		UE_LOG(LogNetPlayerMovement, Warning, TEXT("CreateSavedMove: Hit limit of %d saved moves (timing out or very bad ping?)"), SavedMoves.Num());
		// Free all saved moves
		for (int32 i=0; i < SavedMoves.Num(); i++)
		{
			FreeMove(SavedMoves[i]);
		}
		SavedMoves.Reset();
	}

	if (FreeMoves.Num() == 0)
	{
		// No free moves, allocate a new one.
		FSavedMovePtr NewMove = AllocateNewMove();
		checkSlow(NewMove.IsValid());
		NewMove->Clear();
		return NewMove;
	}
	else
	{
		// Pull from the free pool
		const bool bAllowShrinking = false;
		FSavedMovePtr FirstFree = FreeMoves.Pop(bAllowShrinking);
		FirstFree->Clear();
		return FirstFree;
	}
}


FSavedMovePtr FNetworkPredictionData_Client_Character::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Character());
}


void FNetworkPredictionData_Client_Character::FreeMove(const FSavedMovePtr& Move)
{
	if (Move.IsValid())
	{
		// Only keep a pool of a limited number of moves.
		if (FreeMoves.Num() < MaxFreeMoveCount)
		{
			FreeMoves.Push(Move);
		}

		// Shouldn't keep a reference to the move on the free list.
		if (PendingMove == Move)
		{
			PendingMove = NULL;
		}
		if( LastAckedMove == Move )
		{
			LastAckedMove = NULL;
		}
	}
}

int32 FNetworkPredictionData_Client_Character::GetSavedMoveIndex(float TimeStamp) const
{
	if( SavedMoves.Num() > 0 )
	{
		// If LastAckedMove isn't using an old TimeStamp (before reset), we can prevent the iteration if incoming TimeStamp is outdated
		if( LastAckedMove.IsValid() && !LastAckedMove->bOldTimeStampBeforeReset && (TimeStamp <= LastAckedMove->TimeStamp) )
		{
			return INDEX_NONE;
		}

		// Otherwise see if we can find this move.
		for (int32 Index=0; Index<SavedMoves.Num(); Index++)
		{
			const FSavedMovePtr& CurrentMove = SavedMoves[Index];
			if( CurrentMove->TimeStamp == TimeStamp )
			{
				return Index;
			}
		}
	}
	return INDEX_NONE;
}

void FNetworkPredictionData_Client_Character::AckMove(int32 AckedMoveIndex) 
{
	// It is important that we know the move exists before we go deleting outdated moves.
	// Timestamps are not guaranteed to be increasing order all the time, since they can be reset!
	if( AckedMoveIndex != INDEX_NONE )
	{
		// Keep reference to LastAckedMove
		const FSavedMovePtr& AckedMove = SavedMoves[AckedMoveIndex];
		UE_LOG(LogNetPlayerMovement, VeryVerbose, TEXT("AckedMove Index: %2d (%2d moves). TimeStamp: %f, CurrentTimeStamp: %f"), AckedMoveIndex, SavedMoves.Num(), AckedMove->TimeStamp, CurrentTimeStamp);
		if( LastAckedMove.IsValid() )
		{
			FreeMove(LastAckedMove);
		}
		LastAckedMove = AckedMove;

		// Free expired moves.
		for(int32 MoveIndex=0; MoveIndex<AckedMoveIndex; MoveIndex++)
		{
			const FSavedMovePtr& Move = SavedMoves[MoveIndex];
			FreeMove(Move);
		}

		// And finally cull all of those, so only the unacknowledged moves remain in SavedMoves.
		const bool bAllowShrinking = false;
		SavedMoves.RemoveAt(0, AckedMoveIndex + 1, bAllowShrinking);
	}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS // For deprecated members of FNetworkPredictionData_Server_Character

FNetworkPredictionData_Server_Character::FNetworkPredictionData_Server_Character(const UCharacterMovementComponent& ServerMovement)
	: PendingAdjustment()
	, CurrentClientTimeStamp(0.f)
	, LastUpdateTime(0.f)
	, ServerTimeStampLastServerMove(0.f)
	, MaxResponseTime(0.125f) // Deprecated, use MaxMoveDeltaTime instead
	, MaxMoveDeltaTime(0.125f)
	, bForceClientUpdate(false)
	, LifetimeRawTimeDiscrepancy(0.f)
	, TimeDiscrepancy(0.f)
	, bResolvingTimeDiscrepancy(false)
	, TimeDiscrepancyResolutionMoveDeltaOverride(0.f)
	, TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick(0.f)
	, WorldCreationTime(0.f)
{
	AGameNetworkManager* GameNetworkManager = AGameNetworkManager::StaticClass()->GetDefaultObject<AGameNetworkManager>();
	if (GameNetworkManager)
	{
		MaxMoveDeltaTime = GameNetworkManager->MaxMoveDeltaTime;
		if (GameNetworkManager->MaxMoveDeltaTime > GameNetworkManager->MAXCLIENTUPDATEINTERVAL)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("GameNetworkManager::MaxMoveDeltaTime (%f) is greater than GameNetworkManager::MAXCLIENTUPDATEINTERVAL (%f)! Server will interfere with move deltas that large!"), GameNetworkManager->MaxMoveDeltaTime, GameNetworkManager->MAXCLIENTUPDATEINTERVAL);
		}
	}

	const UWorld* World = ServerMovement.GetWorld();
	if (World)
	{
		WorldCreationTime = World->GetTimeSeconds();
		ServerTimeStamp = World->GetTimeSeconds();
	}

	MaxResponseTime = MaxMoveDeltaTime; // Deprecated, use MaxMoveDeltaTime instead
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS // For deprecated members of FNetworkPredictionData_Server_Character


FNetworkPredictionData_Server_Character::~FNetworkPredictionData_Server_Character()
{
}


float FNetworkPredictionData_Server_Character::GetServerMoveDeltaTime(float ClientTimeStamp, float ActorTimeDilation) const
{
	if (bResolvingTimeDiscrepancy)
	{
		return TimeDiscrepancyResolutionMoveDeltaOverride;
	}
	else
	{
		return GetBaseServerMoveDeltaTime(ClientTimeStamp, ActorTimeDilation);
	}
}

float FNetworkPredictionData_Server_Character::GetBaseServerMoveDeltaTime(float ClientTimeStamp, float ActorTimeDilation) const
{
	const float DeltaTime = FMath::Min(MaxMoveDeltaTime * ActorTimeDilation, ClientTimeStamp - CurrentClientTimeStamp);
	return DeltaTime;
}


FSavedMove_Character::FSavedMove_Character()
{
	AccelMagThreshold = 1.f;
	AccelDotThreshold = 0.9f;
	AccelDotThresholdCombine = 0.996f; // approx 5 degrees.
}

FSavedMove_Character::~FSavedMove_Character()
{
}

void FSavedMove_Character::Clear()
{
	bPressedJump = false;
	bWantsToCrouch = false;
	bForceMaxAccel = false;
	bForceNoCombine = false;
	bOldTimeStampBeforeReset = false;

	TimeStamp = 0.f;
	DeltaTime = 0.f;
	CustomTimeDilation = 1.0f;
	JumpKeyHoldTime = 0.0f;
	JumpCurrentCount = 0.0f;
	JumpMaxCount = 1;
	MovementMode = 0;

	StartLocation = FVector::ZeroVector;
	StartRelativeLocation = FVector::ZeroVector;
	StartVelocity = FVector::ZeroVector;
	StartFloor = FFindFloorResult();
	StartRotation = FRotator::ZeroRotator;
	StartControlRotation = FRotator::ZeroRotator;
	StartBaseRotation = FQuat::Identity;
	StartCapsuleRadius = 0.f;
	StartCapsuleHalfHeight = 0.f;
	StartBase = NULL;
	StartBoneName = NAME_None;

	SavedLocation = FVector::ZeroVector;
	SavedRotation = FRotator::ZeroRotator;
	SavedRelativeLocation = FVector::ZeroVector;
	Acceleration = FVector::ZeroVector;
	SavedControlRotation = FRotator::ZeroRotator;
	EndBase = NULL;
	EndBoneName = NAME_None;

	RootMotionMontage = NULL;
	RootMotionTrackPosition = 0.f;
	RootMotionMovement.Clear();

	SavedRootMotion.Clear();
}


void FSavedMove_Character::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData)
{
	DeltaTime = InDeltaTime;
	
	SetInitialPosition(Character);

	AccelMag = NewAccel.Size();
	AccelNormal = (AccelMag > SMALL_NUMBER ? NewAccel / AccelMag : FVector::ZeroVector);
	
	// Round value, so that client and server match exactly (and so we can send with less bandwidth). This rounded value is copied back to the client in ReplicateMoveToServer.
	// This is done after the AccelMag and AccelNormal are computed above, because those are only used client-side for combining move logic and need to remain accurate.
	Acceleration = Character->GetCharacterMovement()->RoundAcceleration(NewAccel);

	bPressedJump = Character->bPressedJump;
	JumpKeyHoldTime = Character->JumpKeyHoldTime;
	JumpMaxCount = Character->JumpMaxCount;
	
	// CheckJumpInput will increment JumpCurrentCount.
	// Therefore, for replicated moves we want it to set it at 1 less to properly
	// handle the change.
	JumpCurrentCount = Character->JumpCurrentCount > 0 ? Character->JumpCurrentCount - 1 : 0;
	bWantsToCrouch = Character->GetCharacterMovement()->bWantsToCrouch;
	bForceMaxAccel = Character->GetCharacterMovement()->bForceMaxAccel;
	MovementMode = Character->GetCharacterMovement()->PackNetworkMovementMode();

	// Root motion source-containing moves should never be combined
	// Main discovered issue being a move without root motion combining with
	// a move with it will cause the DeltaTime for that next move to be larger than
	// intended (effectively root motion applies to movement that happened prior to its activation)
	if (Character->GetCharacterMovement()->CurrentRootMotion.HasActiveRootMotionSources())
	{
		bForceNoCombine = true;
	}

	TimeStamp = ClientData.CurrentTimeStamp;
}

void FSavedMove_Character::SetInitialPosition(ACharacter* Character)
{
	StartLocation = Character->GetActorLocation();
	StartRotation = Character->GetActorRotation();
	StartVelocity = Character->GetCharacterMovement()->Velocity;
	UPrimitiveComponent* const MovementBase = Character->GetMovementBase();
	StartBase = MovementBase;
	StartBaseRotation = FQuat::Identity;
	StartFloor = Character->GetCharacterMovement()->CurrentFloor;
	CustomTimeDilation = Character->CustomTimeDilation;
	StartBoneName = Character->GetBasedMovement().BoneName;

	if (MovementBaseUtility::UseRelativeLocation(MovementBase))
	{
		StartRelativeLocation = Character->GetBasedMovement().Location;
		FVector StartBaseLocation_Unused;
		MovementBaseUtility::GetMovementBaseTransform(MovementBase, StartBoneName, StartBaseLocation_Unused, StartBaseRotation);
	}

	StartControlRotation = Character->GetControlRotation().Clamp();
	Character->GetCapsuleComponent()->GetScaledCapsuleSize(StartCapsuleRadius, StartCapsuleHalfHeight);
}

void FSavedMove_Character::PostUpdate(ACharacter* Character, FSavedMove_Character::EPostUpdateMode PostUpdateMode)
{
	// Common code for both recording and after a replay.
	{
		MovementMode = Character->GetCharacterMovement()->PackNetworkMovementMode();
		SavedLocation = Character->GetActorLocation();
		SavedRotation = Character->GetActorRotation();
		SavedVelocity = Character->GetVelocity();
#if ENABLE_NAN_DIAGNOSTIC
		const float WarnVelocitySqr = 20000.f * 20000.f;
		if (SavedVelocity.SizeSquared() > WarnVelocitySqr)
		{
			if (Character->SavedRootMotion.HasActiveRootMotionSources())
			{
				UE_LOG(LogCharacterMovement, Log, TEXT("FSavedMove_Character::PostUpdate detected very high Velocity! (%s), but with active root motion sources (could be intentional)"), *SavedVelocity.ToString());
			}
			else
			{
				UE_LOG(LogCharacterMovement, Warning, TEXT("FSavedMove_Character::PostUpdate detected very high Velocity! (%s)"), *SavedVelocity.ToString());
			}
		}
#endif
		UPrimitiveComponent* const MovementBase = Character->GetMovementBase();
		EndBase = MovementBase;
		EndBoneName = Character->GetBasedMovement().BoneName;
		if (MovementBaseUtility::UseRelativeLocation(MovementBase))
		{
			SavedRelativeLocation = Character->GetBasedMovement().Location;
		}

		SavedControlRotation = Character->GetControlRotation().Clamp();
	}

	// Only save RootMotion params when initially recording
	if (PostUpdateMode == PostUpdate_Record)
	{
		const FAnimMontageInstance* RootMotionMontageInstance = Character->GetRootMotionAnimMontageInstance();
		if (RootMotionMontageInstance && !RootMotionMontageInstance->IsRootMotionDisabled())
		{
			RootMotionMontage = RootMotionMontageInstance->Montage;
			RootMotionTrackPosition = RootMotionMontageInstance->GetPosition();
			RootMotionMovement = Character->ClientRootMotionParams;
		}

		// Save off Root Motion Sources
		if( Character->SavedRootMotion.HasActiveRootMotionSources() )
		{
			SavedRootMotion = Character->SavedRootMotion;
		}
	}
	else if (PostUpdateMode == PostUpdate_Replay)
	{
		if( Character->bClientResimulateRootMotionSources )
		{
			// When replaying moves, the next move should use the results of this move
			// so that future replayed moves account for the server correction
			Character->SavedRootMotion = Character->GetCharacterMovement()->CurrentRootMotion;
		}
	}
}

bool FSavedMove_Character::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	// Check if any important movement flags have changed status.
	if( (bPressedJump != LastAckedMove->bPressedJump) || (bWantsToCrouch != LastAckedMove->bWantsToCrouch) )
	{
		return true;
	}

	if (MovementMode != LastAckedMove->MovementMode)
	{
		return true;
	}

	// check if acceleration has changed significantly
	if( Acceleration != LastAckedMove->Acceleration ) 
	{
		// Compare magnitude and orientation
		if( (FMath::Abs(AccelMag - LastAckedMove->AccelMag) > AccelMagThreshold) || ((AccelNormal | LastAckedMove->AccelNormal) < AccelDotThreshold) )
		{
			return true;
		}
	}
	return false;
}

FVector FSavedMove_Character::GetRevertedLocation() const
{
	const UPrimitiveComponent* MovementBase = StartBase.Get();
	if (MovementBaseUtility::UseRelativeLocation(MovementBase))
	{
		FVector BaseLocation; FQuat BaseRotation;
		MovementBaseUtility::GetMovementBaseTransform(MovementBase, StartBoneName, BaseLocation, BaseRotation);
		return BaseLocation + StartRelativeLocation;
	}

	return StartLocation;
}

bool UCharacterMovementComponent::CanDelaySendingMove(const FSavedMovePtr& NewMove)
{
	return true;
}

float UCharacterMovementComponent::GetClientNetSendDeltaTime(const APlayerController* PC, const FNetworkPredictionData_Client_Character* ClientData, const FSavedMovePtr& NewMove) const
{
	const UPlayer* Player = (PC ? PC->Player : nullptr);
	const UWorld* MyWorld = GetWorld();
	const AGameStateBase* const GameState = MyWorld->GetGameState();
	const AGameNetworkManager* const GameNetworkManager = GetDefault<AGameNetworkManager>();
	float NetMoveDelta = GameNetworkManager->ClientNetSendMoveDeltaTime;

	// send moves more frequently in small games where server isn't likely to be saturated
	if (Player && (Player->CurrentNetSpeed > GameNetworkManager->ClientNetSendMoveThrottleAtNetSpeed) && (GameState != nullptr) && (GameState->PlayerArray.Num() <= GameNetworkManager->ClientNetSendMoveThrottleOverPlayerCount))
	{
		NetMoveDelta = GameNetworkManager->ClientNetSendMoveDeltaTime;
	}
	else if (Player)
	{
		NetMoveDelta = FMath::Max(GameNetworkManager->ClientNetSendMoveDeltaTimeThrottled, 2 * GameNetworkManager->MoveRepSize / Player->CurrentNetSpeed);
	}
	
	return NetMoveDelta;
}

bool FSavedMove_Character::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	if (bForceNoCombine || NewMove->bForceNoCombine)
	{
		return false;
	}

	// Cannot combine moves which contain root motion for now.
	// @fixme laurent - we should be able to combine most of them though, but current scheme of resetting pawn location and resimulating forward doesn't work.
	// as we don't want to tick montage twice (so we don't fire events twice). So we need to rearchitecture this so we tick only the second part of the move, and reuse the first part.
	if( (RootMotionMontage != NULL) || (NewMove->RootMotionMontage != NULL) )
	{
		return false;
	}

	if (NewMove->Acceleration.IsZero())
	{
		if (!Acceleration.IsZero())
		{
			return false;
		}

		if (!StartVelocity.IsZero() || !NewMove->StartVelocity.IsZero())
		{
			return false;
		}
	}
	else
	{
		if (NewMove->DeltaTime + DeltaTime >= MaxDelta)
		{
			return false;
		}
			
		if (!FVector::Coincident(AccelNormal, NewMove->AccelNormal, AccelDotThresholdCombine))
		{
			return false;
		}	
	}

	if (bPressedJump || NewMove->bPressedJump)
	{
		return false;
	}
	
	if (bWantsToCrouch != NewMove->bWantsToCrouch)
	{
		return false;
	}

	if (StartBase != NewMove->StartBase)
	{
		return false;
	}

	if (StartBoneName != NewMove->StartBoneName)
	{
		return false;
	}

	if (MovementMode != NewMove->MovementMode)
	{
		return false;
	}

	if (StartCapsuleRadius != NewMove->StartCapsuleRadius)
	{
		return false;
	}

	if (StartCapsuleHalfHeight != NewMove->StartCapsuleHalfHeight)
	{
		return false;
	}

	if (!StartBaseRotation.Equals(NewMove->StartBaseRotation)) // only if base hasn't rotated
	{
		return false;
	}

	if (CustomTimeDilation != NewMove->CustomTimeDilation)
	{
		return false;
	}

	return true;
}

void FSavedMove_Character::PrepMoveFor(ACharacter* Character)
{
	if( RootMotionMontage != NULL )
	{
		// If we need to resimulate Root Motion, then do so.
		if( Character->bClientResimulateRootMotion )
		{
			// Make sure RootMotion montage matches what we are playing now.
			FAnimMontageInstance * RootMotionMontageInstance = Character->GetRootMotionAnimMontageInstance();
			if( RootMotionMontageInstance && (RootMotionMontage == RootMotionMontageInstance->Montage) )
			{
				RootMotionMovement.Clear();
				RootMotionTrackPosition = RootMotionMontageInstance->GetPosition();
				RootMotionMontageInstance->SimulateAdvance(DeltaTime, RootMotionTrackPosition, RootMotionMovement);
				RootMotionMontageInstance->SetPosition(RootMotionTrackPosition);
			}
		}

		// Restore root motion to that of this SavedMove to be used during replaying the Move
		Character->GetCharacterMovement()->RootMotionParams = RootMotionMovement;
	}

	// Resimulate Root Motion Sources if we need to - occurs after server RPCs over a correction during root motion sources.
	if( SavedRootMotion.HasActiveRootMotionSources() )
	{
		if( Character->bClientResimulateRootMotionSources )
		{
			// Note: This may need to change to a SimulatePrepare() that doesn't depend on everything
			// being "currently active" - if we have sources that are no longer around or valid,
			// we're not able to properly re-prepare them, and should just keep whatever we currently have

			// Apply any corrections/state from either last played move or last received from server (in ACharacter::SavedRootMotion)
			UE_LOG(LogRootMotion, VeryVerbose, TEXT("SavedMove SavedRootMotion getting updated for SavedMove replays: %s"), *Character->GetName());
			SavedRootMotion.UpdateStateFrom(Character->SavedRootMotion);
			SavedRootMotion.CleanUpInvalidRootMotion(DeltaTime, *Character, *Character->GetCharacterMovement());
			SavedRootMotion.PrepareRootMotion(DeltaTime, *Character, *Character->GetCharacterMovement());
		}

		// Restore root motion to that of this SavedMove to be used during replaying the Move
		Character->GetCharacterMovement()->CurrentRootMotion = SavedRootMotion;
	}

	Character->GetCharacterMovement()->bForceMaxAccel = bForceMaxAccel;
	Character->JumpKeyHoldTime = JumpKeyHoldTime;
	Character->JumpMaxCount = JumpMaxCount;
	Character->JumpCurrentCount = JumpCurrentCount;
}


uint8 FSavedMove_Character::GetCompressedFlags() const
{
	uint8 Result = 0;

	if (bPressedJump)
	{
		Result |= FLAG_JumpPressed;
	}

	if (bWantsToCrouch)
	{
		Result |= FLAG_WantsToCrouch;
	}

	return Result;
}

void UCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	if (!CharacterOwner)
	{
		return;
	}

	const bool bWasJumping = CharacterOwner->bPressedJump;

	CharacterOwner->bPressedJump = ((Flags & FSavedMove_Character::FLAG_JumpPressed) != 0);	
	bWantsToCrouch = ((Flags & FSavedMove_Character::FLAG_WantsToCrouch) != 0);

	// Reset JumpKeyHoldTime when player presses Jump key on server as well.
	if (!bWasJumping && CharacterOwner->bPressedJump)
	{
		CharacterOwner->bWasJumping = false;
		CharacterOwner->JumpKeyHoldTime = 0.0f;
	}
}

void UCharacterMovementComponent::FlushServerMoves()
{
	// Send pendingMove to server if this character is replicating movement
	if (CharacterOwner && CharacterOwner->bReplicateMovement)
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (!ClientData)
		{
			return;
		}

		if (ClientData->PendingMove.IsValid() != false)
		{
			const UWorld* MyWorld = GetWorld();

			ClientData->ClientUpdateTime = MyWorld->TimeSeconds;

			FSavedMovePtr NewMove = ClientData->PendingMove;

			ClientData->PendingMove = NULL;
			CallServerMove(NewMove.Get(), nullptr);
		}
	}
}

