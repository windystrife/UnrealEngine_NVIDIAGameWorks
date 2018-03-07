// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MinimalClient.h"

#include "Misc/FeedbackContext.h"
#include "Misc/NetworkVersion.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/ActorChannel.h"
#include "Net/DataChannel.h"

#include "ClientUnitTest.h"
#include "NetcodeUnitTest.h"
#include "UnitTestEnvironment.h"
#include "UnitTestManager.h"
#include "NUTUtilDebug.h"
#include "NUTUtilReflection.h"
#include "Net/NUTUtilNet.h"
#include "Net/UnitTestPackageMap.h"
#include "Net/UnitTestChannel.h"
#include "Net/UnitTestActorChannel.h"


// @todo #JohnB: Eventually, you want to remove this classes dependence on the unit test code, and have it be independent.
//					For now though, just allow the dependence, to ease the transition


/**
 * FMinClientParms
 */
void FMinClientParms::ValidateParms()
{
	ValidateMinFlags(MinClientFlags);


	// Validate the rest of the flags which cross-check against non-flag variables, or otherwise should be runtime-only checks

	// You can't whitelist client RPC's (i.e. unblock whitelisted RPC's), unless all RPC's are blocked by default
	UNIT_ASSERT(!(MinClientFlags & EMinClientFlags::AcceptRPCs) || AllowedClientRPCs.Num() == 0);

#if UE_BUILD_SHIPPING
	// Rejecting actors requires non-shipping mode
	UNIT_ASSERT(!!(MinClientFlags & EMinClientFlags::AcceptActors));
#endif
}


/**
 * UMinimalClient
 */

UMinimalClient::UMinimalClient(const FObjectInitializer& ObjectInitializor)
	: Super(ObjectInitializor)
	, FProtMinClientParms()
	, FProtMinClientHooks()
	, bConnected(false)
	, UnitWorld(nullptr)
	, UnitNetDriver(nullptr)
	, UnitConn(nullptr)
	, PendingNetActorChans()
{
}

bool UMinimalClient::Connect(FMinClientParms Parms, FMinClientHooks Hooks)
{
	bool bSuccess = false;

	// Make sure we're not already setup
	check(Hooks.ConnectedDel.IsBound());
	check(MinClientFlags == EMinClientFlags::None && !ConnectedDel.IsBound());

	Parms.ValidateParms();

	Parms.CopyParms(*this);
	Hooks.CopyHooks(*this);

	if (UnitWorld == nullptr)
	{
		// Make all of this happen in a blank, newly constructed world
		UnitWorld = NUTNet::CreateUnitTestWorld();

		if (UnitWorld != nullptr)
		{
			if (ConnectMinimalClient())
			{
				if (GEngine != nullptr)
				{
#if TARGET_UE4_CL >= CL_DEPRECATEDEL
					InternalNotifyNetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this,
																	&UMinimalClient::InternalNotifyNetworkFailure);
#else
					GEngine->OnNetworkFailure().AddUObject(this, &UMinimalClient::InternalNotifyNetworkFailure);
#endif
				}

				if (!(MinClientFlags & EMinClientFlags::AcceptRPCs) || !!(MinClientFlags & EMinClientFlags::NotifyProcessNetEvent))
				{
					FProcessEventHook::Get().AddRPCHook(UnitWorld,
						FOnProcessNetEvent::CreateUObject(this, &UMinimalClient::NotifyReceiveRPC));
				}

				bSuccess = true;
			}
			else
			{
				FString LogMsg = TEXT("Failed to create minimal client connection");

				UNIT_LOG_OBJ(Owner, ELogType::StatusFailure, TEXT("%s"), *LogMsg);
				UNIT_STATUS_LOG_OBJ(Owner, ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
			}
		}
		else
		{
			FString LogMsg = TEXT("Failed to create unit test world");

			UNIT_LOG_OBJ(Owner, ELogType::StatusFailure, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG_OBJ(Owner, ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
		}
	}
	else
	{
		FString LogMsg = TEXT("Unit test world already exists, can't create minimal client");

		UNIT_LOG_OBJ(Owner, ELogType::StatusWarning, TEXT("%s"), *LogMsg);
		UNIT_STATUS_LOG_OBJ(Owner, ELogType::StatusVerbose, TEXT("%s"), *LogMsg);
	}

	return bSuccess;
}

void UMinimalClient::Cleanup()
{
	if (UnitConn != nullptr)
	{
		UnitConn->Close();
	}

	DisconnectMinimalClient();

	if (UnitNetDriver != nullptr)
	{
		UnitNetDriver->Notify = nullptr;
	}

	UnitNetDriver = nullptr;
	UnitConn = nullptr;

	PendingNetActorChans.Empty();

	if (GEngine != nullptr)
	{
#if TARGET_UE4_CL >= CL_DEPRECATEDEL
		GEngine->OnNetworkFailure().Remove(InternalNotifyNetworkFailureDelegateHandle);
#else
		GEngine->OnNetworkFailure().RemoveUObject(this, &UMinimalClient::InternalNotifyNetworkFailure);
#endif
	}

	// Immediately cleanup (or rather, at start of next tick, as that's earliest possible time) after sending the RPC
	if (UnitWorld != nullptr)
	{
		FProcessEventHook::Get().RemoveRPCHook(UnitWorld);

		NUTNet::MarkUnitTestWorldForCleanup(UnitWorld);
		UnitWorld = nullptr;
	}
}

FOutBunch* UMinimalClient::CreateChannelBunch(EChannelType ChType, int32 ChIndex/*=INDEX_NONE*/)
{
	FOutBunch* ReturnVal = nullptr;
	UChannel* ControlChan = UnitConn != nullptr ? UnitConn->Channels[0] : nullptr;

	if (ControlChan != nullptr)
	{
		if (ChIndex == INDEX_NONE)
		{
			for (ChIndex=0; ChIndex<ARRAY_COUNT(UnitConn->Channels); ChIndex++)
			{
				if (UnitConn->Channels[ChIndex] == nullptr)
				{
					break;
				}
			}

			check(UnitConn->Channels[ChIndex] == nullptr);
		}

		if (ControlChan != nullptr && ControlChan->IsNetReady(false))
		{
			int32 BunchSequence = ++UnitConn->OutReliable[ChIndex];

			ReturnVal = new FOutBunch(ControlChan, false);

			ReturnVal->Next = nullptr;
			ReturnVal->Time = 0.0;
			ReturnVal->ReceivedAck = false;
			ReturnVal->PacketId = 0;
			ReturnVal->bDormant = false;
			ReturnVal->Channel = nullptr;
			ReturnVal->ChIndex = ChIndex;
			ReturnVal->ChType = ChType;
			ReturnVal->bReliable = 1;
			ReturnVal->ChSequence = BunchSequence;

			// NOTE: Might not cover all bOpen or 'channel already open' cases
			if (UnitConn->Channels[ChIndex] == nullptr)
			{
				ReturnVal->bOpen = 1;
			}
			else if (UnitConn->Channels[ChIndex]->OpenPacketId.First == INDEX_NONE)
			{
				ReturnVal->bOpen = 1;

				UnitConn->Channels[ChIndex]->OpenPacketId.First = BunchSequence;
				UnitConn->Channels[ChIndex]->OpenPacketId.Last = BunchSequence;
			}
		}
	}

	return ReturnVal;
}

bool UMinimalClient::SendControlBunch(FOutBunch* ControlChanBunch)
{
	bool bSuccess = false;

	if (ControlChanBunch != nullptr && UnitConn != nullptr && UnitConn->Channels[0] != nullptr)
	{
		// Since it's the unit test control channel, sending the packet abnormally, append to OutRec manually
		if (ControlChanBunch->bReliable)
		{
			UChannel* ControlChan = UnitConn->Channels[0];

			for (FOutBunch* CurOut=ControlChan->OutRec; CurOut!=nullptr; CurOut=CurOut->Next)
			{
				if (CurOut->Next == nullptr)
				{
					CurOut->Next = ControlChanBunch;
					ControlChan->NumOutRec++;

					break;
				}
			}
		}

		UnitConn->SendRawBunch(*ControlChanBunch, true);

		bSuccess = true;
	}

	return bSuccess;
}

void UMinimalClient::UnitTick(float DeltaTimm)
{
	if (!!(MinClientFlags & EMinClientFlags::NotifyNetActors) && PendingNetActorChans.Num() > 0 && UnitConn != nullptr)
	{
		for (int32 i=PendingNetActorChans.Num()-1; i>=0; i--)
		{
			UActorChannel* CurChan = Cast<UActorChannel>(UnitConn->Channels[PendingNetActorChans[i]]);

			if (CurChan != nullptr && CurChan->Actor != nullptr)
			{
				NetActorDel.ExecuteIfBound(CurChan, CurChan->Actor);
				PendingNetActorChans.RemoveAt(i);
			}
		}
	}
}

bool UMinimalClient::IsTickable() const
{
	return (!!(MinClientFlags & EMinClientFlags::NotifyNetActors) && PendingNetActorChans.Num() > 0);
}

bool UMinimalClient::ConnectMinimalClient()
{
	bool bSuccess = false;

	check(UnitWorld != nullptr);

	CreateNetDriver();

	if (UnitNetDriver != nullptr)
	{
		// Hack: Replace the control and actor channels, with stripped down unit test channels
		UnitNetDriver->ChannelClasses[CHTYPE_Control] = UUnitTestChannel::StaticClass();
		UnitNetDriver->ChannelClasses[CHTYPE_Actor] = UUnitTestActorChannel::StaticClass();

		// @todo #JohnB: Block voice channel?

		UnitNetDriver->InitialConnectTimeout = FMath::Max(UnitNetDriver->InitialConnectTimeout, (float)Owner->UnitTestTimeout);
		UnitNetDriver->ConnectionTimeout = FMath::Max(UnitNetDriver->ConnectionTimeout, (float)Owner->UnitTestTimeout);

#if !UE_BUILD_SHIPPING
		if (!(MinClientFlags & EMinClientFlags::SendRPCs) || !!(MinClientFlags & EMinClientFlags::DumpSendRPC))
		{
			UnitNetDriver->SendRPCDel.BindUObject(this, &UMinimalClient::NotifySendRPC);
		}
#endif

		bool bBeaconConnect = !!(MinClientFlags & EMinClientFlags::BeaconConnect);
		FString ConnectAddress = (bBeaconConnect ? BeaconAddress : ServerAddress);
		FURL DefaultURL;
		FURL TravelURL(&DefaultURL, *ConnectAddress, TRAVEL_Absolute);
		FString ConnectionError;
		UClass* NetConnClass = UnitNetDriver->NetConnectionClass;
		UNetConnection* DefConn = NetConnClass != nullptr ? Cast<UNetConnection>(NetConnClass->GetDefaultObject()) : nullptr;

		bSuccess = DefConn != nullptr;

		if (bSuccess)
		{
			// Replace the package map class
			TSubclassOf<UPackageMap> OldClass = DefConn->PackageMapClass;
			UProperty* OldPostConstructLink = NetConnClass->PostConstructLink;
			UProperty* PackageMapProp = FindFieldChecked<UProperty>(NetConnClass, TEXT("PackageMapClass"));

			// Hack - force property initialization for the PackageMapClass property, so changing its default value works.
			check(PackageMapProp != nullptr && PackageMapProp->PostConstructLinkNext == nullptr);

			PackageMapProp->PostConstructLinkNext = NetConnClass->PostConstructLink;
			NetConnClass->PostConstructLink = PackageMapProp;
			DefConn->PackageMapClass = UUnitTestPackageMap::StaticClass();

			bSuccess = UnitNetDriver->InitConnect(this, TravelURL, ConnectionError);

			DefConn->PackageMapClass = OldClass;
			NetConnClass->PostConstructLink = OldPostConstructLink;
			PackageMapProp->PostConstructLinkNext = nullptr;
		}
		else
		{
			UNIT_LOG_OBJ(Owner, ELogType::StatusFailure, TEXT("Failed to replace PackageMapClass, minimal client connection failed."));
		}

		if (bSuccess)
		{
			UnitConn = UnitNetDriver->ServerConnection;

			check(UnitConn->PackageMapClass == UUnitTestPackageMap::StaticClass());

			FString LogMsg = FString::Printf(TEXT("Successfully created minimal client connection to IP '%s'"), *ConnectAddress);

			UNIT_LOG_OBJ(Owner, ELogType::StatusImportant, TEXT("%s"), *LogMsg);
			UNIT_STATUS_LOG_OBJ(Owner, ELogType::StatusVerbose, TEXT("%s"), *LogMsg);


#if !UE_BUILD_SHIPPING
			UnitConn->ReceivedRawPacketDel = FOnReceivedRawPacket::CreateUObject(this, &UMinimalClient::NotifyReceivedRawPacket);

			if (!!(MinClientFlags & EMinClientFlags::DumpSendRaw))
			{
				UnitConn->LowLevelSendDel = FOnLowLevelSend::CreateUObject(this, &UMinimalClient::NotifySocketSend);
			}
			else
			{
				UnitConn->LowLevelSendDel = LowLevelSendDel;
			}
#endif

			// Work around a minor UNetConnection bug, where QueuedBits is not initialized, until after the first Tick
			UnitConn->QueuedBits = -(MAX_PACKET_SIZE * 8);

			UUnitTestChannel* UnitControlChan = CastChecked<UUnitTestChannel>(UnitConn->Channels[0]);

			UnitControlChan->MinClient = this;

#if TARGET_UE4_CL >= CL_STATELESSCONNECT
			if (UnitConn->Handler.IsValid())
			{
				UnitConn->Handler->BeginHandshaking(
										FPacketHandlerHandshakeComplete::CreateUObject(this, &UMinimalClient::SendInitialJoin));
			}
			else
			{
				SendInitialJoin();
			}
#endif
		}
		else
		{
			UNIT_LOG_OBJ(Owner, ELogType::StatusFailure, TEXT("Failed to kickoff connect to IP '%s', error: %s"), *ConnectAddress,
							*ConnectionError);
		}
	}
	else
	{
		UNIT_LOG_OBJ(Owner, ELogType::StatusFailure, TEXT("Failed to create an instance of the unit test net driver"));
	}

	return bSuccess;
}

void UMinimalClient::CreateNetDriver()
{
	check(UnitNetDriver == nullptr);

	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);

	if (GameEngine != nullptr && UnitWorld != nullptr)
	{
		static int UnitTestNetDriverCount = 0;

		// Setup a new driver name entry
		bool bFoundDef = false;
		FName UnitDefName = TEXT("UnitTestNetDriver");

		for (int32 i=0; i<GameEngine->NetDriverDefinitions.Num(); i++)
		{
			if (GameEngine->NetDriverDefinitions[i].DefName == UnitDefName)
			{
				bFoundDef = true;
				break;
			}
		}

		if (!bFoundDef)
		{
			FNetDriverDefinition NewDriverEntry;

			NewDriverEntry.DefName = UnitDefName;
			NewDriverEntry.DriverClassName = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
			NewDriverEntry.DriverClassNameFallback = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");

			GameEngine->NetDriverDefinitions.Add(NewDriverEntry);
		}


		FName NewDriverName = *FString::Printf(TEXT("UnitTestNetDriver_%i"), UnitTestNetDriverCount++);

		// Now create a reference to the driver
		if (GameEngine->CreateNamedNetDriver(UnitWorld, NewDriverName, UnitDefName))
		{
			UnitNetDriver = GameEngine->FindNamedNetDriver(UnitWorld, NewDriverName);
		}


		if (UnitNetDriver != nullptr)
		{
			UnitNetDriver->SetWorld(UnitWorld);
			UnitWorld->SetNetDriver(UnitNetDriver);

			UnitNetDriver->InitConnectionClass();


			FLevelCollection* Collection = (FLevelCollection*)UnitWorld->GetActiveLevelCollection();

			// Hack-set the net driver in the worlds level collection
			if (Collection != nullptr)
			{
				Collection->SetNetDriver(UnitNetDriver);
			}
			else
			{
				UNIT_LOG_OBJ(Owner, ELogType::StatusWarning,
								TEXT("CreateNetDriver: No LevelCollection found for created world, may block replication."));
			}

			UNIT_LOG_OBJ(Owner,, TEXT("CreateNetDriver: Created named net driver: %s, NetDriverName: %s, for World: %s"),
							*UnitNetDriver->GetFullName(), *UnitNetDriver->NetDriverName.ToString(), *UnitWorld->GetFullName());
		}
		else
		{
			UNIT_LOG_OBJ(Owner, ELogType::StatusFailure, TEXT("CreateNetDriver: CreateNamedNetDriver failed"));
		}
	}
	else if (GameEngine == nullptr)
	{
		UNIT_LOG_OBJ(Owner, ELogType::StatusFailure, TEXT("CreateNetDriver: GameEngine is nullptr"));
	}
	else //if (UnitWorld == nullptr)
	{
		UNIT_LOG_OBJ(Owner, ELogType::StatusFailure, TEXT("CreateNetDriver: UnitWorld is nullptr"));
	}
}

void UMinimalClient::SendInitialJoin()
{
	FOutBunch* ControlChanBunch = CreateChannelBunch(CHTYPE_Control, 0);

	if (ControlChanBunch != nullptr)
	{
		// Need to send 'NMT_Hello' to start off the connection (the challenge is not replied to)
		uint8 IsLittleEndian = uint8(PLATFORM_LITTLE_ENDIAN);

		// We need to construct the NMT_Hello packet manually, for the initial connection
		uint8 MessageType = NMT_Hello;

		*ControlChanBunch << MessageType;
		*ControlChanBunch << IsLittleEndian;

		uint32 LocalNetworkVersion = FNetworkVersion::GetLocalNetworkVersion();
		*ControlChanBunch << LocalNetworkVersion;


		bool bSkipControlJoin = !!(MinClientFlags & EMinClientFlags::SkipControlJoin);
		bool bBeaconConnect = !!(MinClientFlags & EMinClientFlags::BeaconConnect);
		FString BlankStr = TEXT("");
		FString ConnectURL = UUnitTest::UnitEnv->GetDefaultClientConnectURL();

		if (bBeaconConnect)
		{
			if (!bSkipControlJoin)
			{
				MessageType = NMT_BeaconJoin;
				*ControlChanBunch << MessageType;
				*ControlChanBunch << BeaconType;

				int32 UIDSize = JoinUID.Len();

				*ControlChanBunch << UIDSize;
				*ControlChanBunch << JoinUID;

				// Also immediately ack the beacon GUID setup; we're just going to let the server setup the client beacon,
				// through the actor channel
				MessageType = NMT_BeaconNetGUIDAck;
				*ControlChanBunch << MessageType;
				*ControlChanBunch << BeaconType;
			}
		}
		else
		{
			// Then send NMT_Login
			MessageType = NMT_Login;
			*ControlChanBunch << MessageType;
			*ControlChanBunch << BlankStr;
			*ControlChanBunch << ConnectURL;

			int32 UIDSize = JoinUID.Len();

			*ControlChanBunch << UIDSize;
			*ControlChanBunch << JoinUID;


			// Now send NMT_Join, to trigger a fake player, which should then trigger replication of basic actor channels
			if (!bSkipControlJoin)
			{
				MessageType = NMT_Join;
				*ControlChanBunch << MessageType;
			}
		}


		// Big hack: Store OutRec value on the unit test control channel, to enable 'retry-send' code
		if (UnitConn->Channels[0] != nullptr)
		{
			UnitConn->Channels[0]->OutRec = ControlChanBunch;
		}

		UnitConn->SendRawBunch(*ControlChanBunch, true);


		// At this point, fire of notification that we are connected
		bConnected = true;

		ConnectedDel.ExecuteIfBound();
	}
	else
	{
		UNIT_LOG_OBJ(Owner, ELogType::StatusFailure, TEXT("Failed to kickoff connection, could not create control channel bunch."));
	}
}

void UMinimalClient::DisconnectMinimalClient()
{
	if (UnitWorld != nullptr && UnitNetDriver != nullptr)
	{
		GEngine->DestroyNamedNetDriver(UnitWorld, UnitNetDriver->NetDriverName);
	}
}

void UMinimalClient::NotifySocketSend(void* Data, int32 Count, bool& bBlockSend)
{
	if (!!(MinClientFlags & EMinClientFlags::DumpSendRaw))
	{
		UNIT_LOG_OBJ(Owner, ELogType::StatusDebug, TEXT("NotifySocketSend: Packet dump:"));

		UNIT_LOG_BEGIN(Owner, ELogType::StatusDebug | ELogType::StyleMonospace);
		NUTDebug::LogHexDump((const uint8*)Data, Count, true, true);
		UNIT_LOG_END();
	}

#if !UE_BUILD_SHIPPING
	LowLevelSendDel.ExecuteIfBound(Data, Count, bBlockSend);
#endif
}

void UMinimalClient::NotifyReceivedRawPacket(void* Data, int32 Count, bool& bBlockReceive)
{
#if !UE_BUILD_SHIPPING
	GActiveReceiveUnitConnection = UnitConn;

	ReceivedRawPacketDel.ExecuteIfBound(Data, Count);

	if (Owner != nullptr)
	{
		if (!!(MinClientFlags & EMinClientFlags::DumpReceivedRaw))
		{
			UNIT_LOG_OBJ(Owner, ELogType::StatusDebug, TEXT("NotifyReceivedRawPacket: Packet dump:"));

			UNIT_LOG_BEGIN(Owner, ELogType::StatusDebug | ELogType::StyleMonospace);
			NUTDebug::LogHexDump((const uint8*)Data, Count, true, true);
			UNIT_LOG_END();
		}
	}


	// The rest of the original ReceivedRawPacket function call is blocked, so temporarily disable the delegate,
	// and re-trigger it here, so that we correctly encapsulate its call with GActiveReceiveUnitConnection
	FOnReceivedRawPacket TempDel = UnitConn->ReceivedRawPacketDel;

	UnitConn->ReceivedRawPacketDel.Unbind();

	UnitConn->UNetConnection::ReceivedRawPacket(Data, Count);

	UnitConn->ReceivedRawPacketDel = TempDel;


	GActiveReceiveUnitConnection = nullptr;

	// Block the original function call - replaced with the above
	bBlockReceive = true;
#endif
}

void UMinimalClient::NotifyReceiveRPC(AActor* Actor, UFunction* Function, void* Parameters, bool& bBlockRPC)
{
	UNIT_EVENT_BEGIN(Owner);

	// If specified, block RPC's by default - the delegate below has a chance to override this
	if (!(MinClientFlags & EMinClientFlags::AcceptRPCs))
	{
		bBlockRPC = true;
	}


	ReceiveRPCDel.ExecuteIfBound(Actor, Function, Parameters, bBlockRPC);

	FString FuncName = Function->GetName();

	if (bBlockRPC && AllowedClientRPCs.Contains(FuncName))
	{
		bBlockRPC = false;
	}

	if (bBlockRPC)
	{
		FString FuncParms = NUTUtilRefl::FunctionParmsToString(Function, Parameters);

		UNIT_LOG_OBJ(Owner,, TEXT("Blocking receive RPC '%s' for actor '%s'"), *FuncName, *Actor->GetFullName());

		if (FuncParms.Len() > 0)
		{
			UNIT_LOG_OBJ(Owner,, TEXT("     '%s' parameters: %s"), *FuncName, *FuncParms);
		}
	}


	if (!!(MinClientFlags & EMinClientFlags::DumpReceivedRPC) && !bBlockRPC)
	{
		FString FuncParms = NUTUtilRefl::FunctionParmsToString(Function, Parameters);

		UNIT_LOG_OBJ(Owner, ELogType::StatusDebug, TEXT("Received RPC '%s' for actor '%s'"), *FuncName, *Actor->GetFullName());

		if (FuncParms.Len() > 0)
		{
			UNIT_LOG_OBJ(Owner,, TEXT("     '%s' parameters: %s"), *FuncName, *FuncParms);
		}
	}

	UNIT_EVENT_END;
}

void UMinimalClient::NotifySendRPC(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack,
									UObject* SubObject, bool& bBlockSendRPC)
{
	bBlockSendRPC = !(MinClientFlags & EMinClientFlags::SendRPCs);

#if !UE_BUILD_SHIPPING
	// Pass on to the delegate, and give it an opportunity to override whether the RPC is sent
	SendRPCDel.ExecuteIfBound(Actor, Function, Parameters, OutParms, Stack, SubObject, bBlockSendRPC);
#endif

	if (!bBlockSendRPC)
	{
		if (!!(MinClientFlags & EMinClientFlags::DumpSendRPC))
		{
			UNIT_LOG_OBJ(Owner, ELogType::StatusDebug, TEXT("Send RPC '%s' for actor '%s' (SubObject '%s')"), *Function->GetName(),
						*Actor->GetFullName(), (SubObject != NULL ? *SubObject->GetFullName() : TEXT("nullptr")));
		}
	}
	else
	{
		if (!(MinClientFlags & EMinClientFlags::SendRPCs))
		{
			UNIT_LOG_OBJ(Owner, , TEXT("Blocking send RPC '%s' in actor '%s' (SubObject '%s')"), *Function->GetName(),
						*Actor->GetFullName(), (SubObject != NULL ? *SubObject->GetFullName() : TEXT("nullptr")));
		}
	}
}

void UMinimalClient::InternalNotifyNetworkFailure(UWorld* InWorld, UNetDriver* InNetDriver, ENetworkFailure::Type FailureType,
													const FString& ErrorString)
{
	if (InNetDriver == (UNetDriver*)UnitNetDriver)
	{
		UNIT_EVENT_BEGIN(Owner);

		NetworkFailureDel.ExecuteIfBound(FailureType, ErrorString);

		UNIT_EVENT_END;
	}
}

bool UMinimalClient::NotifyAcceptingChannel(UChannel* Channel)
{
	bool bAccepted = false;

	if (Channel->ChType == CHTYPE_Actor)
	{
		bAccepted = !!(MinClientFlags & EMinClientFlags::AcceptActors);

		if (!!(MinClientFlags & EMinClientFlags::NotifyNetActors))
		{
			PendingNetActorChans.Add(Channel->ChIndex);
		}
	}

	return bAccepted;
}

// @todo #JohnB: Remove dependency on ClientUnitTest eventually
UMinimalClient* UMinimalClient::GetMinClientFromConn(UNetConnection* InConn)
{
	UMinimalClient* ReturnVal = nullptr;

	for (UUnitTest* CurUnitTest : GUnitTestManager->ActiveUnitTests)
	{
		UClientUnitTest* CurClientUnitTest = Cast<UClientUnitTest>(CurUnitTest);
		UMinimalClient* MinClient = (CurClientUnitTest != nullptr ? CurClientUnitTest->MinClient : nullptr);
		UNetConnection* CurUnitConn = (MinClient != nullptr ? MinClient->GetConn() : nullptr);

		if (CurUnitConn != nullptr && CurUnitConn == InConn)
		{
			ReturnVal = MinClient;
			break;
		}
	}

	return ReturnVal;
}
