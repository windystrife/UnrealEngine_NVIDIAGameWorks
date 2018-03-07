// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Controller.cpp: 

=============================================================================*/
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "AI/Navigation/NavigationSystem.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "NetworkingDistanceConstants.h"
#include "VisualLogger/VisualLogger.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Logging/MessageLog.h"

// @todo this is here only due to circular dependency to AIModule. To be removed
#include "Engine/Canvas.h"
#include "Navigation/PathFollowingComponent.h"

#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY(LogPath);

#define LOCTEXT_NAMESPACE "Controller"


AController::AController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	PrimaryActorTick.bCanEverTick = true;
	bHidden = true;
#if WITH_EDITORONLY_DATA
	bHiddenEd = true;
#endif // WITH_EDITORONLY_DATA
	bOnlyRelevantToOwner = true;

	TransformComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TransformComponent0"));
	RootComponent = TransformComponent;

	bCanBeDamaged = false;
	bAttachToPawn = false;
	bIsPlayerController = false;

	if (RootComponent)
	{
		// We attach the RootComponent to the pawn for location updates,
		// but we want to drive rotation with ControlRotation regardless of attachment state.
		RootComponent->bAbsoluteRotation = true;
	}
}

void AController::K2_DestroyActor()
{
	// do nothing, disallow destroying controller from Blueprints
}

bool AController::IsLocalController() const
{
	const ENetMode NetMode = GetNetMode();

	if (NetMode == NM_Standalone)
	{
		// Not networked.
		return true;
	}
	
	if (NetMode == NM_Client && Role == ROLE_AutonomousProxy)
	{
		// Networked client in control.
		return true;
	}

	if (GetRemoteRole() != ROLE_AutonomousProxy && Role == ROLE_Authority)
	{
		// Local authority in control.
		return true;
	}

	return false;
}

void AController::FailedToSpawnPawn()
{
	ChangeState(NAME_Inactive);
}

void AController::SetInitialLocationAndRotation(const FVector& NewLocation, const FRotator& NewRotation)
{
	SetActorLocationAndRotation(NewLocation, NewRotation);
	SetControlRotation(NewRotation);
}

FRotator AController::GetControlRotation() const
{
	ControlRotation.DiagnosticCheckNaN();
	return ControlRotation;
}

void AController::SetControlRotation(const FRotator& NewRotation)
{
#if ENABLE_NAN_DIAGNOSTIC
	if (NewRotation.ContainsNaN())
	{
		logOrEnsureNanError(TEXT("AController::SetControlRotation about to apply NaN-containing rotation! (%s)"), *NewRotation.ToString());
		return;
	}
#endif
	if (!ControlRotation.Equals(NewRotation, 1e-3f))
	{
		ControlRotation = NewRotation;

		if (RootComponent && RootComponent->bAbsoluteRotation)
		{
			RootComponent->SetWorldRotation(GetControlRotation());
		}
	}
	else
	{
		//UE_LOG(LogPlayerController, Log, TEXT("Skipping SetControlRotation for %s (Pawn %s)"), *GetNameSafe(this), *GetNameSafe(GetPawn()));
	}
}


void AController::SetIgnoreMoveInput(bool bNewMoveInput)
{
	IgnoreMoveInput = FMath::Max(IgnoreMoveInput + (bNewMoveInput ? +1 : -1), 0);
}

void AController::ResetIgnoreMoveInput()
{
	IgnoreMoveInput = 0;
}

bool AController::IsMoveInputIgnored() const
{
	return (IgnoreMoveInput > 0);
}

void AController::SetIgnoreLookInput(bool bNewLookInput)
{
	IgnoreLookInput = FMath::Max(IgnoreLookInput + (bNewLookInput ? +1 : -1), 0);
}

void AController::ResetIgnoreLookInput()
{
	IgnoreLookInput = 0;
}

bool AController::IsLookInputIgnored() const
{
	return (IgnoreLookInput > 0);
}

void AController::ResetIgnoreInputFlags()
{
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
}


void AController::AttachToPawn(APawn* InPawn)
{
	if (bAttachToPawn && RootComponent)
	{
		if (InPawn)
		{
			// Only attach if not already attached.
			if (InPawn->GetRootComponent() && RootComponent->GetAttachParent() != InPawn->GetRootComponent())
			{
				RootComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				RootComponent->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
				RootComponent->AttachToComponent(InPawn->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			}
		}
		else
		{
			DetachFromPawn();
		}
	}
}

void AController::DetachFromPawn()
{
	if (bAttachToPawn && RootComponent && RootComponent->GetAttachParent() && Cast<APawn>(RootComponent->GetAttachmentRootActor()))
	{
		RootComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
}


AActor* AController::GetViewTarget() const
{
	if (Pawn)
	{
		return Pawn;
	}

	return const_cast<AController*>(this);
}

void AController::GetPlayerViewPoint( FVector& out_Location, FRotator& out_Rotation ) const
{
	GetActorEyesViewPoint( out_Location, out_Rotation);
}

bool AController::LineOfSightTo(const AActor* Other, FVector ViewPoint, bool bAlternateChecks) const
{
	if( !Other )
	{
		return false;
	}

	if ( ViewPoint.IsZero() )
	{
		FRotator ViewRotation;
		GetActorEyesViewPoint(ViewPoint, ViewRotation);
	}

	FCollisionQueryParams CollisionParms(SCENE_QUERY_STAT(LineOfSight), true, Other);
	CollisionParms.AddIgnoredActor(this->GetPawn());
	FVector TargetLocation = Other->GetTargetLocation(Pawn);
	bool bHit = GetWorld()->LineTraceTestByChannel(ViewPoint, TargetLocation, ECC_Visibility, CollisionParms);
	if( !bHit )
	{
		return true;
	}

	// if other isn't using a cylinder for collision and isn't a Pawn (which already requires an accurate cylinder for AI)
	// then don't go any further as it likely will not be tracing to the correct location
	if (!Cast<const APawn>(Other) && Cast<UCapsuleComponent>(Other->GetRootComponent()) == NULL)
	{
		return false;
	}
	float distSq = (Other->GetActorLocation() - ViewPoint).SizeSquared();
	if ( distSq > FARSIGHTTHRESHOLDSQUARED )
	{
		return false;
	}
	if ( !Cast<const APawn>(Other) && (distSq > NEARSIGHTTHRESHOLDSQUARED) ) 
	{
		return false;
	}

	float OtherRadius, OtherHeight;
	Other->GetSimpleCollisionCylinder(OtherRadius, OtherHeight);
	
	//try viewpoint to head
	bHit = GetWorld()->LineTraceTestByChannel(ViewPoint,  Other->GetActorLocation() + FVector(0.f,0.f,OtherHeight), ECC_Visibility, CollisionParms);
	return !bHit;
}

void AController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if ( !IsPendingKill() )
	{
		GetWorld()->AddController( this );

		// Since we avoid updating rotation in SetControlRotation() if it hasn't changed,
		// we should make sure that the initial RootComponent rotation matches it if ControlRotation was set directly.
		if (RootComponent && RootComponent->bAbsoluteRotation)
		{
			RootComponent->SetWorldRotation(GetControlRotation());
		}
	}
}

void AController::Possess(APawn* InPawn)
{
	if (!HasAuthority())
	{
		FMessageLog("PIE").Warning(FText::Format(
			LOCTEXT("ControllerPossessAuthorityOnly", "Possess function should only be used by the network authority for {0}"),
			FText::FromName(GetFName())
			));
		return;
	}

	REDIRECT_OBJECT_TO_VLOG(InPawn, this);

	if (InPawn != NULL)
	{
		if (GetPawn() && GetPawn() != InPawn)
		{
			UnPossess();
		}

		if (InPawn->Controller != NULL)
		{
			InPawn->Controller->UnPossess();
		}

		InPawn->PossessedBy(this);
		SetPawn(InPawn);

		// update rotation to match possessed pawn's rotation
		SetControlRotation( Pawn->GetActorRotation() );

		Pawn->Restart();
	}
}

void AController::UnPossess()
{
	if ( Pawn != NULL )
	{
		Pawn->UnPossessed();
		SetPawn(NULL);
	}
}


void AController::PawnPendingDestroy(APawn* inPawn)
{
	if ( IsInState(NAME_Inactive) )
	{
		UE_LOG(LogPath, Log, TEXT("PawnPendingDestroy while inactive %s"), *GetName());
	}

	if ( inPawn != Pawn )
	{
		return;
	}

	UnPossess();
	ChangeState(NAME_Inactive);

	if (PlayerState == NULL)
	{
		Destroy();
	}
}

void AController::Reset()
{
	Super::Reset();
	StartSpot = NULL;
}

/// @cond DOXYGEN_WARNINGS

void AController::ClientSetLocation_Implementation( FVector NewLocation, FRotator NewRotation )
{
	ClientSetRotation(NewRotation);
	if (Pawn != NULL)
	{
		Pawn->TeleportTo(NewLocation, Pawn->GetActorRotation());
	}
}

void AController::ClientSetRotation_Implementation( FRotator NewRotation, bool bResetCamera )
{
	SetControlRotation(NewRotation);
	if (Pawn != NULL)
	{
		Pawn->FaceRotation( NewRotation, 0.f );
	}
}

/// @endcond

void AController::RemovePawnTickDependency(APawn* InOldPawn)
{
	if (InOldPawn != NULL)
	{
		UPawnMovementComponent* PawnMovement = InOldPawn->GetMovementComponent();
		if (PawnMovement)
		{
			PawnMovement->PrimaryComponentTick.RemovePrerequisite(this, this->PrimaryActorTick);
		}
		
		InOldPawn->PrimaryActorTick.RemovePrerequisite(this, this->PrimaryActorTick);
	}
}


void AController::AddPawnTickDependency(APawn* NewPawn)
{
	if (NewPawn != NULL)
	{
		bool bNeedsPawnPrereq = true;
		UPawnMovementComponent* PawnMovement = NewPawn->GetMovementComponent();
		if (PawnMovement && PawnMovement->PrimaryComponentTick.bCanEverTick)
		{
			PawnMovement->PrimaryComponentTick.AddPrerequisite(this, this->PrimaryActorTick);

			// Don't need a prereq on the pawn if the movement component already sets up a prereq.
			if (PawnMovement->bTickBeforeOwner || NewPawn->PrimaryActorTick.GetPrerequisites().Contains(FTickPrerequisite(PawnMovement, PawnMovement->PrimaryComponentTick)))
			{
				bNeedsPawnPrereq = false;
			}
		}
		
		if (bNeedsPawnPrereq)
		{
			NewPawn->PrimaryActorTick.AddPrerequisite(this, this->PrimaryActorTick);
		}
	}
}


void AController::SetPawn(APawn* InPawn)
{
	RemovePawnTickDependency(Pawn);

	Pawn = InPawn;
	Character = (Pawn ? Cast<ACharacter>(Pawn) : NULL);

	AttachToPawn(Pawn);

	AddPawnTickDependency(Pawn);
}

void AController::SetPawnFromRep(APawn* InPawn)
{
	// This function is needed to ensure OnRep_Pawn is called in the case we need to set AController::Pawn
	// due to APawn::Controller being replicated first. See additional notes in APawn::OnRep_Controller.
	RemovePawnTickDependency(Pawn);
	Pawn = InPawn;
	OnRep_Pawn();
}

void AController::OnRep_Pawn()
{
	// Detect when pawn changes, so we can NULL out the controller on the old pawn
	if ( OldPawn != NULL && Pawn != OldPawn.Get() && OldPawn->Controller == this )
	{
		// Set the old controller to NULL, since we are no longer the owner, and can't rely on it replicating to us anymore
		OldPawn->Controller = NULL;
	}

	OldPawn = Pawn;

	SetPawn(Pawn);
}

void AController::OnRep_PlayerState()
{
	if (PlayerState != NULL)
	{
		PlayerState->ClientInitialize(this);
	}
}

void AController::Destroyed()
{
	if (Role == ROLE_Authority && PlayerState != NULL)
	{
		// if we are a player, log out
		AGameModeBase* const GameMode = GetWorld()->GetAuthGameMode();
		if (GameMode)
		{
			GameMode->Logout(this);
		}

		CleanupPlayerState();
	}

	UnPossess();
	GetWorld()->RemoveController( this );
	Super::Destroyed();
}


void AController::CleanupPlayerState()
{
	PlayerState->Destroy();
	PlayerState = NULL;
}

void AController::InstigatedAnyDamage(float Damage, const class UDamageType* DamageType, class AActor* DamagedActor, class AActor* DamageCauser)
{
	ReceiveInstigatedAnyDamage(Damage, DamageType, DamagedActor, DamageCauser);
	OnInstigatedAnyDamage.Broadcast(Damage, DamageType, DamagedActor, DamageCauser);
}

void AController::InitPlayerState()
{
	if ( GetNetMode() != NM_Client )
	{
		UWorld* const World = GetWorld();
		const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : NULL;

		// If the GameMode is null, this might be a network client that's trying to
		// record a replay. Try to use the default game mode in this case so that
		// we can still spawn a PlayerState.
		if (GameMode == NULL)
		{
			const AGameStateBase* const GameState = World ? World->GetGameState() : NULL;
			GameMode = GameState ? GameState->GetDefaultGameMode() : NULL;
		}

		if (GameMode != NULL)
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Owner = this;
			SpawnInfo.Instigator = Instigator;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.ObjectFlags |= RF_Transient;	// We never want player states to save into a map
			PlayerState = World->SpawnActor<APlayerState>(GameMode->PlayerStateClass, SpawnInfo );
	
			// force a default player name if necessary
			if (PlayerState && PlayerState->PlayerName.IsEmpty())
			{
				// don't call SetPlayerName() as that will broadcast entry messages but the GameMode hasn't had a chance
				// to potentially apply a player/bot name yet
				PlayerState->PlayerName = GameMode->DefaultPlayerName.ToString();
			}
		}
	}
}


void AController::GameHasEnded(AActor* EndGameFocus, bool bIsWinner)
{
}


FRotator AController::GetDesiredRotation() const
{
	return GetControlRotation();
}


void AController::GetActorEyesViewPoint( FVector& out_Location, FRotator& out_Rotation ) const
{
	// If we have a Pawn, this is our view point.
	if ( Pawn != NULL )
	{
		Pawn->GetActorEyesViewPoint( out_Location, out_Rotation );
	}
	// otherwise, controllers don't have a physical location
}


void AController::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	if ( Pawn == NULL )
	{
		if (PlayerState == NULL)
		{
			DisplayDebugManager.DrawString(TEXT("NO PlayerState"));
		}
		else
		{
			PlayerState->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
		}
		Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);
		return;
	}

	DisplayDebugManager.SetDrawColor(FColor(255, 0, 0));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("CONTROLLER %s Pawn %s"), *GetName(), *Pawn->GetName()));
}

FString AController::GetHumanReadableName() const
{
	return PlayerState ? PlayerState->PlayerName : *GetName();
}

void AController::CurrentLevelUnloaded() {}

void AController::ChangeState(FName NewState)
{
	if(NewState != StateName)
	{
		// end current state
		if(StateName == NAME_Inactive)
		{
			EndInactiveState();
		}

		// Set new state name
		StateName = NewState;

		// start new state
		if(StateName == NAME_Inactive)
		{
			BeginInactiveState();
		}
	}
}

FName AController::GetStateName() const
{
	return StateName;
}

bool AController::IsInState(FName InStateName) const
{
	return (StateName == InStateName);
}

void AController::BeginInactiveState() {}

void AController::EndInactiveState() {}


APlayerController* AController::CastToPlayerController()
{
	return Cast<APlayerController>(this);
}

APawn* AController::K2_GetPawn() const
{
	return GetPawn();
}

const FNavAgentProperties& AController::GetNavAgentPropertiesRef() const
{
	return Pawn ? Pawn->GetNavAgentPropertiesRef() : FNavAgentProperties::DefaultProperties;
}

FVector AController::GetNavAgentLocation() const
{
	return Pawn ? Pawn->GetNavAgentLocation() : FVector::ZeroVector;
}

void AController::GetMoveGoalReachTest(const AActor* MovingActor, const FVector& MoveOffset, FVector& GoalOffset, float& GoalRadius, float& GoalHalfHeight) const 
{
	if (Pawn)
	{
		Pawn->GetMoveGoalReachTest(MovingActor, MoveOffset, GoalOffset, GoalRadius, GoalHalfHeight); 
	}
}

bool AController::ShouldPostponePathUpdates() const
{
	return Pawn ? Pawn->ShouldPostponePathUpdates() : false;
}

bool AController::IsFollowingAPath() const
{
	UPathFollowingComponent* PathFollowingComp = FindComponentByClass<UPathFollowingComponent>();
	return (PathFollowingComp != nullptr) && (PathFollowingComp->GetStatus() != EPathFollowingStatus::Idle);
}

void AController::UpdateNavigationComponents()
{
	UPathFollowingComponent* PathFollowingComp = FindComponentByClass<UPathFollowingComponent>();
	if (PathFollowingComp != NULL)
	{
		PathFollowingComp->UpdateCachedComponents();
	}
}

void AController::InitNavigationControl(UPathFollowingComponent*& PathFollowingComp)
{
	PathFollowingComp = FindComponentByClass<UPathFollowingComponent>();
	if (PathFollowingComp == NULL)
	{
		PathFollowingComp = NewObject<UPathFollowingComponent>(this);
		PathFollowingComp->RegisterComponentWithWorld(GetWorld());
		PathFollowingComp->Initialize();
	}
}

void AController::StopMovement()
{
	UE_VLOG(this, LogNavigation, Log, TEXT("AController::StopMovement: %s STOP MOVEMENT"), *GetNameSafe(GetPawn()));

	UPathFollowingComponent* PathFollowingComp = FindComponentByClass<UPathFollowingComponent>();
	if (PathFollowingComp != NULL)
	{
		PathFollowingComp->AbortMove(*this, FPathFollowingResultFlags::MovementStop);
	}
}

void AController::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( AController, PlayerState );
	DOREPLIFETIME_CONDITION_NOTIFY(AController, Pawn, COND_None, REPNOTIFY_Always);
}

#undef LOCTEXT_NAMESPACE
