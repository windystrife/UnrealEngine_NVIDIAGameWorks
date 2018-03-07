// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Replication.cpp: Engine actor replication implementation
=============================================================================*/

#include "CoreMinimal.h"
#include "UObject/CoreNet.h"
#include "EngineGlobals.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "UnrealEngine.h"
#include "Matinee/MatineeInterface.h"
#include "Matinee/MatineeActor.h"
#include "Net/UnrealNetwork.h"

void AMatineeActor::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME_CONDITION( AMatineeActor, MatineeData,		COND_InitialOnly );
	DOREPLIFETIME_CONDITION( AMatineeActor, GroupActorInfos,	COND_InitialOnly );

	DOREPLIFETIME( AMatineeActor, bIsPlaying );
	DOREPLIFETIME( AMatineeActor, bLooping );
	DOREPLIFETIME( AMatineeActor, bReversePlayback );
	DOREPLIFETIME( AMatineeActor, bPaused );
	DOREPLIFETIME( AMatineeActor, PlayRate );
	DOREPLIFETIME( AMatineeActor, InterpPosition );
	DOREPLIFETIME( AMatineeActor, ReplicationForceIsPlaying );
}

//
// Static variables for networking.
//

static UInterpData*		SavedInterpData;
static bool				SavedbIsPlaying;
static bool				SavedbReversePlayback;
static float			SavedPosition;
static uint8			SavedReplicationForceIsPlaying;

float AMatineeActor::GetNetPriority(const FVector& ViewPos, const FVector& ViewDir, AActor* Viewer, AActor* ViewTarget, UActorChannel* InChannel, float Time, bool bLowBandwidth)
{
	float Result = Super::GetNetPriority(ViewPos, ViewDir, Viewer, ViewTarget, InChannel, Time, bLowBandwidth);
	// attempt to replicate MatineeActors approximately in the order that they were spawned to reduce ordering issues
	// when LDs make multiple matinees affect the same target(s)
	// not great, but without a full dependency setup this is the best we can do
	if (InChannel == NULL)
	{
		Result += 1.0f - CreationTime / GetWorld()->TimeSeconds;
	}
	return Result;
}

void AMatineeActor::PreNetReceive()
{
	Super::PreNetReceive();

	SavedInterpData = MatineeData;

	if (MatineeData != NULL)
	{
		SavedbIsPlaying = bIsPlaying;
		SavedPosition = InterpPosition;
		SavedbReversePlayback = bReversePlayback;
		SavedReplicationForceIsPlaying = ReplicationForceIsPlaying;
	}
}

/** @hack: saves and restores fade state for a PC when it goes out of scope
 * used for fade track hack below */
struct FSavedFadeState
{
public:
	FSavedFadeState(APlayerCameraManager* InCamera)
		: Camera(InCamera), bEnableFading(InCamera->bEnableFading), FadeAmount(InCamera->FadeAmount), FadeTimeRemaining(InCamera->FadeTimeRemaining)
	{}
	~FSavedFadeState()
	{
		Camera->bEnableFading = bEnableFading;
		Camera->FadeAmount = FadeAmount;
		Camera->FadeTimeRemaining = FadeTimeRemaining;
	}
private:
	APlayerCameraManager* Camera;
	bool bEnableFading;
	float FadeAmount;
	float FadeTimeRemaining;
};

void AMatineeActor::PostNetReceive()
{
	Super::PostNetReceive();

	if (MatineeData != NULL)
	{
		TArray<AActor*> ControlledActors;
		// Build a list of actors controlled by this matinee actor
		for( int32 InfoIndex = 0; InfoIndex < GroupActorInfos.Num(); ++InfoIndex )
		{
			const FInterpGroupActorInfo& Info = GroupActorInfos[ InfoIndex ];

			for (int32 ActorIndex = 0; ActorIndex < Info.Actors.Num(); ++ActorIndex )
			{
				AActor* Actor = Info.Actors[ ActorIndex ];
				if (Actor != NULL)
				{
					ControlledActors.Add( Actor );
				}
			}
		}

		// if we just received the matinee action, set 'saved' values to default so we make sure to apply previously received values
		if (SavedInterpData == NULL)
		{
			AMatineeActor* Default = GetClass()->GetDefaultObject<AMatineeActor>();
			SavedbIsPlaying = Default->bIsPlaying;
			SavedPosition = Default->InterpPosition;
			SavedbReversePlayback = Default->bReversePlayback;
		}

		// Handle replication of flag saying that bIsPlaying really should have replicated as true.
		if (SavedReplicationForceIsPlaying != ReplicationForceIsPlaying)
		{
			bIsPlaying = true;
		}

		// apply bReversePlayback
		if (SavedbReversePlayback!= bReversePlayback)
		{
			if (SavedbIsPlaying && bIsPlaying)
			{
				// notify actors that something has changed
				for (int32 ActorIndex = 0; ActorIndex < ControlledActors.Num(); ++ActorIndex )
				{
					IMatineeInterface * IMI = Cast<IMatineeInterface>(ControlledActors[ActorIndex]);
					if (IMI)
					{
						IMI->InterpolationChanged(this);
					}
				}
			}
		}
	
		// start up interpolation, if necessary
		if (!SavedbIsPlaying && (bIsPlaying || InterpPosition != SavedPosition))
		{	
			InitInterp();

			// if we're playing forward, call Play() to process any special properties on InterpAction that may affect the meaning of 'Position' (bNoResetOnRewind, etc)
			if (!bReversePlayback)
			{
				Play();
			}

			// find affected actors and set their ControllingMatineeActor
			// @warning: this code requires the linked actors to be static object references (i.e., some other Kismet action can't be assigning them)
			// this might not work for AI pawns
		
			for (int32 ActorIndex = 0; ActorIndex < ControlledActors.Num(); ++ActorIndex )
			{
				AActor* Actor = ControlledActors[ActorIndex];
				UInterpGroupInst * GrInst = FindGroupInst(Actor);
				if (Actor != NULL && !Actor->IsPendingKill() && GrInst != NULL) 
				{
					Actor->AddControllingMatineeActor(*this);
					// fire an event if we're really playing (and not just starting it up to do a position update)
					if (bIsPlaying)
					{
						IMatineeInterface * IMI = Cast<IMatineeInterface>(Actor);
						if (IMI)
						{
							IMI->InterpolationStarted(this);
						}
					}
				}
			}

		}

		// if we received a different current position
		if (InterpPosition != SavedPosition)
		{
			//@hack: negate fade tracks if we're updating a stopped matinee
			// the right fix is probably to pass bJump=true to UpdateInterp() when (!bIsPlaying && !SavedbIsPlaying),
			// but that may have lots of other side effects I don't have time to test right now
			TArray<FSavedFadeState> SavedFadeStates;
			if (!bIsPlaying && !SavedbIsPlaying && MatineeData != NULL)
			{
				for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
				{
					if (It->PlayerController != NULL && It->PlayerController->PlayerCameraManager != NULL)
					{
						new(SavedFadeStates)FSavedFadeState(It->PlayerController->PlayerCameraManager);
					}
				}
			}

			if (bIsPlaying && SavedPosition != -1 && FMath::Abs(InterpPosition - SavedPosition) < ClientSidePositionErrorTolerance)
			{
				// The error value between us and the server is too small to change gameplay, but will cause visual pops
				InterpPosition = SavedPosition;
			}
			else
			{
				// set to position replicated from server
				UpdateInterp(InterpPosition, false, false);
			}
		}

		// terminate interpolation, if necessary
		if ((SavedbIsPlaying || InterpPosition != SavedPosition) && !bIsPlaying)
		{
			TermInterp();

			// find affected actors and remove InterpAction from their LatentActions list
			for (int32 ActorIndex = 0; ActorIndex < ControlledActors.Num(); ++ActorIndex )
			{
				AActor* Actor = ControlledActors[ ActorIndex ];
				if (Actor != NULL)
				{
					Actor->RemoveControllingMatineeActor(*this);

					// fire an event if we were really playing (and not just starting it up to do a position update)
					if (SavedbIsPlaying)
					{
						IMatineeInterface * IMI = Cast<IMatineeInterface>(Actor);
						if (IMI)
						{
							IMI->InterpolationFinished(this);
						}
					}
				}
			}
		}
	}
}
