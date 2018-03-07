// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Player.cpp: Unreal player implementation.
=============================================================================*/
 
#include "Engine/Player.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetConnection.h"
#include "EngineUtils.h"

#include "GameFramework/HUD.h"
#include "GameFramework/PlayerInput.h"

#include "GameFramework/CheatManager.h"
#include "GameFramework/GameStateBase.h"

//////////////////////////////////////////////////////////////////////////
// UPlayer

UPlayer::UPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FString UPlayer::ConsoleCommand(const FString& Cmd, bool bWriteToLog)
{
	UNetConnection* NetConn = Cast<UNetConnection>(this);
	bool bIsBeacon = NetConn && NetConn->OwningActor && !PlayerController;

	UConsole* ViewportConsole = (GEngine->GameViewport != nullptr) ? GEngine->GameViewport->ViewportConsole : nullptr;
	FConsoleOutputDevice StrOut(ViewportConsole);
	FOutputDevice& EffectiveOutputDevice = (!bWriteToLog || (ViewportConsole != nullptr)) ? StrOut : (FOutputDevice&)(*GLog);

	const int32 CmdLen = Cmd.Len();
	TCHAR* CommandBuffer = (TCHAR*)FMemory::Malloc((CmdLen + 1)*sizeof(TCHAR));
	TCHAR* Line = (TCHAR*)FMemory::Malloc((CmdLen + 1)*sizeof(TCHAR));

	const TCHAR* Command = CommandBuffer;
	// copy the command into a modifiable buffer
	FCString::Strcpy(CommandBuffer, (CmdLen + 1), *Cmd.Left(CmdLen));

	// iterate over the line, breaking up on |'s
	while (FParse::Line(&Command, Line, CmdLen + 1))	// The FParse::Line function expects the full array size, including the NULL character.
	{
		// if dissociated with the PC, stop processing commands
		if (bIsBeacon || PlayerController)
		{
			if (!Exec(GetWorld(), Line, EffectiveOutputDevice))
			{
				StrOut.Logf(TEXT("Command not recognized: %s"), Line);
			}
		}
	}

	// Free temp arrays
	FMemory::Free(CommandBuffer);
	CommandBuffer = nullptr;

	FMemory::Free(Line);
	Line = nullptr;

	if (!bWriteToLog)
	{
		return StrOut;
	}

	return TEXT("");
}

APlayerController* UPlayer::GetPlayerController(UWorld* InWorld) const
{
	if (InWorld == nullptr)
	{
		return PlayerController;
	}

	for ( FConstPlayerControllerIterator Iterator = InWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if ( (*Iterator)->GetLocalPlayer() == this )
		{
			return Iterator->Get();
		}
	}

	return nullptr;
}

bool UPlayer::Exec( UWorld* InWorld, const TCHAR* Cmd,FOutputDevice& Ar)
{
	if(PlayerController)
	{
		// Since UGameViewportClient calls Exec on UWorld, we only need to explicitly
		// call UWorld::Exec if we either have a null GEngine or a null ViewportClient
		UWorld* World = PlayerController->GetWorld();
		check(World);
		check(InWorld == NULL || InWorld == World);
		const bool bWorldNeedsExec = GEngine == NULL || Cast<ULocalPlayer>(this) == NULL || static_cast<ULocalPlayer*>(this)->ViewportClient == NULL;
		APawn* PCPawn = PlayerController->GetPawnOrSpectator();
		if( bWorldNeedsExec && World->Exec(World, Cmd,Ar) )
		{
			return true;
		}
		else if( PlayerController->PlayerInput && PlayerController->PlayerInput->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if( PlayerController->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if( PCPawn && PCPawn->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if( PlayerController->MyHUD && PlayerController->MyHUD->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if( World->GetAuthGameMode() && World->GetAuthGameMode()->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if( PlayerController->CheatManager && PlayerController->CheatManager->ProcessConsoleExec(Cmd,Ar,PCPawn) )
		{
			return true;
		}
		else if (World->GetGameState() && World->GetGameState()->ProcessConsoleExec(Cmd, Ar, PCPawn))
		{
			return true;
		}
		else if (PlayerController->PlayerCameraManager && PlayerController->PlayerCameraManager->ProcessConsoleExec(Cmd, Ar, PCPawn))
		{
			return true;
		}
	}
	return false;
}

void UPlayer::SwitchController(class APlayerController* PC)
{
	// Detach old player.
	if( this->PlayerController )
	{
		this->PlayerController->Player = NULL;
	}

	// Set the viewport.
	PC->Player = this;
	this->PlayerController = PC;
}
