// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Engine/LevelScriptActor.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/InputDelegateBinding.h"
#include "Engine/LevelScriptBlueprint.h"


//////////////////////////////////////////////////////////////////////////
// ALevelScriptActor

ALevelScriptActor::ALevelScriptActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

#if WITH_EDITORONLY_DATA
	bActorLabelEditable = false;
	bEditable = false;
#endif // WITH_EDITORONLY_DATA
	PrimaryActorTick.bCanEverTick = true;

	bCanBeDamaged = false;
	bInputEnabled = true;
 
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
}

#if WITH_EDITOR
void ALevelScriptActor::PostDuplicate(bool bDuplicateForPIE)
{
	ULevelScriptBlueprint* MyBlueprint = Cast<ULevelScriptBlueprint>(GetClass()->ClassGeneratedBy);
	if (MyBlueprint && !GIsDuplicatingClassForReinstancing && !IsPendingKill())
	{
		MyBlueprint->SetObjectBeingDebugged(this);
	}

	Super::PostDuplicate(bDuplicateForPIE);
}

void ALevelScriptActor::BeginDestroy()
{
	if (ULevelScriptBlueprint* MyBlueprint = Cast<ULevelScriptBlueprint>(GetClass()->ClassGeneratedBy))
	{
		MyBlueprint->SetObjectBeingDebugged(NULL);
	}

	Super::BeginDestroy();
}
#endif

void ALevelScriptActor::PreInitializeComponents()
{
	if (UInputDelegateBinding::SupportsInputDelegate(GetClass()))
	{
		// create an InputComponent object so that the level script actor can bind key events
		InputComponent = NewObject<UInputComponent>(this);
		InputComponent->RegisterComponent();

		UInputDelegateBinding::BindInputDelegates(GetClass(), InputComponent);
	}
	Super::PreInitializeComponents();
}

bool ALevelScriptActor::RemoteEvent(FName EventName)
{
	bool bFoundEvent = false;

	// Iterate over all levels, and try to find a matching function on the level's script actor
	for( TArray<ULevel*>::TConstIterator it = GetWorld()->GetLevels().CreateConstIterator(); it; ++it )
	{
		ULevel* CurLevel = *it;
		if( CurLevel && CurLevel->bIsVisible )
		{
			ALevelScriptActor* LSA = CurLevel->GetLevelScriptActor();
			if( LSA )
			{
				// Find an event with no parameters
				UFunction* EventTarget = LSA->FindFunction(EventName);
				if( EventTarget && EventTarget->NumParms == 0)
				{
					LSA->ProcessEvent(EventTarget, NULL);
					bFoundEvent = true;
				}
			}
		}
	}

	return bFoundEvent;
}

void ALevelScriptActor::SetCinematicMode(bool bCinematicMode, bool bHidePlayer, bool bAffectsHUD, bool bAffectsMovement, bool bAffectsTurning)
{
	// Loop through all player controllers and call their version of SetCinematicMode which is where all the magic happens and replication occurs
	for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		(*Iterator)->SetCinematicMode(bCinematicMode, bHidePlayer, bAffectsHUD, bAffectsMovement, bAffectsTurning);
	}
}

void ALevelScriptActor::EnableInput(class APlayerController* PlayerController)
{
	if (PlayerController != NULL)
	{
		UE_LOG(LogLevel, Warning, TEXT("EnableInput on a LevelScript actor can not be specified for only one PlayerController.  Enabling for all PlayerControllers."));
	}
	bInputEnabled = true;
}

void ALevelScriptActor::DisableInput(class APlayerController* PlayerController)
{
	if (PlayerController != NULL)
	{
		UE_LOG(LogLevel, Warning, TEXT("DisableInput on a LevelScript actor can not be specified for only one PlayerController.  Disabling for all PlayerControllers."));
	}
	bInputEnabled = false;
}
