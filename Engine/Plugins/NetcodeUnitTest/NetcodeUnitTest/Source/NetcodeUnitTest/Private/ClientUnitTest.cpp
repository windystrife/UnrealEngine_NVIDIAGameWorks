// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ClientUnitTest.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformNamedPipe.h"
#include "Engine/ActorChannel.h"


#include "UnitTestManager.h"
#include "MinimalClient.h"
#include "NUTActor.h"
#include "Net/UnitTestChannel.h"
#include "UnitTestEnvironment.h"
#include "NUTUtilDebug.h"
#include "NUTUtilReflection.h"


UClass* UClientUnitTest::OnlineBeaconClass = FindObject<UClass>(ANY_PACKAGE, TEXT("OnlineBeaconClient"));

// @todo #JohnBMultiFakeClient: Eventually, move >all< of the minimal/headless client handling code, into a new/separate class,
//				so that a single unit test can have multiple minimal clients on a server.
//				This would be useful for licensees, for doing load testing:
//				https://udn.unrealengine.com/questions/247014/clientserver-automation.html

/**
 * UClientUnitTest
 */

UClientUnitTest::UClientUnitTest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, UnitTestFlags(EUnitTestFlags::None)
	, MinClientFlags(EMinClientFlags::None)
	, BaseServerURL(TEXT(""))
	, BaseServerParameters(TEXT(""))
	, BaseClientURL(TEXT(""))
	, BaseClientParameters(TEXT(""))
	, AllowedClientActors()
	, AllowedClientRPCs()
	, ServerHandle(nullptr)
	, ServerAddress(TEXT(""))
	, BeaconAddress(TEXT(""))
	, ClientHandle(nullptr)
	, bBlockingServerDelay(false)
	, bBlockingClientDelay(false)
	, bBlockingFakeClientDelay(false)
	, NextBlockingTimeout(0.0)
	, MinClient(nullptr)
	, bTriggerredInitialConnect(false)
	, UnitPC(nullptr)
	, bUnitPawnSetup(false)
	, bUnitPlayerStateSetup(false)
	, UnitNUTActor(nullptr)
	, bUnitNUTActorSetup(false)
	, UnitBeacon(nullptr)
	, bReceivedPong(false)
	, bPendingNetworkFailure(false)
	, bDetectedMCPOnline(false)
	, bSentBunch(false)
{
}


void UClientUnitTest::NotifyMinClientConnected()
{
	if (HasAllRequirements())
	{
		ResetTimeout(TEXT("ExecuteClientUnitTest (NotifyMinClientConnected)"));
		ExecuteClientUnitTest();
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePing))
	{
		SendNUTControl(ENUTControlCommand::Ping, TEXT(""));
	}
}

void UClientUnitTest::NotifyControlMessage(FInBunch& Bunch, uint8 MessageType)
{
	if (!!(UnitTestFlags & EUnitTestFlags::DumpControlMessages))
	{
		UNIT_LOG(ELogType::StatusDebug, TEXT("NotifyControlMessage: MessageType: %i (%s), Data Length: %i (%i), Raw Data:"), MessageType,
					(FNetControlMessageInfo::IsRegistered(MessageType) ? FNetControlMessageInfo::GetName(MessageType) :
					TEXT("UNKNOWN")), Bunch.GetBytesLeft(), Bunch.GetBitsLeft());

		if (!Bunch.IsError() && Bunch.GetBitsLeft() > 0)
		{
			UNIT_LOG_BEGIN(this, ELogType::StatusDebug | ELogType::StyleMonospace);
			NUTDebug::LogHexDump(Bunch.GetDataPosChecked(), Bunch.GetBytesLeft(), true, true);
			UNIT_LOG_END();
		}
	}
}

void UClientUnitTest::NotifyHandleClientPlayer(APlayerController* PC, UNetConnection* Connection)
{
	UnitPC = PC;

	UnitEnv->HandleClientPlayer(UnitTestFlags, PC);

	ResetTimeout(TEXT("NotifyHandleClientPlayer"));

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePlayerController) && HasAllRequirements())
	{
		ResetTimeout(TEXT("ExecuteClientUnitTest (NotifyHandleClientPlayer)"));
		ExecuteClientUnitTest();
	}
}

void UClientUnitTest::NotifyAllowNetActor(UClass* ActorClass, bool bActorChannel, bool& bBlockActor)
{
	if (!!(UnitTestFlags & EUnitTestFlags::RequireNUTActor) && ActorClass == ANUTActor::StaticClass() && UnitNUTActor == nullptr)
	{
		bBlockActor = false;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::AcceptPlayerController) && ActorClass->IsChildOf(APlayerController::StaticClass()) &&
		UnitPC == nullptr)
	{
		bBlockActor = false;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePawn) && ActorClass->IsChildOf(ACharacter::StaticClass()) &&
		(!UnitPC.IsValid() || UnitPC->GetCharacter() == nullptr))
	{
		bBlockActor = false;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePlayerState) && ActorClass->IsChildOf(APlayerState::StaticClass()) &&
		(!UnitPC.IsValid() || UnitPC->PlayerState == nullptr))
	{
		bBlockActor = false;
	}

	check(OnlineBeaconClass != nullptr);

	if (!!(UnitTestFlags & EUnitTestFlags::RequireBeacon) && ActorClass->IsChildOf(OnlineBeaconClass) && UnitBeacon == nullptr)
	{
		bBlockActor = false;
	}

	// @todo #JohnB: Move to minimal client, similar to how AllowClientRPCs was moved? (not needed yet...)
	if (bBlockActor && AllowedClientActors.Num() > 0)
	{
		const auto CheckIsChildOf =
			[&](const UClass* CurEntry)
			{
				return ActorClass->IsChildOf(CurEntry);
			};

		// Use 'ContainsByPredicate' as iterator
		bBlockActor = !AllowedClientActors.ContainsByPredicate(CheckIsChildOf);
	}
}

void UClientUnitTest::NotifyNetActor(UActorChannel* ActorChannel, AActor* Actor)
{
	if (!UnitNUTActor.IsValid())
	{
		// Set this even if not required, as it's needed for some UI elements to function
		UnitNUTActor = Cast<ANUTActor>(Actor);

		if (UnitNUTActor.IsValid())
		{
			// NOTE: ExecuteClientUnitTest triggered for this, in UnitTick - not here.
			ResetTimeout(TEXT("NotifyNetActor - UnitNUTActor"));
		}
	}

	check(OnlineBeaconClass != nullptr);

	if (!!(UnitTestFlags & EUnitTestFlags::BeaconConnect) && !UnitBeacon.IsValid() && Actor->IsA(OnlineBeaconClass))
	{
		UnitBeacon = Actor;

		NUTNet::HandleBeaconReplicate(UnitBeacon.Get(), MinClient->GetConn());

		if (!!(UnitTestFlags & EUnitTestFlags::RequireBeacon) && HasAllRequirements())
		{
			ResetTimeout(TEXT("ExecuteClientUnitTest (NotifyNetActor - UnitBeacon)"));
			ExecuteClientUnitTest();
		}
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePawn) && !bUnitPawnSetup && UnitPC.IsValid() && Cast<ACharacter>(Actor) != nullptr &&
		UnitPC->GetCharacter() != nullptr)
	{
		bUnitPawnSetup = true;

		ResetTimeout(TEXT("NotifyNetActor - bUnitPawnSetup"));

		if (HasAllRequirements())
		{
			ResetTimeout(TEXT("ExecuteClientUnitTest (NotifyNetActor - bUnitPawnSetup)"));
			ExecuteClientUnitTest();
		}
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePlayerState) && !bUnitPlayerStateSetup && UnitPC.IsValid() &&
		Cast<APlayerState>(Actor) != nullptr && UnitPC->PlayerState != nullptr)
	{
		bUnitPlayerStateSetup = true;

		ResetTimeout(TEXT("NotifyNetActor - bUnitPlayerStateSetup"));

		if (HasAllRequirements())
		{
			ResetTimeout(TEXT("ExecuteClientUnitTest (NotifyNetActor - bUnitPlayerStateSetup)"));
			ExecuteClientUnitTest();
		}
	}
}

void UClientUnitTest::NotifyNetworkFailure(ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (!!(UnitTestFlags & EUnitTestFlags::AutoReconnect))
	{
		UNIT_LOG(ELogType::StatusImportant, TEXT("Detected fake client disconnect when AutoReconnect is enabled. Reconnecting."));

		TriggerAutoReconnect();
	}
	else
	{
		// Only process this error, if a result has not already been returned
		if (VerificationState == EUnitTestVerification::Unverified)
		{
			FString LogMsg = FString::Printf(TEXT("Got network failure of type '%s' (%s)"), ENetworkFailure::ToString(FailureType),
												*ErrorString);

			if (!(UnitTestFlags & EUnitTestFlags::IgnoreDisconnect))
			{
				if (!!(UnitTestFlags & EUnitTestFlags::ExpectDisconnect))
				{
					LogMsg += TEXT(".");

					UNIT_LOG(ELogType::StatusWarning, TEXT("%s"), *LogMsg);
					UNIT_STATUS_LOG(ELogType::StatusWarning | ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

					bPendingNetworkFailure = true;
				}
				else
				{
					LogMsg += TEXT(", marking unit test as needing update.");

					UNIT_LOG(ELogType::StatusFailure | ELogType::StyleBold, TEXT("%s"), *LogMsg);
					UNIT_STATUS_LOG(ELogType::StatusFailure | ELogType::StatusVerbose | ELogType::StyleBold, TEXT("%s"), *LogMsg);

					VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;
				}
			}
			else
			{
				LogMsg += TEXT(".");

				UNIT_LOG(ELogType::StatusWarning, TEXT("%s"), *LogMsg);
				UNIT_STATUS_LOG(ELogType::StatusWarning | ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
			}
		}

		// Shut down the fake client now (relevant for developer mode)
		if (VerificationState != EUnitTestVerification::Unverified)
		{
			CleanupMinimalClient();
		}
	}
}

void UClientUnitTest::NotifySocketSendRawPacket(void* Data, int32 Count, bool& bBlockSend)
{
	bSentBunch = !bBlockSend;
}

void UClientUnitTest::ReceivedControlBunch(FInBunch& Bunch)
{
	if (!Bunch.AtEnd())
	{
		uint8 MessageType = 0;
		Bunch << MessageType;

		if (!Bunch.IsError())
		{
			if (MessageType == NMT_NUTControl)
			{
				ENUTControlCommand CmdType;
				FString Command;
				FNetControlMessage<NMT_NUTControl>::Receive(Bunch, CmdType, Command);

				if (!!(UnitTestFlags & EUnitTestFlags::RequirePing) && !bReceivedPong && CmdType == ENUTControlCommand::Pong)
				{
					bReceivedPong = true;

					ResetTimeout(TEXT("ReceivedControlBunch - Ping"));

					if (HasAllRequirements())
					{
						ResetTimeout(TEXT("ExecuteClientUnitTest (ReceivedControlBunch - Ping)"));
						ExecuteClientUnitTest();
					}
				}
				else
				{
					NotifyNUTControl(CmdType, Command);
				}
			}
			else
			{
				NotifyControlMessage(Bunch, MessageType);
			}
		}
	}
}

void UClientUnitTest::NotifyReceiveRPC(AActor* Actor, UFunction* Function, void* Parameters, bool& bBlockRPC)
{
	FString FuncName = Function->GetName();

	// Handle detection and proper setup of the PlayerController's pawn
	if (!!(UnitTestFlags & EUnitTestFlags::RequirePawn) && !bUnitPawnSetup && UnitPC != nullptr)
	{
		if (FuncName == TEXT("ClientRestart"))
		{
			UNIT_LOG(ELogType::StatusImportant, TEXT("Got ClientRestart"));

			// Trigger the event directly here, and block execution in the original code, so we can execute code post-ProcessEvent
			Actor->UObject::ProcessEvent(Function, Parameters);


			// If the pawn is set, now execute the exploit
			if (UnitPC->GetCharacter())
			{
				bUnitPawnSetup = true;

				ResetTimeout(TEXT("bUnitPawnSetup"));

				if (HasAllRequirements())
				{
					ResetTimeout(TEXT("ExecuteClientUnitTest (bUnitPawnSetup)"));
					ExecuteClientUnitTest();
				}
			}
			// If the pawn was not set, get the server to check again
			else
			{
				FString LogMsg = TEXT("Pawn was not set, sending ServerCheckClientPossession request");

				ResetTimeout(LogMsg);
				UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);

				UnitPC->ServerCheckClientPossession();
			}


			bBlockRPC = true;
		}
		// Retries setting the pawn, which will trigger ClientRestart locally, and enters into the above code with the Pawn set
		else if (FuncName == TEXT("ClientRetryClientRestart"))
		{
			bBlockRPC = false;
		}
	}
}

void UClientUnitTest::NotifyProcessLog(TWeakPtr<FUnitTestProcess> InProcess, const TArray<FString>& InLogLines)
{
	// Get partial log messages that indicate startup progress/completion
	const TArray<FString>* ServerStartProgressLogs = nullptr;
	const TArray<FString>* ServerReadyLogs = nullptr;
	const TArray<FString>* ServerTimeoutResetLogs = nullptr;
	const TArray<FString>* ClientTimeoutResetLogs = nullptr;

	UnitEnv->GetServerProgressLogs(ServerStartProgressLogs, ServerReadyLogs, ServerTimeoutResetLogs);
	UnitEnv->GetClientProgressLogs(ClientTimeoutResetLogs);

	// Using 'ContainsByPredicate' as an iterator
	FString MatchedLine;

	const auto SearchInLogLine =
		[&](const FString& ProgressLine)
		{
			bool bFound = false;

			for (auto CurLine : InLogLines)
			{
				if (CurLine.Contains(ProgressLine))
				{
					MatchedLine = CurLine;
					bFound = true;

					break;
				}
			}

			return bFound;
		};


	if (ServerHandle.IsValid() && InProcess.HasSameObject(ServerHandle.Pin().Get()))
	{
		// If launching a server, delay joining by the fake client, until the server has fully setup, and reset the unit test timeout,
		// each time there is a server log event, that indicates progress in starting up
		if (!!(UnitTestFlags & EUnitTestFlags::LaunchServer))
		{
			UNetConnection* UnitConn = (MinClient != nullptr ? MinClient->GetConn() : nullptr);

			if (!bTriggerredInitialConnect && (UnitConn == nullptr || UnitConn->State == EConnectionState::USOCK_Pending))
			{
				if (ServerReadyLogs->ContainsByPredicate(SearchInLogLine))
				{
					// Fire off fake client connection
					if (UnitConn == nullptr)
					{
						bool bBlockingProcess = IsBlockingProcessPresent(true);

						if (bBlockingProcess)
						{
							FString LogMsg = TEXT("Detected successful server startup, delaying fake client due to blocking process.");

							UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
							UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

							bBlockingFakeClientDelay = true;
						}
						else
						{
							FString LogMsg = TEXT("Detected successful server startup, launching fake client.");

							UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
							UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

							ConnectMinimalClient();
						}
					}

					ResetTimeout(FString(TEXT("ServerReady: ")) + MatchedLine);
				}
				else if (ServerStartProgressLogs->ContainsByPredicate(SearchInLogLine))
				{
					ResetTimeout(FString(TEXT("ServerStartProgress: ")) + MatchedLine);
				}
			}

			if (ServerTimeoutResetLogs->Num() > 0)
			{
				if (ServerTimeoutResetLogs->ContainsByPredicate(SearchInLogLine))
				{
					ResetTimeout(FString(TEXT("ServerTimeoutReset: ")) + MatchedLine, true, 60);
				}
			}
		}

		if (!!(UnitTestFlags & EUnitTestFlags::RequireMCP) && !bDetectedMCPOnline)
		{
			for (FString CurLine : InLogLines)
			{
				if (CurLine.Contains(TEXT("MCP: Service status updated")) && CurLine.Contains(TEXT("-> [Connected]")))
				{
					UNIT_LOG(ELogType::StatusImportant, TEXT("Successfully detected MCP online status."));

					bDetectedMCPOnline = true;
					break;
				}
			}
		}
	}

	if (!!(UnitTestFlags & EUnitTestFlags::LaunchClient) && ClientHandle.IsValid() && InProcess.HasSameObject(ClientHandle.Pin().Get()))
	{
		if (ClientTimeoutResetLogs->Num() > 0)
		{
			if (ClientTimeoutResetLogs->ContainsByPredicate(SearchInLogLine))
			{
				ResetTimeout(FString(TEXT("ClientTimeoutReset: ")) + MatchedLine, true, 60);
			}
		}
	}

	// @todo #JohnBLowPri: Consider also, adding a way to communicate with launched clients,
	//				to reset their connection timeout upon server progress, if they fully startup before the server does
}

void UClientUnitTest::NotifyProcessFinished(TWeakPtr<FUnitTestProcess> InProcess)
{
	Super::NotifyProcessFinished(InProcess);

	if (InProcess.IsValid())
	{
		bool bServerFinished = false;
		bool bClientFinished  = false;

		if (ServerHandle.IsValid() && ServerHandle.HasSameObject(InProcess.Pin().Get()))
		{
			bServerFinished = true;
		}
		else if (ClientHandle.IsValid() && ClientHandle.HasSameObject(InProcess.Pin().Get()))
		{
			bClientFinished = true;
		}

		if (bServerFinished || bClientFinished)
		{
			bool bProcessError = false;
			FString UpdateMsg;

			// If the server just finished, cleanup the fake client
			if (bServerFinished)
			{
				FString LogMsg = TEXT("Server process has finished, cleaning up fake client.");

				UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
				UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);


				// Immediately cleanup the fake client (don't wait for end-of-life cleanup in CleanupUnitTest)
				CleanupMinimalClient();


				// If a server exit was unexpected, mark the unit test as broken
				if (!(UnitTestFlags & EUnitTestFlags::IgnoreServerCrash) && VerificationState == EUnitTestVerification::Unverified)
				{
					UpdateMsg = TEXT("Unexpected server exit, marking unit test as needing update.");
					bProcessError = true;
				}
			}

			// If a client exit was unexpected, mark the unit test as broken
			if (bClientFinished && !(UnitTestFlags & EUnitTestFlags::IgnoreClientCrash) &&
				VerificationState == EUnitTestVerification::Unverified)
			{
				UpdateMsg = TEXT("Unexpected client exit, marking unit test as needing update.");
				bProcessError = true;
			}


			// If either the client/server finished, process the error
			if (bProcessError)
			{
				UNIT_LOG(ELogType::StatusFailure | ELogType::StyleBold, TEXT("%s"), *UpdateMsg);
				UNIT_STATUS_LOG(ELogType::StatusFailure | ELogType::StatusVerbose | ELogType::StyleBold, TEXT("%s"), *UpdateMsg);

				VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;
			}
		}
	}
}

void UClientUnitTest::NotifySuspendRequest()
{
#if PLATFORM_WINDOWS
	TSharedPtr<FUnitTestProcess> CurProcess = (ServerHandle.IsValid() ? ServerHandle.Pin() : nullptr);

	if (CurProcess.IsValid())
	{
		// Suspend request
		if (CurProcess->SuspendState == ESuspendState::Active)
		{
			if (SendNUTControl(ENUTControlCommand::SuspendProcess, TEXT("")))
			{
				NotifyProcessSuspendState(ServerHandle, ESuspendState::Suspended);

				UNIT_LOG(, TEXT("Sent suspend request to server (may take time to execute, if server is still starting)."));
			}
			else
			{
				UNIT_LOG(, TEXT("Failed to send suspend request to server"));
			}
		}
		// Resume request
		else if (CurProcess->SuspendState == ESuspendState::Suspended)
		{
			// Send the resume request over a named pipe - this is the only line of communication once suspended
			FString ResumePipeName = FString::Printf(TEXT("%s%i"), NUT_SUSPEND_PIPE, CurProcess->ProcessID);
			FPlatformNamedPipe ResumePipe;

			if (ResumePipe.Create(ResumePipeName, false, false))
			{
				if (ResumePipe.IsReadyForRW())
				{
					int32 ResumeVal = 1;
					ResumePipe.WriteInt32(ResumeVal);

					UNIT_LOG(, TEXT("Sent resume request to server."));

					NotifyProcessSuspendState(ServerHandle, ESuspendState::Active);
				}
				else
				{
					UNIT_LOG(, TEXT("WARNING: Resume pipe not ready for read/write (server still starting?)."));
				}

				ResumePipe.Destroy();
			}
			else
			{
				UNIT_LOG(, TEXT("Failed to create named pipe, for sending resume request (server still starting?)."));
			}
		}
	}
#else
	UNIT_LOG(ELogType::StatusImportant, TEXT("Suspend/Resume is only supported in Windows."));
#endif
}

void UClientUnitTest::NotifyProcessSuspendState(TWeakPtr<FUnitTestProcess> InProcess, ESuspendState InSuspendState)
{
	Super::NotifyProcessSuspendState(InProcess, InSuspendState);

	if (InProcess == ServerHandle)
	{
		OnSuspendStateChange.ExecuteIfBound(InSuspendState);
	}
}


bool UClientUnitTest::NotifyConsoleCommandRequest(FString CommandContext, FString Command)
{
	bool bHandled = Super::NotifyConsoleCommandRequest(CommandContext, Command);

	if (!bHandled)
	{
		if (CommandContext == TEXT("Local"))
		{
			UNIT_LOG_BEGIN(this, ELogType::OriginConsole);
			bHandled = GEngine->Exec((MinClient != nullptr ? MinClient->GetUnitWorld() : nullptr), *Command, *GLog);
			UNIT_LOG_END();
		}
		else if (CommandContext == TEXT("Server"))
		{
			// @todo #JohnBBug: Perhaps add extra checks here, to be sure we're ready to send console commands?
			//
			//				UPDATE: Yes, this is a good idea, because if the client hasn't gotten to the correct login stage
			//				(NMT_Join or such, need to check when server rejects non-login control commands),
			//				then it leads to an early disconnect when you try to spam-send a command early.
			//
			//				It's easy to test this, just type in a command before join, and hold down enter on the edit box to spam it.

			if (SendNUTControl(ENUTControlCommand::Command_NoResult, Command))
			{
				UNIT_LOG(ELogType::OriginConsole, TEXT("Sent command '%s' to server."), *Command);

				bHandled = true;
			}
			else
			{
				UNIT_LOG(ELogType::OriginConsole, TEXT("Failed to send console command '%s' to server."), *Command);
			}
		}
		else if (CommandContext == TEXT("Client"))
		{
			// @todo #JohnBFeature

			UNIT_LOG(ELogType::OriginConsole, TEXT("Client console commands not yet implemented"));
		}
	}

	return bHandled;
}

void UClientUnitTest::GetCommandContextList(TArray<TSharedPtr<FString>>& OutList, FString& OutDefaultContext)
{
	Super::GetCommandContextList(OutList, OutDefaultContext);

	OutList.Add(MakeShareable(new FString(TEXT("Local"))));

	if (!!(UnitTestFlags & EUnitTestFlags::LaunchServer))
	{
		OutList.Add(MakeShareable(new FString(TEXT("Server"))));
	}

	if (!!(UnitTestFlags & EUnitTestFlags::LaunchClient))
	{
		OutList.Add(MakeShareable(new FString(TEXT("Client"))));
	}

	OutDefaultContext = TEXT("Local");
}


bool UClientUnitTest::SendNUTControl(ENUTControlCommand CommandType, FString Command)
{
	bool bSuccess = false;

	FOutBunch* ControlChanBunch = MinClient->CreateChannelBunch(CHTYPE_Control, 0);

	if (ControlChanBunch != nullptr)
	{
		uint8 ControlMsg = NMT_NUTControl;
		ENUTControlCommand CmdType = CommandType;

		*ControlChanBunch << ControlMsg;
		*ControlChanBunch << CmdType;
		*ControlChanBunch << Command;

		bSuccess = MinClient->SendControlBunch(ControlChanBunch);
	}
	else
	{
		FString LogMsg = TEXT("Failed to create control channel bunch.");

		UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
	}

	return bSuccess;
}

bool UClientUnitTest::SendRPCChecked(UObject* Target, const TCHAR* FunctionName, void* Parms, int16 ParmsSize,
										int16 ParmsSizeCorrection/*=0*/)
{
	bool bSuccess = false;
	UFunction* TargetFunc = Target->FindFunction(FName(FunctionName));

	PreSendRPC();

	if (TargetFunc != nullptr)
	{
		if (TargetFunc->ParmsSize == ParmsSize + ParmsSizeCorrection)
		{
			if (MinClient->GetConn()->IsNetReady(false))
			{
				Target->ProcessEvent(TargetFunc, Parms);
			}
			else
			{
				UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to send RPC '%s', network saturated."), FunctionName);
			}
		}
		else
		{
			UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to send RPC '%s', mismatched parameters: '%i' vs '%i' (%i - %i)."),
						FunctionName, TargetFunc->ParmsSize, ParmsSize + ParmsSizeCorrection, ParmsSize, -ParmsSizeCorrection);
		}
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to send RPC, could not find RPC: %s"), FunctionName);
	}

	bSuccess = PostSendRPC(FunctionName, Target);

	return bSuccess;
}

bool UClientUnitTest::SendRPCChecked(UObject* Target, FFuncReflection& FuncRefl)
{
	bool bSuccess = false;

	if (FuncRefl.IsValid())
	{
		bSuccess = SendRPCChecked(Target, *FuncRefl.Function->GetName(), FuncRefl.GetParms(), FuncRefl.Function->ParmsSize);
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to send RPC '%s', function reflection failed."), FuncRefl.FunctionName);
	}

	return bSuccess;
}

bool UClientUnitTest::SendUnitRPCChecked_Internal(UObject* Target, FString RPCName)
{
	bool bSuccess = false;

	PreSendRPC();

	if (UnitNUTActor.IsValid())
	{
		UnitNUTActor->ExecuteOnServer(Target, RPCName);
	}
	else
	{
		const TCHAR* LogMsg = TEXT("SendUnitRPCChecked: UnitNUTActor not set.");

		UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), LogMsg);
	}

	bSuccess = PostSendRPC(RPCName, UnitNUTActor.Get());

	return bSuccess;
}

void UClientUnitTest::PreSendRPC()
{
	// Flush before and after, so no queued data is counted as a send, and so that the queued RPC is immediately sent and detected
	MinClient->GetConn()->FlushNet();

	bSentBunch = false;
}

bool UClientUnitTest::PostSendRPC(FString RPCName, UObject* Target/*=nullptr*/)
{
	bool bSuccess = false;
	UActorComponent* TargetComponent = Cast<UActorComponent>(Target);
	AActor* TargetActor = (TargetComponent != nullptr ? TargetComponent->GetOwner() : Cast<AActor>(Target));
	UNetConnection* UnitConn = MinClient->GetConn();
	UChannel* TargetChan = UnitConn->ActorChannels.FindRef(TargetActor);

	UnitConn->FlushNet();

	// Just hack-erase bunch overflow tracking for this actors channel
	if (TargetChan != nullptr)
	{
		TargetChan->NumOutRec = 0;
	}

	// If sending failed, trigger an overall unit test failure
	if (!bSentBunch)
	{
		FString LogMsg = FString::Printf(TEXT("Failed to send RPC '%s', unit test needs update."), *RPCName);

		// If specific/known failure cases are encountered, append them to the log message, to aid debugging
		// (try to enumerate all possible cases)
		if (TargetActor != nullptr)
		{
			FString LogAppend = TEXT("");
			UWorld* TargetWorld = TargetActor->GetWorld();

			if (IsGarbageCollecting())
			{
				LogAppend += TEXT(", IsGarbageCollecting() returned TRUE");
			}

			if (TargetWorld == nullptr)
			{
				LogAppend += TEXT(", TargetWorld == nullptr");
			}
			else if (!TargetWorld->AreActorsInitialized() && !GAllowActorScriptExecutionInEditor)
			{
				LogAppend += TEXT(", AreActorsInitialized() returned FALSE");
			}

			if (TargetActor->IsPendingKill())
			{
				LogAppend += TEXT(", IsPendingKill() returned TRUE");
			}

			UFunction* TargetFunc = TargetActor->FindFunction(FName(*RPCName));

			if (TargetFunc == nullptr)
			{
				LogAppend += TEXT(", TargetFunc == nullptr");
			}
			else
			{
				int32 Callspace = TargetActor->GetFunctionCallspace(TargetFunc, nullptr, nullptr);

				if (!(Callspace & FunctionCallspace::Remote))
				{
					LogAppend += FString::Printf(TEXT(", GetFunctionCallspace() returned non-remote, value: %i (%s)"), Callspace,
													FunctionCallspace::ToString((FunctionCallspace::Type)Callspace));
				}
			}

			if (TargetActor->GetNetDriver() == nullptr)
			{
				FName TargetNetDriver = TargetActor->GetNetDriverName();

				LogAppend += FString::Printf(TEXT(", GetNetDriver() returned nullptr - NetDriverName: %s"),
												*TargetNetDriver.ToString());

				if (TargetNetDriver == NAME_GameNetDriver && TargetWorld != nullptr && TargetWorld->GetNetDriver() == nullptr)
				{
					LogAppend += FString::Printf(TEXT(", TargetWorld->GetNetDriver() returned nullptr - World: %s"),
													*TargetWorld->GetFullName());
				}
			}

			UNetConnection* TargetConn = TargetActor->GetNetConnection();

			if (TargetConn == nullptr)
			{
				LogAppend += TEXT(", GetNetConnection() returned nullptr");
			}
			else if (!TargetConn->IsNetReady(0))
			{
				LogAppend += TEXT(", IsNetReady() returned FALSE");
			}

			if (TargetChan == nullptr)
			{
				LogAppend += TEXT(", TargetChan == nullptr");
			}
			else if (TargetChan->OpenPacketId.First == INDEX_NONE)
			{
				LogAppend += TEXT(", Channel not open");
			}


			if (LogAppend.Len() > 0)
			{
				LogMsg += FString::Printf(TEXT(" (%s)"), *LogAppend.Mid(2));
			}
		}

		UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

		VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;

		bSuccess = false;
	}
	else
	{
		bSuccess = true;
	}

	return bSuccess;
}

void UClientUnitTest::SendGenericExploitFailLog()
{
	SendNUTControl(ENUTControlCommand::Command_NoResult, GetGenericExploitFailLog());
}

bool UClientUnitTest::ValidateUnitTestSettings(bool bCDOCheck/*=false*/)
{
	bool bSuccess = Super::ValidateUnitTestSettings();

	ValidateUnitFlags(UnitTestFlags, MinClientFlags);


	// Validate the rest of the flags which cross-check against non-flag variables

	// If launching a server, make sure the base URL for the server is set
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::LaunchServer) || BaseServerURL.Len() > 0);

	// If launching a client, make sure some default client parameters have been set (to avoid e.g. launching fullscreen)
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::LaunchClient) || BaseClientParameters.Len() > 0);


	// You can't specify an allowed actors whitelist, without the AcceptActors flag
	UNIT_ASSERT(AllowedClientActors.Num() == 0 || !!(MinClientFlags & EMinClientFlags::AcceptActors));

#if UE_BUILD_SHIPPING
	// You can't hook ProcessEvent or block RPCs in shipping builds, as the main engine hook is not available in shipping; soft-fail
	if (!!(UnitTestFlags & EUnitTestFlags::NotifyProcessEvent) || !(MinClientFlags & EMinClientFlags::NotifyProcessNetEvent))
	{
		UNIT_LOG(ELogType::StatusFailure | ELogType::StyleBold, TEXT("Unit tests run in shipping mode, can't hook ProcessEvent."));

		bSuccess = false;
	}
#endif

	// If the ping requirements flag is set, it should be the ONLY one set
	//	(which means only one bit should be set, and one bit means it should be power-of-two)
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequirePing) ||
					FMath::IsPowerOfTwo((uint32)(UnitTestFlags & EUnitTestFlags::RequirementsMask)));

	// If you require a pawn, validate the existence of certain RPC's that are needed for pawn setup and verification
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::RequirePawn) || (
				GetDefault<APlayerController>()->FindFunction(FName(TEXT("ClientRestart"))) != nullptr &&
				GetDefault<APlayerController>()->FindFunction(FName(TEXT("ClientRetryClientRestart"))) != nullptr));

	// If connecting to a beacon, you must specify the beacon type
	UNIT_ASSERT(!(UnitTestFlags & EUnitTestFlags::BeaconConnect) || ServerBeaconType.Len() > 0);


	// Don't accept any 'Ignore' flags, once the unit test is finalized (they're debug only - all crashes must be handled in final code)
	UNIT_ASSERT(bWorkInProgress || !(UnitTestFlags & (EUnitTestFlags::IgnoreServerCrash | EUnitTestFlags::IgnoreClientCrash |
				EUnitTestFlags::IgnoreDisconnect)));


	return bSuccess;
}

EUnitTestFlags UClientUnitTest::GetMetRequirements()
{
	EUnitTestFlags ReturnVal = EUnitTestFlags::None;

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePlayerController) && UnitPC != nullptr)
	{
		ReturnVal |= EUnitTestFlags::RequirePlayerController;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePawn) && UnitPC != nullptr && UnitPC->GetCharacter() != nullptr && bUnitPawnSetup)
	{
		ReturnVal |= EUnitTestFlags::RequirePawn;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePlayerState) && UnitPC != nullptr && UnitPC->PlayerState != nullptr &&
		bUnitPlayerStateSetup)
	{
		ReturnVal |= EUnitTestFlags::RequirePlayerState;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequirePing) && bReceivedPong)
	{
		ReturnVal |= EUnitTestFlags::RequirePing;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequireNUTActor) && UnitNUTActor.IsValid() && bUnitNUTActorSetup)
	{
		ReturnVal |= EUnitTestFlags::RequireNUTActor;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequireBeacon) && UnitBeacon.IsValid())
	{
		ReturnVal |= EUnitTestFlags::RequireBeacon;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequireMCP) && bDetectedMCPOnline)
	{
		ReturnVal |= EUnitTestFlags::RequireMCP;
	}

	// ExecuteClientUnitTest should be triggered manually - unless you override HasAllCustomRequirements
	if (!!(UnitTestFlags & EUnitTestFlags::RequireCustom) && HasAllCustomRequirements())
	{
		ReturnVal |= EUnitTestFlags::RequireCustom;
	}

	return ReturnVal;
}

bool UClientUnitTest::HasAllRequirements(bool bIgnoreCustom/*=false*/)
{
	bool bReturnVal = true;

	// The fake client creation/connection is now delayed, so need to wait for that too
	if (MinClient == nullptr || !MinClient->IsConnected())
	{
		bReturnVal = false;
	}

	EUnitTestFlags RequiredFlags = (UnitTestFlags & EUnitTestFlags::RequirementsMask);

	if (bIgnoreCustom)
	{
		RequiredFlags &= ~EUnitTestFlags::RequireCustom;
	}

	if ((RequiredFlags & GetMetRequirements()) != RequiredFlags)
	{
		bReturnVal = false;
	}

	return bReturnVal;
}

ELogType UClientUnitTest::GetExpectedLogTypes()
{
	ELogType ReturnVal = Super::GetExpectedLogTypes();

	if (!!(UnitTestFlags & EUnitTestFlags::LaunchServer))
	{
		ReturnVal |= ELogType::Server;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::LaunchClient))
	{
		ReturnVal |= ELogType::Client;
	}

	if (!!(UnitTestFlags & EUnitTestFlags::DumpControlMessages))
	{
		ReturnVal |= ELogType::StatusDebug;
	}

	if (!!(MinClientFlags & (EMinClientFlags::DumpReceivedRaw | EMinClientFlags::DumpSendRaw)))
	{
		ReturnVal |= ELogType::StatusDebug;
	}

	return ReturnVal;
}

void UClientUnitTest::ResetTimeout(FString ResetReason, bool bResetConnTimeout/*=false*/, uint32 MinDuration/*=0*/)
{
	// Extend the timeout to at least two minutes, if a crash is expected, as sometimes crash dumps take a very long time
	if (!!(UnitTestFlags & EUnitTestFlags::ExpectServerCrash) &&
			(ResetReason.Contains("ExecuteClientUnitTest") || ResetReason.Contains("Detected crash.")))
	{
		MinDuration = FMath::Max<uint32>(MinDuration, 120);
		bResetConnTimeout = true;
	}

	Super::ResetTimeout(ResetReason, bResetConnTimeout, MinDuration);

	if (bResetConnTimeout)
	{
		ResetConnTimeout((float)(FMath::Max(MinDuration, UnitTestTimeout)));
	}
}

void UClientUnitTest::ResetConnTimeout(float Duration)
{
	UNetConnection* UnitConn = (MinClient != nullptr ? MinClient->GetConn() : nullptr);
	UNetDriver* UnitDriver = (UnitConn != nullptr ? UnitConn->Driver : nullptr);

	if (UnitConn != nullptr && UnitConn->State != USOCK_Closed && UnitDriver != nullptr)
	{
		// @todo #JohnBHack: This is a slightly hacky way of setting the timeout to a large value, which will be overridden by newly
		//				received packets, making it unsuitable for most situations (except crashes - but that could still be subject
		//				to a race condition)
		double NewLastReceiveTime = UnitDriver->Time + Duration;

		UnitConn->LastReceiveTime = FMath::Max(NewLastReceiveTime, UnitConn->LastReceiveTime);
	}
}


bool UClientUnitTest::ExecuteUnitTest()
{
	bool bSuccess = ValidateUnitTestSettings();

	if (bSuccess)
	{
		if (!!(UnitTestFlags & EUnitTestFlags::LaunchServer))
		{
			bool bBlockingProcess = IsBlockingProcessPresent(true);

			if (bBlockingProcess)
			{
				FString LogMsg = TEXT("Delaying server startup due to blocking process");

				UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
				UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

				bBlockingServerDelay = true;
			}
			else
			{
				StartUnitTestServer();
			}

			if (!!(UnitTestFlags & EUnitTestFlags::LaunchClient))
			{
				if (bBlockingProcess)
				{
					FString LogMsg = TEXT("Delaying client startup due to blocking process");

					UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
					UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

					bBlockingClientDelay = true;
				}
				else
				{
					// Client handle is set outside of StartUnitTestClient, in case support for multiple clients is added later
					ClientHandle = StartUnitTestClient(ServerAddress);
				}
			}
		}
	}
	else
	{
		FString LogMsg = TEXT("Failed to validate unit test settings/environment");

		UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
	}

	return bSuccess;
}

void UClientUnitTest::CleanupUnitTest()
{
	if (MinClient != nullptr)
	{
		FProcessEventHook::Get().RemoveEventHook(MinClient->GetUnitWorld());
	}

	CleanupMinimalClient();

	Super::CleanupUnitTest();
}

bool UClientUnitTest::ConnectMinimalClient(const TCHAR* InNetID/*=nullptr*/)
{
	bool bSuccess = false;
	FMinClientHooks Hooks;

	check(MinClient == nullptr);

	Hooks.ConnectedDel = FOnMinClientConnected::CreateUObject(this, &UClientUnitTest::NotifyMinClientConnected);
	Hooks.NetworkFailureDel = FOnMinClientNetworkFailure::CreateUObject(this, &UClientUnitTest::NotifyNetworkFailure);
#if !UE_BUILD_SHIPPING
	Hooks.SendRPCDel = FOnSendRPC::CreateUObject(this, &UClientUnitTest::NotifySendRPC);
#endif
	Hooks.ReceivedControlBunchDel = FOnMinClientReceivedControlBunch::CreateUObject(this, &UClientUnitTest::ReceivedControlBunch);
	Hooks.RepActorSpawnDel = FOnMinClientRepActorSpawn::CreateUObject(this, &UClientUnitTest::NotifyAllowNetActor);
	Hooks.HandleClientPlayerDel = FOnHandleClientPlayer::CreateUObject(this, &UClientUnitTest::NotifyHandleClientPlayer);

	if (!!(UnitTestFlags & EUnitTestFlags::CaptureReceivedRaw))
	{
		Hooks.ReceivedRawPacketDel = FOnMinClientReceivedRawPacket::CreateUObject(this, &UClientUnitTest::NotifyReceivedRawPacket);
	}

#if !UE_BUILD_SHIPPING
	Hooks.LowLevelSendDel.BindUObject(this, &UClientUnitTest::NotifySocketSendRawPacket);
#endif


	EMinClientFlags CurMinClientFlags = FromUnitTestFlags(UnitTestFlags) | MinClientFlags;

	if (!!(CurMinClientFlags & EMinClientFlags::NotifyNetActors))
	{
		Hooks.NetActorDel.BindUObject(this, &UClientUnitTest::NotifyNetActor);
	}


	Hooks.ReceiveRPCDel.BindUObject(this, &UClientUnitTest::NotifyReceiveRPC);


	FMinClientParms Parms;

	Parms.MinClientFlags = CurMinClientFlags;
	Parms.Owner = this;
	Parms.ServerAddress = ServerAddress;
	Parms.BeaconAddress = BeaconAddress;
	Parms.BeaconType = ServerBeaconType;

	if (InNetID != nullptr)
	{
		Parms.JoinUID = InNetID;
	}

	Parms.AllowedClientRPCs = AllowedClientRPCs;

	MinClient = NewObject<UMinimalClient>();
	bSuccess = MinClient->Connect(Parms, Hooks);

	if (bSuccess)
	{
		if (!!(UnitTestFlags & EUnitTestFlags::NotifyProcessEvent))
		{
#if !UE_BUILD_SHIPPING
			FProcessEventHook::Get().AddEventHook(MinClient->GetUnitWorld(),
				FOnProcessNetEvent::CreateUObject(this, &UClientUnitTest::NotifyProcessEvent));
#else
			FString LogMsg = TEXT("Require ProcessEvent hook, but current build configuration does not support it.");

			UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

			bSuccess = false;
#endif
		}

		bTriggerredInitialConnect = true;
	}
	else
	{
		FString LogMsg = TEXT("Failed to connect minimal client.");

		UNIT_LOG(ELogType::StatusFailure, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
	}

	return bSuccess;
}

void UClientUnitTest::CleanupMinimalClient()
{
	if (MinClient != nullptr)
	{
		MinClient->Cleanup();
	}

	UnitPC = nullptr;
	bUnitPawnSetup = false;
	bUnitPlayerStateSetup = false;
	UnitNUTActor = nullptr;
	bUnitNUTActorSetup = false;
	UnitBeacon = nullptr;
	bReceivedPong = false;
	bPendingNetworkFailure = false;
}

void UClientUnitTest::TriggerAutoReconnect()
{
	UNIT_LOG(ELogType::StatusImportant, TEXT("Performing Auto-Reconnect."))

	CleanupMinimalClient();
	ConnectMinimalClient();
}


void UClientUnitTest::StartUnitTestServer()
{
	if (!ServerHandle.IsValid())
	{
		FString LogMsg = TEXT("Unit test launching a server");

		UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

		// Determine the new server port
		int DefaultPort = 0;
		GConfig->GetInt(TEXT("URL"), TEXT("Port"), DefaultPort, GEngineIni);

		// Increment the server port used by 10, for every unit test
		static int ServerPortOffset = 0;
		int ServerPort = DefaultPort + 50 + (++ServerPortOffset * 10);
		int ServerBeaconPort = ServerPort + 5;


		// Setup the launch URL
		FString ServerParameters = ConstructServerParameters() + FString::Printf(TEXT(" -Port=%i"), ServerPort);

		if (!!(UnitTestFlags & EUnitTestFlags::BeaconConnect))
		{
			ServerParameters += FString::Printf(TEXT(" -BeaconPort=%i"), ServerBeaconPort);
		}

		ServerHandle = StartUE4UnitTestProcess(ServerParameters);

		if (ServerHandle.IsValid())
		{
			ServerAddress = FString::Printf(TEXT("127.0.0.1:%i"), ServerPort);

			if (!!(UnitTestFlags & EUnitTestFlags::BeaconConnect))
			{
				BeaconAddress = FString::Printf(TEXT("127.0.0.1:%i"), ServerBeaconPort);
			}

			auto CurHandle = ServerHandle.Pin();

			CurHandle->ProcessTag = FString::Printf(TEXT("UE4_Server_%i"), CurHandle->ProcessID);
			CurHandle->BaseLogType = ELogType::Server;
			CurHandle->LogPrefix = TEXT("[SERVER]");
			CurHandle->MainLogColor = COLOR_CYAN;
			CurHandle->SlateLogColor = FLinearColor(0.f, 1.f, 1.f);
		}
	}
	else
	{
		UNIT_LOG(ELogType::StatusFailure, TEXT("ERROR: Server process already started."));
	}
}

FString UClientUnitTest::ConstructServerParameters()
{
	// Construct the server log parameter
	FString GameLogDir = FPaths::ProjectLogDir();
	FString ServerLogParam;

	if (!UnitLogDir.IsEmpty() && UnitLogDir.StartsWith(GameLogDir))
	{
		ServerLogParam = TEXT(" -Log=") + UnitLogDir.Mid(GameLogDir.Len()) + TEXT("UnitTestServer.log");
	}
	else
	{
		ServerLogParam = TEXT(" -Log=UnitTestServer.log");
	}

	// NOTE: In the absence of "-ddc=noshared", a VPN connection can cause UE4 to take a long time to startup
	// NOTE: Without '-CrashForUAT'/'-unattended' the auto-reporter can pop up
	// NOTE: Without '-UseAutoReporter' the crash report executable is launched
	// NOTE: Without '?bIsLanMatch', the Steam net driver will be active, when OnlineSubsystemSteam is in use
	FString Parameters = FString(FApp::GetProjectName()) + TEXT(" ") + BaseServerURL + TEXT("?bIsLanMatch") + TEXT(" -server ") +
							BaseServerParameters + ServerLogParam +
							TEXT(" -forcelogflush -stdout -AllowStdOutLogVerbosity -ddc=noshared") +
							TEXT(" -unattended -CrashForUAT -UseAutoReporter");

							// Removed this, to support detection of shader compilation, based on shader compiler .exe
							//TEXT(" -NoShaderWorker");

	return Parameters;
}

TWeakPtr<FUnitTestProcess> UClientUnitTest::StartUnitTestClient(FString ConnectIP, bool bMinimized/*=true*/)
{
	TWeakPtr<FUnitTestProcess> ReturnVal = nullptr;

	FString LogMsg = TEXT("Unit test launching a client");

	UNIT_LOG(ELogType::StatusImportant, TEXT("%s"), *LogMsg);
	UNIT_STATUS_LOG(ELogType::StatusVerbose, TEXT("%s"), *LogMsg);

	FString ClientParameters = ConstructClientParameters(ConnectIP);

	ReturnVal = StartUE4UnitTestProcess(ClientParameters, bMinimized);

	if (ReturnVal.IsValid())
	{
		auto CurHandle = ReturnVal.Pin();

		// @todo #JohnBMultiClient: If you add support for multiple clients, make the log prefix numbered, also try to differentiate colours
		CurHandle->ProcessTag = FString::Printf(TEXT("UE4_Client_%i"), CurHandle->ProcessID);
		CurHandle->BaseLogType = ELogType::Client;
		CurHandle->LogPrefix = TEXT("[CLIENT]");
		CurHandle->MainLogColor = COLOR_GREEN;
		CurHandle->SlateLogColor = FLinearColor(0.f, 1.f, 0.f);
	}

	return ReturnVal;
}

FString UClientUnitTest::ConstructClientParameters(FString ConnectIP)
{
	// Construct the client log parameter
	FString GameLogDir = FPaths::ProjectLogDir();
	FString ClientLogParam;

	if (!UnitLogDir.IsEmpty() && UnitLogDir.StartsWith(GameLogDir))
	{
		ClientLogParam = TEXT(" -Log=") + UnitLogDir.Mid(GameLogDir.Len()) + TEXT("UnitTestClient.log");
	}
	else
	{
		ClientLogParam = TEXT(" -Log=UnitTestClient.log");
	}

	// NOTE: In the absence of "-ddc=noshared", a VPN connection can cause UE4 to take a long time to startup
	// NOTE: Without '-CrashForUAT'/'-unattended' the auto-reporter can pop up
	// NOTE: Without '-UseAutoReporter' the crash report executable is launched
	FString Parameters = FString(FApp::GetProjectName()) + TEXT(" ") + ConnectIP + BaseClientURL + TEXT(" -game ") + BaseClientParameters +
							ClientLogParam + TEXT(" -forcelogflush -stdout -AllowStdOutLogVerbosity -ddc=noshared -nosplash") +
							TEXT(" -unattended -CrashForUAT -nosound -UseAutoReporter");

							// Removed this, to support detection of shader compilation, based on shader compiler .exe
							//TEXT(" -NoShaderWorker")

	return Parameters;
}

void UClientUnitTest::PrintUnitTestProcessErrors(TSharedPtr<FUnitTestProcess> InHandle)
{
	// If this was the server, and we were not expecting a crash, print out a warning
	if (!(UnitTestFlags & EUnitTestFlags::ExpectServerCrash) && ServerHandle.IsValid() && InHandle == ServerHandle.Pin())
	{
		FString LogMsg = TEXT("WARNING: Got server crash, but unit test not marked as expecting a server crash.");

		STATUS_SET_COLOR(FLinearColor(1.0, 1.0, 0.0));

		UNIT_LOG(ELogType::StatusWarning, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusWarning, TEXT("%s"), *LogMsg);

		STATUS_RESET_COLOR();
	}

	Super::PrintUnitTestProcessErrors(InHandle);
}

void UClientUnitTest::UnitTick(float DeltaTime)
{
	if (bBlockingServerDelay || bBlockingClientDelay || bBlockingFakeClientDelay)
	{
		bool bBlockingProcess = IsBlockingProcessPresent();

		if (!bBlockingProcess)
		{
			ResetTimeout(TEXT("Blocking Process Reset"), true, 60);

			auto IsWaitingOnTimeout =
				[&]()
				{
					return NextBlockingTimeout > FPlatformTime::Seconds();
				};

			if (bBlockingServerDelay && !IsWaitingOnTimeout())
			{
				StartUnitTestServer();

				bBlockingServerDelay = false;
				NextBlockingTimeout = FPlatformTime::Seconds() + 10.0;
			}

			if (bBlockingClientDelay && !IsWaitingOnTimeout())
			{
				ClientHandle = StartUnitTestClient(ServerAddress);

				bBlockingClientDelay = false;
				NextBlockingTimeout = FPlatformTime::Seconds() + 10.0;
			}

			if (bBlockingFakeClientDelay && !IsWaitingOnTimeout())
			{
				ConnectMinimalClient();

				bTriggerredInitialConnect = true;
				bBlockingFakeClientDelay = false;
				NextBlockingTimeout = FPlatformTime::Seconds() + 10.0;
			}
		}
	}

	if (MinClient != nullptr && MinClient->IsTickable())
	{
		MinClient->UnitTick(DeltaTime);
	}

	if (!!(UnitTestFlags & EUnitTestFlags::RequireNUTActor) && !bUnitNUTActorSetup && UnitNUTActor.IsValid() &&
		(!!(UnitTestFlags & EUnitTestFlags::RequireBeacon) || UnitNUTActor->GetOwner() != nullptr))
	{
		bUnitNUTActorSetup = true;

		if (HasAllRequirements())
		{
			ResetTimeout(TEXT("ExecuteClientUnitTest (bUnitNUTActorSetup)"));
			ExecuteClientUnitTest();
		}
	}

	// Prevent net connection timeout in developer mode
	if (bDeveloperMode)
	{
		ResetConnTimeout(120.f);
	}

	Super::UnitTick(DeltaTime);

	// After there has been a chance to process remaining server output, finish handling the pending disconnect
	if (VerificationState == EUnitTestVerification::Unverified && bPendingNetworkFailure)
	{
		const TCHAR* LogMsg = TEXT("Handling pending disconnect, marking unit test as needing update.");

		UNIT_LOG(ELogType::StatusFailure | ELogType::StyleBold, TEXT("%s"), LogMsg);
		UNIT_STATUS_LOG(ELogType::StatusFailure | ELogType::StatusVerbose | ELogType::StyleBold, TEXT("%s"), LogMsg);

		VerificationState = EUnitTestVerification::VerifiedNeedsUpdate;

		bPendingNetworkFailure = false;
	}
}

bool UClientUnitTest::IsTickable() const
{
	bool bReturnVal = Super::IsTickable();

	bReturnVal = bReturnVal || bDeveloperMode || bBlockingServerDelay || bBlockingClientDelay || bBlockingFakeClientDelay ||
					(MinClient != nullptr && MinClient->IsTickable()) || bPendingNetworkFailure;

	return bReturnVal;
}

void UClientUnitTest::LogComplete()
{
	Super::LogComplete();

	if (!HasAllRequirements())
	{
		EUnitTestFlags UnmetRequirements = EUnitTestFlags::RequirementsMask & UnitTestFlags & ~(GetMetRequirements());
		EUnitTestFlags CurRequirement = (EUnitTestFlags)1;
		FString UnmetStr;

		while (UnmetRequirements != EUnitTestFlags::None)
		{
			if (!!(CurRequirement & UnmetRequirements))
			{
				if (UnmetStr != TEXT(""))
				{
					UnmetStr += TEXT(", ");
				}

				UnmetStr += GetUnitTestFlagName(CurRequirement);

				UnmetRequirements &= ~CurRequirement;
			}

			CurRequirement = (EUnitTestFlags)((uint32)CurRequirement << 1);
		}

		UNIT_LOG(ELogType::StatusFailure, TEXT("Failed to meet unit test requirements: %s"), *UnmetStr);
	}
}

