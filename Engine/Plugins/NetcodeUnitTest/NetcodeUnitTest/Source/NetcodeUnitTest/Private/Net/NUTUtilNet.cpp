// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Net/NUTUtilNet.h"
#include "UObject/CoreOnline.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Engine/Engine.h"
#include "PacketHandlers/StatelessConnectHandlerComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/DataBunch.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Net/DataChannel.h"

#include "NUTUtil.h"
#include "NUTUtilReflection.h"
#include "UnitTest.h"
#include "ClientUnitTest.h"
#include "MinimalClient.h"
#include "UnitTestEnvironment.h"
#include "Net/UnitTestPackageMap.h"
#include "Net/UnitTestChannel.h"


// Forward declarations
class FWorldTickHook;


/** Active unit test worlds */
TArray<UWorld*> UnitTestWorlds;

/** Unit testing worlds, pending cleanup */
TArray<UWorld*> PendingUnitWorldCleanup;

/** Active world tick hooks */
TArray<FWorldTickHook*> ActiveTickHooks;


/**
 * FProcessNetEventHook
 */

void FProcessEventHook::AddRPCHook(UWorld* InWorld, FOnProcessNetEvent InHook)
{
#if !UE_BUILD_SHIPPING
	if (NetEventHooks.Num() == 0 && EventHooks.Num() == 0)
	{
		AActor::ProcessEventDelegate.BindRaw(this, &FProcessEventHook::HandleProcessEvent);
	}

	NetEventHooks.Add(InWorld, InHook);
#else
	check(false);
#endif
}

void FProcessEventHook::RemoveRPCHook(UWorld* InWorld)
{
#if !UE_BUILD_SHIPPING
	NetEventHooks.Remove(InWorld);

	if (NetEventHooks.Num() == 0 && EventHooks.Num() == 0)
	{
		AActor::ProcessEventDelegate.Unbind();
	}
#else
	check(false);
#endif
}

void FProcessEventHook::AddEventHook(UWorld* InWorld, FOnProcessNetEvent InHook)
{
#if !UE_BUILD_SHIPPING
	if (NetEventHooks.Num() == 0 && EventHooks.Num() == 0)
	{
		AActor::ProcessEventDelegate.BindRaw(this, &FProcessEventHook::HandleProcessEvent);
	}

	EventHooks.Add(InWorld, InHook);
#else
	check(false);
#endif
}

void FProcessEventHook::RemoveEventHook(UWorld* InWorld)
{
#if !UE_BUILD_SHIPPING
	EventHooks.Remove(InWorld);

	if (NetEventHooks.Num() == 0 && EventHooks.Num() == 0)
	{
		AActor::ProcessEventDelegate.Unbind();
	}
#else
	check(false);
#endif
}

bool FProcessEventHook::HandleProcessEvent(AActor* Actor, UFunction* Function, void* Parameters)
{
	bool bBlockEvent = false;

	if (Actor != nullptr && Function != nullptr)
	{
		bool bNetClientRPC = !!(Function->FunctionFlags & FUNC_Net) && !!(Function->FunctionFlags & FUNC_NetClient);
		bool bValidEvent = (bNetClientRPC && NetEventHooks.Num() > 0) || EventHooks.Num() > 0;

		if (bValidEvent)
		{
			UWorld* CurWorld = Actor->GetWorld();

			if (CurWorld != nullptr)
			{
				FOnProcessNetEvent* Hook = (bNetClientRPC ? NetEventHooks.Find(CurWorld) : EventHooks.Find(CurWorld));

				if (Hook != nullptr)
				{
					Hook->Execute(Actor, Function, Parameters, bBlockEvent);
				}
			}
		}
	}

	return bBlockEvent;
}


/**
 * FNetworkNotifyHook
 */

EAcceptConnection::Type FNetworkNotifyHook::NotifyAcceptingConnection()
{
	EAcceptConnection::Type ReturnVal = EAcceptConnection::Ignore;

	if (NotifyAcceptingConnectionDelegate.IsBound())
	{
		ReturnVal = NotifyAcceptingConnectionDelegate.Execute();
	}

	// Until I have a use-case for doing otherwise, make the original authoritative
	if (HookedNotify != NULL)
	{
		ReturnVal = HookedNotify->NotifyAcceptingConnection();
	}

	return ReturnVal;
}

void FNetworkNotifyHook::NotifyAcceptedConnection(UNetConnection* Connection)
{
	NotifyAcceptedConnectionDelegate.ExecuteIfBound(Connection);

	if (HookedNotify != NULL)
	{
		HookedNotify->NotifyAcceptedConnection(Connection);
	}
}

bool FNetworkNotifyHook::NotifyAcceptingChannel(UChannel* Channel)
{
	bool bReturnVal = false;

	if (NotifyAcceptingChannelDelegate.IsBound())
	{
		bReturnVal = NotifyAcceptingChannelDelegate.Execute(Channel);
	}

	// Until I have a use-case for doing otherwise, make the original authoritative
	if (HookedNotify != NULL)
	{
		bReturnVal = HookedNotify->NotifyAcceptingChannel(Channel);
	}

	return bReturnVal;
}

void FNetworkNotifyHook::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch)
{
	bool bHandled = false;

	if (NotifyControlMessageDelegate.IsBound())
	{
		bHandled = NotifyControlMessageDelegate.Execute(Connection, MessageType, Bunch);
	}

	// Only pass on to original, if the control message was not handled
	if (!bHandled && HookedNotify != NULL)
	{
		HookedNotify->NotifyControlMessage(Connection, MessageType, Bunch);
	}
}


/**
 * FScopedNetObjectReplace
 */

FScopedNetObjectReplace::FScopedNetObjectReplace(UClientUnitTest* InUnitTest, UObject* InObjToReplace, UObject* InObjReplacement)
	: UnitTest(InUnitTest)
	, ObjToReplace(InObjToReplace)
{
	UNetConnection* UnitConn = (UnitTest != nullptr && UnitTest->MinClient != nullptr ? UnitTest->MinClient->GetConn() : nullptr);

	if (UnitConn != nullptr)
	{
		UUnitTestPackageMap* PackageMap = Cast<UUnitTestPackageMap>(UnitConn->PackageMap);

		if (PackageMap != nullptr)
		{
			check(!PackageMap->ReplaceObjects.Contains(ObjToReplace));

			PackageMap->ReplaceObjects.Add(ObjToReplace, InObjReplacement);
		}
		else
		{
			check(false);
		}
	}
	else
	{
		check(false);
	}
}

FScopedNetObjectReplace::~FScopedNetObjectReplace()
{
	UNetConnection* UnitConn = (UnitTest != nullptr && UnitTest->MinClient != nullptr ? UnitTest->MinClient->GetConn() : nullptr);

	if (UnitConn != nullptr)
	{
		UUnitTestPackageMap* PackageMap = Cast<UUnitTestPackageMap>(UnitConn->PackageMap);

		if (PackageMap != nullptr)
		{
			check(PackageMap->ReplaceObjects.Contains(ObjToReplace));

			PackageMap->ReplaceObjects.Remove(ObjToReplace);
		}
		else
		{
			check(false);
		}
	}
	else
	{
		check(false);
	}
}


/**
 * NUTNet
 */

void NUTNet::HandleBeaconReplicate(AActor* InBeacon, UNetConnection* InConnection)
{
	// Due to the way the beacon is created in unit tests (replicated, instead of taking over a local beacon client),
	// the NetDriver and BeaconConnection values have to be hack-set, to enable sending of RPC's
	InBeacon->SetNetDriverName(InConnection->Driver->NetDriverName);

	FVMReflection(InBeacon)->*"BeaconConnection" = InConnection;
}


UWorld* NUTNet::CreateUnitTestWorld(bool bHookTick/*=true*/)
{
	UWorld* ReturnVal = NULL;

	// Unfortunately, this hack is needed, to avoid a crash when running as commandlet
	// NOTE: Sometimes, depending upon build settings, PRIVATE_GIsRunningCommandlet is a define instead of a global
#ifndef PRIVATE_GIsRunningCommandlet
	bool bIsCommandlet = PRIVATE_GIsRunningCommandlet;

	PRIVATE_GIsRunningCommandlet = false;
#endif

	ReturnVal = UWorld::CreateWorld(EWorldType::None, false);

#ifndef PRIVATE_GIsRunningCommandlet
	PRIVATE_GIsRunningCommandlet = bIsCommandlet;
#endif


	if (ReturnVal != NULL)
	{
		UnitTestWorlds.Add(ReturnVal);

		// Hook the new worlds 'tick' event, so that we can capture logging
		if (bHookTick)
		{
			FWorldTickHook* TickHook = new FWorldTickHook(ReturnVal);

			ActiveTickHooks.Add(TickHook);

			TickHook->Init();
		}


		// Hack-mark the world as having begun play (when it has not)
		ReturnVal->bBegunPlay = true;

		// Hack-mark the world as having initialized actors (to allow RPC hooks)
		ReturnVal->bActorsInitialized = true;

		// Enable pause, using the PlayerController of the primary world (unless we're in the editor)
		if (!GIsEditor)
		{
			AWorldSettings* CurSettings = ReturnVal->GetWorldSettings();

			if (CurSettings != NULL)
			{
				ULocalPlayer* PrimLocPlayer = GEngine->GetFirstGamePlayer(NUTUtil::GetPrimaryWorld());
				APlayerController* PrimPC = (PrimLocPlayer != NULL ? PrimLocPlayer->PlayerController : NULL);
				APlayerState* PrimState = (PrimPC != NULL ? PrimPC->PlayerState : NULL);

				if (PrimState != NULL)
				{
					CurSettings->Pauser = PrimState;
				}
			}
		}

		// Create a blank world context, to prevent crashes
		FWorldContext& CurContext = GEngine->CreateNewWorldContext(EWorldType::None);
		CurContext.SetCurrentWorld(ReturnVal);
	}

	return ReturnVal;
}

void NUTNet::MarkUnitTestWorldForCleanup(UWorld* CleanupWorld, bool bImmediate/*=false*/)
{
	UnitTestWorlds.Remove(CleanupWorld);
	PendingUnitWorldCleanup.Add(CleanupWorld);

	if (!bImmediate)
	{
		GEngine->DeferredCommands.AddUnique(TEXT("CleanupUnitTestWorlds"));
	}
	else
	{
		CleanupUnitTestWorlds();
	}
}

void NUTNet::CleanupUnitTestWorlds()
{
	for (auto CleanupIt=PendingUnitWorldCleanup.CreateIterator(); CleanupIt; ++CleanupIt)
	{
		UWorld* CurWorld = *CleanupIt;

		// Iterate all ActorComponents in this world, and unmark them as having begun play - to prevent a crash during GC
		for (TActorIterator<AActor> ActorIt(CurWorld); ActorIt; ++ActorIt)
		{
			for (UActorComponent* CurComp : ActorIt->GetComponents())
			{
				if (CurComp->HasBegunPlay())
				{
					// Big hack - call only the parent class UActorComponent::EndPlay function, such that only bHasBegunPlay is unset
					bool bBeginDestroyed = CurComp->HasAnyFlags(RF_BeginDestroyed);

					CurComp->SetFlags(RF_BeginDestroyed);

					CurComp->UActorComponent::EndPlay(EEndPlayReason::Quit);

					if (!bBeginDestroyed)
					{
						CurComp->ClearFlags(RF_BeginDestroyed);
					}
				}
			}
		}

		// Remove the tick-hook, for this world
		int32 TickHookIdx = ActiveTickHooks.IndexOfByPredicate(
			[&CurWorld](const FWorldTickHook* CurTickHook)
			{
				return CurTickHook != NULL && CurTickHook->AttachedWorld == CurWorld;
			});

		if (TickHookIdx != INDEX_NONE)
		{
			ActiveTickHooks.RemoveAt(TickHookIdx);
		}

		GEngine->DestroyWorldContext(CurWorld);
		CurWorld->DestroyWorld(false);
	}

	PendingUnitWorldCleanup.Empty();


	// Immediately garbage collect remaining objects, to finish net driver cleanup
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
}

bool NUTNet::IsUnitTestWorld(UWorld* InWorld)
{
	return UnitTestWorlds.Contains(InWorld);
}

