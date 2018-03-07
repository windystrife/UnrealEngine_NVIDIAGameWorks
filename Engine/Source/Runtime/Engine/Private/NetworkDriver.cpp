// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NetworkDriver.cpp: Unreal network driver base class.
=============================================================================*/

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/NetworkGuid.h"
#include "Stats/Stats.h"
#include "Misc/MemStack.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/CoreNet.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "PacketHandler.h"
#include "PacketHandlers/StatelessConnectHandlerComponent.h"
#include "Engine/NetDriver.h"
#include "Engine/LocalPlayer.h"
#include "Net/DataBunch.h"
#include "Engine/NetConnection.h"
#include "DrawDebugHelpers.h"
#include "UnrealEngine.h"
#include "EngineUtils.h"
#include "Net/NetworkProfiler.h"
#include "Engine/PackageMapClient.h"
#include "Net/RepLayout.h"
#include "Net/DataReplication.h"
#include "Engine/ControlChannel.h"
#include "Engine/ActorChannel.h"
#include "Engine/VoiceChannel.h"
#include "Engine/NetworkObjectList.h"
#include "GameFramework/GameNetworkManager.h"
#include "Net/OnlineEngineInterface.h"
#include "NetworkingDistanceConstants.h"
#include "Engine/ChildConnection.h"
#include "Net/DataChannel.h"
#include "GameFramework/PlayerState.h"
#include "Net/PerfCountersHelpers.h"


#if USE_SERVER_PERF_COUNTERS
#include "PerfCountersModule.h"
#endif

#if WITH_EDITOR
#include "Editor.h"
#endif

// Default net driver stats
DEFINE_STAT(STAT_Ping);
DEFINE_STAT(STAT_Channels);
DEFINE_STAT(STAT_MaxPacketOverhead);
DEFINE_STAT(STAT_InRate);
DEFINE_STAT(STAT_OutRate);
DEFINE_STAT(STAT_OutSaturation);
DEFINE_STAT(STAT_InRateClientMax);
DEFINE_STAT(STAT_InRateClientMin);
DEFINE_STAT(STAT_InRateClientAvg);
DEFINE_STAT(STAT_InPacketsClientMax);
DEFINE_STAT(STAT_InPacketsClientMin);
DEFINE_STAT(STAT_InPacketsClientAvg);
DEFINE_STAT(STAT_OutRateClientMax);
DEFINE_STAT(STAT_OutRateClientMin);
DEFINE_STAT(STAT_OutRateClientAvg);
DEFINE_STAT(STAT_OutPacketsClientMax);
DEFINE_STAT(STAT_OutPacketsClientMin);
DEFINE_STAT(STAT_OutPacketsClientAvg);
DEFINE_STAT(STAT_NetNumClients);
DEFINE_STAT(STAT_InPackets);
DEFINE_STAT(STAT_OutPackets);
DEFINE_STAT(STAT_InBunches);
DEFINE_STAT(STAT_OutBunches);
DEFINE_STAT(STAT_OutLoss);
DEFINE_STAT(STAT_InLoss);
DEFINE_STAT(STAT_NumConsideredActors);
DEFINE_STAT(STAT_PrioritizedActors);
DEFINE_STAT(STAT_NumRelevantActors);
DEFINE_STAT(STAT_NumRelevantDeletedActors);
DEFINE_STAT(STAT_NumReplicatedActorAttempts);
DEFINE_STAT(STAT_NumReplicatedActors);
DEFINE_STAT(STAT_NumActorChannels);
DEFINE_STAT(STAT_NumActors);
DEFINE_STAT(STAT_NumNetActors);
DEFINE_STAT(STAT_NumDormantActors);
DEFINE_STAT(STAT_NumInitiallyDormantActors);
DEFINE_STAT(STAT_NumNetGUIDsAckd);
DEFINE_STAT(STAT_NumNetGUIDsPending);
DEFINE_STAT(STAT_NumNetGUIDsUnAckd);
DEFINE_STAT(STAT_ObjPathBytes);
DEFINE_STAT(STAT_NetGUIDInRate);
DEFINE_STAT(STAT_NetGUIDOutRate);
DEFINE_STAT(STAT_NetSaturated);

// Voice specific stats
DEFINE_STAT(STAT_VoiceBytesSent);
DEFINE_STAT(STAT_VoiceBytesRecv);
DEFINE_STAT(STAT_VoicePacketsSent);
DEFINE_STAT(STAT_VoicePacketsRecv);
DEFINE_STAT(STAT_PercentInVoice);
DEFINE_STAT(STAT_PercentOutVoice);

#if !UE_BUILD_SHIPPING
// Packet stats
DEFINE_STAT(STAT_MaxPacket);
DEFINE_STAT(STAT_MaxPacketMinusReserved);
DEFINE_STAT(STAT_PacketReservedTotal);
DEFINE_STAT(STAT_PacketReservedNetConnection);
DEFINE_STAT(STAT_PacketReservedPacketHandler);
DEFINE_STAT(STAT_PacketReservedHandshake);
#endif


#if UE_BUILD_SHIPPING
#define DEBUG_REMOTEFUNCTION(Format, ...)
#else
#define DEBUG_REMOTEFUNCTION(Format, ...) UE_LOG(LogNet, VeryVerbose, Format, __VA_ARGS__);
#endif

// CVars
static TAutoConsoleVariable<int32> CVarSetNetDormancyEnabled(
	TEXT("net.DormancyEnable"),
	1,
	TEXT("Enables Network Dormancy System for reducing CPU and bandwidth overhead of infrequently updated actors\n")
	TEXT("1 Enables network dormancy. 0 disables network dormancy."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarNetDormancyDraw(
	TEXT("net.DormancyDraw"),
	0,
	TEXT("Draws debug information for network dormancy\n")
	TEXT("1 Enables network dormancy debugging. 0 disables."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarNetDormancyDrawCullDistance(
	TEXT("net.DormancyDrawCullDistance"),
	5000.f,
	TEXT("Cull distance for net.DormancyDraw. World Units")
	TEXT("Max world units an actor can be away from the local view to draw its dormancy status"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarNetDormancyValidate(
	TEXT("net.DormancyValidate"),
	0,
	TEXT("Validates that dormant actors do not change state while in a dormant state (on server only)")
	TEXT("0: Dont validate. 1: Validate on wake up. 2: Validate on each net update"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarUseAdaptiveNetUpdateFrequency(
	TEXT( "net.UseAdaptiveNetUpdateFrequency" ), 
	1, 
	TEXT( "If 1, NetUpdateFrequency will be calculated based on how often actors actually send something when replicating" ) );

TAutoConsoleVariable<int32> CVarNetAllowEncryption(
	TEXT("net.AllowEncryption"),
	1,
	TEXT("If true, the engine will attempt to load an encryption PacketHandler component and fill in the EncryptionToken parameter of the NMT_Hello message based on the ?EncryptionToken= URL option and call callbacks if it's non-empty."));

/*-----------------------------------------------------------------------------
	UNetDriver implementation.
-----------------------------------------------------------------------------*/

UNetDriver::UNetDriver(const FObjectInitializer& ObjectInitializer)
:	UObject(ObjectInitializer)
,	MaxInternetClientRate(10000)
, 	MaxClientRate(15000)
,   bNoTimeouts(false)
,   ServerConnection(nullptr)
,	ClientConnections()
,	ConnectionlessHandler()
,	StatelessConnectComponent()
,   World(nullptr)
,   Notify(nullptr)
,	Time( 0.f )
,	LastTickDispatchRealtime( 0.f )
,   bIsPeer(false)
,	InBytes(0)
,	OutBytes(0)
,	NetGUIDOutBytes(0)
,	NetGUIDInBytes(0)
,	InPackets(0)
,	OutPackets(0)
,	InBunches(0)
,	OutBunches(0)
,	InPacketsLost(0)
,	OutPacketsLost(0)
,	InOutOfOrderPackets(0)
,	OutOutOfOrderPackets(0)
,	StatUpdateTime(0.0)
,	StatPeriod(1.f)
,	bCollectNetStats(false)
,	LastCleanupTime(0.0)
,	NetTag(0)
,	DebugRelevantActors(false)
#if !UE_BUILD_SHIPPING
,	SendRPCDel()
#endif
,	ProcessQueuedBunchesCurrentFrameMilliseconds(0.0f)
,	NetworkObjects(new FNetworkObjectList)
,	LagState(ENetworkLagState::NotLagging)
,	DuplicateLevelID(INDEX_NONE)
{
	ChannelClasses[CHTYPE_Control]	= UControlChannel::StaticClass();
	ChannelClasses[CHTYPE_Actor]	= UActorChannel::StaticClass();
	ChannelClasses[CHTYPE_Voice]	= UVoiceChannel::StaticClass();
}

void UNetDriver::InitPacketSimulationSettings()
{
#if DO_ENABLE_NET_TEST
	// read the settings from .ini and command line, with the command line taking precedence	
	PacketSimulationSettings = FPacketSimulationSettings();
	PacketSimulationSettings.LoadConfig(*NetDriverName.ToString());
	PacketSimulationSettings.RegisterCommands();
	PacketSimulationSettings.ParseSettings(FCommandLine::Get(), *NetDriverName.ToString());
#endif
}

void UNetDriver::PostInitProperties()
{
	Super::PostInitProperties();

	// By default we're the game net driver and any child ones must override this
	NetDriverName = NAME_GameNetDriver;

	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		InitPacketSimulationSettings();

		RoleProperty		= FindObjectChecked<UProperty>( AActor::StaticClass(), TEXT("Role"      ) );
		RemoteRoleProperty	= FindObjectChecked<UProperty>( AActor::StaticClass(), TEXT("RemoteRole") );

		GuidCache			= TSharedPtr< FNetGUIDCache >( new FNetGUIDCache( this ) );
		NetCache			= TSharedPtr< FClassNetCacheMgr >( new FClassNetCacheMgr() );

		ProfileStats		= FParse::Param(FCommandLine::Get(),TEXT("profilestats"));

#if !UE_BUILD_SHIPPING
		bNoTimeouts = bNoTimeouts || FParse::Param(FCommandLine::Get(), TEXT("NoTimeouts")) ? true : false;
#endif // !UE_BUILD_SHIPPING

#if WITH_EDITOR
		// Do not time out in PIE since the server is local.
		bNoTimeouts = bNoTimeouts || (GEditor && GEditor->PlayWorld);
#endif // WITH_EDITOR
	
		OnLevelRemovedFromWorldHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UNetDriver::OnLevelRemovedFromWorld);
	}
}

void UNetDriver::AssertValid()
{
}

/*static*/ bool UNetDriver::IsAdaptiveNetUpdateFrequencyEnabled()
{
	const bool bUseAdapativeNetFrequency = CVarUseAdaptiveNetUpdateFrequency.GetValueOnAnyThread() > 0;
	return bUseAdapativeNetFrequency;
}

FNetworkObjectInfo* UNetDriver::GetNetworkObjectInfo(const AActor* InActor)
{
	TSharedPtr<FNetworkObjectInfo>* NetworkObjectInfo = GetNetworkObjectList().Add(const_cast<AActor*>(InActor), NetDriverName);

	return NetworkObjectInfo ? NetworkObjectInfo->Get() : nullptr;
}

const FNetworkObjectInfo* UNetDriver::GetNetworkObjectInfo(const AActor* InActor) const
{
	return const_cast<UNetDriver*>(this)->GetNetworkObjectInfo(InActor);
}

const FNetworkObjectInfo* UNetDriver::GetNetworkActor(const AActor* InActor) const
{
	return GetNetworkObjectInfo(InActor);
}

FNetworkObjectInfo* UNetDriver::GetNetworkActor(const AActor* InActor)
{
	return const_cast<UNetDriver*>(this)->GetNetworkObjectInfo(InActor);
}

bool UNetDriver::IsNetworkActorUpdateFrequencyThrottled(const FNetworkObjectInfo& InNetworkActor) const
{
	bool bThrottled = false;
	if (IsAdaptiveNetUpdateFrequencyEnabled())
	{
		// Must have been replicated once for this to happen (and for OptimalNetUpdateDelta to have been set)
		const AActor* Actor = InNetworkActor.Actor;
		if (Actor && InNetworkActor.LastNetReplicateTime != 0)
		{
			const float ExpectedNetDelay = (1.0f / Actor->NetUpdateFrequency);
			if (InNetworkActor.OptimalNetUpdateDelta > ExpectedNetDelay)
			{
				bThrottled = true;
			}
		}
	}

	return bThrottled;
}

bool UNetDriver::IsNetworkActorUpdateFrequencyThrottled(const AActor* InActor) const
{
	bool bThrottled = false;
	if (InActor && IsAdaptiveNetUpdateFrequencyEnabled())
	{
		if (const FNetworkObjectInfo* NetActor = GetNetworkObjectInfo(InActor))
		{
			bThrottled = IsNetworkActorUpdateFrequencyThrottled(*NetActor);
		}
	}

	return bThrottled;
}

void UNetDriver::CancelAdaptiveReplication(FNetworkObjectInfo& InNetworkActor)
{
	if (IsAdaptiveNetUpdateFrequencyEnabled())
	{
		if (AActor* Actor = InNetworkActor.Actor)
		{
			if (UWorld* ActorWorld = Actor->GetWorld())
			{
				const float ExpectedNetDelay = (1.0f / Actor->NetUpdateFrequency);
				Actor->SetNetUpdateTime( ActorWorld->GetTimeSeconds() + FMath::FRandRange( 0.5f, 1.0f ) * ExpectedNetDelay );
				InNetworkActor.OptimalNetUpdateDelta = ExpectedNetDelay;
				// TODO: we really need a way to cancel the throttling completely. OptimalNetUpdateDelta is going to be recalculated based on LastNetReplicateTime.
			}
		}
	}
}

static TAutoConsoleVariable<int32> CVarOptimizedRemapping( TEXT( "net.OptimizedRemapping" ), 1, TEXT( "Uses optimized path to remap unmapped network guids" ) );

void UNetDriver::TickFlush(float DeltaSeconds)
{
#if USE_SERVER_PERF_COUNTERS
	double ServerReplicateActorsTimeMs = 0.0f;
#endif // USE_SERVER_PERF_COUNTERS

	if ( IsServer() && ClientConnections.Num() > 0 && ClientConnections[0]->InternalAck == false )
	{
		// Update all clients.
#if WITH_SERVER_CODE

#if USE_SERVER_PERF_COUNTERS
		double ServerReplicateActorsTimeStart = FPlatformTime::Seconds();
#endif // USE_SERVER_PERF_COUNTERS

		int32 Updated = ServerReplicateActors( DeltaSeconds );

#if USE_SERVER_PERF_COUNTERS
		ServerReplicateActorsTimeMs = (FPlatformTime::Seconds() - ServerReplicateActorsTimeStart) * 1000.0;
#endif // USE_SERVER_PERF_COUNTERS

		static int32 LastUpdateCount = 0;
		// Only log the zero replicated actors once after replicating an actor
		if ((LastUpdateCount && !Updated) || Updated)
		{
			UE_LOG(LogNetTraffic, Verbose, TEXT("%s replicated %d actors"), *GetDescription(), Updated);
		}
		LastUpdateCount = Updated;
#endif // WITH_SERVER_CODE
	}

	// Reset queued bunch amortization timer
	ProcessQueuedBunchesCurrentFrameMilliseconds = 0.0f;

	const double CurrentRealtimeSeconds = FPlatformTime::Seconds();

	bool bCollectServerStats = false;
#if USE_SERVER_PERF_COUNTERS || STATS
	bCollectServerStats = true;
#endif

	if (bCollectNetStats || bCollectServerStats)
	{
		// Update network stats (only main game net driver for now) if stats or perf counters are used
		if (NetDriverName == NAME_GameNetDriver &&
			CurrentRealtimeSeconds - StatUpdateTime > StatPeriod)
		{
			int32 ClientInBytesMax = 0;
			int32 ClientInBytesMin = 0;
			int32 ClientInBytesAvg = 0;
			int32 ClientInPacketsMax = 0;
			int32 ClientInPacketsMin = 0;
			int32 ClientInPacketsAvg = 0;
			int32 ClientOutBytesMax = 0;
			int32 ClientOutBytesMin = 0;
			int32 ClientOutBytesAvg = 0;
			int32 ClientOutPacketsMax = 0;
			int32 ClientOutPacketsMin = 0;
			int32 ClientOutPacketsAvg = 0;
			int NumClients = 0;
			int32 MaxPacketOverhead = 0;
			float RemoteSaturationMax = 0.0f;

			// these need to be updated even if we are not collecting stats, since they get reported to analytics/QoS
			for (UNetConnection * Client : ClientConnections)
			{
				if (Client)
				{
#define UpdatePerClientMinMaxAvg(VariableName) \
				Client##VariableName##Max = FMath::Max(Client##VariableName##Max, Client->VariableName##PerSecond); \
				if (Client##VariableName##Min == 0 || Client->VariableName##PerSecond < Client##VariableName##Min) \
				{ \
					Client##VariableName##Min = Client->VariableName##PerSecond; \
				} \
				Client##VariableName##Avg += Client->VariableName##PerSecond; \

					UpdatePerClientMinMaxAvg(InBytes);
					UpdatePerClientMinMaxAvg(OutBytes);
					UpdatePerClientMinMaxAvg(InPackets);
					UpdatePerClientMinMaxAvg(OutPackets);

					MaxPacketOverhead = FMath::Max(Client->PacketOverhead, MaxPacketOverhead);

#undef UpdatePerClientMinMaxAvg

					++NumClients;
				}
			}

			if (NumClients > 1)
			{
				ClientInBytesAvg /= NumClients;
				ClientInPacketsAvg /= NumClients;
				ClientOutBytesAvg /= NumClients;
				ClientOutPacketsAvg /= NumClients;
			}

			int32 Ping = 0;
			int32 NumOpenChannels = 0;
			int32 NumActorChannels = 0;
			int32 NumDormantActors = 0;
			int32 NumActors = 0;
			int32 AckCount = 0;
			int32 UnAckCount = 0;
			int32 PendingCount = 0;
			int32 NetSaturated = 0;

			if (
#if STATS
				FThreadStats::IsCollectingData() ||
#endif
				bCollectNetStats)
			{
				const float RealTime = CurrentRealtimeSeconds - StatUpdateTime;

				// Use the elapsed time to keep things scaled to one measured unit
				InBytes = FMath::TruncToInt(InBytes / RealTime);
				OutBytes = FMath::TruncToInt(OutBytes / RealTime);

				NetGUIDOutBytes = FMath::TruncToInt(NetGUIDOutBytes / RealTime);
				NetGUIDInBytes = FMath::TruncToInt(NetGUIDInBytes / RealTime);

				// Save off for stats later

				InBytesPerSecond = InBytes;
				OutBytesPerSecond = OutBytes;

				InPackets = FMath::TruncToInt(InPackets / RealTime);
				OutPackets = FMath::TruncToInt(OutPackets / RealTime);
				InBunches = FMath::TruncToInt(InBunches / RealTime);
				OutBunches = FMath::TruncToInt(OutBunches / RealTime);
				OutPacketsLost = FMath::TruncToInt(100.f * OutPacketsLost / FMath::Max((float)OutPackets, 1.f));
				InPacketsLost = FMath::TruncToInt(100.f * InPacketsLost / FMath::Max((float)InPackets + InPacketsLost, 1.f));

				if (ServerConnection != NULL && ServerConnection->PlayerController != NULL && ServerConnection->PlayerController->PlayerState != NULL)
				{
					Ping = FMath::TruncToInt(ServerConnection->PlayerController->PlayerState->ExactPing);
				}

				if (ServerConnection != NULL)
				{
					NumOpenChannels = ServerConnection->OpenChannels.Num();
					RemoteSaturationMax = FMath::Max(RemoteSaturationMax, ServerConnection->RemoteSaturation);
				}

				for (int32 i = 0; i < ClientConnections.Num(); i++)
				{
					NumOpenChannels += ClientConnections[i]->OpenChannels.Num();
					RemoteSaturationMax = FMath::Max(RemoteSaturationMax, ClientConnections[i]->RemoteSaturation);
				}

				// Use the elapsed time to keep things scaled to one measured unit
				VoicePacketsSent = FMath::TruncToInt(VoicePacketsSent / RealTime);
				VoicePacketsRecv = FMath::TruncToInt(VoicePacketsRecv / RealTime);
				VoiceBytesSent = FMath::TruncToInt(VoiceBytesSent / RealTime);
				VoiceBytesRecv = FMath::TruncToInt(VoiceBytesRecv / RealTime);

				// Determine voice percentages
				VoiceInPercent = (InBytes > 0) ? FMath::TruncToInt(100.f * (float)VoiceBytesRecv / (float)InBytes) : 0;
				VoiceOutPercent = (OutBytes > 0) ? FMath::TruncToInt(100.f * (float)VoiceBytesSent / (float)OutBytes) : 0;

				UNetConnection * Connection = (ServerConnection ? ServerConnection : (ClientConnections.Num() > 0 ? ClientConnections[0] : NULL));
				if (Connection)
				{
					NumActorChannels = Connection->ActorChannels.Num();
					NumDormantActors = Connection->Driver->GetNetworkObjectList().GetNumDormantActorsForConnection( Connection );

					if (World)
					{
						NumActors = World->GetActorCount();
					}
#if STATS
					Connection->PackageMap->GetNetGUIDStats(AckCount, UnAckCount, PendingCount);
#endif
					NetSaturated = Connection->IsNetReady(false) ? 0 : 1;
				}
			}

#if STATS
			// Copy the net status values over
			SET_DWORD_STAT(STAT_Ping, Ping);
			SET_DWORD_STAT(STAT_Channels, NumOpenChannels);
			SET_DWORD_STAT(STAT_MaxPacketOverhead, MaxPacketOverhead);

			SET_DWORD_STAT(STAT_OutLoss, OutPacketsLost);
			SET_DWORD_STAT(STAT_InLoss, InPacketsLost);
			SET_DWORD_STAT(STAT_InRate, InBytes);
			SET_DWORD_STAT(STAT_OutRate, OutBytes);
			SET_DWORD_STAT(STAT_OutSaturation, RemoteSaturationMax);
			SET_DWORD_STAT(STAT_InRateClientMax, ClientInBytesMax);
			SET_DWORD_STAT(STAT_InRateClientMin, ClientInBytesMin);
			SET_DWORD_STAT(STAT_InRateClientAvg, ClientInBytesAvg);
			SET_DWORD_STAT(STAT_InPacketsClientMax, ClientInPacketsMax);
			SET_DWORD_STAT(STAT_InPacketsClientMin, ClientInPacketsMin);
			SET_DWORD_STAT(STAT_InPacketsClientAvg, ClientInPacketsAvg);
			SET_DWORD_STAT(STAT_OutRateClientMax, ClientOutBytesMax);
			SET_DWORD_STAT(STAT_OutRateClientMin, ClientOutBytesMin);
			SET_DWORD_STAT(STAT_OutRateClientAvg, ClientOutBytesAvg);
			SET_DWORD_STAT(STAT_OutPacketsClientMax, ClientOutPacketsMax);
			SET_DWORD_STAT(STAT_OutPacketsClientMin, ClientOutPacketsMin);
			SET_DWORD_STAT(STAT_OutPacketsClientAvg, ClientOutPacketsAvg);

			SET_DWORD_STAT(STAT_NetNumClients, NumClients);
			SET_DWORD_STAT(STAT_InPackets, InPackets);
			SET_DWORD_STAT(STAT_OutPackets, OutPackets);
			SET_DWORD_STAT(STAT_InBunches, InBunches);
			SET_DWORD_STAT(STAT_OutBunches, OutBunches);

			SET_DWORD_STAT(STAT_NetGUIDInRate, NetGUIDInBytes);
			SET_DWORD_STAT(STAT_NetGUIDOutRate, NetGUIDOutBytes);

			SET_DWORD_STAT(STAT_VoicePacketsSent, VoicePacketsSent);
			SET_DWORD_STAT(STAT_VoicePacketsRecv, VoicePacketsRecv);
			SET_DWORD_STAT(STAT_VoiceBytesSent, VoiceBytesSent);
			SET_DWORD_STAT(STAT_VoiceBytesRecv, VoiceBytesRecv);

			SET_DWORD_STAT(STAT_PercentInVoice, VoiceInPercent);
			SET_DWORD_STAT(STAT_PercentOutVoice, VoiceOutPercent);

			SET_DWORD_STAT(STAT_NumActorChannels, NumActorChannels);
			SET_DWORD_STAT(STAT_NumDormantActors, NumDormantActors);
			SET_DWORD_STAT(STAT_NumActors, NumActors);
			SET_DWORD_STAT(STAT_NumNetActors, GetNetworkObjectList().GetActiveObjects().Num());
			SET_DWORD_STAT(STAT_NumNetGUIDsAckd, AckCount);
			SET_DWORD_STAT(STAT_NumNetGUIDsPending, UnAckCount);
			SET_DWORD_STAT(STAT_NumNetGUIDsUnAckd, PendingCount);
			SET_DWORD_STAT(STAT_NetSaturated, NetSaturated);
#endif // STATS

#if USE_SERVER_PERF_COUNTERS
			IPerfCounters* PerfCounters = IPerfCountersModule::Get().GetPerformanceCounters();
			if (PerfCounters)
			{
				// Update total connections
				PerfCounters->Set(TEXT("NumConnections"), ClientConnections.Num());

				const int kNumBuckets = 8;	// evenly spaced with increment of 30 ms; last bucket collects all off-scale pings as well
				if (ClientConnections.Num() > 0)
				{
					// Update per connection statistics
					float MinPing = MAX_FLT;
					float AvgPing = 0;
					float MaxPing = -MAX_FLT;
					float PingCount = 0;

					int32 Buckets[kNumBuckets] = { 0 };

					for (int32 i = 0; i < ClientConnections.Num(); i++)
					{
						UNetConnection* Connection = ClientConnections[i];

						if (Connection != nullptr)
						{
							if (Connection->PlayerController != nullptr && Connection->PlayerController->PlayerState != nullptr)
							{
								// Ping value calculated per client
								float ConnPing = Connection->PlayerController->PlayerState->ExactPing;

								int Bucket = FMath::Max(0, FMath::Min(kNumBuckets - 1, (static_cast<int>(ConnPing) / 30)));
								++Buckets[Bucket];

								if (ConnPing < MinPing)
								{
									MinPing = ConnPing;
								}

								if (ConnPing > MaxPing)
								{
									MaxPing = ConnPing;
								}

								AvgPing += ConnPing;
								PingCount++;
							}
						}
					}

					if (PingCount > 0)
					{
						AvgPing /= static_cast<float>(PingCount);
					}

					PerfCounters->Set(TEXT("AvgPing"), AvgPing, IPerfCounters::Flags::Transient);
					float CurrentMaxPing = PerfCounters->Get(TEXT("MaxPing"), MaxPing);
					PerfCounters->Set(TEXT("MaxPing"), FMath::Max(MaxPing, CurrentMaxPing), IPerfCounters::Flags::Transient);
					float CurrentMinPing = PerfCounters->Get(TEXT("MinPing"), MinPing);
					PerfCounters->Set(TEXT("MinPing"), FMath::Min(MinPing, CurrentMinPing), IPerfCounters::Flags::Transient);

					// update buckets
					for (int BucketIdx = 0; BucketIdx < ARRAY_COUNT(Buckets); ++BucketIdx)
					{
						PerfCountersIncrement(FString::Printf(TEXT("PingBucketInt%d"), BucketIdx), Buckets[BucketIdx], 0, IPerfCounters::Flags::Transient);
					}
				}
				else
				{
					PerfCounters->Set(TEXT("AvgPing"), 0.0f, IPerfCounters::Flags::Transient);
					PerfCounters->Set(TEXT("MaxPing"), -FLT_MAX, IPerfCounters::Flags::Transient);
					PerfCounters->Set(TEXT("MinPing"), FLT_MAX, IPerfCounters::Flags::Transient);

					for (int BucketIdx = 0; BucketIdx < kNumBuckets; ++BucketIdx)
					{
						PerfCounters->Set(FString::Printf(TEXT("PingBucketInt%d"), BucketIdx), 0, IPerfCounters::Flags::Transient);
					}
				}

				// set the per connection stats (these are calculated earlier).
				// Note that NumClients may be != NumConnections. Also, if NumClients is 0, the rest of counters should be 0 as well
				PerfCounters->Set(TEXT("NumClients"), NumClients);
				PerfCounters->Set(TEXT("MaxPacketOverhead"), MaxPacketOverhead);
				PerfCounters->Set(TEXT("InRateClientMax"), ClientInBytesMax);
				PerfCounters->Set(TEXT("InRateClientMin"), ClientInBytesMin);
				PerfCounters->Set(TEXT("InRateClientAvg"), ClientInBytesAvg);
				PerfCounters->Set(TEXT("InPacketsClientMax"), ClientInPacketsMax);
				PerfCounters->Set(TEXT("InPacketsClientMin"), ClientInPacketsMin);
				PerfCounters->Set(TEXT("InPacketsClientAvg"), ClientInPacketsAvg);
				PerfCounters->Set(TEXT("OutRateClientMax"), ClientOutBytesMax);
				PerfCounters->Set(TEXT("OutRateClientMin"), ClientOutBytesMin);
				PerfCounters->Set(TEXT("OutRateClientAvg"), ClientOutBytesAvg);
				PerfCounters->Set(TEXT("OutPacketsClientMax"), ClientOutPacketsMax);
				PerfCounters->Set(TEXT("OutPacketsClientMin"), ClientOutPacketsMin);
				PerfCounters->Set(TEXT("OutPacketsClientAvg"), ClientOutPacketsAvg);

				PerfCounters->Set(TEXT("InRate"), InBytes);
				PerfCounters->Set(TEXT("OutRate"), OutBytes);
				PerfCounters->Set(TEXT("InPacketsLost"), InPacketsLost);
				PerfCounters->Set(TEXT("OutPacketsLost"), OutPacketsLost);
				PerfCounters->Set(TEXT("InPackets"), InPackets);
				PerfCounters->Set(TEXT("OutPackets"), OutPackets);
				PerfCounters->Set(TEXT("InBunches"), InBunches);
				PerfCounters->Set(TEXT("OutBunches"), OutBunches);

				PerfCounters->Set(TEXT("ServerReplicateActorsTimeMs"), ServerReplicateActorsTimeMs);
				PerfCounters->Set(TEXT("OutSaturationMax"), RemoteSaturationMax);
			}
#endif // USE_SERVER_PERF_COUNTERS

			// Reset everything
			InBytes = 0;
			OutBytes = 0;
			NetGUIDOutBytes = 0;
			NetGUIDInBytes = 0;
			InPackets = 0;
			OutPackets = 0;
			InBunches = 0;
			OutBunches = 0;
			OutPacketsLost = 0;
			InPacketsLost = 0;
			VoicePacketsSent = 0;
			VoiceBytesSent = 0;
			VoicePacketsRecv = 0;
			VoiceBytesRecv = 0;
			VoiceInPercent = 0;
			VoiceOutPercent = 0;
			StatUpdateTime = CurrentRealtimeSeconds;
		}
	} // bCollectNetStats ||(USE_SERVER_PERF_COUNTERS) || STATS

	// Poll all sockets.
	if( ServerConnection )
	{
		// Queue client voice packets in the server's voice channel
		ProcessLocalClientPackets();
		ServerConnection->Tick();
	}
	else
	{
		// Queue up any voice packets the server has locally
		ProcessLocalServerPackets();
	}

	for( int32 i=0; i<ClientConnections.Num(); i++ )
	{
		ClientConnections[i]->Tick();
	}

	if (ConnectionlessHandler.IsValid())
	{
		ConnectionlessHandler->Tick(DeltaSeconds);

		FlushHandler();
	}

	if (CVarNetDormancyDraw.GetValueOnAnyThread() > 0)
	{
		DrawNetDriverDebug();
	}

	if ( CVarOptimizedRemapping.GetValueOnAnyThread() && GuidCache.IsValid() )
	{
		SCOPE_CYCLE_COUNTER( STAT_NetUpdateUnmappedObjectsTime );

		// Go over recently imported network guids, and see if there are any replicators that need to map them
		TSet< FNetworkGUID >& ImportedNetGuids = GuidCache->ImportedNetGuids;

		TSet< FObjectReplicator* > ForceUpdateReplicators;

		for (FObjectReplicator* Replicator : UnmappedReplicators)
		{
			if (Replicator->bForceUpdateUnmapped)
			{
				Replicator->bForceUpdateUnmapped = false;
				ForceUpdateReplicators.Add(Replicator);
			}
		}

		if ( ImportedNetGuids.Num() || ForceUpdateReplicators.Num() )
		{
			TArray< FNetworkGUID > NewlyMappedGuids;

			for ( auto It = ImportedNetGuids.CreateIterator(); It; ++It )
			{
				const FNetworkGUID NetworkGuid = *It;

				if ( GuidCache->GetObjectFromNetGUID( NetworkGuid, false ) != nullptr )
				{
					NewlyMappedGuids.Add( NetworkGuid );
					It.RemoveCurrent();
				}

				if ( GuidCache->IsGUIDBroken( NetworkGuid, false ) )
				{
					It.RemoveCurrent();
				}
			}

			if ( NewlyMappedGuids.Num() || ForceUpdateReplicators.Num() )
			{
				TSet< FObjectReplicator* > AllReplicators = ForceUpdateReplicators;

				for ( const FNetworkGUID& NetGuid : NewlyMappedGuids )
				{
					TSet< FObjectReplicator* >* Replicators = GuidToReplicatorMap.Find( NetGuid );

					if ( Replicators )
					{
						AllReplicators.Append( *Replicators );
					}
				}

				for ( FObjectReplicator* Replicator : AllReplicators )
				{
					if ( UnmappedReplicators.Contains( Replicator ) )
					{
						bool bHasMoreUnmapped = false;
						Replicator->UpdateUnmappedObjects( bHasMoreUnmapped );

						if ( !bHasMoreUnmapped )
						{
							UnmappedReplicators.Remove( Replicator );
						}
					}
				}
			}
		}
	}
	else
	{
		SCOPE_CYCLE_COUNTER( STAT_NetUpdateUnmappedObjectsTime );

		// Update properties that are unmapped, try to hook up the object pointers if they exist now
		for ( auto It = UnmappedReplicators.CreateIterator(); It; ++It )
		{
			FObjectReplicator* Replicator = *It;

			bool bHasMoreUnmapped = false;

			Replicator->UpdateUnmappedObjects( bHasMoreUnmapped );

			if ( !bHasMoreUnmapped )
			{
				// If there are no more unmapped objects, we can also stop checking
				It.RemoveCurrent();
			}
		}
	}

	// Go over RepChangedPropertyTrackerMap periodically, and remove entries that no longer have valid objects
	// Unfortunately if you mark an object as pending kill, it will no longer find itself in this map,
	// so we do this as a fail safe to make sure we never leak memory from this map
	const double CleanupTimeSeconds = 10.0;

	if ( CurrentRealtimeSeconds - LastCleanupTime > CleanupTimeSeconds )
	{
		for ( auto It = RepChangedPropertyTrackerMap.CreateIterator(); It; ++It )
		{
			if ( !It.Key().IsValid() )
			{
				It.RemoveCurrent();
			}
		}

		for ( auto It = ReplicationChangeListMap.CreateIterator(); It; ++It )
		{
			if ( !It.Key().IsValid() )
			{
				It.RemoveCurrent();
			}
		}

		LastCleanupTime = CurrentRealtimeSeconds;
	}

	// Update the lag state
	UpdateNetworkLagState();
}

void UNetDriver::UpdateNetworkLagState()
{
	ENetworkLagState::Type OldLagState = LagState;

	// Percentage of the timeout time that a connection is considered "lagging"
	const float TimeoutPercentThreshold = 0.75f;

	if ( IsServer() )
	{
		// Server network lag detection

		// See if all clients connected to us are lagging. If so there might be network connection problems.
		// Only trigger this if there are a few connections since a single client could have just crashed or disconnected suddenly,
		// and is less likely to happen with multiple clients simultaneously.
		int32 NumValidConnections = 0;
		int32 NumLaggingConnections = 0;
		for (UNetConnection* Connection : ClientConnections)
		{
			if (Connection)
			{
				NumValidConnections++;

				const float HalfTimeout = Connection->GetTimeoutValue() * TimeoutPercentThreshold;
				const float DeltaTimeSinceLastMessage = Time - Connection->LastReceiveTime;
				if (DeltaTimeSinceLastMessage > HalfTimeout)
				{
					NumLaggingConnections++;
				}
			}
		}

		if (NumValidConnections >= 2 && NumValidConnections == NumLaggingConnections)
		{
			// All connections that we could measure are lagging and there are enough to know it is not likely the fault of the clients.
			LagState = ENetworkLagState::Lagging;
		}
		else
		{
			// We have at least one non-lagging client or we don't have enough clients to know if the server is lagging.
			LagState = ENetworkLagState::NotLagging;
		}
	}
	else
	{
		// Client network lag detection.
		
		// Just check the server connection.
		if (ensure(ServerConnection))
		{
			const float HalfTimeout = ServerConnection->GetTimeoutValue() * TimeoutPercentThreshold;
			const float DeltaTimeSinceLastMessage = Time - ServerConnection->LastReceiveTime;
			if (DeltaTimeSinceLastMessage > HalfTimeout)
			{
				// We have exceeded half our timeout. We are lagging.
				LagState = ENetworkLagState::Lagging;
			}
			else
			{
				// Not lagging yet. We have received a message recently.
				LagState = ENetworkLagState::NotLagging;
			}
		}
	}

	if (OldLagState != LagState)
	{
		GEngine->BroadcastNetworkLagStateChanged(GetWorld(), this, LagState);
	}
}

/**
 * Determines which other connections should receive the voice packet and
 * queues the packet for those connections. Used for sending both local/remote voice packets.
 *
 * @param VoicePacket the packet to be queued
 * @param CameFromConn the connection this packet came from (NULL if local)
 */
void UNetDriver::ReplicateVoicePacket(TSharedPtr<FVoicePacket> VoicePacket, UNetConnection* CameFromConn)
{
	// Iterate the connections and see if they want the packet
	for (int32 Index = 0; Index < ClientConnections.Num(); Index++)
	{
		UNetConnection* Conn = ClientConnections[Index];
		// Skip the originating connection
		if (CameFromConn != Conn)
		{
			// If server then determine if it should replicate the voice packet from another sender to this connection
			const bool bReplicateAsServer = !bIsPeer && Conn->ShouldReplicateVoicePacketFrom(*VoicePacket->GetSender());
			// If client peer then determine if it should send the voice packet to another client peer
			//const bool bReplicateAsPeer = (bIsPeer && AllowPeerVoice) && Conn->ShouldReplicateVoicePacketToPeer(Conn->PlayerId);

			if (bReplicateAsServer)// || bReplicateAsPeer)
			{
				UVoiceChannel* VoiceChannel = Conn->GetVoiceChannel();
				if (VoiceChannel != NULL)
				{
					// Add the voice packet for network sending
					VoiceChannel->AddVoicePacket(VoicePacket);
				}
			}
		}
	}
}

/**
 * Process any local talker packets that need to be sent to clients
 */
void UNetDriver::ProcessLocalServerPackets()
{
	if (World)
	{
		int32 NumLocalTalkers = UOnlineEngineInterface::Get()->GetNumLocalTalkers(World);
		// Process all of the local packets
		for (int32 Index = 0; Index < NumLocalTalkers; Index++)
		{
			// Returns a ref counted copy of the local voice data or NULL if nothing to send
			TSharedPtr<FVoicePacket> LocalPacket = UOnlineEngineInterface::Get()->GetLocalPacket(World, Index);
			// Check for something to send for this local talker
			if (LocalPacket.IsValid())
			{
				// See if anyone wants this packet
				ReplicateVoicePacket(LocalPacket, NULL);

				// once all local voice packets are processed then call ClearVoicePackets()
			}
		}
	}
}

/**
 * Process any local talker packets that need to be sent to the server
 */
void UNetDriver::ProcessLocalClientPackets()
{
	if (World)
	{
		int32 NumLocalTalkers = UOnlineEngineInterface::Get()->GetNumLocalTalkers(World);
		if (NumLocalTalkers)
		{
			UVoiceChannel* VoiceChannel = ServerConnection->GetVoiceChannel();
			if (VoiceChannel)
			{
				// Process all of the local packets
				for (int32 Index = 0; Index < NumLocalTalkers; Index++)
				{
					// Returns a ref counted copy of the local voice data or NULL if nothing to send
					TSharedPtr<FVoicePacket> LocalPacket = UOnlineEngineInterface::Get()->GetLocalPacket(World, Index);
					// Check for something to send for this local talker
					if (LocalPacket.IsValid())
					{
						// If there is a voice channel to the server, submit the packets
						//if (ShouldSendVoicePacketsToServer())
						{
							// Add the voice packet for network sending
							VoiceChannel->AddVoicePacket(LocalPacket);
						}

						// once all local voice packets are processed then call ClearLocalVoicePackets()
					}
				}
			}
		}
	}
}

void UNetDriver::PostTickFlush()
{
	if (World)
	{
		UOnlineEngineInterface::Get()->ClearVoicePackets(World);
	}
}

bool UNetDriver::InitConnectionClass(void)
{
	if (NetConnectionClass == NULL && NetConnectionClassName != TEXT(""))
	{
		NetConnectionClass = LoadClass<UNetConnection>(NULL,*NetConnectionClassName,NULL,LOAD_None,NULL);
		if (NetConnectionClass == NULL)
		{
			UE_LOG(LogNet, Error,TEXT("Failed to load class '%s'"),*NetConnectionClassName);
		}
	}
	return NetConnectionClass != NULL;
}

bool UNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	LastTickDispatchRealtime = FPlatformTime::Seconds();
	bool bSuccess = InitConnectionClass();

	if (!bInitAsClient)
	{
		ConnectionlessHandler.Reset(nullptr);
	}

	Notify = InNotify;

	return bSuccess;
}

void UNetDriver::InitConnectionlessHandler()
{
	check(!ConnectionlessHandler.IsValid());

#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("NoPacketHandler")))
#endif
	{
		ConnectionlessHandler = MakeUnique<PacketHandler>();

		if (ConnectionlessHandler.IsValid())
		{
			ConnectionlessHandler->bConnectionlessHandler = true;

			ConnectionlessHandler->Initialize(Handler::Mode::Server, MAX_PACKET_SIZE, true);

			// Add handling for the stateless connect handshake, for connectionless packets, as the outermost layer
			TSharedPtr<HandlerComponent> NewComponent =
				ConnectionlessHandler->AddHandler(TEXT("Engine.EngineHandlerComponentFactory(StatelessConnectHandlerComponent)"), true);

			StatelessConnectComponent = StaticCastSharedPtr<StatelessConnectHandlerComponent>(NewComponent);

			if (StatelessConnectComponent.IsValid())
			{
				StatelessConnectComponent.Pin()->SetDriver(this);
			}

			ConnectionlessHandler->InitializeComponents();
		}
	}
}

void UNetDriver::FlushHandler()
{
	BufferedPacket* QueuedPacket = ConnectionlessHandler->GetQueuedConnectionlessPacket();

	while (QueuedPacket != nullptr)
	{
		LowLevelSend(QueuedPacket->Address, QueuedPacket->Data, QueuedPacket->CountBits);

		delete QueuedPacket;

		QueuedPacket = ConnectionlessHandler->GetQueuedConnectionlessPacket();
	}
}

ENetMode UNetDriver::GetNetMode() const
{
	// Special case for PIE - forcing dedicated server behavior
#if WITH_EDITOR
	if (World && World->WorldType == EWorldType::PIE && IsServer())
	{
		if ( GEngine->GetWorldContextFromWorldChecked(World).RunAsDedicated )
		{
			return NM_DedicatedServer;
		}
	}
#endif

	// Normal
	return (IsServer() ? (GIsClient ? NM_ListenServer : NM_DedicatedServer) : NM_Client);
}

void UNetDriver::RegisterTickEvents(class UWorld* InWorld)
{
	if (InWorld)
	{
		TickDispatchDelegateHandle  = InWorld->OnTickDispatch ().AddUObject(this, &UNetDriver::TickDispatch);
		TickFlushDelegateHandle     = InWorld->OnTickFlush    ().AddUObject(this, &UNetDriver::TickFlush);
		PostTickFlushDelegateHandle = InWorld->OnPostTickFlush().AddUObject(this, &UNetDriver::PostTickFlush);
	}
}

void UNetDriver::UnregisterTickEvents(class UWorld* InWorld)
{
	if (InWorld)
	{
		InWorld->OnTickDispatch ().Remove(TickDispatchDelegateHandle);
		InWorld->OnTickFlush    ().Remove(TickFlushDelegateHandle);
		InWorld->OnPostTickFlush().Remove(PostTickFlushDelegateHandle);
	}
}

/** Shutdown all connections managed by this net driver */
void UNetDriver::Shutdown()
{
	// Client closing connection to server
	if (ServerConnection)
	{
		// Calls Channel[0]->Close to send a close bunch to server
		ServerConnection->Close();
		ServerConnection->FlushNet();
	}

	// Server closing connections with clients
	if (ClientConnections.Num() > 0)
	{
		for (int32 ClientIndex = 0; ClientIndex < ClientConnections.Num(); ClientIndex++)
		{
			FString ErrorMsg = NSLOCTEXT("NetworkErrors", "HostClosedConnection", "Host closed the connection.").ToString();
			FNetControlMessage<NMT_Failure>::Send(ClientConnections[ClientIndex], ErrorMsg);
			ClientConnections[ClientIndex]->FlushNet(true);
		}

		for (int32 ClientIndex = ClientConnections.Num() - 1; ClientIndex >= 0; ClientIndex--)
		{
			if (ClientConnections[ClientIndex]->PlayerController)
			{
				APawn* Pawn = ClientConnections[ClientIndex]->PlayerController->GetPawn();
				if( Pawn )
				{
					Pawn->Destroy( true );
				}
			}

			// Calls Close() internally and removes from ClientConnections
			ClientConnections[ClientIndex]->CleanUp();
		}
	}

	// Empty our replication map here before we're destroyed, 
	// even though we use AddReferencedObjects to keep the referenced properties
	// in here from being collected, when we're all GC'd the order seems non-deterministic
	RepLayoutMap.Empty();
	ReplicationChangeListMap.Empty();

	ConnectionlessHandler.Reset(nullptr);

#if DO_ENABLE_NET_TEST
	PacketSimulationSettings.UnregisterCommands();
#endif
}

bool UNetDriver::IsServer() const
{
	// Client connections ALWAYS set the server connection object in InitConnect()
	// @todo ONLINE improve this with a bool
	return ServerConnection == NULL;
}

void UNetDriver::TickDispatch( float DeltaTime )
{
	SendCycles=RecvCycles=0;

	const double CurrentRealtime = FPlatformTime::Seconds();

	const float DeltaRealtime = CurrentRealtime - LastTickDispatchRealtime;

	LastTickDispatchRealtime = CurrentRealtime;

	// Check to see if too much time is passing between ticks
	// Setting this to somewhat large value for now, but small enough to catch blocking calls that are causing timeouts
	const float TickLogThreshold = 5.0f;

	if ( DeltaTime > TickLogThreshold || DeltaRealtime > TickLogThreshold )
	{
		UE_LOG( LogNet, Log, TEXT( "UNetDriver::TickDispatch: Very long time between ticks. DeltaTime: %2.2f, Realtime: %2.2f. %s" ), DeltaTime, DeltaRealtime, *GetName() );
	}

	// Get new time.
	Time += DeltaTime;

	// Checks for standby cheats if enabled
	UpdateStandbyCheatStatus();

	// Delete any straggler connections.
	if( !ServerConnection )
	{
		for( int32 i=ClientConnections.Num()-1; i>=0; i-- )
		{
			if( ClientConnections[i]->State==USOCK_Closed )
			{
				ClientConnections[i]->CleanUp();
			}
		}
	}
}

bool UNetDriver::IsLevelInitializedForActor(const AActor* InActor, const UNetConnection* InConnection) const
{
	check(InActor);
	check(InConnection);
	check(World == InActor->GetWorld());

	// we can't create channels while the client is in the wrong world
	const bool bCorrectWorld = (InConnection->ClientWorldPackageName == World->GetOutermost()->GetFName() && InConnection->ClientHasInitializedLevelFor(InActor));
	// exception: Special case for PlayerControllers as they are required for the client to travel to the new world correctly			
	const bool bIsConnectionPC = (InActor == InConnection->PlayerController);
	return bCorrectWorld || bIsConnectionPC;
}

//
// Internal RPC calling.
//
void UNetDriver::InternalProcessRemoteFunction
	(
	AActor*			Actor,
	UObject*		SubObject,
	UNetConnection*	Connection,
	UFunction*		Function,
	void*			Parms,
	FOutParmRec*	OutParms,
	FFrame*			Stack,
	bool			IsServer
	)
{
	// get the top most function
	while( Function->GetSuperFunction() )
	{
		Function = Function->GetSuperFunction();
	}

	// If saturated and function is unimportant, skip it. Note unreliable multicasts are queued at the actor channel level so they are not gated here.
	if( !(Function->FunctionFlags & FUNC_NetReliable) && (!(Function->FunctionFlags & FUNC_NetMulticast)) && !Connection->IsNetReady(0) )
	{
		DEBUG_REMOTEFUNCTION(TEXT("Network saturated, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
		return;
	}

	// Route RPC calls to actual connection
	if (Connection->GetUChildConnection())
	{
		Connection = ((UChildConnection*)Connection)->Parent;
	}

	// Prevent RPC calls to closed connections
	if (Connection->State == USOCK_Closed)
	{
		DEBUG_REMOTEFUNCTION(TEXT("Attempting to call RPC on a closed connection. Not calling %s::%s"), *Actor->GetName(), *Function->GetName());
		return;
	}

	// If we have a subobject, thats who we are actually calling this on. If no subobject, we are calling on the actor.
	UObject* TargetObj = SubObject ? SubObject : Actor;

	// Make sure this function exists for both parties.
	const FClassNetCache* ClassCache = NetCache->GetClassNetCache( TargetObj->GetClass() );
	if (!ClassCache)
	{
		DEBUG_REMOTEFUNCTION(TEXT("ClassNetCache empty, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
		return;
	}
		
	const FFieldNetCache* FieldCache = ClassCache->GetFromField( Function );

	if ( !FieldCache )
	{
		DEBUG_REMOTEFUNCTION(TEXT("FieldCache empty, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
		return;
	}
		
	// Get the actor channel.
	UActorChannel* Ch = Connection->ActorChannels.FindRef(Actor);
	if( !Ch )
	{
		if( IsServer )
		{
			if ( Actor->IsPendingKillPending() )
			{
				// Don't try opening a channel for me, I am in the process of being destroyed. Ignore my RPCs.
				return;
			}

			if (IsLevelInitializedForActor(Actor, Connection))
			{
				Ch = (UActorChannel *)Connection->CreateChannel( CHTYPE_Actor, 1 );
			}
			else
			{
				UE_LOG(LogNet, Verbose, TEXT("Can't send function '%s' on actor '%s' because client hasn't loaded the level '%s' containing it"), *Function->GetName(), *Actor->GetName(), *Actor->GetLevel()->GetName());
				return;
			}
		}
		if (!Ch)
		{
			return;
		}
		if (IsServer)
		{
			Ch->SetChannelActor(Actor);
		}	
	}

	// Make sure initial channel-opening replication has taken place.
	if( Ch->OpenPacketId.First==INDEX_NONE )
	{
		if (!IsServer)
		{
			DEBUG_REMOTEFUNCTION(TEXT("Initial channel replication has not occurred, not calling %s::%s"), *Actor->GetName(), *Function->GetName());
			return;
		}

		// triggering replication of an Actor while already in the middle of replication can result in invalid data being sent and is therefore illegal
		if (Ch->bIsReplicatingActor)
		{
			FString Error(FString::Printf(TEXT("Attempt to replicate function '%s' on Actor '%s' while it is in the middle of variable replication!"), *Function->GetName(), *Actor->GetName()));
			UE_LOG(LogScript, Error, TEXT("%s"), *Error);
			ensureMsgf(false, *Error);
			return;
		}

		// Bump the ReplicationFrame value to invalidate any properties marked as "unchanged" for this frame.
		ReplicationFrame++;
		
		Ch->GetActor()->CallPreReplication(this);
		Ch->ReplicateActor();
	}

	// Clients may be "closing" this connection but still processing bunches, we can't send anything if we have an invalid ChIndex.
	if (Ch->ChIndex == -1)
	{
		ensure(!IsServer);
		return;
	}

	// Form the RPC preamble.
	FOutBunch Bunch( Ch, 0 );

	// Reliability.
	//warning: RPC's might overflow, preventing reliable functions from getting thorough.
	if (Function->FunctionFlags & FUNC_NetReliable)
	{
		Bunch.bReliable = 1;
	}

	// verify we haven't overflowed unacked bunch buffer (Connection is not net ready)
	//@warning: needs to be after parameter evaluation for script stack integrity
	if ( Bunch.IsError() )
	{
		if ( !Bunch.bReliable )
		{
			// Not reliable, so not fatal. This can happen a lot in debug builds at startup if client is slow to get in game
			UE_LOG( LogNet, Warning, TEXT( "Can't send function '%s' on '%s': Reliable buffer overflow. FieldCache->FieldNetIndex: %d Max %d. Ch MaxPacket: %d" ), *Function->GetName(), *Actor->GetName(), FieldCache->FieldNetIndex, ClassCache->GetMaxIndex(), Ch->Connection->MaxPacket );
		}
		else
		{
			// The connection has overflowed the reliable buffer. We cannot recover from this. Disconnect this user.
			UE_LOG( LogNet, Warning, TEXT( "Closing connection. Can't send function '%s' on '%s': Reliable buffer overflow. FieldCache->FieldNetIndex: %d Max %d. Ch MaxPacket: %d." ), *Function->GetName(), *Actor->GetName(), FieldCache->FieldNetIndex, ClassCache->GetMaxIndex(), Ch->Connection->MaxPacket );

			FString ErrorMsg = NSLOCTEXT( "NetworkErrors", "ClientReliableBufferOverflow", "Outgoing reliable buffer overflow" ).ToString();
			FNetControlMessage<NMT_Failure>::Send( Connection, ErrorMsg );
			Connection->FlushNet( true );
			Connection->Close();

			PerfCountersIncrement( TEXT( "ClosedConnectionsDueToReliableBufferOverflow" ) );
		}
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	extern TAutoConsoleVariable< int32 > CVarNetReliableDebug;

	if ( CVarNetReliableDebug.GetValueOnAnyThread() > 0 )
	{
		Bunch.DebugString = FString::Printf( TEXT( "%.2f RPC: %s - %s" ), Connection->Driver->Time, *Actor->GetName(), *Function->GetName() );
	}
#endif

	TArray< UProperty * > LocalOutParms;

	if( Stack == nullptr )
	{
		// Look for CPF_OutParm's, we'll need to copy these into the local parameter memory manually
		// The receiving side will pull these back out when needed
		for ( TFieldIterator<UProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
		{
			if ( It->HasAnyPropertyFlags( CPF_OutParm ) )
			{
				if ( OutParms == NULL )
				{
					UE_LOG( LogNet, Warning, TEXT( "Missing OutParms. Property: %s, Function: %s, Actor: %s" ), *It->GetName(), *Function->GetName(), *Actor->GetName() );
					continue;
				}

				FOutParmRec * Out = OutParms;

				checkSlow( Out );

				while ( Out->Property != *It )
				{
					Out = Out->NextOutParm;
					checkSlow( Out );
				}

				void* Dest = It->ContainerPtrToValuePtr< void >( Parms );

				const int32 CopySize = It->ElementSize * It->ArrayDim;

				check( ( (uint8*)Dest - (uint8*)Parms ) + CopySize <= Function->ParmsSize );

				It->CopyCompleteValue( Dest, Out->PropAddr );

				LocalOutParms.Add( *It );
			}
		}
	}

	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("net.RPC.Debug"));
	const bool LogAsWarning = (CVar && CVar->GetValueOnAnyThread() == 1);

	FNetBitWriter TempWriter( Bunch.PackageMap, 0 );

	// Use the replication layout to send the rpc parameter values
	TSharedPtr<FRepLayout> RepLayout = GetFunctionRepLayout( Function );
	RepLayout->SendPropertiesForRPC( Actor, Function, Ch, TempWriter, Parms );

	if (TempWriter.IsError())
	{
		if (LogAsWarning)
		{
			UE_LOG(LogNet, Warning, TEXT("Error: Can't send function '%s' on '%s': Failed to serialize properties"), *Function->GetName(), *TargetObj->GetFullName());
		}
		else
		{
			UE_LOG(LogNet, Log, TEXT("Error: Can't send function '%s' on '%s': Failed to serialize properties"), *Function->GetName(), *TargetObj->GetFullName());
		}
	}
	else
	{
		// Make sure net field export group is registered
		FNetFieldExportGroup* NetFieldExportGroup = Ch->GetOrCreateNetFieldExportGroupForClassNetCache( TargetObj );

		int32 HeaderBits	= 0;
		int32 ParameterBits	= 0;

		// Queue unreliable multicast 
		const bool QueueBunch = ( !Bunch.bReliable && Function->FunctionFlags & FUNC_NetMulticast );

		if ( QueueBunch )
		{
			Ch->WriteFieldHeaderAndPayload( Bunch, ClassCache, FieldCache, NetFieldExportGroup, TempWriter );
			ParameterBits = Bunch.GetNumBits();
		}
		else
		{
			FNetBitWriter TempBlockWriter( Bunch.PackageMap, 0 );
			Ch->WriteFieldHeaderAndPayload( TempBlockWriter, ClassCache, FieldCache, NetFieldExportGroup, TempWriter );
			ParameterBits = TempBlockWriter.GetNumBits();
			HeaderBits = Ch->WriteContentBlockPayload( TargetObj, Bunch, false, TempBlockWriter );
		}

		// Destroy the memory used for the copied out parameters
		for ( int32 i = 0; i < LocalOutParms.Num(); i++ )
		{
			check( LocalOutParms[i]->HasAnyPropertyFlags( CPF_OutParm ) );
			LocalOutParms[i]->DestroyValue_InContainer( Parms );
		}

		// Send the bunch.
		if( Bunch.IsError() )
		{
			UE_LOG(LogNet, Log, TEXT("Error: Can't send function '%s' on '%s': RPC bunch overflowed (too much data in parameters?)"), *Function->GetName(), *TargetObj->GetFullName());
			ensureMsgf(false,TEXT("Error: Can't send function '%s' on '%s': RPC bunch overflowed (too much data in parameters?)"), *Function->GetName(), *TargetObj->GetFullName());
		}
		else if (Ch->Closing)
		{
			UE_LOG(LogNetTraffic, Log, TEXT("RPC bunch on closing channel") );
		}
		else
		{
			// Make sure we're tracking all the bits in the bunch
			check(Bunch.GetNumBits() == HeaderBits + ParameterBits);

			if (QueueBunch)
			{
				// Unreliable multicast functions are queued and sent out during property replication
				if (LogAsWarning)
				{
					UE_LOG(LogNetTraffic, Warning,	TEXT("      Queing unreliable multicast RPC: %s::%s [%.1f bytes]"), *Actor->GetName(), *Function->GetName(), Bunch.GetNumBits() / 8.f );
				}
				else
				{
					UE_LOG(LogNetTraffic, Log,		TEXT("      Queing unreliable multicast RPC: %s::%s [%.1f bytes]"), *Actor->GetName(), *Function->GetName(), Bunch.GetNumBits() / 8.f );
				}

				NETWORK_PROFILER(GNetworkProfiler.TrackQueuedRPC(Connection, TargetObj, Actor, Function, HeaderBits, ParameterBits, 0));
				Ch->QueueRemoteFunctionBunch(TargetObj, Function, Bunch);
			}
			else
			{
				if (LogAsWarning)
				{
					UE_LOG(LogNetTraffic, Warning,	TEXT("      Sent RPC: %s::%s [%.1f bytes]"), *Actor->GetName(), *Function->GetName(), Bunch.GetNumBits() / 8.f );
				}
				else
				{
					UE_LOG(LogNetTraffic, Log,		TEXT("      Sent RPC: %s::%s [%.1f bytes]"), *Actor->GetName(), *Function->GetName(), Bunch.GetNumBits() / 8.f );
				}

				NETWORK_PROFILER(GNetworkProfiler.TrackSendRPC(Actor, Function, HeaderBits, ParameterBits, 0, Connection));
				Ch->SendBunch( &Bunch, 1 );
			}
		}
	}

	if ( Connection->InternalAck )
	{
		Connection->FlushNet();
	}
}

void UNetDriver::UpdateStandbyCheatStatus(void)
{
#if WITH_SERVER_CODE
	// Only the server needs to check
	if (ServerConnection == NULL && ClientConnections.Num())
	{
		// Only check for cheats if enabled and one wasn't previously detected
		if (bIsStandbyCheckingEnabled &&
			bHasStandbyCheatTriggered == false &&
			ClientConnections.Num() > 2)
		{
			int32 CountBadTx = 0;
			int32 CountBadRx = 0;
			int32 CountBadPing = 0;
			
			UWorld* FoundWorld = NULL;
			// Look at each connection checking for a receive time and an ack time
			for (int32 Index = 0; Index < ClientConnections.Num(); Index++)
			{
				UNetConnection* NetConn = ClientConnections[Index];
				// Don't check connections that aren't fully formed (still loading & no controller)
				// Controller won't be present until the join message is sent, which is after loading has completed
				if (NetConn)
				{
					APlayerController* PlayerController = NetConn->PlayerController;
					if(PlayerController)
					{
						if( PlayerController->GetWorld() && 
							PlayerController->GetWorld()->GetTimeSeconds() - PlayerController->CreationTime > JoinInProgressStandbyWaitTime &&
							// Ignore players with pending delete (kicked/timed out, but connection not closed)
							PlayerController->IsPendingKillPending() == false)
						{
							if (!FoundWorld)
							{
								FoundWorld = PlayerController->GetWorld();
							}
							else
							{
								check(FoundWorld == PlayerController->GetWorld());
							}
							if (Time - NetConn->LastReceiveTime > StandbyRxCheatTime)
							{
								CountBadRx++;
							}
							if (Time - NetConn->LastRecvAckTime > StandbyTxCheatTime)
							{
								CountBadTx++;
							}
							// Check for host tampering or crappy upstream bandwidth
							if (PlayerController->PlayerState &&
								PlayerController->PlayerState->Ping * 4 > BadPingThreshold)
							{
								CountBadPing++;
							}
						}
					}
				}
			}
			
			if (FoundWorld)
			{
				AGameNetworkManager* const NetworkManager = FoundWorld->NetworkManager;
				if (NetworkManager)
				{
					// See if we hit the percentage required for either TX or RX standby detection
					if (float(CountBadRx) / float(ClientConnections.Num()) > PercentMissingForRxStandby)
					{
						bHasStandbyCheatTriggered = true;
						NetworkManager->StandbyCheatDetected(STDBY_Rx);
					}
					else if (float(CountBadPing) / float(ClientConnections.Num()) > PercentForBadPing)
					{
						bHasStandbyCheatTriggered = true;
						NetworkManager->StandbyCheatDetected(STDBY_BadPing);
					}
					// Check for the host not sending to the clients
					else if (float(CountBadTx) / float(ClientConnections.Num()) > PercentMissingForTxStandby)
					{
						bHasStandbyCheatTriggered = true;
						NetworkManager->StandbyCheatDetected(STDBY_Tx);
					}
				}
			}
		}
	}
#endif
}

void UNetDriver::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	// Prevent referenced objects from being garbage collected.
	Ar << ClientConnections << ServerConnection << RoleProperty << RemoteRoleProperty;

	if (Ar.IsCountingMemory())
	{
		ClientConnections.CountBytes(Ar);
	}
}

void UNetDriver::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Make sure we tell listeners we are no longer lagging in case they set something up when lagging started.
		if (GEngine && LagState != ENetworkLagState::NotLagging)
		{
			LagState = ENetworkLagState::NotLagging;
			GEngine->BroadcastNetworkLagStateChanged(GetWorld(), this, LagState);
		}

		// Destroy server connection.
		if( ServerConnection )
		{
			ServerConnection->CleanUp();
		}
		// Destroy client connections.
		while( ClientConnections.Num() )
		{
			UNetConnection* ClientConnection = ClientConnections[0];
			ClientConnection->CleanUp();
		}
		// Low level destroy.
		LowLevelDestroy();

		// Delete the guid cache
		GuidCache.Reset();

		FWorldDelegates::LevelRemovedFromWorld.Remove(OnLevelRemovedFromWorldHandle);
	}
	else
	{
		check(ServerConnection==NULL);
		check(ClientConnections.Num()==0);
		check(!GuidCache.IsValid());
	}

	// Make sure we've properly shut down all of the FObjectReplicator's
	check( GuidToReplicatorMap.Num() == 0 );
	check( TotalTrackedGuidMemoryBytes == 0 );
	check( UnmappedReplicators.Num() == 0 );

	Super::FinishDestroy();
}

void UNetDriver::LowLevelDestroy()
{
	// We are closing down all our sockets and low level communications.
	// Sever the link with UWorld to ensure we don't tick again
	SetWorld(NULL);
}


#if !UE_BUILD_SHIPPING
bool UNetDriver::HandleSocketsCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Print list of open connections.
	Ar.Logf( TEXT("%s Connections:"), *GetDescription() );
	if( ServerConnection )
	{
		Ar.Logf( TEXT("   Server %s"), *ServerConnection->LowLevelDescribe() );
		for( int32 i=0; i<ServerConnection->OpenChannels.Num(); i++ )
		{
			Ar.Logf( TEXT("      Channel %i: %s"), ServerConnection->OpenChannels[i]->ChIndex, *ServerConnection->OpenChannels[i]->Describe() );
		}
	}
#if WITH_SERVER_CODE
	for( int32 i=0; i<ClientConnections.Num(); i++ )
	{
		UNetConnection* Connection = ClientConnections[i];
		Ar.Logf( TEXT("   Client %s"), *Connection->LowLevelDescribe() );
		for( int32 j=0; j<Connection->OpenChannels.Num(); j++ )
		{
			Ar.Logf( TEXT("      Channel %i: %s"), Connection->OpenChannels[j]->ChIndex, *Connection->OpenChannels[j]->Describe() );
		}
	}
#endif
	return true;
}

bool UNetDriver::HandlePackageMapCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Print packagemap for open connections
	Ar.Logf(TEXT("Package Map:"));
	if (ServerConnection != NULL)
	{
		Ar.Logf(TEXT("   Server %s"), *ServerConnection->LowLevelDescribe());
		ServerConnection->PackageMap->LogDebugInfo(Ar);
	}
#if WITH_SERVER_CODE
	for (int32 i = 0; i < ClientConnections.Num(); i++)
	{
		UNetConnection* Connection = ClientConnections[i];
		Ar.Logf( TEXT("   Client %s"), *Connection->LowLevelDescribe() );
		Connection->PackageMap->LogDebugInfo(Ar);
	}
#endif
	return true;
}

bool UNetDriver::HandleNetFloodCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	UNetConnection* TestConn = NULL;
	if (ServerConnection != NULL)
	{
		TestConn = ServerConnection;
	}
#if WITH_SERVER_CODE
	else if (ClientConnections.Num() > 0)
	{
		TestConn = ClientConnections[0];
	}
#endif
	if (TestConn != NULL)
	{
		Ar.Logf(TEXT("Flooding connection 0 with control messages"));

		for (int32 i = 0; i < 256 && TestConn->State == USOCK_Open; i++)
		{
			FNetControlMessage<NMT_Netspeed>::Send(TestConn, TestConn->CurrentNetSpeed);
			TestConn->FlushNet();
		}
	}
	return true;
}

bool UNetDriver::HandleNetDebugTextCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	// Send a text string for testing connection
	FString TestStr = FParse::Token(Cmd,false);
	if (ServerConnection != NULL)
	{
		UE_LOG(LogNet, Log, TEXT("%s sending NMT_DebugText [%s] to [%s]"), 
			*GetDescription(),*TestStr, *ServerConnection->LowLevelDescribe());

		FNetControlMessage<NMT_DebugText>::Send(ServerConnection,TestStr);
		ServerConnection->FlushNet(true);
	}
#if WITH_SERVER_CODE
	else
	{
		for (int32 ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
		{
			UNetConnection* Connection = ClientConnections[ClientIdx];
			if (Connection)
			{
				UE_LOG(LogNet, Log, TEXT("%s sending NMT_DebugText [%s] to [%s]"), 
					*GetDescription(),*TestStr, *Connection->LowLevelDescribe());

				FNetControlMessage<NMT_DebugText>::Send(Connection,TestStr);
				Connection->FlushNet(true);
			}
		}
	}
#endif // WITH_SERVER_CODE
	return true;
}

bool UNetDriver::HandleNetDisconnectCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	FString Msg = NSLOCTEXT("NetworkErrors", "NETDISCONNECTMSG", "NETDISCONNECT MSG").ToString();
	if (ServerConnection != NULL)
	{
		UE_LOG(LogNet, Log, TEXT("%s disconnecting connection from host [%s]"), 
			*GetDescription(),*ServerConnection->LowLevelDescribe());

		FNetControlMessage<NMT_Failure>::Send(ServerConnection, Msg);
	}
#if WITH_SERVER_CODE
	else
	{
		for (int32 ClientIdx=0; ClientIdx < ClientConnections.Num(); ClientIdx++)
		{
			UNetConnection* Connection = ClientConnections[ClientIdx];
			if (Connection)
			{
				UE_LOG(LogNet, Log, TEXT("%s disconnecting from client [%s]"), 
					*GetDescription(),*Connection->LowLevelDescribe());

				FNetControlMessage<NMT_Failure>::Send(Connection, Msg);
				Connection->FlushNet(true);
			}
		}
	}
#endif
	return true;
}

bool UNetDriver::HandleNetDumpServerRPCCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
#if WITH_SERVER_CODE
	for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
	{
		bool bHasNetFields = false;

		for ( int32 i = 0; i < ClassIt->NetFields.Num(); i++ )
		{
			UFunction * Function = Cast<UFunction>( ClassIt->NetFields[i] );

			if ( Function != NULL && Function->FunctionFlags & FUNC_NetServer )
			{
				bHasNetFields = true;
				break;
			}
		}

		if ( !bHasNetFields )
		{
			continue;
		}

		Ar.Logf( TEXT( "Class: %s" ), *ClassIt->GetName() );

		for ( int32 i = 0; i < ClassIt->NetFields.Num(); i++ )
		{
			UFunction * Function = Cast<UFunction>( ClassIt->NetFields[i] );

			if ( Function != NULL && Function->FunctionFlags & FUNC_NetServer )
			{
				const FClassNetCache * ClassCache = NetCache->GetClassNetCache( *ClassIt );

				const FFieldNetCache * FieldCache = ClassCache->GetFromField( Function );

				TArray< UProperty * > Parms;

				for ( TFieldIterator<UProperty> It( Function ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
				{
					Parms.Add( *It );
				}

				if ( Parms.Num() == 0 )
				{
					Ar.Logf( TEXT( "    [0x%03x] %s();" ), FieldCache->FieldNetIndex, *Function->GetName() );
					continue;
				}

				FString ParmString;

				for ( int32 j = 0; j < Parms.Num(); j++ )
				{
					if ( Cast<UStructProperty>( Parms[j] ) )
					{
						ParmString += Cast<UStructProperty>( Parms[j] )->Struct->GetName();
					}
					else
					{
						ParmString += Parms[j]->GetClass()->GetName();
					}

					ParmString += TEXT( " " );

					ParmString += Parms[j]->GetName();

					if ( j < Parms.Num() - 1 )
					{
						ParmString += TEXT( ", " );
					}
				}						

				Ar.Logf( TEXT( "    [0x%03x] %s( %s );" ), FieldCache->FieldNetIndex, *Function->GetName(), *ParmString );
			}
		}
	}
#endif
	return true;
}

#endif // !UE_BUILD_SHIPPING

bool UNetDriver::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
#if !UE_BUILD_SHIPPING
	if( FParse::Command(&Cmd,TEXT("SOCKETS")) )
	{
		return HandleSocketsCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("PACKAGEMAP")))
	{
		return HandlePackageMapCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("NETFLOOD")))
	{
		return HandleNetFloodCommand( Cmd, Ar );
	}
#if DO_ENABLE_NET_TEST
	// This will allow changing the Pkt* options at runtime
	else if (PacketSimulationSettings.ParseSettings(Cmd, *NetDriverName.ToString()))
	{
		if (ServerConnection)
		{
			// Notify the server connection of the change
			ServerConnection->UpdatePacketSimulationSettings();
		}
#if WITH_SERVER_CODE
		else
		{
			// Notify all client connections that the settings have changed
			for (int32 Index = 0; Index < ClientConnections.Num(); Index++)
			{
				ClientConnections[Index]->UpdatePacketSimulationSettings();
			}
		}		
#endif
		return true;
	}
#endif
	else if (FParse::Command(&Cmd, TEXT("NETDEBUGTEXT")))
	{
		return HandleNetDebugTextCommand( Cmd, Ar );
	}
	else if (FParse::Command(&Cmd, TEXT("NETDISCONNECT")))
	{
		return HandleNetDisconnectCommand( Cmd, Ar );	
	}
	else if (FParse::Command(&Cmd, TEXT("DUMPSERVERRPC")))
	{
		return HandleNetDumpServerRPCCommand( Cmd, Ar );	
	}
	else
#endif // !UE_BUILD_SHIPPING
	{
		return false;
	}
}

FActorDestructionInfo *	CreateDestructionInfo( UNetDriver * NetDriver, AActor* ThisActor, FActorDestructionInfo *DestructionInfo )
{
	if (DestructionInfo)
		return DestructionInfo;

	FNetworkGUID NetGUID = NetDriver->GuidCache->GetOrAssignNetGUID( ThisActor );

	FActorDestructionInfo &NewInfo = NetDriver->DestroyedStartupOrDormantActors.FindOrAdd( NetGUID );
	NewInfo.DestroyedPosition = ThisActor->GetActorLocation();
	NewInfo.NetGUID = NetGUID;
	NewInfo.Level = ThisActor->GetLevel();
	NewInfo.ObjOuter = ThisActor->GetOuter();
	NewInfo.PathName = ThisActor->GetName();

	if (NewInfo.Level.IsValid() && !NewInfo.Level->IsPersistentLevel() )
	{
		NewInfo.StreamingLevelName = NewInfo.Level->GetOutermost()->GetFName();
	}
	else
	{
		NewInfo.StreamingLevelName = NAME_None;
	}

	return &NewInfo;
}

void UNetDriver::NotifyActorDestroyed( AActor* ThisActor, bool IsSeamlessTravel )
{
	// Remove the actor from the property tracker map
	RepChangedPropertyTrackerMap.Remove(ThisActor);

	FActorDestructionInfo* DestructionInfo = NULL;
	const bool bIsServer = ServerConnection == NULL;
	if (bIsServer)
	{
		const FNetworkObjectInfo* NetworkObjectInfo = GetNetworkObjectInfo( ThisActor );

		const bool bIsActorStatic = !GuidCache->IsDynamicObject( ThisActor );
		const bool bActorHasRole = ThisActor->GetRemoteRole() != ROLE_None;
		const bool bShouldCreateDestructionInfo = bIsServer && bIsActorStatic && bActorHasRole && !IsSeamlessTravel;

		if(bShouldCreateDestructionInfo)
		{
			UE_LOG(LogNet, VeryVerbose, TEXT("NotifyActorDestroyed %s - StartupActor"), *ThisActor->GetPathName() );
			DestructionInfo = CreateDestructionInfo( this, ThisActor, DestructionInfo);
		}

		for( int32 i=ClientConnections.Num()-1; i>=0; i-- )
		{
			UNetConnection* Connection = ClientConnections[i];
			if( ThisActor->bNetTemporary )
				Connection->SentTemporaries.Remove( ThisActor );
			UActorChannel* Channel = Connection->ActorChannels.FindRef(ThisActor);
			if( Channel )
			{
				check(Channel->OpenedLocally);
				Channel->bClearRecentActorRefs = false;
				Channel->Close();
			}
			else
			{
				const bool bDormantOrRecentlyDormant = NetworkObjectInfo && (NetworkObjectInfo->DormantConnections.Contains(Connection) || NetworkObjectInfo->RecentlyDormantConnections.Contains(Connection));

				if (bShouldCreateDestructionInfo || bDormantOrRecentlyDormant)
				{
					// Make a new destruction info if necessary. It is necessary if the actor is dormant or recently dormant because
					// even though the client knew about the actor at some point, it doesn't have a channel to handle destruction.
					DestructionInfo = CreateDestructionInfo(this, ThisActor, DestructionInfo);
					Connection->DestroyedStartupOrDormantActors.Add(DestructionInfo->NetGUID);
				}
			}

			// Remove it from any dormancy lists				
			Connection->DormantReplicatorMap.Remove( ThisActor );
		}
	}

	// Remove this actor from the network object list
	GetNetworkObjectList().Remove( ThisActor );
}

void UNetDriver::NotifyStreamingLevelUnload( ULevel* Level)
{
	if (ServerConnection && ServerConnection->PackageMap)
	{
		UE_LOG(LogNet, Log, TEXT("NotifyStreamingLevelUnload: %s"), *Level->GetName() );

		if (Level->LevelScriptActor)
		{
			UActorChannel * Channel = ServerConnection->ActorChannels.FindRef((AActor*)Level->LevelScriptActor);
			if (Channel)
			{
				UE_LOG(LogNet, Log, TEXT("NotifyStreamingLevelUnload: BREAKING"));

				Channel->Actor = NULL;
				Channel->Broken = true;
				Channel->CleanupReplicators();
			}
		}

		ServerConnection->PackageMap->NotifyStreamingLevelUnload(Level);
	}

	for( int32 i=ClientConnections.Num()-1; i>=0; i-- )
	{
		UNetConnection* Connection = ClientConnections[i];
		if (Connection && Connection->PackageMap)
		{
			Connection->PackageMap->NotifyStreamingLevelUnload(Level);
		}
	}
}

/** Called when an actor is being unloaded during a seamless travel or do due level streaming 
 *  The main point is that it calls the normal NotifyActorDestroyed to destroy the channel on the server
 *	but also removes the Actor reference, sets broken flag, and cleans up actor class references on clients.
 */
void UNetDriver::NotifyActorLevelUnloaded( AActor* TheActor )
{
	// server
	NotifyActorDestroyed(TheActor, true);
	// client
	if (ServerConnection != NULL)
	{
		// we can't kill the channel until the server says so, so just clear the actor ref and break the channel
		UActorChannel* Channel = ServerConnection->ActorChannels.FindRef(TheActor);
		if (Channel != NULL)
		{
			ServerConnection->ActorChannels.Remove(TheActor);
			Channel->Actor = NULL;
			Channel->Broken = true;
			Channel->CleanupReplicators();
		}
	}
}

/** UNetDriver::FlushActorDormancy(AActor* Actor)
 *	 Flushes the actor from the NetDriver's dormant list and/or cancels pending dormancy on the actor channel.
 *
 *	 This does not change the Actor's actual NetDormant state. If a dormant actor is Flushed, it will net update at least one more
 *	 time, and then go back to dormant.
 */
void UNetDriver::FlushActorDormancy(AActor* Actor)
{
	// Note: Going into dormancy is completely handled in ServerReplicateActor. We want to avoid
	// event-based handling of going into dormancy, because we have to deal with connections joining in progress.
	// It is better to have ::ServerReplicateActor check the AActor and UChannel's states to determined if an actor
	// needs to be moved into dormancy. The same amount of work will be done (1 time per connection when an actor goes dorm)
	// and we avoid having to do special things when a new client joins.
	//
	// Going out of dormancy can be event based like this since it only affects clients already joined. Its more efficient in this
	// way too, since we dont have to check every dormant actor in ::ServerReplicateActor to see if it needs to go out of dormancy

#if WITH_SERVER_CODE
	if (CVarSetNetDormancyEnabled.GetValueOnAnyThread() == 0)
		return;

	check(Actor);
	check(ServerConnection == NULL);

	// Go through each connection and remove the actor from the dormancy list
	for (int32 i=0; i < ClientConnections.Num(); ++i)
	{
		UNetConnection *NetConnection = ClientConnections[i];
		if(NetConnection != NULL)
		{
			NetConnection->FlushDormancy(Actor);
		}
	}
#endif // WITH_SERVER_CODE
}

void UNetDriver::ForcePropertyCompare( AActor* Actor )
{
#if WITH_SERVER_CODE
	check( Actor );

	for ( int32 i=0; i < ClientConnections.Num(); ++i )
	{
		UNetConnection *NetConnection = ClientConnections[i];
		if ( NetConnection != NULL )
		{
			NetConnection->ForcePropertyCompare( Actor );
		}
	}
#endif // WITH_SERVER_CODE
}

void UNetDriver::ForceActorRelevantNextUpdate(AActor* Actor)
{
#if WITH_SERVER_CODE
	check(Actor);
	
	GetNetworkObjectList().ForceActorRelevantNextUpdate(Actor, NetDriverName);
#endif // WITH_SERVER_CODE
}

UChildConnection* UNetDriver::CreateChild(UNetConnection* Parent)
{
	UE_LOG(LogNet, Log, TEXT("Creating child connection with %s parent"), *Parent->GetName());
	auto Child = NewObject<UChildConnection>();
	Child->Driver = this;
	Child->URL = FURL();
	Child->State = Parent->State;
	Child->URL.Host = Parent->URL.Host;
	Child->Parent = Parent;
	Child->PackageMap = Parent->PackageMap;
	Child->CurrentNetSpeed = Parent->CurrentNetSpeed;
	Parent->Children.Add(Child);
	return Child;
}

void UNetDriver::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UNetDriver* This = CastChecked<UNetDriver>(InThis);
	Super::AddReferencedObjects(This, Collector);

	// Compact any invalid entries
	for (auto It = This->RepLayoutMap.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = This->ReplicationChangeListMap.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

#if DO_ENABLE_NET_TEST

void UNetDriver::SetPacketSimulationSettings(FPacketSimulationSettings NewSettings)
{
	PacketSimulationSettings = NewSettings;
	if (ServerConnection)
	{
		ServerConnection->UpdatePacketSimulationSettings();
	}
	for (UNetConnection* ClientConnection : ClientConnections)
	{
		if (ClientConnection)
		{
			ClientConnection->UpdatePacketSimulationSettings();
		}
	}
}

class FPacketSimulationConsoleCommandVisitor 
{
public:
	static void OnPacketSimulationConsoleCommand(const TCHAR *Name, IConsoleObject* CVar, TArray<IConsoleObject*>& Sink)
	{
		Sink.Add(CVar);
	}
};

/** reads in settings from the .ini file 
 * @note: overwrites all previous settings
 */
void FPacketSimulationSettings::LoadConfig(const TCHAR* OptionalQualifier)
{
	if (ConfigHelperInt(TEXT("PktLoss"), PktLoss, OptionalQualifier))
	{
		PktLoss = FMath::Clamp<int32>(PktLoss, 0, 100);
	}
	
	bool InPktOrder = !!PktOrder;
	ConfigHelperBool(TEXT("PktOrder"), InPktOrder, OptionalQualifier);
	PktOrder = int32(InPktOrder);
	
	ConfigHelperInt(TEXT("PktLag"), PktLag, OptionalQualifier);
	
	if (ConfigHelperInt(TEXT("PktDup"), PktDup, OptionalQualifier))
	{
		PktDup = FMath::Clamp<int32>(PktDup, 0, 100);
	}
	
	if (ConfigHelperInt(TEXT("PktLagVariance"), PktLagVariance, OptionalQualifier))
	{
		PktLagVariance = FMath::Clamp<int32>(PktLagVariance, 0, 100);
	}
}

bool FPacketSimulationSettings::ConfigHelperInt(const TCHAR* Name, int32& Value, const TCHAR* OptionalQualifier)
{
	if (OptionalQualifier)
	{
		if (GConfig->GetInt(TEXT("PacketSimulationSettings"), *FString::Printf(TEXT("%s%s"), OptionalQualifier, Name), Value, GEngineIni))
		{
			return true;
		}
	}

	if (GConfig->GetInt(TEXT("PacketSimulationSettings"), Name, Value, GEngineIni))
	{
		return true;
	}

	return false;
}

bool FPacketSimulationSettings::ConfigHelperBool(const TCHAR* Name, bool& Value, const TCHAR* OptionalQualifier)
{
	if (OptionalQualifier)
	{
		if (GConfig->GetBool(TEXT("PacketSimulationSettings"), *FString::Printf(TEXT("%s%s"), OptionalQualifier, Name), Value, GEngineIni))
		{
			return true;
		}
	}

	if (GConfig->GetBool(TEXT("PacketSimulationSettings"), Name, Value, GEngineIni))
	{
		return true;
	}

	return false;
}

void FPacketSimulationSettings::RegisterCommands()
{
	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	
	// Register exec commands with the console manager for auto-completion if they havent been registered already by another net driver
	if (!ConsoleManager.IsNameRegistered(TEXT("Net PktLoss=")))
	{
		ConsoleManager.RegisterConsoleCommand(TEXT("Net PktLoss="), TEXT("PktLoss=<n> (simulates network packet loss)"));
		ConsoleManager.RegisterConsoleCommand(TEXT("Net PktOrder="), TEXT("PktOrder=<n> (simulates network packet received out of order)"));
		ConsoleManager.RegisterConsoleCommand(TEXT("Net PktDup="), TEXT("PktDup=<n> (simulates sending/receiving duplicate network packets)"));
		ConsoleManager.RegisterConsoleCommand(TEXT("Net PktLag="), TEXT("PktLag=<n> (simulates network packet lag)"));
		ConsoleManager.RegisterConsoleCommand(TEXT("Net PktLagVariance="), TEXT("PktLagVariance=<n> (simulates variable network packet lag)"));
	}
}


void FPacketSimulationSettings::UnregisterCommands()
{
	// Never unregister the console commands. Since net drivers come and go, and we can sometimes have more than 1, etc. 
	// We could do better bookkeeping for this, but its not worth it right now. Just ensure the commands are always there for tab completion.
#if 0
	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	// Gather up all relevant console commands
	TArray<IConsoleObject*> PacketSimulationConsoleCommands;
	ConsoleManager.ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateStatic< TArray<IConsoleObject*>& >(
		&FPacketSimulationConsoleCommandVisitor::OnPacketSimulationConsoleCommand,
		PacketSimulationConsoleCommands ), TEXT("Net Pkt"));

	// Unregister them from the console manager
	for(int32 i = 0; i < PacketSimulationConsoleCommands.Num(); ++i)
	{
		ConsoleManager.UnregisterConsoleObject(PacketSimulationConsoleCommands[i]);
	}
#endif
}

/**
 * Reads the settings from a string: command line or an exec
 *
 * @param Stream the string to read the settings from
 */
bool FPacketSimulationSettings::ParseSettings(const TCHAR* Cmd, const TCHAR* OptionalQualifier)
{
	UE_LOG(LogTemp, Display, TEXT("ParseSettings for %s"), OptionalQualifier);
	// note that each setting is tested.
	// this is because the same function will be used to parse the command line as well
	bool bParsed = false;

	if( ParseHelper(Cmd, TEXT("PktLoss="), PktLoss, OptionalQualifier) )
	{
		bParsed = true;
		FMath::Clamp<int32>( PktLoss, 0, 100 );
		UE_LOG(LogNet, Log, TEXT("PktLoss set to %d"), PktLoss);
	}
	if( ParseHelper(Cmd, TEXT("PktOrder="), PktOrder, OptionalQualifier) )
	{
		bParsed = true;
		FMath::Clamp<int32>( PktOrder, 0, 1 );
		UE_LOG(LogNet, Log, TEXT("PktOrder set to %d"), PktOrder);
	}
	if( ParseHelper(Cmd, TEXT("PktLag="), PktLag, OptionalQualifier) )
	{
		bParsed = true;
		UE_LOG(LogNet, Log, TEXT("PktLag set to %d"), PktLag);
	}
	if( ParseHelper(Cmd, TEXT("PktDup="), PktDup, OptionalQualifier) )
	{
		bParsed = true;
		FMath::Clamp<int32>( PktDup, 0, 100 );
		UE_LOG(LogNet, Log, TEXT("PktDup set to %d"), PktDup);
	}	
	if (ParseHelper(Cmd, TEXT("PktLagVariance="), PktLagVariance, OptionalQualifier))
	{
		bParsed = true;
		FMath::Clamp<int32>( PktLagVariance, 0, 100 );
		UE_LOG(LogNet, Log, TEXT("PktLagVariance set to %d"), PktLagVariance);
	}
	return bParsed;
}

bool FPacketSimulationSettings::ParseHelper(const TCHAR* Cmd, const TCHAR* Name, int32& Value, const TCHAR* OptionalQualifier)
{
	if (OptionalQualifier)
	{
		if (FParse::Value(Cmd, *FString::Printf(TEXT("%s%s"), OptionalQualifier, Name), Value))
		{
			return true;
		}
	}

	if (FParse::Value(Cmd, Name, Value))
	{
		return true;
	}
	return false;
}

#endif

FNetViewer::FNetViewer(UNetConnection* InConnection, float DeltaSeconds) :
	Connection(InConnection),
	InViewer(InConnection->PlayerController ? InConnection->PlayerController : InConnection->OwningActor),
	ViewTarget(InConnection->ViewTarget),
	ViewLocation(ForceInit),
	ViewDir(ForceInit)
{
	check(InConnection->OwningActor);
	check(!InConnection->PlayerController || (InConnection->PlayerController == InConnection->OwningActor));

	APlayerController* ViewingController = InConnection->PlayerController;

	// Get viewer coordinates.
	ViewLocation = ViewTarget->GetActorLocation();
	if (ViewingController)
	{
		FRotator ViewRotation = ViewingController->GetControlRotation();
		ViewingController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		ViewDir = ViewRotation.Vector();
	}

	// Compute ahead-vectors for prediction.
	FVector Ahead = FVector::ZeroVector;
	if (InConnection->TickCount & 1)
	{
		float PredictSeconds = (InConnection->TickCount & 2) ? 0.4f : 0.9f;
		Ahead = PredictSeconds * ViewTarget->GetVelocity();
		APawn* ViewerPawn = Cast<APawn>(ViewTarget);
		if( ViewerPawn && ViewerPawn->GetMovementBase() && ViewerPawn->GetMovementBase()->GetOwner() )
		{
			Ahead += PredictSeconds * ViewerPawn->GetMovementBase()->GetOwner()->GetVelocity();
		}
		if (!Ahead.IsZero())
		{
			FHitResult Hit(1.0f);
			FVector PredictedLocation = ViewLocation + Ahead;

			UWorld* World = NULL;
			if( InConnection->PlayerController )
			{
				World = InConnection->PlayerController->GetWorld();
			}
			else if (ViewerPawn)
			{
				World = ViewerPawn->GetWorld();
			}
			check( World );
			if (World->LineTraceSingleByObjectType(Hit, ViewLocation, PredictedLocation, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(ServerForwardView), true, ViewTarget)))
			{
				// hit something, view location is hit location
				ViewLocation = Hit.Location;
			}
			else
			{
				// No hit, so view location is predicted location
				ViewLocation = PredictedLocation;
			}
		}
	}
}

FActorPriority::FActorPriority(UNetConnection* InConnection, UActorChannel* InChannel, FNetworkObjectInfo* InActorInfo, const TArray<struct FNetViewer>& Viewers, bool bLowBandwidth)
	: ActorInfo(InActorInfo), Channel(InChannel), DestructionInfo(NULL)
{	
	float Time  = Channel ? (InConnection->Driver->Time - Channel->LastUpdateTime) : InConnection->Driver->SpawnPrioritySeconds;
	// take the highest priority of the viewers on this connection
	Priority = 0;
	for (int32 i = 0; i < Viewers.Num(); i++)
	{
		Priority = FMath::Max<int32>(Priority, FMath::RoundToInt(65536.0f * ActorInfo->Actor->GetNetPriority(Viewers[i].ViewLocation, Viewers[i].ViewDir, Viewers[i].InViewer, Viewers[i].ViewTarget, InChannel, Time, bLowBandwidth)));
	}
}

FActorPriority::FActorPriority(class UNetConnection* InConnection, struct FActorDestructionInfo * Info, const TArray<struct FNetViewer>& Viewers )
	: ActorInfo(NULL), Channel(NULL), DestructionInfo(Info)
{
	
	Priority = 0;

	for (int32 i = 0; i < Viewers.Num(); i++)
	{
		float Time  = InConnection->Driver->SpawnPrioritySeconds;

		FVector Dir = DestructionInfo->DestroyedPosition - Viewers[i].ViewLocation;
		float DistSq = Dir.SizeSquared();
		
		// adjust priority based on distance and whether actor is in front of viewer
		if ( (Viewers[i].ViewDir | Dir) < 0.f )
		{
			if ( DistSq > NEARSIGHTTHRESHOLDSQUARED )
				Time *= 0.2f;
			else if ( DistSq > CLOSEPROXIMITYSQUARED )
				Time *= 0.4f;
		}
		else if ( DistSq > MEDSIGHTTHRESHOLDSQUARED )
			Time *= 0.4f;

		Priority = FMath::Max<int32>(Priority, 65536.0f * Time);
	}
}

#if WITH_SERVER_CODE
int32 UNetDriver::ServerReplicateActors_PrepConnections( const float DeltaSeconds )
{
	int32 NumClientsToTick = ClientConnections.Num();

	// by default only throttle update for listen servers unless specified on the commandline
	static bool bForceClientTickingThrottle = FParse::Param( FCommandLine::Get(), TEXT( "limitclientticks" ) );
	if ( bForceClientTickingThrottle || GetNetMode() == NM_ListenServer )
	{
		// determine how many clients to tick this frame based on GEngine->NetTickRate (always tick at least one client), double for lan play
		// FIXME: DeltaTimeOverflow is a static, and will conflict with other running net drivers, we investigate storing it on the driver itself!
		static float DeltaTimeOverflow = 0.f;
		// updates are doubled for lan play
		static bool LanPlay = FParse::Param( FCommandLine::Get(), TEXT( "lanplay" ) );
		//@todo - ideally we wouldn't want to tick more clients with a higher deltatime as that's not going to be good for performance and probably saturate bandwidth in hitchy situations, maybe 
		// come up with a solution that is greedier with higher framerates, but still won't risk saturating server upstream bandwidth
		float ClientUpdatesThisFrame = GEngine->NetClientTicksPerSecond * ( DeltaSeconds + DeltaTimeOverflow ) * ( LanPlay ? 2.f : 1.f );
		NumClientsToTick = FMath::Min<int32>( NumClientsToTick, FMath::TruncToInt( ClientUpdatesThisFrame ) );
		//UE_LOG(LogNet, Log, TEXT("%2.3f: Ticking %d clients this frame, %2.3f/%2.4f"),GetWorld()->GetTimeSeconds(),NumClientsToTick,DeltaSeconds,ClientUpdatesThisFrame);
		if ( NumClientsToTick == 0 )
		{
			// if no clients are ticked this frame accumulate the time elapsed for the next frame
			DeltaTimeOverflow += DeltaSeconds;
			return 0;
		}
		DeltaTimeOverflow = 0.f;
	}

	bool bFoundReadyConnection = false;

	for ( int32 ConnIdx = 0; ConnIdx < ClientConnections.Num(); ConnIdx++ )
	{
		UNetConnection* Connection = ClientConnections[ConnIdx];
		check( Connection );
		check( Connection->State == USOCK_Pending || Connection->State == USOCK_Open || Connection->State == USOCK_Closed );
		checkSlow( Connection->GetUChildConnection() == NULL );

		// Handle not ready channels.
		//@note: we cannot add Connection->IsNetReady(0) here to check for saturation, as if that's the case we still want to figure out the list of relevant actors
		//			to reset their NetUpdateTime so that they will get sent as soon as the connection is no longer saturated
		AActor* OwningActor = Connection->OwningActor;
		if ( OwningActor != NULL && Connection->State == USOCK_Open && ( Connection->Driver->Time - Connection->LastReceiveTime < 1.5f ) )
		{
			check( World == OwningActor->GetWorld() );

			bFoundReadyConnection = true;

			// the view target is what the player controller is looking at OR the owning actor itself when using beacons
			Connection->ViewTarget = Connection->PlayerController ? Connection->PlayerController->GetViewTarget() : OwningActor;

			for ( int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++ )
			{
				UNetConnection *Child = Connection->Children[ChildIdx];
				APlayerController* ChildPlayerController = Child->PlayerController;
				if ( ChildPlayerController != NULL )
				{
					Child->ViewTarget = ChildPlayerController->GetViewTarget();
				}
				else
				{
					Child->ViewTarget = NULL;
				}
			}
		}
		else
		{
			Connection->ViewTarget = NULL;
			for ( int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++ )
			{
				Connection->Children[ChildIdx]->ViewTarget = NULL;
			}
		}
	}

	return bFoundReadyConnection ? NumClientsToTick : 0;
}

void UNetDriver::ServerReplicateActors_BuildConsiderList( TArray<FNetworkObjectInfo*>& OutConsiderList, const float ServerTickTime )
{
	SCOPE_CYCLE_COUNTER( STAT_NetConsiderActorsTime );

	UE_LOG( LogNetTraffic, Log, TEXT( "ServerReplicateActors_BuildConsiderList, Building ConsiderList %4.2f" ), World->GetTimeSeconds() );

	int32 NumInitiallyDormant = 0;

	const bool bUseAdapativeNetFrequency = IsAdaptiveNetUpdateFrequencyEnabled();

	TArray<AActor*> ActorsToRemove;

	for ( const TSharedPtr<FNetworkObjectInfo>& ObjectInfo : GetNetworkObjectList().GetActiveObjects() )
	{
		FNetworkObjectInfo* ActorInfo = ObjectInfo.Get();

		if ( !ActorInfo->bPendingNetUpdate && World->TimeSeconds <= ActorInfo->NextUpdateTime )
		{
			continue;		// It's not time for this actor to perform an update, skip it
		}

		AActor* Actor = ActorInfo->Actor;

		if ( Actor->IsPendingKill() )
		{
			ActorsToRemove.Add( Actor );
			continue;
		}

		if ( Actor->GetRemoteRole() == ROLE_None )
		{
			ActorsToRemove.Add( Actor );
			continue;
		}

		// This actor may belong to a different net driver, make sure this is the correct one
		// (this can happen when using beacon net drivers for example)
		if ( Actor->GetNetDriverName() != NetDriverName )
		{
			UE_LOG( LogNetTraffic, Error, TEXT( "Actor %s in wrong network actors list!" ), *Actor->GetName() );
			continue;
		}

		// Verify the actor is actually initialized (it might have been intentionally spawn deferred until a later frame)
		if ( !Actor->IsActorInitialized() )
		{
			continue;
		}

		// Don't send actors that may still be streaming in or out
		ULevel* Level = Actor->GetLevel();
		if ( Level->HasVisibilityChangeRequestPending() || Level->bIsAssociatingLevel )
		{
			continue;
		}

		if ( Actor->NetDormancy == DORM_Initial && Actor->IsNetStartupActor() )
		{
			// This stat isn't that useful in its current form when using NetworkActors list
			// We'll want to track initially dormant actors some other way to track them with stats
			SCOPE_CYCLE_COUNTER( STAT_NetInitialDormantCheckTime );
			NumInitiallyDormant++;
			ActorsToRemove.Add( Actor );
			//UE_LOG(LogNetTraffic, Log, TEXT("Skipping Actor %s - its initially dormant!"), *Actor->GetName() );
			continue;
		}

		checkSlow( Actor->NeedsLoadForClient() ); // We have no business sending this unless the client can load
		checkSlow( World == Actor->GetWorld() );

		// Set defaults if this actor is replicating for first time
		if ( ActorInfo->LastNetReplicateTime == 0 )
		{
			ActorInfo->LastNetReplicateTime = World->TimeSeconds;
			ActorInfo->OptimalNetUpdateDelta = 1.0f / Actor->NetUpdateFrequency;
		}

		const float ScaleDownStartTime = 2.0f;
		const float ScaleDownTimeRange = 5.0f;

		const float LastReplicateDelta = World->TimeSeconds - ActorInfo->LastNetReplicateTime;

		if ( LastReplicateDelta > ScaleDownStartTime )
		{
			if ( Actor->MinNetUpdateFrequency == 0.0f )
			{
				Actor->MinNetUpdateFrequency = 2.0f;
			}

			// Calculate min delta (max rate actor will update), and max delta (slowest rate actor will update)
			const float MinOptimalDelta = 1.0f / Actor->NetUpdateFrequency;									  // Don't go faster than NetUpdateFrequency
			const float MaxOptimalDelta = FMath::Max( 1.0f / Actor->MinNetUpdateFrequency, MinOptimalDelta ); // Don't go slower than MinNetUpdateFrequency (or NetUpdateFrequency if it's slower)

			// Interpolate between MinOptimalDelta/MaxOptimalDelta based on how long it's been since this actor actually sent anything
			const float Alpha = FMath::Clamp( ( LastReplicateDelta - ScaleDownStartTime ) / ScaleDownTimeRange, 0.0f, 1.0f );
			ActorInfo->OptimalNetUpdateDelta = FMath::Lerp( MinOptimalDelta, MaxOptimalDelta, Alpha );
		}

		// Setup ActorInfo->NextUpdateTime, which will be the next time this actor will replicate properties to connections
		// NOTE - We don't do this if bPendingNetUpdate is true, since this means we're forcing an update due to at least one connection
		//	that wasn't to replicate previously (due to saturation, etc)
		// NOTE - This also means all other connections will force an update (even if they just updated, we should look into this)
		if ( !ActorInfo->bPendingNetUpdate )
		{
			UE_LOG( LogNetTraffic, Log, TEXT( "actor %s requesting new net update, time: %2.3f" ), *Actor->GetName(), World->TimeSeconds );

			const float NextUpdateDelta = bUseAdapativeNetFrequency ? ActorInfo->OptimalNetUpdateDelta : 1.0f / Actor->NetUpdateFrequency;

			// then set the next update time
			ActorInfo->NextUpdateTime = World->TimeSeconds + FMath::SRand() * ServerTickTime + NextUpdateDelta;

			// and mark when the actor first requested an update
			//@note: using Time because it's compared against UActorChannel.LastUpdateTime which also uses that value
			ActorInfo->LastNetUpdateTime = Time;
		}

		// and clear the pending update flag assuming all clients will be able to consider it
		ActorInfo->bPendingNetUpdate = false;

		// add it to the list to consider below
		// For performance reasons, make sure we don't resize the array. It should already be appropriately sized above!
		ensure( OutConsiderList.Num() < OutConsiderList.Max() );
		OutConsiderList.Add( ActorInfo );

		// Call PreReplication on all actors that will be considered
		Actor->CallPreReplication( this );
	}

	for ( AActor* Actor : ActorsToRemove )
	{
		GetNetworkObjectList().Remove( Actor );
	}

	// Update stats
	SET_DWORD_STAT( STAT_NumInitiallyDormantActors, NumInitiallyDormant );
	SET_DWORD_STAT( STAT_NumConsideredActors, OutConsiderList.Num() );
}

// Returns true if this actor should replicate to *any* of the passed in connections
static FORCEINLINE_DEBUGGABLE bool IsActorRelevantToConnection( const AActor* Actor, const TArray<FNetViewer>& ConnectionViewers )
{
	for ( int32 viewerIdx = 0; viewerIdx < ConnectionViewers.Num(); viewerIdx++ )
	{
		if ( Actor->IsNetRelevantFor( ConnectionViewers[viewerIdx].InViewer, ConnectionViewers[viewerIdx].ViewTarget, ConnectionViewers[viewerIdx].ViewLocation ) )
		{
			return true;
		}
	}

	return false;
}

// Returns true if this actor is owned by, and should replicate to *any* of the passed in connections
static FORCEINLINE_DEBUGGABLE UNetConnection* IsActorOwnedByAndRelevantToConnection( const AActor* Actor, const TArray<FNetViewer>& ConnectionViewers, bool& bOutHasNullViewTarget )
{
	const AActor* ActorOwner = Actor->GetNetOwner();

	bOutHasNullViewTarget = false;

	for ( int i = 0; i < ConnectionViewers.Num(); i++ )
	{
		UNetConnection* ViewerConnection = ConnectionViewers[i].Connection;

		if ( ViewerConnection->ViewTarget == nullptr )
		{
			bOutHasNullViewTarget = true;
		}

		if ( ActorOwner == ViewerConnection->PlayerController ||
			 ( ViewerConnection->PlayerController && ActorOwner == ViewerConnection->PlayerController->GetPawn() ) ||
			 (ViewerConnection->ViewTarget && ViewerConnection->ViewTarget->IsRelevancyOwnerFor( Actor, ActorOwner, ViewerConnection->OwningActor ) ) )
		{
			return ViewerConnection;
		}
	}

	return nullptr;
}

// Returns true if this actor is considered dormant (and all properties caught up) to the current connection
static FORCEINLINE_DEBUGGABLE bool IsActorDormant( FNetworkObjectInfo* ActorInfo, const UNetConnection* Connection )
{
	// If actor is already dormant on this channel, then skip replication entirely
	return ActorInfo->DormantConnections.Contains( Connection );
}

// Returns true if this actor wants to go dormant for a particular connection
static FORCEINLINE_DEBUGGABLE bool ShouldActorGoDormant( AActor* Actor, const TArray<FNetViewer>& ConnectionViewers, UActorChannel* Channel, const float Time, const bool bLowNetBandwidth )
{
	if ( Actor->NetDormancy <= DORM_Awake || !Channel || Channel->bPendingDormancy || Channel->Dormant )
	{
		// Either shouldn't go dormant, or is already dormant
		return false;
	}

	if ( Actor->NetDormancy == DORM_DormantPartial )
	{
		for ( int32 viewerIdx = 0; viewerIdx < ConnectionViewers.Num(); viewerIdx++ )
		{
			if ( !Actor->GetNetDormancy( ConnectionViewers[viewerIdx].ViewLocation, ConnectionViewers[viewerIdx].ViewDir, ConnectionViewers[viewerIdx].InViewer, ConnectionViewers[viewerIdx].ViewTarget, Channel, Time, bLowNetBandwidth ) )
			{
				return false;
			}
		}
	}

	return true;
}

int32 UNetDriver::ServerReplicateActors_PrioritizeActors( UNetConnection* Connection, const TArray<FNetViewer>& ConnectionViewers, const TArray<FNetworkObjectInfo*> ConsiderList, const bool bCPUSaturated, FActorPriority*& OutPriorityList, FActorPriority**& OutPriorityActors )
{
	SCOPE_CYCLE_COUNTER( STAT_NetPrioritizeActorsTime );

	// Get list of visible/relevant actors.

	NetTag++;
	Connection->TickCount++;

	// Set up to skip all sent temporary actors
	for ( int32 j = 0; j < Connection->SentTemporaries.Num(); j++ )
	{
		Connection->SentTemporaries[j]->NetTag = NetTag;
	}

	// Make list of all actors to consider.
	check( World == Connection->OwningActor->GetWorld() );

	int32 FinalSortedCount = 0;
	int32 DeletedCount = 0;

	const int32 MaxSortedActors = ConsiderList.Num() + DestroyedStartupOrDormantActors.Num();
	if ( MaxSortedActors > 0 )
	{
		OutPriorityList = new ( FMemStack::Get(), MaxSortedActors ) FActorPriority;
		OutPriorityActors = new ( FMemStack::Get(), MaxSortedActors ) FActorPriority*;

		check( World == Connection->ViewTarget->GetWorld() );

		AGameNetworkManager* const NetworkManager = World->NetworkManager;
		const bool bLowNetBandwidth = NetworkManager ? NetworkManager->IsInLowBandwidthMode() : false;

		for ( FNetworkObjectInfo* ActorInfo : ConsiderList )
		{
			AActor* Actor = ActorInfo->Actor;

			UActorChannel* Channel = Connection->ActorChannels.FindRef( Actor );

			UNetConnection* PriorityConnection = Connection;

			if ( Actor->bOnlyRelevantToOwner )
			{
				// This actor should be owned by a particular connection, see if that connection is the one passed in
				bool bHasNullViewTarget = false;

				PriorityConnection = IsActorOwnedByAndRelevantToConnection( Actor, ConnectionViewers, bHasNullViewTarget );

				if ( PriorityConnection == nullptr )
				{
					// Not owned by this connection, if we have a channel, close it, and continue
					// NOTE - We won't close the channel if any connection has a NULL view target.
					//	This is to give all connections a chance to own it
					if ( !bHasNullViewTarget && Channel != NULL && Time - Channel->RelevantTime >= RelevantTimeout )
					{
						Channel->Close();
					}

					// This connection doesn't own this actor
					continue;
				}
			}
			else if ( CVarSetNetDormancyEnabled.GetValueOnGameThread() != 0 )
			{
				// Skip Actor if dormant
				if ( IsActorDormant( ActorInfo, Connection ) )
				{
					continue;
				}

				// See of actor wants to try and go dormant
				if ( ShouldActorGoDormant( Actor, ConnectionViewers, Channel, Time, bLowNetBandwidth ) )
				{
					// Channel is marked to go dormant now once all properties have been replicated (but is not dormant yet)
					Channel->StartBecomingDormant();
				}
			}

			// Skip actor if not relevant and theres no channel already.
			// Historically Relevancy checks were deferred until after prioritization because they were expensive (line traces).
			// Relevancy is now cheap and we are dealing with larger lists of considered actors, so we want to keep the list of
			// prioritized actors low.
			if ( !Channel )
			{
				if ( !IsLevelInitializedForActor( Actor, Connection ) )
				{
					// If the level this actor belongs to isn't loaded on client, don't bother sending
					continue;
				}

				if ( !IsActorRelevantToConnection( Actor, ConnectionViewers ) )
				{
					// If not relevant (and we don't have a channel), skip
					continue;
				}
			}

			// Actor is relevant to this connection, add it to the list
			// NOTE - We use NetTag to make sure SentTemporaries didn't already mark this actor to be skipped
			if ( Actor->NetTag != NetTag )
			{
				UE_LOG( LogNetTraffic, Log, TEXT( "Consider %s alwaysrelevant %d frequency %f " ), *Actor->GetName(), Actor->bAlwaysRelevant, Actor->NetUpdateFrequency );

				Actor->NetTag = NetTag;

				OutPriorityList[FinalSortedCount] = FActorPriority( PriorityConnection, Channel, ActorInfo, ConnectionViewers, bLowNetBandwidth );
				OutPriorityActors[FinalSortedCount] = OutPriorityList + FinalSortedCount;

				FinalSortedCount++;

				if ( DebugRelevantActors )
				{
					LastPrioritizedActors.Add( Actor );
				}
			}
		}

		// Add in deleted actors
		for ( auto It = Connection->DestroyedStartupOrDormantActors.CreateIterator(); It; ++It )
		{
			FActorDestructionInfo& DInfo = DestroyedStartupOrDormantActors.FindChecked( *It );
			OutPriorityList[FinalSortedCount] = FActorPriority( Connection, &DInfo, ConnectionViewers );
			OutPriorityActors[FinalSortedCount] = OutPriorityList + FinalSortedCount;
			FinalSortedCount++;
			DeletedCount++;
		}

		// Sort by priority
		Sort( OutPriorityActors, FinalSortedCount, FCompareFActorPriority() );
	}

	UE_LOG( LogNetTraffic, Log, TEXT( "ServerReplicateActors_PrioritizeActors: Potential %04i ConsiderList %03i FinalSortedCount %03i" ), MaxSortedActors, ConsiderList.Num(), FinalSortedCount );

	// Setup stats
	SET_DWORD_STAT( STAT_PrioritizedActors, FinalSortedCount );
	SET_DWORD_STAT( STAT_NumRelevantDeletedActors, DeletedCount );

	return FinalSortedCount;
}

int32 UNetDriver::ServerReplicateActors_ProcessPrioritizedActors( UNetConnection* Connection, const TArray<FNetViewer>& ConnectionViewers, FActorPriority** PriorityActors, const int32 FinalSortedCount, int32& OutUpdated )
{
	int32 ActorUpdatesThisConnection		= 0;
	int32 ActorUpdatesThisConnectionSent	= 0;
	int32 FinalRelevantCount				= 0;

	if ( !Connection->IsNetReady( 0 ) )
	{
		// Connection saturated, don't process any actors

		// Update stats even though there was no processing.
		SET_DWORD_STAT( STAT_NumReplicatedActorAttempts, ActorUpdatesThisConnection );
		SET_DWORD_STAT( STAT_NumReplicatedActors, ActorUpdatesThisConnectionSent );
		SET_DWORD_STAT( STAT_NumRelevantActors, FinalRelevantCount );

		return 0;
	}

	for ( int32 j = 0; j < FinalSortedCount; j++ )
	{
		FNetworkObjectInfo*	ActorInfo = PriorityActors[j]->ActorInfo;

		// Deletion entry
		if ( ActorInfo == NULL && PriorityActors[j]->DestructionInfo )
		{
			// Make sure client has streaming level loaded
			if ( PriorityActors[j]->DestructionInfo->StreamingLevelName != NAME_None && !Connection->ClientVisibleLevelNames.Contains( PriorityActors[j]->DestructionInfo->StreamingLevelName ) )
			{
				// This deletion entry is for an actor in a streaming level the connection doesn't have loaded, so skip it
				continue;
			}

			UActorChannel* Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
			if ( Channel )
			{
				FinalRelevantCount++;
				UE_LOG( LogNetTraffic, Log, TEXT( "Server replicate actor creating destroy channel for NetGUID <%s,%s> Priority: %d" ), *PriorityActors[j]->DestructionInfo->NetGUID.ToString(), *PriorityActors[j]->DestructionInfo->PathName, PriorityActors[j]->Priority );

				Channel->SetChannelActorForDestroy( PriorityActors[j]->DestructionInfo );						   // Send a close bunch on the new channel
				Connection->DestroyedStartupOrDormantActors.Remove( PriorityActors[j]->DestructionInfo->NetGUID ); // Remove from connections to-be-destroyed list (close bunch of reliable, so it will make it there)
			}
			continue;
		}

#if !( UE_BUILD_SHIPPING || UE_BUILD_TEST )
		static IConsoleVariable* DebugObjectCvar = IConsoleManager::Get().FindConsoleVariable( TEXT( "net.PackageMap.DebugObject" ) );
		static IConsoleVariable* DebugAllObjectsCvar = IConsoleManager::Get().FindConsoleVariable( TEXT( "net.PackageMap.DebugAll" ) );
		if ( ActorInfo &&
			 ( ( DebugObjectCvar && !DebugObjectCvar->GetString().IsEmpty() && ActorInfo->Actor->GetName().Contains( DebugObjectCvar->GetString() ) ) ||
			   ( DebugAllObjectsCvar && DebugAllObjectsCvar->GetInt() != 0 ) ) )
		{
			UE_LOG( LogNetPackageMap, Log, TEXT( "Evaluating actor for replication %s" ), *ActorInfo->Actor->GetName() );
		}
#endif

		// Normal actor replication
		UActorChannel* Channel = PriorityActors[j]->Channel;
		UE_LOG( LogNetTraffic, Log, TEXT( " Maybe Replicate %s" ), *ActorInfo->Actor->GetName() );
		if ( !Channel || Channel->Actor ) //make sure didn't just close this channel
		{
			AActor* Actor = ActorInfo->Actor;
			bool bIsRelevant = false;

			const bool bLevelInitializedForActor = IsLevelInitializedForActor( Actor, Connection );

			// only check visibility on already visible actors every 1.0 + 0.5R seconds
			// bTearOff actors should never be checked
			if ( bLevelInitializedForActor )
			{
				if ( !Actor->bTearOff && ( !Channel || Time - Channel->RelevantTime > 1.f ) )
				{
					if ( IsActorRelevantToConnection( Actor, ConnectionViewers ) )
					{
						bIsRelevant = true;
					}
					else if ( DebugRelevantActors )
					{
						LastNonRelevantActors.Add( Actor );
					}
				}
			}
			else
			{
				// Actor is no longer relevant because the world it is/was in is not loaded by client
				// exception: player controllers should never show up here
				UE_LOG( LogNetTraffic, Log, TEXT( "- Level not initialized for actor %s" ), *Actor->GetName() );
			}

			// if the actor is now relevant or was recently relevant
			const bool bIsRecentlyRelevant = bIsRelevant || ( Channel && Time - Channel->RelevantTime < RelevantTimeout ) || ActorInfo->bForceRelevantNextUpdate;

			ActorInfo->bForceRelevantNextUpdate = false;

			if ( bIsRecentlyRelevant )
			{
				FinalRelevantCount++;

				// Find or create the channel for this actor.
				// we can't create the channel if the client is in a different world than we are
				// or the package map doesn't support the actor's class/archetype (or the actor itself in the case of serializable actors)
				// or it's an editor placed actor and the client hasn't initialized the level it's in
				if ( Channel == NULL && GuidCache->SupportsObject( Actor->GetClass() ) && GuidCache->SupportsObject( Actor->IsNetStartupActor() ? Actor : Actor->GetArchetype() ) )
				{
					if ( bLevelInitializedForActor )
					{
						// Create a new channel for this actor.
						Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
						if ( Channel )
						{
							Channel->SetChannelActor( Actor );
						}
					}
					// if we couldn't replicate it for a reason that should be temporary, and this Actor is updated very infrequently, make sure we update it again soon
					else if ( Actor->NetUpdateFrequency < 1.0f )
					{
						UE_LOG( LogNetTraffic, Log, TEXT( "Unable to replicate %s" ), *Actor->GetName() );
						ActorInfo->NextUpdateTime = Actor->GetWorld()->TimeSeconds + 0.2f * FMath::FRand();
					}
				}

				if ( Channel )
				{
					// if it is relevant then mark the channel as relevant for a short amount of time
					if ( bIsRelevant )
					{
						Channel->RelevantTime = Time + 0.5f * FMath::SRand();
					}
					// if the channel isn't saturated
					if ( Channel->IsNetReady( 0 ) )
					{
						// replicate the actor
						UE_LOG( LogNetTraffic, Log, TEXT( "- Replicate %s. %d" ), *Actor->GetName(), PriorityActors[j]->Priority );
						if ( DebugRelevantActors )
						{
							LastRelevantActors.Add( Actor );
						}

						if ( Channel->ReplicateActor() )
						{
							ActorUpdatesThisConnectionSent++;
							if ( DebugRelevantActors )
							{
								LastSentActors.Add( Actor );
							}

							// Calculate min delta (max rate actor will upate), and max delta (slowest rate actor will update)
							const float MinOptimalDelta				= 1.0f / Actor->NetUpdateFrequency;
							const float MaxOptimalDelta				= FMath::Max( 1.0f / Actor->MinNetUpdateFrequency, MinOptimalDelta );
							const float DeltaBetweenReplications	= ( World->TimeSeconds - ActorInfo->LastNetReplicateTime );

							// Choose an optimal time, we choose 70% of the actual rate to allow frequency to go up if needed
							ActorInfo->OptimalNetUpdateDelta = FMath::Clamp( DeltaBetweenReplications * 0.7f, MinOptimalDelta, MaxOptimalDelta );
							ActorInfo->LastNetReplicateTime = World->TimeSeconds;
						}
						ActorUpdatesThisConnection++;
						OutUpdated++;
					}
					else
					{
						UE_LOG( LogNetTraffic, Log, TEXT( "- Channel saturated, forcing pending update for %s" ), *Actor->GetName() );
						// otherwise force this actor to be considered in the next tick again
						Actor->ForceNetUpdate();
					}
					// second check for channel saturation
					if ( !Connection->IsNetReady( 0 ) )
					{
						// We can bail out now since this connection is saturated, we'll return how far we got though
						SET_DWORD_STAT( STAT_NumReplicatedActorAttempts, ActorUpdatesThisConnection );
						SET_DWORD_STAT( STAT_NumReplicatedActors, ActorUpdatesThisConnectionSent );
						SET_DWORD_STAT( STAT_NumRelevantActors, FinalRelevantCount );
						return j;
					}
				}
			}

			// If the actor wasn't recently relevant, or if it was torn off, close the actor channel if it exists for this connection
			if ( ( !bIsRecentlyRelevant || Actor->bTearOff ) && Channel != NULL )
			{
				// Non startup (map) actors have their channels closed immediately, which destroys them.
				// Startup actors get to keep their channels open.

				// Fixme: this should be a setting
				if ( !bLevelInitializedForActor || !Actor->IsNetStartupActor() )
				{
					UE_LOG( LogNetTraffic, Log, TEXT( "- Closing channel for no longer relevant actor %s" ), *Actor->GetName() );
					Channel->Close();
				}
			}
		}
	}

	SET_DWORD_STAT( STAT_NumReplicatedActorAttempts, ActorUpdatesThisConnection );
	SET_DWORD_STAT( STAT_NumReplicatedActors, ActorUpdatesThisConnectionSent );
	SET_DWORD_STAT( STAT_NumRelevantActors, FinalRelevantCount );

	return FinalSortedCount;
}
#endif

int32 UNetDriver::ServerReplicateActors(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NetServerRepActorsTime);

#if WITH_SERVER_CODE
	if ( ClientConnections.Num() == 0 )
	{
		return 0;
	}

	check( World );

	int32 Updated = 0;

	// Bump the ReplicationFrame value to invalidate any properties marked as "unchanged" for this frame.
	ReplicationFrame++;

	const int32 NumClientsToTick = ServerReplicateActors_PrepConnections( DeltaSeconds );

	if ( NumClientsToTick == 0 )
	{
		// No connections are ready this frame
		return 0;
	}

	AWorldSettings* WorldSettings = World->GetWorldSettings();

	bool bCPUSaturated		= false;
	float ServerTickTime	= GEngine->GetMaxTickRate( DeltaSeconds );
	if ( ServerTickTime == 0.f )
	{
		ServerTickTime = DeltaSeconds;
	}
	else
	{
		ServerTickTime	= 1.f/ServerTickTime;
		bCPUSaturated	= DeltaSeconds > 1.2f * ServerTickTime;
	}

	TArray<FNetworkObjectInfo*> ConsiderList;
	ConsiderList.Reserve( GetNetworkObjectList().GetActiveObjects().Num() );

	// Build the consider list (actors that are ready to replicate)
	ServerReplicateActors_BuildConsiderList( ConsiderList, ServerTickTime );

	FMemMark Mark( FMemStack::Get() );

	for ( int32 i=0; i < ClientConnections.Num(); i++ )
	{
		UNetConnection* Connection = ClientConnections[i];
		check(Connection);

		// net.DormancyValidate can be set to 2 to validate all dormant actors against last known state before going dormant
		if ( CVarNetDormancyValidate.GetValueOnAnyThread() == 2 )
		{
			for ( auto It = Connection->DormantReplicatorMap.CreateIterator(); It; ++It )
			{
				FObjectReplicator& Replicator = It.Value().Get();

				if ( Replicator.OwningChannel != nullptr )
				{
					Replicator.ValidateAgainstState( Replicator.OwningChannel->GetActor() );
				}
			}
		}

		// if this client shouldn't be ticked this frame
		if (i >= NumClientsToTick)
		{
			//UE_LOG(LogNet, Log, TEXT("skipping update to %s"),*Connection->GetName());
			// then mark each considered actor as bPendingNetUpdate so that they will be considered again the next frame when the connection is actually ticked
			for (int32 ConsiderIdx = 0; ConsiderIdx < ConsiderList.Num(); ConsiderIdx++)
			{
				AActor *Actor = ConsiderList[ConsiderIdx]->Actor;
				// if the actor hasn't already been flagged by another connection,
				if (Actor != NULL && !ConsiderList[ConsiderIdx]->bPendingNetUpdate)
				{
					// find the channel
					UActorChannel *Channel = Connection->ActorChannels.FindRef(Actor);
					// and if the channel last update time doesn't match the last net update time for the actor
					if (Channel != NULL && Channel->LastUpdateTime < ConsiderList[ConsiderIdx]->LastNetUpdateTime)
					{
						//UE_LOG(LogNet, Log, TEXT("flagging %s for a future update"),*Actor->GetName());
						// flag it for a pending update
						ConsiderList[ConsiderIdx]->bPendingNetUpdate = true;
					}
				}
			}
			// clear the time sensitive flag to avoid sending an extra packet to this connection
			Connection->TimeSensitive = false;
		}
		else if (Connection->ViewTarget)
		{		
			// Make a list of viewers this connection should consider (this connection and children of this connection)
			TArray<FNetViewer>& ConnectionViewers = WorldSettings->ReplicationViewers;

			ConnectionViewers.Reset();
			new( ConnectionViewers )FNetViewer( Connection, DeltaSeconds );
			for ( int32 ViewerIndex = 0; ViewerIndex < Connection->Children.Num(); ViewerIndex++ )
			{
				if ( Connection->Children[ViewerIndex]->ViewTarget != NULL )
				{
					new( ConnectionViewers )FNetViewer( Connection->Children[ViewerIndex], DeltaSeconds );
				}
			}

			// send ClientAdjustment if necessary
			// we do this here so that we send a maximum of one per packet to that client; there is no value in stacking additional corrections
			if ( Connection->PlayerController )
			{
				Connection->PlayerController->SendClientAdjustment();
			}

			for ( int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++ )
			{
				if ( Connection->Children[ChildIdx]->PlayerController != NULL )
				{
					Connection->Children[ChildIdx]->PlayerController->SendClientAdjustment();
				}
			}

			FMemMark RelevantActorMark(FMemStack::Get());

			FActorPriority* PriorityList	= NULL;
			FActorPriority** PriorityActors = NULL;

			// Get a sorted list of actors for this connection
			const int32 FinalSortedCount = ServerReplicateActors_PrioritizeActors( Connection, ConnectionViewers, ConsiderList, bCPUSaturated, PriorityList, PriorityActors );

			// Process the sorted list of actors for this connection
			const int32 LastProcessedActor = ServerReplicateActors_ProcessPrioritizedActors( Connection, ConnectionViewers, PriorityActors, FinalSortedCount, Updated );

			// relevant actors that could not be processed this frame are marked to be considered for next frame
			for ( int32 k=LastProcessedActor; k<FinalSortedCount; k++ )
			{
				if (!PriorityActors[k]->ActorInfo)
				{
					// A deletion entry, skip it because we dont have anywhere to store a 'better give higher priority next time'
					continue;
				}

				AActor* Actor = PriorityActors[k]->ActorInfo->Actor;

				UActorChannel* Channel = PriorityActors[k]->Channel;
				
				UE_LOG(LogNetTraffic, Verbose, TEXT("Saturated. %s"), *Actor->GetName());
				if (Channel != NULL && Time - Channel->RelevantTime <= 1.f)
				{
					UE_LOG(LogNetTraffic, Log, TEXT(" Saturated. Mark %s NetUpdateTime to be checked for next tick"), *Actor->GetName());
					PriorityActors[k]->ActorInfo->bPendingNetUpdate = true;
				}
				else if ( IsActorRelevantToConnection( Actor, ConnectionViewers ) )
				{
					// If this actor was relevant but didn't get processed, force another update for next frame
					UE_LOG( LogNetTraffic, Log, TEXT( " Saturated. Mark %s NetUpdateTime to be checked for next tick" ), *Actor->GetName() );
					PriorityActors[k]->ActorInfo->bPendingNetUpdate = true;
					if ( Channel != NULL )
					{
						Channel->RelevantTime = Time + 0.5f * FMath::SRand();
					}
				}
			}
			RelevantActorMark.Pop();

			ConnectionViewers.Reset();
		}
	}

	// shuffle the list of connections if not all connections were ticked
	if (NumClientsToTick < ClientConnections.Num())
	{
		int32 NumConnectionsToMove = NumClientsToTick;
		while (NumConnectionsToMove > 0)
		{
			// move all the ticked connections to the end of the list so that the other connections are considered first for the next frame
			UNetConnection *Connection = ClientConnections[0];
			ClientConnections.RemoveAt(0,1);
			ClientConnections.Add(Connection);
			NumConnectionsToMove--;
		}
	}
	Mark.Pop();

	if (DebugRelevantActors)
	{
		PrintDebugRelevantActors();
		LastPrioritizedActors.Empty();
		LastSentActors.Empty();
		LastRelevantActors.Empty();
		LastNonRelevantActors.Empty();

		DebugRelevantActors  = false;
	}

	return Updated;
#else
	return 0;
#endif // WITH_SERVER_CODE
}

void UNetDriver::SetNetDriverName(FName NewNetDriverNamed)
{
	NetDriverName = NewNetDriverNamed;
	InitPacketSimulationSettings();
}

void UNetDriver::PrintDebugRelevantActors()
{
	struct SLocal
	{
		static void AggregateAndPrint( TArray< TWeakObjectPtr<AActor> >	&List, FString txt )
		{
			TMap< TWeakObjectPtr<UClass>, int32>	ClassSummary;
			TMap< TWeakObjectPtr<UClass>, int32>	SuperClassSummary;

			for (auto It = List.CreateIterator(); It; ++It)
			{
				if (AActor* Actor = It->Get())
				{

					ClassSummary.FindOrAdd(Actor->GetClass())++;
					if (Actor->GetClass()->GetSuperStruct())
					{
						SuperClassSummary.FindOrAdd( Actor->GetClass()->GetSuperClass() )++;
					}
				}
			}

			struct FCompareActorClassCount
			{
				FORCEINLINE bool operator()( int32 A, int32 B ) const
				{
					return A < B;
				}
			};


			ClassSummary.ValueSort(FCompareActorClassCount());
			SuperClassSummary.ValueSort(FCompareActorClassCount());

			UE_LOG(LogNet, Warning, TEXT("------------------------------") );
			UE_LOG(LogNet, Warning, TEXT(" %s Class Summary"), *txt );
			UE_LOG(LogNet, Warning, TEXT("------------------------------") );

			for (auto It = ClassSummary.CreateIterator(); It; ++It)
			{
				UE_LOG(LogNet, Warning, TEXT("%4d - %s (%s)"), It.Value(), *It.Key()->GetName(), It.Key()->GetSuperStruct() ? *It.Key()->GetSuperStruct()->GetName() : TEXT("NULL") );
			}

			UE_LOG(LogNet, Warning, TEXT("---------------------------------") );
			UE_LOG(LogNet, Warning, TEXT(" %s Parent Class Summary "), *txt );
			UE_LOG(LogNet, Warning, TEXT("------------------------------") );

			for (auto It = SuperClassSummary.CreateIterator(); It; ++It)
			{
				UE_LOG(LogNet, Warning, TEXT("%4d - %s (%s)"), It.Value(), *It.Key()->GetName(), It.Key()->GetSuperStruct() ? *It.Key()->GetSuperStruct()->GetName() : TEXT("NULL") );
			}

			UE_LOG(LogNet, Warning, TEXT("---------------------------------") );
			UE_LOG(LogNet, Warning, TEXT(" %s Total: %d"), *txt, List.Num() );
			UE_LOG(LogNet, Warning, TEXT("---------------------------------") );
		}
	};

	SLocal::AggregateAndPrint( LastPrioritizedActors, TEXT(" Prioritized Actor") );
	SLocal::AggregateAndPrint( LastRelevantActors, TEXT(" Relevant Actor") );
	SLocal::AggregateAndPrint( LastNonRelevantActors, TEXT(" NonRelevant Actor") );
	SLocal::AggregateAndPrint( LastSentActors, TEXT(" Sent Actor") );

	UE_LOG(LogNet, Warning, TEXT("---------------------------------") );
	UE_LOG(LogNet, Warning, TEXT(" Num Connections: %d"), ClientConnections.Num() );

	UE_LOG(LogNet, Warning, TEXT("---------------------------------") );

}

void UNetDriver::DrawNetDriverDebug()
{
#if ENABLE_DRAW_DEBUG
	UNetConnection *Connection = (ServerConnection ? ServerConnection : (ClientConnections.Num() >= 1 ? ClientConnections[0] : NULL));
	if (!Connection)
	{
		return;
	}

	UWorld* LocalWorld = GetWorld();
	if (!LocalWorld)
	{
		return;
	}

	ULocalPlayer*	LocalPlayer = NULL;
	for(FLocalPlayerIterator It(GEngine, LocalWorld);It;++It)
	{
		LocalPlayer = *It;
		break;
	}
	if (!LocalPlayer)
	{
		return;
	}

	const float CullDistSqr = FMath::Square(CVarNetDormancyDrawCullDistance.GetValueOnAnyThread());

	for (FActorIterator It(LocalWorld); It; ++It)
	{
		if ((It->GetActorLocation() - LocalPlayer->LastViewLocation).SizeSquared() > CullDistSqr)
		{
			continue;
		}
		
		const FNetworkObjectInfo* NetworkObjectInfo = Connection->Driver->GetNetworkObjectInfo( *It );

		FColor	DrawColor;
		if ( NetworkObjectInfo && NetworkObjectInfo->DormantConnections.Contains( Connection ) )
		{
			DrawColor = FColor::Red;
		}
		else if (Connection->ActorChannels.FindRef(*It) != NULL)
		{
			DrawColor = FColor::Green;
		}
		else
		{
			continue;
		}

		FBox Box = 	It->GetComponentsBoundingBox();
		DrawDebugBox( LocalWorld, Box.GetCenter(), Box.GetExtent(), FQuat::Identity, DrawColor, false );
	}
#endif
}

bool UNetDriver::NetObjectIsDynamic(const UObject *Object) const
{
	const UActorComponent *ActorComponent = Cast<const UActorComponent>(Object);
	if (ActorComponent)
	{
		// Actor components are dynamic if their owning actor is.
		return NetObjectIsDynamic(Object->GetOuter());
	}

	const AActor *Actor = Cast<const AActor>(Object);
	if (!Actor || Actor->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || Actor->IsNetStartupActor())
	{
		return false;
	}

	return true;
}

void UNetDriver::AddClientConnection(UNetConnection * NewConnection)
{
	UE_LOG( LogNet, Log, TEXT( "AddClientConnection: Added client connection: %s" ), *NewConnection->Describe() );

	ClientConnections.Add(NewConnection);

	PerfCountersIncrement(TEXT("AddedConnections"));

	// When new connections join, we need to make sure to add all fully dormant actors back to the network list, so they can get processed for the new connection
	// They'll eventually fall back off to this list when they are dormant on the new connection
	GetNetworkObjectList().HandleConnectionAdded();

	for (auto It = DestroyedStartupOrDormantActors.CreateIterator(); It; ++It)
	{
		if (It.Key().IsStatic())
		{
			UE_LOG(LogNet, VeryVerbose, TEXT("Adding actor NetGUID <%s> to new connection's destroy list"), *It.Key().ToString());
			NewConnection->DestroyedStartupOrDormantActors.Add(It.Key());
		}
	}
}

void UNetDriver::SetWorld(class UWorld* InWorld)
{
	if (World)
	{
		// Remove old world association
		UnregisterTickEvents(World);
		World = NULL;
		Notify = NULL;

		GetNetworkObjectList().Reset();
	}

	if (InWorld)
	{
		// Setup new world association
		World = InWorld;
		Notify = InWorld;
		RegisterTickEvents(InWorld);

		GetNetworkObjectList().AddInitialObjects(InWorld, NetDriverName);
	}
}

void UNetDriver::ResetGameWorldState()
{
	DestroyedStartupOrDormantActors.Empty();

	if ( NetCache.IsValid() )
	{
		NetCache->ClearClassNetCache();	// Clear the cache net: it will recreate itself after seamless travel
	}

	GetNetworkObjectList().ResetDormancyState();

	if (ServerConnection)
	{
		ServerConnection->ResetGameWorldState();
	}
	for (auto It = ClientConnections.CreateIterator(); It; ++It)
	{
		(*It)->ResetGameWorldState();
	}
}

void UNetDriver::CleanPackageMaps()
{
	if ( GuidCache.IsValid() )
	{ 
		GuidCache->CleanReferences();
	}
}

void UNetDriver::PreSeamlessTravelGarbageCollect()
{
	ResetGameWorldState();
}

void UNetDriver::PostSeamlessTravelGarbageCollect()
{
	CleanPackageMaps();
}

static void	DumpRelevantActors( UWorld* InWorld )
{
	UNetDriver *NetDriver = InWorld->NetDriver;
	if (!NetDriver)
	{
		return;
	}

	NetDriver->DebugRelevantActors = true;
}

TSharedPtr<FRepChangedPropertyTracker> UNetDriver::FindOrCreateRepChangedPropertyTracker(UObject* Obj)
{
	TSharedPtr<FRepChangedPropertyTracker> * GlobalPropertyTrackerPtr = RepChangedPropertyTrackerMap.Find( Obj );

	if ( !GlobalPropertyTrackerPtr ) 
	{
		const bool bIsReplay = GetWorld() != nullptr && static_cast< void* >( GetWorld()->DemoNetDriver ) == static_cast< void* >( this );
		const bool bIsClientReplayRecording = GetWorld() != nullptr ? GetWorld()->IsRecordingClientReplay() : false;
		FRepChangedPropertyTracker * Tracker = new FRepChangedPropertyTracker( bIsReplay, bIsClientReplayRecording );

		GetObjectClassRepLayout( Obj->GetClass() )->InitChangedTracker( Tracker );

		GlobalPropertyTrackerPtr = &RepChangedPropertyTrackerMap.Add( Obj, TSharedPtr<FRepChangedPropertyTracker>( Tracker ) );
	}

	return *GlobalPropertyTrackerPtr;
}

TSharedPtr<FRepLayout> UNetDriver::GetObjectClassRepLayout( UClass * Class )
{
	TSharedPtr<FRepLayout> * RepLayoutPtr = RepLayoutMap.Find( Class );

	if ( !RepLayoutPtr ) 
	{
		FRepLayout * RepLayout = new FRepLayout();
		RepLayout->InitFromObjectClass( Class );
		RepLayoutPtr = &RepLayoutMap.Add( Class, TSharedPtr<FRepLayout>( RepLayout ) );
	}

	return *RepLayoutPtr;
}

TSharedPtr<FRepLayout> UNetDriver::GetFunctionRepLayout( UFunction * Function )
{
	TSharedPtr<FRepLayout> * RepLayoutPtr = RepLayoutMap.Find( Function );

	if ( !RepLayoutPtr ) 
	{
		FRepLayout * RepLayout = new FRepLayout();
		RepLayout->InitFromFunction( Function );
		RepLayoutPtr = &RepLayoutMap.Add( Function, TSharedPtr<FRepLayout>( RepLayout ) );
	}

	return *RepLayoutPtr;
}

TSharedPtr<FRepLayout> UNetDriver::GetStructRepLayout( UStruct * Struct )
{
	TSharedPtr<FRepLayout> * RepLayoutPtr = RepLayoutMap.Find( Struct );

	if ( !RepLayoutPtr ) 
	{
		FRepLayout * RepLayout = new FRepLayout();
		RepLayout->InitFromStruct( Struct );
		RepLayoutPtr = &RepLayoutMap.Add( Struct, TSharedPtr<FRepLayout>( RepLayout ) );
	}

	return *RepLayoutPtr;
}

TSharedPtr< FReplicationChangelistMgr > UNetDriver::GetReplicationChangeListMgr( UObject* Object )
{
	TSharedPtr< FReplicationChangelistMgr >* ReplicationChangeListMgrPtr = ReplicationChangeListMap.Find( Object );

	if ( !ReplicationChangeListMgrPtr )
	{
		ReplicationChangeListMgrPtr = &ReplicationChangeListMap.Add( Object, TSharedPtr< FReplicationChangelistMgr >( new FReplicationChangelistMgr( this, Object ) ) );
	}

	return *ReplicationChangeListMgrPtr;
}

void UNetDriver::OnLevelRemovedFromWorld(class ULevel* InLevel, class UWorld* InWorld)
{
	if (InWorld == World)
	{
		for (AActor* Actor : InLevel->Actors)
		{
			if (Actor)
			{
				NotifyActorLevelUnloaded(Actor);
				GetNetworkObjectList().Remove(Actor);
			}
		}

		TArray<FNetworkGUID> RemovedGUIDs;
		for (auto It = DestroyedStartupOrDormantActors.CreateIterator(); It; ++It)
		{
			if (It->Value.Level == InLevel)
			{
				RemovedGUIDs.Add(It->Key);
				It.RemoveCurrent();
			}
		}

		if (RemovedGUIDs.Num())
		{
			for (UNetConnection* Connection : ClientConnections)
			{
				for (const FNetworkGUID& GUIDToRemove : RemovedGUIDs)
				{
					Connection->DestroyedStartupOrDormantActors.Remove(GUIDToRemove);
				}
			}
		}
	}
}

FAutoConsoleCommandWithWorld	DumpRelevantActorsCommand(
	TEXT("net.DumpRelevantActors"), 
	TEXT( "Dumps information on relevant actors during next network update" ), 
	FConsoleCommandWithWorldDelegate::CreateStatic(DumpRelevantActors)
	);

/**
 * Exec handler that routes online specific execs to the proper subsystem
 *
 * @param InWorld World context
 * @param Cmd 	the exec command being executed
 * @param Ar 	the archive to log results to
 *
 * @return true if the handler consumed the input, false to continue searching handlers
 */
static bool NetDriverExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bHandled = false;

	// Ignore any execs that don't start with NET
	if (FParse::Command(&Cmd, TEXT("NET")))
	{
		UNetDriver* NamedDriver = NULL;
		TCHAR TokenStr[128];

		// Route the command to a specific beacon if a name is specified or all of them otherwise
		if (FParse::Token(Cmd, TokenStr, ARRAY_COUNT(TokenStr), true))
		{
			NamedDriver = GEngine->FindNamedNetDriver(InWorld, FName(TokenStr));
			if (NamedDriver != NULL)
			{
				bHandled = NamedDriver->Exec(InWorld, Cmd, Ar);
			}
			else
			{
				FWorldContext &Context = GEngine->GetWorldContextFromWorldChecked(InWorld);

				Cmd -= FCString::Strlen(TokenStr);
				for (int32 NetDriverIdx=0; NetDriverIdx < Context.ActiveNetDrivers.Num(); NetDriverIdx++)
				{
					NamedDriver = Context.ActiveNetDrivers[NetDriverIdx].NetDriver;
					if (NamedDriver)
					{
						bHandled |= NamedDriver->Exec(InWorld, Cmd, Ar);
					}
				}
			}
		}
	}

	return bHandled;
}

/** Our entry point for all net driver related exec routing */
FStaticSelfRegisteringExec NetDriverExecRegistration(NetDriverExec);
