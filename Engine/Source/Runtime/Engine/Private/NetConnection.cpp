// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NetConnection.cpp: Unreal connection base class.
=============================================================================*/

#include "Engine/NetConnection.h"
#include "Misc/CommandLine.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "UObject/Package.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LevelStreaming.h"
#include "PacketHandlers/StatelessConnectHandlerComponent.h"
#include "Engine/LocalPlayer.h"
#include "UnrealEngine.h"
#include "EngineUtils.h"
#include "Misc/NetworkVersion.h"
#include "Net/UnrealNetwork.h"
#include "Net/NetworkProfiler.h"
#include "Net/DataReplication.h"
#include "Engine/ActorChannel.h"
#include "Engine/ChildConnection.h"
#include "Net/DataChannel.h"
#include "Engine/PackageMapClient.h"
#include "Engine/NetworkObjectList.h"
#include "EncryptionComponent.h"

#include "Net/PerfCountersHelpers.h"
#include "GameDelegates.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarPingExcludeFrameTime( TEXT( "net.PingExcludeFrameTime" ), 0, TEXT( "Calculate RTT time between NIC's of server and client." ) );
static TAutoConsoleVariable<int32> CVarPingDisplayServerTime( TEXT( "net.PingDisplayServerTime" ), 0, TEXT( "Show server frame time" ) );
#endif

static TAutoConsoleVariable<int32> CVarTickAllOpenChannels( TEXT( "net.TickAllOpenChannels" ), 0, TEXT( "If nonzero, each net connection will tick all of its open channels every tick. Leaving this off will improve performance." ) );

static TAutoConsoleVariable<int32> CVarRandomizeSequence(TEXT("net.RandomizeSequence"), 1, TEXT("Randomize initial packet sequence"));

DECLARE_CYCLE_STAT(TEXT("NetConnection SendAcks"), Stat_NetConnectionSendAck, STATGROUP_Net);
DECLARE_CYCLE_STAT(TEXT("NetConnection Tick"), Stat_NetConnectionTick, STATGROUP_Net);


/*-----------------------------------------------------------------------------
	UNetConnection implementation.
-----------------------------------------------------------------------------*/

UNetConnection* UNetConnection::GNetConnectionBeingCleanedUp = NULL;

UNetConnection::UNetConnection(const FObjectInitializer& ObjectInitializer)
:	UPlayer(ObjectInitializer)
,	Driver				( NULL )
,	PackageMapClass		( UPackageMapClient::StaticClass() )
,	PackageMap			( NULL )
,	ViewTarget			( NULL )
,   OwningActor			( NULL )
,	MaxPacket			( 0 )
,	InternalAck			( false )
,	MaxPacketHandlerBits ( 0 )
,	State				( USOCK_Invalid )
,	Handler()
,	StatelessConnectComponent()
,	PacketOverhead		( 0 )
,	ResponseId			( 0 )

,	QueuedBits			( 0 )
,	TickCount			( 0 )
,	ConnectTime			( 0.0 )

,	AllowMerge			( false )
,	TimeSensitive		( false )
,	LastOutBunch		( NULL )

,	StatPeriod			( 1.f  )
,	BestLag				( 9999 )
,	AvgLag				( 9999 )

,	LagAcc				( 9999 )
,	BestLagAcc			( 9999 )
,	LagCount			( 0 )
,	LastTime			( 0 )
,	FrameTime			( 0 )
,	CumulativeTime		( 0 )
,	AverageFrameTime	( 0 )
,	CountedFrames		( 0 )
,	InBytes				( 0 )
,	OutBytes			( 0 )
,	InPackets			( 0 )
,	OutPackets			( 0 )
,	InBytesPerSecond	( 0 )
,	OutBytesPerSecond	( 0 )
,	InPacketsPerSecond	( 0 )
,	OutPacketsPerSecond	( 0 )

,	SendBuffer			( 0 )
,	InPacketId			( -1 )
,	OutPacketId			( 0 ) // must be initialized as OutAckPacketId + 1 so loss of first packet can be detected
,	OutAckPacketId		( -1 )
,	bLastHasServerFrameTime( false )
,	InitOutReliable		( 0 )
,	InitInReliable		( 0 )
,	EngineNetworkProtocolVersion( FNetworkVersion::GetEngineNetworkProtocolVersion() )
,	GameNetworkProtocolVersion( FNetworkVersion::GetGameNetworkProtocolVersion() )
,	ClientWorldPackageName( NAME_None )
,	bResendAllDataSinceOpen( false )
#if !UE_BUILD_SHIPPING
,	ReceivedRawPacketDel()
#endif
,	PlayerOnlinePlatformName( NAME_None )
{
}

/**
 * Initialize common settings for this connection instance
 *
 * @param InDriver the net driver associated with this connection
 * @param InSocket the socket associated with this connection
 * @param InURL the URL to init with
 * @param InState the connection state to start with for this connection
 * @param InMaxPacket the max packet size that will be used for sending
 * @param InPacketOverhead the packet overhead for this connection type
 */
void UNetConnection::InitBase(UNetDriver* InDriver,class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	// Oodle depends upon this
	check(InMaxPacket <= MAX_PACKET_SIZE);

	// Owning net driver
	Driver = InDriver;

	// Stats
	StatUpdateTime			= Driver->Time;
	LastReceiveTime			= Driver->Time;
	LastReceiveRealtime		= FPlatformTime::Seconds();
	LastGoodPacketRealtime	= FPlatformTime::Seconds();
	LastTime				= FPlatformTime::Seconds();
	LastSendTime			= Driver->Time;
	LastTickTime			= Driver->Time;
	LastRecvAckTime			= Driver->Time;
	ConnectTime				= Driver->Time;

	NetConnectionHistogram.InitHitchTracking();

	// Current state
	State = InState;
	// Copy the URL
	URL = InURL;

	// Use the passed in values
	MaxPacket = InMaxPacket;
	PacketOverhead = InPacketOverhead;

	check(MaxPacket > 0 && PacketOverhead > 0);


	// Reset Handler
	Handler.Reset(NULL);

	InitHandler();


#if DO_ENABLE_NET_TEST
	// Copy the command line settings from the net driver
	UpdatePacketSimulationSettings();
#endif

	// Other parameters.
	CurrentNetSpeed = URL.HasOption(TEXT("LAN")) ? GetDefault<UPlayer>()->ConfiguredLanSpeed : GetDefault<UPlayer>()->ConfiguredInternetSpeed;

	if ( CurrentNetSpeed == 0 )
	{
		CurrentNetSpeed = 2600;
	}
	else
	{
		CurrentNetSpeed = FMath::Max<int32>(CurrentNetSpeed, 1800);
	}

	// Create package map.
	UPackageMapClient* PackageMapClient = NewObject<UPackageMapClient>(this, PackageMapClass);

	if (ensure(PackageMapClient != nullptr))
	{
		PackageMapClient->Initialize(this, Driver->GuidCache);
		PackageMap = PackageMapClient;
	}

	// Create the voice channel
	CreateChannel(CHTYPE_Voice, true, VOICE_CHANNEL_INDEX);
}

/**
 * Initializes an "addressless" connection with the passed in settings
 *
 * @param InDriver the net driver associated with this connection
 * @param InState the connection state to start with for this connection
 * @param InURL the URL to init with
 * @param InConnectionSpeed Optional connection speed override
 */
void UNetConnection::InitConnection(UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed, int32 InMaxPacket)
{
	Driver = InDriver;

	// We won't be sending any packets, so use a default size
	MaxPacket = (InMaxPacket == 0 || InMaxPacket > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : InMaxPacket;
	PacketOverhead = 0;
	State = InState;

#if DO_ENABLE_NET_TEST
	// Copy the command line settings from the net driver
	UpdatePacketSimulationSettings();
#endif

	// Get the 
	if (InConnectionSpeed)
	{
		CurrentNetSpeed = InConnectionSpeed;
	}
	else
	{

		CurrentNetSpeed =  URL.HasOption(TEXT("LAN")) ? GetDefault<UPlayer>()->ConfiguredLanSpeed : GetDefault<UPlayer>()->ConfiguredInternetSpeed;
		if ( CurrentNetSpeed == 0 )
		{
			CurrentNetSpeed = 2600;
		}
		else
		{
			CurrentNetSpeed = FMath::Max<int32>(CurrentNetSpeed, 1800);
		}
	}

	// Create package map.
	auto PackageMapClient = NewObject<UPackageMapClient>(this);
	PackageMapClient->Initialize(this, Driver->GuidCache);
	PackageMap = PackageMapClient;
}

void UNetConnection::InitHandler()
{
	check(!Handler.IsValid());

#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("NoPacketHandler")))
#endif
	{
		Handler = MakeUnique<PacketHandler>();

		if (Handler.IsValid())
		{
			Handler::Mode Mode = Driver->ServerConnection != nullptr ? Handler::Mode::Client : Handler::Mode::Server;

			Handler->InitializeDelegates(FPacketHandlerLowLevelSend::CreateUObject(this, &UNetConnection::LowLevelSend));
			Handler->Initialize(Mode, MaxPacket * 8);


			// Add handling for the stateless connect handshake, for connectionless packets, as the outermost layer
			TSharedPtr<HandlerComponent> NewComponent =
				Handler->AddHandler(TEXT("Engine.EngineHandlerComponentFactory(StatelessConnectHandlerComponent)"), true);

			StatelessConnectComponent = StaticCastSharedPtr<StatelessConnectHandlerComponent>(NewComponent);

			if (StatelessConnectComponent.IsValid())
			{
				StatelessConnectComponent.Pin()->SetDriver(Driver);
			}


			Handler->InitializeComponents();

			MaxPacketHandlerBits = Handler->GetTotalReservedPacketBits();
		}
	}


#if !UE_BUILD_SHIPPING
	uint32 MaxPacketBits = MaxPacket * 8;
	uint32 ReservedTotal = MaxPacketHandlerBits + MAX_PACKET_HEADER_BITS + MAX_PACKET_TRAILER_BITS;

	SET_DWORD_STAT(STAT_MaxPacket, MaxPacketBits);
	SET_DWORD_STAT(STAT_MaxPacketMinusReserved, MaxPacketBits - ReservedTotal);
	SET_DWORD_STAT(STAT_PacketReservedTotal, ReservedTotal);
	SET_DWORD_STAT(STAT_PacketReservedNetConnection, MAX_PACKET_HEADER_BITS + MAX_PACKET_TRAILER_BITS);
	SET_DWORD_STAT(STAT_PacketReservedPacketHandler, MaxPacketHandlerBits);
#endif
}

void UNetConnection::InitSequence(int32 IncomingSequence, int32 OutgoingSequence)
{
	// Make sure the sequence hasn't already been initialized on the server, and ignore multiple initializations on the client
	check(InPacketId == -1 || Driver->ServerConnection != nullptr);

	if (InPacketId == -1 && CVarRandomizeSequence.GetValueOnAnyThread() > 0)
	{
		// Initialize the base UNetConnection packet sequence (not very useful/effective at preventing attacks)
		InPacketId = IncomingSequence - 1;
		OutPacketId = OutgoingSequence;
		OutAckPacketId = OutgoingSequence - 1;

		// Initialize the reliable packet sequence (more useful/effective at preventing attacks)
		InitInReliable = IncomingSequence & (MAX_CHSEQUENCE - 1);
		InitOutReliable = OutgoingSequence & (MAX_CHSEQUENCE - 1);

		UE_LOG(LogNet, Verbose, TEXT("InitSequence: IncomingSequence: %i, OutgoingSequence: %i, InitInReliable: %i, InitOutReliable: %i"), IncomingSequence, OutgoingSequence, InitInReliable, InitOutReliable);

		for (int32 i=0; i<ARRAY_COUNT(InReliable); i++)
		{
			InReliable[i] = InitInReliable;
		}

		for (int32 i=0; i<ARRAY_COUNT(OutReliable); i++)
		{
			OutReliable[i] = InitOutReliable;
		}
	}
}

void UNetConnection::EnableEncryptionWithKey(TArrayView<const uint8> Key)
{
	if (Handler.IsValid())
	{
		UE_LOG(LogNet, Verbose, TEXT("UNetConnection::EnableEncryptionWithKey, %s"), *Describe());

		TSharedPtr<FEncryptionComponent> EncryptionComponent = Handler->GetEncryptionComponent();
		if (EncryptionComponent.IsValid())
		{
			EncryptionComponent->SetEncryptionKey(Key);
			EncryptionComponent->EnableEncryption();
		}
		else
		{
			UE_LOG(LogNet, Warning, TEXT("UNetConnection::EnableEncryptionWithKey, encryption component not found!"));
		}
	}
}

void UNetConnection::EnableEncryptionWithKeyServer(TArrayView<const uint8> Key)
{
	if (State != USOCK_Invalid && State != USOCK_Closed && Driver)
	{
		FNetControlMessage<NMT_EncryptionAck>::Send(this);
		FlushNet();

		EnableEncryptionWithKey(Key);
	}
	else
	{
		UE_LOG(LogNet, Log, TEXT("UNetConnection::EnableEncryptionWithKeyServer, connection in invalid state. %s"), *Describe());
	}
}

void UNetConnection::Serialize( FArchive& Ar )
{
	UObject::Serialize( Ar );
	Ar << PackageMap;
	for( int32 i=0; i<MAX_CHANNELS; i++ )
	{
		Ar << Channels[i];
	}

	if (Ar.IsCountingMemory())
	{
		Children.CountBytes(Ar);
		ClientVisibleLevelNames.CountBytes(Ar);
		QueuedAcks.CountBytes(Ar);
		ResendAcks.CountBytes(Ar);
		OpenChannels.CountBytes(Ar);
		SentTemporaries.CountBytes(Ar);
		ActorChannels.CountBytes(Ar);
	}
}

void UNetConnection::Close()
{
	if (Driver != NULL && State != USOCK_Closed)
	{
		NETWORK_PROFILER(GNetworkProfiler.TrackEvent(TEXT("CLOSE"), *(GetName() + TEXT(" ") + LowLevelGetRemoteAddress()), this));
		UE_LOG(LogNet, Log, TEXT("UNetConnection::Close: %s, Channels: %i, Time: %s"), *Describe(), OpenChannels.Num(), *FDateTime::UtcNow().ToString(TEXT("%Y.%m.%d-%H.%M.%S")));

		if (Channels[0] != NULL)
		{
			Channels[0]->Close();
		}
		State = USOCK_Closed;

		if ((Handler == nullptr || Handler->IsFullyInitialized()) && HasReceivedClientPacket())
		{
			FlushNet();
		}
	}

	LogCallLastTime		= 0;
	LogCallCount		= 0;
	LogSustainedCount	= 0;
}

FString UNetConnection::Describe()
{
	return FString::Printf( TEXT( "[UNetConnection] RemoteAddr: %s, Name: %s, Driver: %s, IsServer: %s, PC: %s, Owner: %s" ),
			*LowLevelGetRemoteAddress( true ),
			*GetName(),
			Driver ? *Driver->GetDescription() : TEXT( "NULL" ),
			Driver && Driver->IsServer() ? TEXT( "YES" ) : TEXT( "NO" ),
			PlayerController ? *PlayerController->GetName() : TEXT( "NULL" ),
			OwningActor ? *OwningActor->GetName() : TEXT( "NULL" ) );
}

void UNetConnection::CleanUp()
{
	// Remove UChildConnection(s)
	for (int32 i = 0; i < Children.Num(); i++)
	{
		Children[i]->CleanUp();
	}
	Children.Empty();

	if ( State != USOCK_Closed )
	{
		UE_LOG( LogNet, Log, TEXT( "UNetConnection::Cleanup: Closing open connection. %s" ), *Describe() );
	}

	Close();

	if (Driver != NULL)
	{
		// Remove from driver.
		if (Driver->ServerConnection)
		{
			check(Driver->ServerConnection == this);
			Driver->ServerConnection = NULL;
		}
		else
		{
			check(Driver->ServerConnection == NULL);
			verify(Driver->ClientConnections.Remove(this) == 1);

			PerfCountersIncrement(TEXT("RemovedConnections"));
		}
	}

	// Kill all channels.
	for (int32 i = OpenChannels.Num() - 1; i >= 0; i--)
	{
		UChannel* OpenChannel = OpenChannels[i];
		if (OpenChannel != NULL)
		{
			OpenChannel->ConditionalCleanUp(true);
		}
	}

	// Cleanup any straggler KeepProcessingActorChannelBunchesMap channels
	for (const TPair<FNetworkGUID, TArray<UActorChannel*>>& MapKeyValuePair : KeepProcessingActorChannelBunchesMap)
	{
		for (UActorChannel* CurChannel : MapKeyValuePair.Value)
		{
			CurChannel->ConditionalCleanUp(true);
		}
	}

	KeepProcessingActorChannelBunchesMap.Empty();

	PackageMap = NULL;

	if (GIsRunning)
	{
		if (OwningActor != NULL)
		{	
			// Cleanup/Destroy the connection actor & controller
			if (!OwningActor->HasAnyFlags( RF_BeginDestroyed | RF_FinishDestroyed ) )
			{
				// UNetConnection::CleanUp can be called from UNetDriver::FinishDestroyed that is called from GC.
				OwningActor->OnNetCleanup(this);
			}
			OwningActor = NULL;
			PlayerController = NULL;
		}
		else 
		{
			FGameDelegates::Get().GetPendingConnectionLostDelegate().Broadcast();
		}
	}

	CleanupDormantActorState();

	Handler.Reset(NULL);

	Driver = NULL;
}

UChildConnection::UChildConnection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UChildConnection::CleanUp()
{
	if (GIsRunning)
	{
		if (OwningActor != NULL)
		{
			if ( !OwningActor->HasAnyFlags( RF_BeginDestroyed | RF_FinishDestroyed ) )
			{
				// Cleanup/Destroy the connection actor & controller	
				OwningActor->OnNetCleanup(this);
			}

			OwningActor = NULL;
			PlayerController = NULL;
		}
	}
	PackageMap = NULL;
	Driver = NULL;
}

void UNetConnection::FinishDestroy()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		CleanUp();
	}

	Super::FinishDestroy();
}

void UNetConnection::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UNetConnection* This = CastChecked<UNetConnection>(InThis);

	// Let GC know that we're referencing some UChannel objects
	for( int32 ChIndex=0; ChIndex < MAX_CHANNELS; ++ChIndex )
	{
		Collector.AddReferencedObject( This->Channels[ChIndex], This );
	}

	// Let GC know that we're referencing some UActorChannel objects
	for ( auto It = This->KeepProcessingActorChannelBunchesMap.CreateIterator(); It; ++It )
	{
		const TArray<UActorChannel*>& ChannelArray = It.Value();
		for ( UActorChannel* CurChannel : ChannelArray )
		{
			Collector.AddReferencedObject( CurChannel, This );
		}
	}

	Super::AddReferencedObjects(This, Collector);
}

UWorld* UNetConnection::GetWorld() const
{
	UWorld* World = nullptr;
	if (Driver)
	{
		World = Driver->GetWorld();
	}

	if (!World && OwningActor)
	{
		World = OwningActor->GetWorld();
	}

	return World;
}

bool UNetConnection::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if ( Super::Exec( InWorld, Cmd,Ar) )
	{
		return true;
	}
	else if ( GEngine->Exec( InWorld, Cmd, Ar ) )
	{
		return true;
	}
	return false;
}
void UNetConnection::AssertValid()
{
	// Make sure this connection is in a reasonable state.
	check(State==USOCK_Closed || State==USOCK_Pending || State==USOCK_Open);

}
void UNetConnection::SendPackageMap()
{
}

bool UNetConnection::ClientHasInitializedLevelFor(const UObject* TestObject) const
{
	check(Driver);
	checkSlow(Driver->IsServer());

	// get the level for the object
	const ULevel* Level = NULL;
	for (const UObject* Obj = TestObject; Obj != NULL; Obj = Obj->GetOuter())
	{
		Level = Cast<const ULevel>(Obj);
		if (Level != NULL)
		{
			break;
		}
	}

	UWorld* World = Driver->GetWorld();
	check(World);
	return (Level == NULL || (Level->IsPersistentLevel() && World->GetOutermost()->GetFName() == ClientWorldPackageName) ||
			ClientVisibleLevelNames.Contains(Level->GetOutermost()->GetFName()) );
}

void UNetConnection::ValidateSendBuffer()
{
	if ( SendBuffer.IsError() )
	{
		UE_LOG( LogNetTraffic, Fatal, TEXT( "UNetConnection::ValidateSendBuffer: Out.IsError() == true. NumBits: %i, NumBytes: %i, MaxBits: %i" ), SendBuffer.GetNumBits(), SendBuffer.GetNumBytes(), SendBuffer.GetMaxBits() );
	}
}

void UNetConnection::InitSendBuffer()
{
	check(MaxPacket > 0);

	int32 FinalBufferSize = (MaxPacket * 8) - MaxPacketHandlerBits;

	// Initialize the one outgoing buffer.
	if (FinalBufferSize == SendBuffer.GetMaxBits())
	{
		// Reset all of our values to their initial state without a malloc/free
		SendBuffer.Reset();
	}
	else
	{
		// First time initialization needs to allocate the buffer
		SendBuffer = FBitWriter(FinalBufferSize);
	}

	ResetPacketBitCounts();

	ValidateSendBuffer();
}

void UNetConnection::ReceivedRawPacket( void* InData, int32 Count )
{
#if !UE_BUILD_SHIPPING
	// Add an opportunity for the hook to block further processing
	bool bBlockReceive = false;

	ReceivedRawPacketDel.ExecuteIfBound(InData, Count, bBlockReceive);

	if (bBlockReceive)
	{
		return;
	}
#endif

	uint8* Data = (uint8*)InData;

	if (Handler.IsValid())
	{
		const ProcessedPacket UnProcessedPacket = Handler->Incoming(Data, Count);

		if (!UnProcessedPacket.bError)
		{
			Count = FMath::DivideAndRoundUp(UnProcessedPacket.CountBits, 8);

			if (Count > 0)
			{
				Data = UnProcessedPacket.Data;
			}
			// This packed has been consumed
			else
			{
				return;
			}
		}
		else
		{
			CLOSE_CONNECTION_DUE_TO_SECURITY_VIOLATION(this, ESecurityEvent::Malformed_Packet,
														TEXT("Packet failed PacketHandler processing."));

			return;
		}
		
		// See if we receive a packet that wasn't fully consumed by the handler before the handler is initialized.
		if (!Handler->IsFullyInitialized())
		{
			UE_LOG(LogNet, Warning, TEXT("PacketHander isn't fully initialized and also didn't fully consume a packet! This will cause the connection to try to send a packet before the initial packet sequence has been established. Ignoring. Connection: %s"), *Describe());
			return;
		}
	}


	// Handle an incoming raw packet from the driver.
	UE_LOG(LogNetTraffic, Verbose, TEXT("%6.3f: Received %i"), FPlatformTime::Seconds() - GStartTime, Count );
	int32 PacketBytes = Count + PacketOverhead;
	InBytes += PacketBytes;
	++InPackets;
	Driver->InBytes += PacketBytes;
	Driver->InPackets++;

	if (Count > 0)
	{
		uint8 LastByte = Data[Count-1];

		if (LastByte != 0)
		{
			int32 BitSize = (Count * 8) - 1;

			// Bit streaming, starts at the Least Significant Bit, and ends at the MSB.
			while (!(LastByte & 0x80))
			{
				LastByte *= 2;
				BitSize--;
			}


			FBitReader Reader(Data, BitSize);

			// Set the network version on the reader
			Reader.SetEngineNetVer( EngineNetworkProtocolVersion );
			Reader.SetGameNetVer( GameNetworkProtocolVersion );

			if (Handler.IsValid())
			{
				Handler->IncomingHigh(Reader);
			}

			if (Reader.GetBitsLeft() > 0)
			{
				ReceivedPacket(Reader);
			}
		}
		// MalformedPacket - Received a packet with 0's in the last byte
		else
		{
			CLOSE_CONNECTION_DUE_TO_SECURITY_VIOLATION(this, ESecurityEvent::Malformed_Packet, TEXT("Received packet with 0's in last byte of packet"));
		}
	}
	// MalformedPacket - Received a packet of 0 bytes
	else 
	{
		CLOSE_CONNECTION_DUE_TO_SECURITY_VIOLATION(this, ESecurityEvent::Malformed_Packet, TEXT("Received zero-size packet"));
	}
}


void UNetConnection::FlushNet(bool bIgnoreSimulation)
{
	check(Driver);

	// Update info.
	ValidateSendBuffer();
	LastEnd = FBitWriterMark();
	TimeSensitive = 0;

	// If there is any pending data to send, send it.
	if ( SendBuffer.GetNumBits() || ( Driver->Time-LastSendTime > Driver->KeepAliveTime && !InternalAck && State != USOCK_Closed ) )
	{
		// Due to the PacketHandler handshake code, servers must never send the client data,
		// before first receiving a client control packet (which is taken as an indication of a complete handshake).
		if ( !HasReceivedClientPacket() && CVarRandomizeSequence.GetValueOnAnyThread() != 0 )
		{
			UE_LOG(LogNet, Log, TEXT("Attempting to send data before handshake is complete. %s"), *Describe());
			Close();
			return;
		}

		// If sending keepalive packet, still write the packet id
		if ( SendBuffer.GetNumBits() == 0 )
		{
			WriteBitsToSendBuffer( NULL, 0 );		// This will force the packet id to be written
		}


		// @todo #JohnB: Since OutgoingHigh uses SendBuffer, its ReservedPacketBits needs to be modified to account for this differently
		if (Handler.IsValid())
		{
			Handler->OutgoingHigh(SendBuffer);
		}


		// Write the UNetConnection-level termination bit
		SendBuffer.WriteBit(1);

		ValidateSendBuffer();

		const int32 NumStrayBits = SendBuffer.GetNumBits();

		// @todo: This is no longer accurate, given potential for PacketHandler termination bit and bit padding
		//NumPaddingBits += (NumStrayBits != 0) ? (8 - NumStrayBits) : 0;


		NETWORK_PROFILER(GNetworkProfiler.FlushOutgoingBunches(this));

		// Send now.
#if DO_ENABLE_NET_TEST
		// if the connection is closing/being destroyed/etc we need to send immediately regardless of settings
		// because we won't be around to send it delayed
		if (State == USOCK_Closed || IsGarbageCollecting() || bIgnoreSimulation || InternalAck)
		{
			// Checked in FlushNet() so each child class doesn't have to implement this
			if (Driver->IsNetResourceValid())
			{
				LowLevelSend(SendBuffer.GetData(), SendBuffer.GetNumBytes(), SendBuffer.GetNumBits());
			}
		}
		else if( PacketSimulationSettings.PktOrder )
		{
			DelayedPacket& B = *(new(Delayed)DelayedPacket(SendBuffer.GetData(), SendBuffer.GetNumBytes(), SendBuffer.GetNumBits()));

			for( int32 i=Delayed.Num()-1; i>=0; i-- )
			{
				if( FMath::FRand()>0.50 )
				{
					if( !PacketSimulationSettings.PktLoss || FMath::FRand()*100.f > PacketSimulationSettings.PktLoss )
					{
						// Checked in FlushNet() so each child class doesn't have to implement this
						if (Driver->IsNetResourceValid())
						{
							LowLevelSend( (char*)&Delayed[i].Data[0], Delayed[i].Data.Num(), Delayed[i].SizeBits );
						}
					}
					Delayed.RemoveAt( i );
				}
			}
		}
		else if( PacketSimulationSettings.PktLag )
		{
			if( !PacketSimulationSettings.PktLoss || FMath::FRand()*100.f > PacketSimulationSettings.PktLoss )
			{
				DelayedPacket& B = *(new(Delayed)DelayedPacket(SendBuffer.GetData(), SendBuffer.GetNumBytes(), SendBuffer.GetNumBits()));

				B.SendTime = FPlatformTime::Seconds() + (double(PacketSimulationSettings.PktLag)  + 2.0f * (FMath::FRand() - 0.5f) * double(PacketSimulationSettings.PktLagVariance))/ 1000.f;
			}
		}
		else if( !PacketSimulationSettings.PktLoss || FMath::FRand()*100.f >= PacketSimulationSettings.PktLoss )
		{
#endif
			// Checked in FlushNet() so each child class doesn't have to implement this
			if (Driver->IsNetResourceValid())
			{
				LowLevelSend(SendBuffer.GetData(), SendBuffer.GetNumBytes(), SendBuffer.GetNumBits());
			}
#if DO_ENABLE_NET_TEST
			if( PacketSimulationSettings.PktDup && FMath::FRand()*100.f < PacketSimulationSettings.PktDup )
			{
				// Checked in FlushNet() so each child class doesn't have to implement this
				if (Driver->IsNetResourceValid())
				{
					LowLevelSend((char*)SendBuffer.GetData(), SendBuffer.GetNumBytes(), SendBuffer.GetNumBits());
				}
			}
		}
#endif
		// Update stuff.
		const int32 Index = OutPacketId & (ARRAY_COUNT(OutLagPacketId)-1);

		// Remember the actual time this packet was sent out, so we can compute ping when the ack comes back
		OutLagPacketId[Index]			= OutPacketId;
		OutLagTime[Index]				= FPlatformTime::Seconds();
		OutBytesPerSecondHistory[Index]	= OutBytesPerSecond / 1024;

		OutPacketId++;
		++OutPackets;
		Driver->OutPackets++;

		//Record the packet time to the histogram
		double LastPacketTimeDiffInMs = (Driver->Time - LastSendTime) * 1000.0;
		NetConnectionHistogram.AddMeasurement(LastPacketTimeDiffInMs);

		LastSendTime = Driver->Time;

		const int32 PacketBytes = SendBuffer.GetNumBytes() + PacketOverhead;

		QueuedBits += (PacketBytes * 8);

		OutBytes += PacketBytes;
		Driver->OutBytes += PacketBytes;
		InitSendBuffer();
	}

	// Move acks around.
	for( int32 i=0; i<QueuedAcks.Num(); i++ )
	{
		ResendAcks.Add(QueuedAcks[i]);
	}
	QueuedAcks.Empty(32);
}

int32 UNetConnection::IsNetReady( bool Saturate )
{
	// Return whether we can send more data without saturation the connection.
	if (Saturate)
	{
		QueuedBits = -SendBuffer.GetNumBits();
	}

	return QueuedBits + SendBuffer.GetNumBits() <= 0;
}

void UNetConnection::ReadInput( float DeltaSeconds )
{}

void UNetConnection::ReceivedNak( int32 NakPacketId )
{
	// Update pending NetGUIDs
	PackageMap->ReceivedNak(NakPacketId);

	// Tell channels about Nak
	for( int32 i=OpenChannels.Num()-1; i>=0; i-- )
	{
		UChannel* Channel = OpenChannels[i];
		Channel->ReceivedNak( NakPacketId );
		if( Channel->OpenPacketId.InRange(NakPacketId) )
			Channel->ReceivedAcks(); //warning: May destroy Channel.
	}
}

void UNetConnection::ReceivedPacket( FBitReader& Reader )
{
	SCOPED_NAMED_EVENT(UNetConnection_ReceivedPacket, FColor::Green);
	AssertValid();

	// Handle PacketId.
	if( Reader.IsError() )
	{
		ensureMsgf(false, TEXT("Packet too small") );
		return;
	}

	ValidateSendBuffer();

	//Record the packet time to the histogram
	double LastPacketTimeDiffInMs = (FPlatformTime::Seconds() - LastReceiveRealtime) * 1000.0;
	NetConnectionHistogram.AddMeasurement(LastPacketTimeDiffInMs);

	// Update receive time to avoid timeout.
	LastReceiveTime		= Driver->Time;
	LastReceiveRealtime = FPlatformTime::Seconds();

	// Check packet ordering.
	const int32 PacketId = InternalAck ? InPacketId + 1 : MakeRelative(Reader.ReadInt(MAX_PACKETID),InPacketId,MAX_PACKETID);
	if( PacketId > InPacketId )
	{
		const int32 PacketsLost = PacketId - InPacketId - 1;
		
		if ( PacketsLost > 10 )
		{
			UE_LOG( LogNetTraffic, Log, TEXT( "High single frame packet loss. PacketsLost: %i %s" ), PacketsLost, *Describe() );
		}

		InPacketsLost += PacketsLost;
		Driver->InPacketsLost += PacketsLost;
		InPacketId = PacketId;
	}
	else
	{
		Driver->InOutOfOrderPackets++;
		// Protect against replay attacks
		// We already protect against this for reliable bunches, and unreliable properties
		// The only bunch we would process would be unreliable RPC's, which could allow for replay attacks
		// So rather than add individual protection for unreliable RPC's as well, just kill it at the source, 
		// which protects everything in one fell swoop
		return;
	}


	bool bSkipAck = false;

	// Disassemble and dispatch all bunches in the packet.
	while( !Reader.AtEnd() && State!=USOCK_Closed )
	{
		// Parse the bunch.
		int32 StartPos = Reader.GetPosBits();
		bool IsAck = !!Reader.ReadBit();
		if ( Reader.IsError() )
		{
			CLOSE_CONNECTION_DUE_TO_SECURITY_VIOLATION(this, ESecurityEvent::Malformed_Packet, TEXT("Bunch missing ack flag"));
			return;
		}

		// Process the bunch.
		if( IsAck )
		{
			LastRecvAckTime = Driver->Time;

			// This is an acknowledgment.
			const int32 AckPacketId = MakeRelative(Reader.ReadInt(MAX_PACKETID),OutAckPacketId,MAX_PACKETID);

			if( Reader.IsError() )
			{
				CLOSE_CONNECTION_DUE_TO_SECURITY_VIOLATION(this, ESecurityEvent::Malformed_Packet, TEXT("Bunch missing ack"));
				return;
			}

			double ServerFrameTime = 0;

			// If this is the server, we're reading in the request to send them our frame time
			// If this is the client, we're reading in confirmation that our request to get frame time from server is granted
			const bool bHasServerFrameTime = !!Reader.ReadBit();

#if !UE_BUILD_SHIPPING	// We never actually read it for shipping
			if ( !Driver->IsServer() )
			{
				if ( bHasServerFrameTime )
				{
					// As a client, our request was granted, read the frame time
					uint8 FrameTimeByte	= 0;
					Reader << FrameTimeByte;
					ServerFrameTime = ( double )FrameTimeByte / 1000;
				}
			}
			else
			{
				// Server remembers so he can use during SendAck to notify to client of his frame time
				bLastHasServerFrameTime = bHasServerFrameTime;
			}
#endif
			uint32 RemoteInKBytesPerSecond = 0;
			Reader.SerializeIntPacked( RemoteInKBytesPerSecond );

			// Resend any old reliable packets that the receiver hasn't acknowledged.
			if( AckPacketId>OutAckPacketId )
			{
				for (int32 NakPacketId = OutAckPacketId + 1; NakPacketId<AckPacketId; NakPacketId++, OutPacketsLost++, Driver->OutPacketsLost++)
				{
					UE_LOG(LogNetTraffic, Verbose, TEXT("   Received virtual nak %i (%.1f)"), NakPacketId, (Reader.GetPosBits()-StartPos)/8.f );
					ReceivedNak( NakPacketId );
				}
				OutAckPacketId = AckPacketId;
			}
			else if( AckPacketId<OutAckPacketId )
			{
				//warning: Double-ack logic makes this unmeasurable.
				//OutOrdAcc++;
			}

			// Update ping
			const int32 Index = AckPacketId & (ARRAY_COUNT(OutLagPacketId)-1);

			if ( OutLagPacketId[Index] == AckPacketId )
			{
				OutLagPacketId[Index] = -1;		// Only use the ack once

#if !UE_BUILD_SHIPPING
				if ( CVarPingDisplayServerTime.GetValueOnAnyThread() > 0 )
				{
					UE_LOG( LogNetTraffic, Warning, TEXT( "ServerFrameTime: %2.2f" ), ServerFrameTime * 1000.0f );
				}

				const float GameTime	= ServerFrameTime + FrameTime;
				const float RTT			= ( FPlatformTime::Seconds() - OutLagTime[Index] ) - ( CVarPingExcludeFrameTime.GetValueOnAnyThread() ? GameTime : 0.0f );
				const float NewLag		= FMath::Max( RTT, 0.0f );
#else
				const float NewLag		= FPlatformTime::Seconds() - OutLagTime[Index];
#endif

				if ( OutBytesPerSecondHistory[Index] > 0 )
				{
					RemoteSaturation = ( 1.0f - FMath::Min( ( float )RemoteInKBytesPerSecond / ( float )OutBytesPerSecondHistory[Index], 1.0f ) ) * 100.0f;
				}
				else
				{
					RemoteSaturation = 0.0f;
				}

				//UE_LOG( LogNet, Warning, TEXT( "Out: %i, InRemote: %i, Saturation: %f" ), OutBytesPerSecondHistory[Index], RemoteInKBytesPerSecond, RemoteSaturation );

				LagAcc += NewLag;
				LagCount++;

				if (PlayerController != NULL)
				{
					PlayerController->UpdatePing(NewLag);
				}
			}

			if ( PackageMap != NULL )
			{
				PackageMap->ReceivedAck( AckPacketId );
			}

			// Forward the ack to the channel.
			UE_LOG(LogNetTraffic, Verbose, TEXT("   Received ack %i (%.1f)"), AckPacketId, (Reader.GetPosBits()-StartPos)/8.f );

			for( int32 i=OpenChannels.Num()-1; i>=0; i-- )
			{
				UChannel* Channel = OpenChannels[i];
				
				if( Channel->OpenPacketId.Last==AckPacketId ) // Necessary for unreliable "bNetTemporary" channels.
				{
					Channel->OpenAcked = 1;
				}
				
				for( FOutBunch* OutBunch=Channel->OutRec; OutBunch; OutBunch=OutBunch->Next )
				{
					if (OutBunch->bOpen)
					{
						UE_LOG(LogNet, VeryVerbose, TEXT("Channel %i reset Ackd because open is reliable. "), Channel->ChIndex );
						Channel->OpenAcked  = 0; // We have a reliable open bunch, don't let the above code set the OpenAcked state,
												 // it must be set in UChannel::ReceivedAcks to verify all open bunches were received.
					}

					if( OutBunch->PacketId==AckPacketId )
					{
						OutBunch->ReceivedAck = 1;
					}
				}				
				Channel->ReceivedAcks(); //warning: May destroy Channel.
			}
		}
		else
		{
			// Parse the incoming data.
			FInBunch Bunch( this );
			int32 IncomingStartPos		= Reader.GetPosBits();
			uint8 bControl				= Reader.ReadBit();
			Bunch.PacketId				= PacketId;
			Bunch.bOpen					= bControl ? Reader.ReadBit() : 0;
			Bunch.bClose				= bControl ? Reader.ReadBit() : 0;
			Bunch.bDormant				= Bunch.bClose ? Reader.ReadBit() : 0;
			Bunch.bIsReplicationPaused  = Reader.ReadBit();
			Bunch.bReliable				= Reader.ReadBit();
			Bunch.ChIndex				= Reader.ReadInt( MAX_CHANNELS );
			Bunch.bHasPackageMapExports	= Reader.ReadBit();
			Bunch.bHasMustBeMappedGUIDs	= Reader.ReadBit();
			Bunch.bPartial				= Reader.ReadBit();

			if ( Bunch.bReliable )
			{
				if ( InternalAck )
				{
					// We can derive the sequence for 100% reliable connections
					Bunch.ChSequence = InReliable[Bunch.ChIndex] + 1;
				}
				else
				{
					// If this is a reliable bunch, use the last processed reliable sequence to read the new reliable sequence
					Bunch.ChSequence = MakeRelative( Reader.ReadInt( MAX_CHSEQUENCE ), InReliable[Bunch.ChIndex], MAX_CHSEQUENCE );
				}
			} 
			else if ( Bunch.bPartial )
			{
				// If this is an unreliable partial bunch, we simply use packet sequence since we already have it
				Bunch.ChSequence = PacketId;
			}
			else
			{
				Bunch.ChSequence = 0;
			}

			Bunch.bPartialInitial = Bunch.bPartial ? Reader.ReadBit() : 0;
			Bunch.bPartialFinal	  = Bunch.bPartial ? Reader.ReadBit() : 0;
			Bunch.ChType       = (Bunch.bReliable||Bunch.bOpen) ? Reader.ReadInt(CHTYPE_MAX) : CHTYPE_None;
			int32 BunchDataBits  = Reader.ReadInt( UNetConnection::MaxPacket*8 );

			if ((Bunch.bClose || Bunch.bOpen) && UE_LOG_ACTIVE(LogNetDormancy,VeryVerbose) )
			{
				UE_LOG(LogNetDormancy, VeryVerbose, TEXT("Received: %s"), *Bunch.ToString());
			}

			if (UE_LOG_ACTIVE(LogNetTraffic,VeryVerbose))
			{
				UE_LOG(LogNetTraffic, VeryVerbose, TEXT("Received: %s"), *Bunch.ToString());
			}

			const int32 HeaderPos = Reader.GetPosBits();

			if( Reader.IsError() )
			{
				CLOSE_CONNECTION_DUE_TO_SECURITY_VIOLATION(this, ESecurityEvent::Malformed_Packet, TEXT("Bunch header overflowed"));
				return;
			}
			Bunch.SetData( Reader, BunchDataBits );
			if( Reader.IsError() )
			{
				// Bunch claims it's larger than the enclosing packet.
				CLOSE_CONNECTION_DUE_TO_SECURITY_VIOLATION(this, ESecurityEvent::Invalid_Data, TEXT("Bunch data overflowed (%i %i+%i/%i)"), IncomingStartPos, HeaderPos, BunchDataBits, Reader.GetNumBits());
				return;
			}

			if ( Bunch.bHasPackageMapExports )
			{
				Driver->NetGUIDInBytes += (BunchDataBits + (HeaderPos - IncomingStartPos)) >> 3;

				if ( InternalAck )
				{
					// NOTE - For replays, we do this even earlier, to try and load this as soon as possible, in case there is an issue creating the channel
					// If a replay fails to create a channel, we want to salvage as much as possible
					Cast<UPackageMapClient>( PackageMap )->ReceiveNetGUIDBunch( Bunch );

					if ( Bunch.IsError() )
					{
						UE_LOG( LogNetTraffic, Error, TEXT( "UNetConnection::ReceivedPacket: Bunch.IsError() after ReceiveNetGUIDBunch. ChIndex: %i" ), Bunch.ChIndex );
					}
				}
			}

			if( Bunch.bReliable )
			{
				UE_LOG(LogNetTraffic, Verbose, TEXT("   Reliable Bunch, Channel %i Sequence %i: Size %.1f+%.1f"), Bunch.ChIndex, Bunch.ChSequence, (HeaderPos-IncomingStartPos)/8.f, (Reader.GetPosBits()-HeaderPos)/8.f );
			}
			else
			{
				UE_LOG(LogNetTraffic, Verbose, TEXT("   Unreliable Bunch, Channel %i: Size %.1f+%.1f"), Bunch.ChIndex, (HeaderPos-IncomingStartPos)/8.f, (Reader.GetPosBits()-HeaderPos)/8.f );
			}

			if ( Bunch.bOpen )
			{
				UE_LOG(LogNetTraffic, Verbose, TEXT("   bOpen Bunch, Channel %i Sequence %i: Size %.1f+%.1f"), Bunch.ChIndex, Bunch.ChSequence, (HeaderPos-IncomingStartPos)/8.f, (Reader.GetPosBits()-HeaderPos)/8.f );
			}

			if ( Channels[Bunch.ChIndex] == NULL && ( Bunch.ChIndex != 0 || Bunch.ChType != CHTYPE_Control ) )
			{
				// Can't handle other channels until control channel exists.
				if ( Channels[0] == NULL )
				{
					//CLOSE_CONNECTION_DUE_TO_SECURITY_VIOLATION(this, ESecurityEvent::Malformed_Packet, TEXT( "UNetConnection::ReceivedPacket: Received non-control bunch before control channel was created. ChIndex: %i, ChType: %i" ), Bunch.ChIndex, Bunch.ChType);
					UE_LOG( LogNetTraffic, Log, TEXT( "UNetConnection::ReceivedPacket: Received non-control bunch before control channel was created. ChIndex: %i, ChType: %i" ), Bunch.ChIndex, Bunch.ChType );
					Close();
					return;
				}
				// on the server, if we receive bunch data for a channel that doesn't exist while we're still logging in,
				// it's either a broken client or a new instance of a previous connection,
				// so reject it
				else if ( PlayerController == NULL && Driver->ClientConnections.Contains( this ) )
				{
					CLOSE_CONNECTION_DUE_TO_SECURITY_VIOLATION(this, ESecurityEvent::Malformed_Packet, TEXT( "UNetConnection::ReceivedPacket: Received non-control bunch before player controller was assigned. ChIndex: %i, ChType: %i" ), Bunch.ChIndex, Bunch.ChType);
					return;
				}
			}
			// ignore control channel close if it hasn't been opened yet
			if ( Bunch.ChIndex == 0 && Channels[0] == NULL && Bunch.bClose && Bunch.ChType == CHTYPE_Control )
			{
				//CLOSE_CONNECTION_DUE_TO_SECURITY_VIOLATION(this, ESecurityEvent::Malformed_Packet, TEXT( "UNetConnection::ReceivedPacket: Received control channel close before open" ));
				UE_LOG( LogNetTraffic, Log, TEXT( "UNetConnection::ReceivedPacket: Received control channel close before open" ) );
				Close();
				return;
			}

			// Receiving data.
			UChannel* Channel = Channels[Bunch.ChIndex];

			// Ignore if reliable packet has already been processed.
			if ( Bunch.bReliable && Bunch.ChSequence <= InReliable[Bunch.ChIndex] )
			{
				UE_LOG( LogNetTraffic, Log, TEXT( "UNetConnection::ReceivedPacket: Received outdated bunch (Channel %d Current Sequence %i)" ), Bunch.ChIndex, InReliable[Bunch.ChIndex] );
				check( !InternalAck );		// Should be impossible with 100% reliable connections
				continue;
			}
			
			// If opening the channel with an unreliable packet, check that it is "bNetTemporary", otherwise discard it
			if( !Channel && !Bunch.bReliable )
			{
				// Unreliable bunches that open channels should be bOpen && (bClose || bPartial)
				// NetTemporary usually means one bunch that is unreliable (bOpen and bClose):	1(bOpen, bClose)
				// But if that bunch export NetGUIDs, it will get split into 2 bunches:			1(bOpen, bPartial) - 2(bClose).
				// (the initial actor bunch itself could also be split into multiple bunches. So bPartial is the right check here)

				const bool ValidUnreliableOpen = Bunch.bOpen && (Bunch.bClose || Bunch.bPartial);
				if (!ValidUnreliableOpen)
				{
					if ( InternalAck )
					{
						// Should be impossible with 100% reliable connections
						UE_LOG( LogNetTraffic, Error, TEXT( "      Received unreliable bunch before open with reliable connection (Channel %d Current Sequence %i)" ), Bunch.ChIndex, InReliable[Bunch.ChIndex] );
					}
					else
					{
						// Simply a log (not a warning, since this can happen under normal conditions, like from a re-join, etc)
						UE_LOG( LogNetTraffic, Log, TEXT( "      Received unreliable bunch before open (Channel %d Current Sequence %i)" ), Bunch.ChIndex, InReliable[Bunch.ChIndex] );
					}

					// Since we won't be processing this packet, don't ack it
					// We don't want the sender to think this bunch was processed when it really wasn't
					bSkipAck = true;
					continue;
				}
			}

			// Create channel if necessary.
			if( !Channel )
			{
				// Validate channel type.
				if ( !Driver->IsKnownChannelType( Bunch.ChType ) )
				{
					// Unknown type.
					CLOSE_CONNECTION_DUE_TO_SECURITY_VIOLATION(this, ESecurityEvent::Invalid_Data, TEXT( "UNetConnection::ReceivedPacket: Connection unknown channel type (%i)" ), (int)Bunch.ChType);
					return;
				}

				// Reliable (either open or later), so create new channel.
				UE_LOG(LogNetTraffic, Log, TEXT("      Bunch Create %i: ChType %i, ChSequence: %i, bReliable: %i, bPartial: %i, bPartialInitial: %i, bPartialFinal: %i"), Bunch.ChIndex, Bunch.ChType, Bunch.ChSequence, (int)Bunch.bReliable, (int)Bunch.bPartial, (int)Bunch.bPartialInitial, (int)Bunch.bPartialFinal );
				Channel = CreateChannel( (EChannelType)Bunch.ChType, false, Bunch.ChIndex );

				// Notify the server of the new channel.
				if( !Driver->Notify->NotifyAcceptingChannel( Channel ) )
				{
					// Channel refused, so close it, flush it, and delete it.
					UE_LOG(LogNet, Verbose, TEXT("      NotifyAcceptingChannel Failed! Channel: %s"), *Channel->Describe() );

					FOutBunch CloseBunch( Channel, 1 );
					check(!CloseBunch.IsError());
					check(CloseBunch.bClose);
					CloseBunch.bReliable = 1;
					Channel->SendBunch( &CloseBunch, 0 );
					FlushNet();
					Channel->ConditionalCleanUp();
					if( Bunch.ChIndex==0 )
					{
						UE_LOG(LogNetTraffic, Log, TEXT("Channel 0 create failed") );
						State = USOCK_Closed;
					}
					continue;
				}
			}

			// Dispatch the raw, unsequenced bunch to the channel.
			bool bLocalSkipAck = false;
			Channel->ReceivedRawBunch( Bunch, bLocalSkipAck ); //warning: May destroy channel.
			if ( bLocalSkipAck )
			{
				bSkipAck = true;
			}
			Driver->InBunches++;

			// Disconnect if we received a corrupted packet from the client (eg server crash attempt).
			if ( !Driver->ServerConnection && ( Bunch.IsCriticalError() || Bunch.IsError() ) )
			{
				UE_LOG( LogNetTraffic, Error, TEXT("Received corrupted packet data from client %s.  Disconnecting."), *LowLevelGetRemoteAddress() );
				State = USOCK_Closed;
				bSkipAck = true;
			}
		}
	}

	ValidateSendBuffer();

	// Acknowledge the packet.
	if ( !bSkipAck )
	{
		LastGoodPacketRealtime = FPlatformTime::Seconds();

		SendAck(PacketId, true);
	}
}

int32 UNetConnection::WriteBitsToSendBuffer( 
	const uint8 *	Bits, 
	const int32		SizeInBits, 
	const uint8 *	ExtraBits, 
	const int32		ExtraSizeInBits,
	EWriteBitsDataType DataType)
{
	ValidateSendBuffer();

#if !UE_BUILD_SHIPPING
	// Now that the stateless handshake is responsible for initializing the packet sequence numbers,
	//	we can't allow any packets to be written to the send buffer until after this has completed
	if (CVarRandomizeSequence.GetValueOnAnyThread() > 0)
	{
		checkf(!Handler.IsValid() || Handler->IsFullyInitialized(), TEXT("Attempted to write to send buffer before packet handler was fully initialized. Connection: %s"), *Describe());
	}
#endif

	const int32 TotalSizeInBits = SizeInBits + ExtraSizeInBits;

	// Flush if we can't add to current buffer
	if ( TotalSizeInBits > GetFreeSendBufferBits() )
	{
		FlushNet();
	}

	// Remember start position in case we want to undo this write
	// Store this after the possible flush above so we have the correct start position in the case that we do flush
	LastStart = FBitWriterMark( SendBuffer );

	// If this is the start of the queue, make sure to add the packet id
	if ( SendBuffer.GetNumBits() == 0 && !InternalAck )
	{
		SendBuffer.WriteIntWrapped( OutPacketId, MAX_PACKETID );
		ValidateSendBuffer();

		NumPacketIdBits += SendBuffer.GetNumBits();
	}

	// Add the bits to the queue
	if ( SizeInBits )
	{
		SendBuffer.SerializeBits( const_cast< uint8* >( Bits ), SizeInBits );
		ValidateSendBuffer();
	}

	// Add any extra bits
	if ( ExtraSizeInBits )
	{
		SendBuffer.SerializeBits( const_cast< uint8* >( ExtraBits ), ExtraSizeInBits );
		ValidateSendBuffer();
	}

	const int32 RememberedPacketId = OutPacketId;

	switch ( DataType )
	{
		case EWriteBitsDataType::Bunch:
			NumBunchBits += SizeInBits + ExtraSizeInBits;
			break;
		case EWriteBitsDataType::Ack:
			NumAckBits += SizeInBits + ExtraSizeInBits;
			break;
		default:
			break;
	}

	// Flush now if we are full
	if ( GetFreeSendBufferBits() == 0 )
	{
		FlushNet();
	}

	return RememberedPacketId;
}

/** Returns number of bits left in current packet that can be used without causing a flush  */
int64 UNetConnection::GetFreeSendBufferBits()
{
	// If we haven't sent anything yet, make sure to account for the packet header + trailer size
	// Otherwise, we only need to account for trailer size
	const int32 ExtraBits = ( SendBuffer.GetNumBits() > 0 ) ? MAX_PACKET_TRAILER_BITS : MAX_PACKET_HEADER_BITS + MAX_PACKET_TRAILER_BITS;

	const int32 NumberOfFreeBits = SendBuffer.GetMaxBits() - ( SendBuffer.GetNumBits() + ExtraBits );

	check( NumberOfFreeBits >= 0 );

	return NumberOfFreeBits;
}

void UNetConnection::PopLastStart()
{
	NumBunchBits -= SendBuffer.GetNumBits() - LastStart.GetNumBits();
	LastStart.Pop(SendBuffer);
	NETWORK_PROFILER(GNetworkProfiler.PopSendBunch(this));
}

TSharedPtr<FObjectReplicator> UNetConnection::CreateReplicatorForNewActorChannel(UObject* Object)
{
	TSharedPtr<FObjectReplicator> NewReplicator = MakeShareable(new FObjectReplicator());
	NewReplicator->InitWithObject( Object, this, true );
	return NewReplicator;
}

void UNetConnection::PurgeAcks()
{
	for ( int32 i = 0; i < ResendAcks.Num(); i++ )
	{
		SendAck( ResendAcks[i], 0 );
	}

	ResendAcks.Empty(32);
}


void UNetConnection::SendAck(int32 AckPacketId, bool FirstTime/*=1*/)
{
	SCOPE_CYCLE_COUNTER(Stat_NetConnectionSendAck);

	ValidateSendBuffer();

	if( !InternalAck )
	{
		if( FirstTime )
		{
			PurgeAcks();
			QueuedAcks.Add(AckPacketId);
		}

		FBitWriter AckData( 32, true );

		AckData.WriteBit( 1 );
		AckData.WriteIntWrapped(AckPacketId, MAX_PACKETID);
		
#if !UE_BUILD_SHIPPING	// We never actually write it for shipping
		const bool bHasServerFrameTime = Driver->IsServer() ? bLastHasServerFrameTime : ( CVarPingExcludeFrameTime.GetValueOnGameThread() > 0 ? true : false );

		AckData.WriteBit( bHasServerFrameTime ? 1 : 0 );

		if ( Driver->IsServer() && bHasServerFrameTime )
		{
			uint8 FrameTimeByte = FMath::Min( FMath::FloorToInt( FrameTime * 1000 ), 255 );
			AckData << FrameTimeByte;
		}
#else
		// We still write the bit in shipping to keep the format the same
		AckData.WriteBit( 0 );
#endif

		// Notify server of our current rate per second at this time
		uint32 InKBytesPerSecond = InBytesPerSecond / 1024;
		AckData.SerializeIntPacked( InKBytesPerSecond );

		NETWORK_PROFILER( GNetworkProfiler.TrackSendAck( AckData.GetNumBits(), this ) );

		WriteBitsToSendBuffer( AckData.GetData(), AckData.GetNumBits(), nullptr, 0, EWriteBitsDataType::Ack );

		AllowMerge = false;

		TimeSensitive = 1;

		UE_LOG(LogNetTraffic, Log, TEXT("   Send ack %i"), AckPacketId);
	}
}


int32 UNetConnection::SendRawBunch( FOutBunch& Bunch, bool InAllowMerge )
{
	ValidateSendBuffer();
	check(!Bunch.ReceivedAck);
	check(!Bunch.IsError());
	Driver->OutBunches++;
	TimeSensitive = 1;

	// Build header.
	FBitWriter Header( MAX_BUNCH_HEADER_BITS );
	Header.WriteBit( 0 );
	Header.WriteBit( Bunch.bOpen || Bunch.bClose );
	if( Bunch.bOpen || Bunch.bClose )
	{
		Header.WriteBit( Bunch.bOpen );
		Header.WriteBit( Bunch.bClose );
		if( Bunch.bClose )
		{
			Header.WriteBit( Bunch.bDormant );
		}
	}
	Header.WriteBit( Bunch.bIsReplicationPaused );
	Header.WriteBit( Bunch.bReliable );
	Header.WriteIntWrapped(Bunch.ChIndex, MAX_CHANNELS);
	Header.WriteBit( Bunch.bHasPackageMapExports );
	Header.WriteBit( Bunch.bHasMustBeMappedGUIDs );
	Header.WriteBit( Bunch.bPartial );

	if ( Bunch.bReliable && !InternalAck )
	{
		Header.WriteIntWrapped(Bunch.ChSequence, MAX_CHSEQUENCE);
	}

	if (Bunch.bPartial)
	{
		Header.WriteBit( Bunch.bPartialInitial );
		Header.WriteBit( Bunch.bPartialFinal );
	}

	if (Bunch.bReliable || Bunch.bOpen)
	{
		Header.WriteIntWrapped(Bunch.ChType, CHTYPE_MAX);
	}
	
	Header.WriteIntWrapped(Bunch.GetNumBits(), UNetConnection::MaxPacket * 8);
	check(!Header.IsError());

	// Remember start position.
	AllowMerge      = InAllowMerge;
	Bunch.Time      = Driver->Time;

	if ((Bunch.bClose || Bunch.bOpen) && UE_LOG_ACTIVE(LogNetDormancy,VeryVerbose) )
	{
		UE_LOG(LogNetDormancy, VeryVerbose, TEXT("Sending: %s"), *Bunch.ToString());
	}

	if (UE_LOG_ACTIVE(LogNetTraffic,VeryVerbose))
	{
		UE_LOG(LogNetTraffic, VeryVerbose, TEXT("Sending: %s"), *Bunch.ToString());
	}

	NETWORK_PROFILER(GNetworkProfiler.PushSendBunch(this, &Bunch, Header.GetNumBits(), Bunch.GetNumBits()));

	// Write the bits to the buffer and remember the packet id used
	Bunch.PacketId = WriteBitsToSendBuffer( Header.GetData(), Header.GetNumBits(), Bunch.GetData(), Bunch.GetNumBits(), EWriteBitsDataType::Bunch );

	UE_LOG(LogNetTraffic, Verbose, TEXT("UNetConnection::SendRawBunch. ChIndex: %d. Bits: %d. PacketId: %d"), Bunch.ChIndex, Bunch.GetNumBits(), Bunch.PacketId );

	if ( PackageMap && Bunch.bHasPackageMapExports )
	{
		PackageMap->NotifyBunchCommit( Bunch.PacketId, &Bunch );
	}

	if ( Bunch.bHasPackageMapExports )
	{
		Driver->NetGUIDOutBytes += (Header.GetNumBits() + Bunch.GetNumBits()) >> 3;
	}

	return Bunch.PacketId;
}

UChannel* UNetConnection::CreateChannel( EChannelType ChType, bool bOpenedLocally, int32 ChIndex )
{
	check(Driver->IsKnownChannelType(ChType));
	AssertValid();

	// If no channel index was specified, find the first available.
	if( ChIndex==INDEX_NONE )
	{
		int32 FirstChannel = 1;
		// Control channel is hardcoded to live at location 0
		if ( ChType == CHTYPE_Control )
		{
			FirstChannel = 0;
		}

		// If this is a voice channel, use its predefined channel index
		if (ChType == CHTYPE_Voice)
		{
			FirstChannel = VOICE_CHANNEL_INDEX;
		}

		// Search the channel array for an available location
		for( ChIndex=FirstChannel; ChIndex<MAX_CHANNELS; ChIndex++ )
		{
			if( !Channels[ChIndex] )
			{
				break;
			}
		}
		// Fail to create if the channel array is full
		if( ChIndex==MAX_CHANNELS )
		{
			return NULL;
		}
	}

	// Make sure channel is valid.
	check(ChIndex<MAX_CHANNELS);
	check(Channels[ChIndex]==NULL);

	// Create channel.
	UChannel* Channel = NewObject<UChannel>(GetTransientPackage(), Driver->ChannelClasses[ChType]);
	Channel->Init( this, ChIndex, bOpenedLocally );
	Channels[ChIndex] = Channel;
	OpenChannels.Add(Channel);
	// Always tick the control & voice channels
	if (Channel->ChType == CHTYPE_Control || Channel->ChType == CHTYPE_Voice)
	{
		StartTickingChannel(Channel);
	}
	UE_LOG(LogNetTraffic, Log, TEXT("Created channel %i of type %i"), ChIndex, (int32)ChType);

	return Channel;
}

/**
 * @return Finds the voice channel for this connection or NULL if none
 */
UVoiceChannel* UNetConnection::GetVoiceChannel()
{
	return Channels[VOICE_CHANNEL_INDEX] != NULL && Channels[VOICE_CHANNEL_INDEX]->ChType == CHTYPE_Voice ?
		(UVoiceChannel*)Channels[VOICE_CHANNEL_INDEX] : NULL;
}

float UNetConnection::GetTimeoutValue()
{
#if !UE_BUILD_SHIPPING
	if (Driver->bNoTimeouts)
	{
		// APlayerController depends on this timeout to destroy itself and free up
		// its resources, so we have to handle this case here as well
		return bPendingDestroy ? 2.f : MAX_FLT;
	}
#endif

	float Timeout = Driver->InitialConnectTimeout;

	if ((State != USOCK_Pending) && (bPendingDestroy || (OwningActor && OwningActor->UseShortConnectTimeout())))
	{
		const float ConnectionTimeout = Driver->ConnectionTimeout;

		// If the connection is pending destroy give it 2 seconds to try to finish sending any reliable packets
		Timeout = bPendingDestroy ? 2.f : ConnectionTimeout;
	}

	// Longtimeouts allows a multiplier to be added to get correct disconnection behavior
	// with with additional leniancy when required. Implicit in debug/editor builds
	static bool LongTimeouts = FParse::Param(FCommandLine::Get(), TEXT("longtimeouts"));

	if (Driver->TimeoutMultiplierForUnoptimizedBuilds > 0 
		&& (LongTimeouts || WITH_EDITOR || UE_BUILD_DEBUG)
		)
	{
		Timeout *= Driver->TimeoutMultiplierForUnoptimizedBuilds;
	}

	return Timeout;
}

void UNetConnection::Tick()
{
	SCOPE_CYCLE_COUNTER(Stat_NetConnectionTick);

	AssertValid();

	// Lag simulation.
#if DO_ENABLE_NET_TEST
	if( PacketSimulationSettings.PktLag )
	{
		for( int32 i=0; i < Delayed.Num(); i++ )
		{
			if( FPlatformTime::Seconds() > Delayed[i].SendTime )
			{
				LowLevelSend((char*)&Delayed[i].Data[0], Delayed[i].Data.Num(), Delayed[i].SizeBits);
				Delayed.RemoveAt( i );
				i--;
			}
			else
			{
				// Break now instead of continuing to iterate through the list. Otherwise LagVariance may cause out of order sends
				break;
			}
		}
	}
#endif

	// Get frame time.
	const double CurrentRealtimeSeconds = FPlatformTime::Seconds();
	FrameTime = CurrentRealtimeSeconds - LastTime;
	LastTime = CurrentRealtimeSeconds;
	CumulativeTime += FrameTime;
	CountedFrames++;
	if(CumulativeTime > 1.f)
	{
		AverageFrameTime = CumulativeTime / CountedFrames;
		CumulativeTime = 0;
		CountedFrames = 0;
	}

	// Pretend everything was acked, for 100% reliable connections or demo recording.
	if( InternalAck )
	{
		OutAckPacketId = OutPacketId;

		LastReceiveTime = Driver->Time;
		LastReceiveRealtime = FPlatformTime::Seconds();
		LastGoodPacketRealtime = FPlatformTime::Seconds();
		for( int32 i=OpenChannels.Num()-1; i>=0; i-- )
		{
			UChannel* It = OpenChannels[i];
			for( FOutBunch* OutBunch=It->OutRec; OutBunch; OutBunch=OutBunch->Next )
			{
				OutBunch->ReceivedAck = 1;
			}

			if (Driver->IsServer() || It->OpenedLocally)
			{
				It->OpenAcked = 1;
			}

			It->ReceivedAcks();
		}
	}

	// Update stats.
	if ( CurrentRealtimeSeconds - StatUpdateTime > StatPeriod )
	{
		// Update stats.
		const float RealTime = CurrentRealtimeSeconds - StatUpdateTime;
		if( LagCount )
		{
			AvgLag = LagAcc/LagCount;
		}
		BestLag = AvgLag;

		InBytesPerSecond = FMath::TruncToInt(static_cast<float>(InBytes) / RealTime);
		OutBytesPerSecond = FMath::TruncToInt(static_cast<float>(OutBytes) / RealTime);
		InPacketsPerSecond = FMath::TruncToInt(static_cast<float>(InPackets) / RealTime);
		OutPacketsPerSecond = FMath::TruncToInt(static_cast<float>(OutPackets) / RealTime);

		// Init counters.
		LagAcc = 0;
		StatUpdateTime = CurrentRealtimeSeconds;
		BestLagAcc = 9999;
		LagCount = 0;
		InPacketsLost = 0;
		OutPacketsLost = 0;
		InBytes = 0;
		OutBytes = 0;
		InPackets = 0;
		OutPackets = 0;
	}

	// Compute time passed since last update.
	const float DeltaTime	= Driver->Time - LastTickTime;
	LastTickTime			= Driver->Time;

	// Handle timeouts.
	const float Timeout = GetTimeoutValue();

	if ((Driver->Time - LastReceiveTime) > Timeout)
	{
		// Compute true realtime since packet was received (as well as truly processed)
		const double Seconds = FPlatformTime::Seconds();

		const float ReceiveRealtimeDelta	= Seconds - LastReceiveRealtime;
		const float GoodRealtimeDelta		= Seconds - LastGoodPacketRealtime;

		// Timeout.
		FString Error = FString::Printf(TEXT("UNetConnection::Tick: Connection TIMED OUT. Closing connection. Elapsed: %2.2f, Real: %2.2f, Good: %2.2f, DriverTime: %2.2f, Threshold: %2.2f, %s"),
			Driver->Time - LastReceiveTime,
			ReceiveRealtimeDelta,
			GoodRealtimeDelta,
			Driver->Time,
			Timeout,
			*Describe());
		UE_LOG(LogNet, Warning, TEXT("%s"), *Error);
		GEngine->BroadcastNetworkFailure(Driver->GetWorld(), Driver, ENetworkFailure::ConnectionTimeout, Error);
		Close();

		PerfCountersIncrement(TEXT("TimedoutConnections"));

		if (Driver == NULL)
		{
            // Possible that the Broadcast above caused someone to kill the net driver, early out
			return;
		}
	}
	else
	{
		// We should never need more ticking channels than open channels
		checkf(ChannelsToTick.Num() <= OpenChannels.Num(), TEXT("More ticking channels (%d) than open channels (%d) for net connection!"), ChannelsToTick.Num(), OpenChannels.Num())

		// Tick the channels.
		if (CVarTickAllOpenChannels.GetValueOnAnyThread() == 0)
		{
			for( int32 i=ChannelsToTick.Num()-1; i>=0; i-- )
			{
				ChannelsToTick[i]->Tick();

				if (ChannelsToTick[i]->CanStopTicking())
				{
					ChannelsToTick.RemoveAt(i);
				}
			}
		}
		else
		{
			for( int32 i=OpenChannels.Num()-1; i>=0; i-- )
			{
				OpenChannels[i]->Tick();
			}
		}

		for ( auto ProcessingActorMapIter = KeepProcessingActorChannelBunchesMap.CreateIterator(); ProcessingActorMapIter; ++ProcessingActorMapIter )
		{
			TArray<UActorChannel*>& ActorChannelArray = ProcessingActorMapIter.Value();
			for ( int32 ActorChannelIdx = 0; ActorChannelIdx < ActorChannelArray.Num(); ++ActorChannelIdx )
			{
				UActorChannel* CurChannel = ActorChannelArray[ActorChannelIdx];
				
				bool bRemoveChannel = false;
				if ( CurChannel && !CurChannel->IsPendingKill() )
				{
					check( CurChannel->ChIndex == -1 );
					if ( CurChannel->ProcessQueuedBunches() )
					{
						// Since we are done processing bunches, we can now actually clean this channel up
						CurChannel->ConditionalCleanUp();

						bRemoveChannel = true;
						UE_LOG( LogNet, VeryVerbose, TEXT("UNetConnection::Tick: Removing from KeepProcessingActorChannelBunchesMap. Num: %i"), KeepProcessingActorChannelBunchesMap.Num() );
					}

				}
				else
				{
					bRemoveChannel = true;
					UE_LOG( LogNet, Verbose, TEXT("UNetConnection::Tick: Removing from KeepProcessingActorChannelBunchesMap before done processing bunches. Num: %i"), KeepProcessingActorChannelBunchesMap.Num() );
				}

				// Remove the actor channel from the array
				if ( bRemoveChannel )
				{
					ActorChannelArray.RemoveAt( ActorChannelIdx, 1, false );
					--ActorChannelIdx;
				}
			}

			if ( ActorChannelArray.Num() == 0 )
			{
				ProcessingActorMapIter.RemoveCurrent();
			}
		}

		// If channel 0 has closed, mark the connection as closed.
		if (Channels[0] == nullptr && (OutReliable[0] != InitOutReliable || InReliable[0] != InitInReliable))
		{
			State = USOCK_Closed;
		}
	}

	// Flush.
	PurgeAcks();

	if ( TimeSensitive || (Driver->Time - LastSendTime) > Driver->KeepAliveTime)
	{
		bool bHandlerHandshakeComplete = !Handler.IsValid() || Handler->IsFullyInitialized();

		// Delay any packet sends on the server, until we've verified that a packet has been received from the client.
		if (bHandlerHandshakeComplete && HasReceivedClientPacket())
		{
			FlushNet();
		}
	}

	// Tick Handler
	if (Handler.IsValid())
	{
		Handler->Tick(FrameTime);

		// Resend any queued up raw packets (these come from the reliability handler)
		BufferedPacket* ResendPacket = Handler->GetQueuedRawPacket();

		if (ResendPacket && Driver->IsNetResourceValid())
		{
			Handler->SetRawSend(true);

			while (ResendPacket != nullptr)
			{
				LowLevelSend(ResendPacket->Data, FMath::DivideAndRoundUp(ResendPacket->CountBits, 8u), ResendPacket->CountBits);
				ResendPacket = Handler->GetQueuedRawPacket();
			}

			Handler->SetRawSend(false);
		}

		BufferedPacket* QueuedPacket = Handler->GetQueuedPacket();

		/* Send all queued packets */
		while(QueuedPacket != nullptr)
		{
			if (Driver->IsNetResourceValid())
			{
				LowLevelSend(QueuedPacket->Data, FMath::DivideAndRoundUp(QueuedPacket->CountBits, 8u), QueuedPacket->CountBits);
			}
			delete QueuedPacket;
			QueuedPacket = Handler->GetQueuedPacket();
		}
	}

	// Update queued byte count.
	// this should be at the end so that the cap is applied *after* sending (and adjusting QueuedBytes for) any remaining data for this tick
	float DeltaBits = CurrentNetSpeed * DeltaTime * 8.f;
	QueuedBits -= FMath::TruncToInt(DeltaBits);
	float AllowedLag = 2.f * DeltaBits;
	if (QueuedBits < -AllowedLag)
	{
		QueuedBits = FMath::TruncToInt(-AllowedLag);
	}
}

void UNetConnection::HandleClientPlayer( APlayerController *PC, UNetConnection* NetConnection )
{
	check(Driver->GetWorld());

	// Hook up the Viewport to the new player actor.
	ULocalPlayer*	LocalPlayer = NULL;
	for(FLocalPlayerIterator It(GEngine, Driver->GetWorld());It;++It)
	{
		LocalPlayer = *It;
		break;
	}

	// Detach old player if it's in the same level.
	check(LocalPlayer);
	if( LocalPlayer->PlayerController && LocalPlayer->PlayerController->GetLevel() == PC->GetLevel())
	{
		if (LocalPlayer->PlayerController->Role == ROLE_Authority)
		{
			// local placeholder PC while waiting for connection to be established
			LocalPlayer->PlayerController->GetWorld()->DestroyActor(LocalPlayer->PlayerController);
		}
		else
		{
			// tell the server the swap is complete
			// we cannot use a replicated function here because the server has already transferred ownership and will reject it
			// so use a control channel message
			int32 Index = INDEX_NONE;
			FNetControlMessage<NMT_PCSwap>::Send(this, Index);
		}
		LocalPlayer->PlayerController->Player = NULL;
		LocalPlayer->PlayerController->NetConnection = NULL;
		LocalPlayer->PlayerController = NULL;
	}

	LocalPlayer->CurrentNetSpeed = CurrentNetSpeed;

	// Init the new playerpawn.
	PC->Role = ROLE_AutonomousProxy;
	PC->NetConnection = NetConnection;
	PC->SetPlayer(LocalPlayer);
	UE_LOG(LogNet, Verbose, TEXT("%s setplayer %s"),*PC->GetName(),*LocalPlayer->GetName());
	LastReceiveTime = Driver->Time;
	State = USOCK_Open;
	PlayerController = PC;
	OwningActor = PC;

	UWorld* World = PlayerController->GetWorld();
	// if we have already loaded some sublevels, tell the server about them
	for (int32 i = 0; i < World->StreamingLevels.Num(); ++i)
	{
		ULevelStreaming* LevelStreaming = World->StreamingLevels[i];
		if (LevelStreaming != NULL)
		{
			const ULevel* Level = LevelStreaming->GetLoadedLevel();
			if ( Level != NULL && Level->bIsVisible && !Level->bClientOnlyVisible )
			{
				PC->ServerUpdateLevelVisibility(PC->NetworkRemapPath(Level->GetOutermost()->GetFName(), false), true );
			}
		}
	}

	// if we have splitscreen viewports, ask the server to join them as well
	bool bSkippedFirst = false;
	for (FLocalPlayerIterator It(GEngine, Driver->GetWorld()); It; ++It)
	{
		if (*It != LocalPlayer)
		{
			// send server command for new child connection
			It->SendSplitJoin();
		}
	}
}

void UChildConnection::HandleClientPlayer(APlayerController* PC, UNetConnection* NetConnection)
{
	// find the first player that doesn't already have a connection
	ULocalPlayer* NewPlayer = NULL;
	uint8 CurrentIndex = 0;
	for (FLocalPlayerIterator It(GEngine, Driver->GetWorld()); It; ++It, CurrentIndex++)
	{
		if (CurrentIndex == PC->NetPlayerIndex)
		{
			NewPlayer = *It;
			break;
		}
	}

	if (NewPlayer == NULL)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		UE_LOG(LogNet, Error, TEXT("Failed to find LocalPlayer for received PlayerController '%s' with index %d. PlayerControllers:"), *PC->GetName(), int32(PC->NetPlayerIndex));
		check( PC->GetWorld() );
		for (TActorIterator<APlayerController> It(PC->GetWorld()); It; ++It)
		{
			if (It->Role < ROLE_Authority)
			{
				UE_LOG(LogNet, Log, TEXT(" - %s"), *It->GetFullName());
			}
		}
		UE_LOG(LogNet, Fatal,TEXT("Failed to find LocalPlayer for received PlayerController"));
#else
		return; // avoid crash
#endif
	}

	// Detach old player.
	check(NewPlayer);
	if (NewPlayer->PlayerController != NULL)
	{
		if (NewPlayer->PlayerController->Role == ROLE_Authority)
		{
			// local placeholder PC while waiting for connection to be established
			NewPlayer->PlayerController->GetWorld()->DestroyActor(NewPlayer->PlayerController);
		}
		else
		{
			// tell the server the swap is complete
			// we cannot use a replicated function here because the server has already transferred ownership and will reject it
			// so use a control channel message
			int32 Index = Parent->Children.Find(this);
			FNetControlMessage<NMT_PCSwap>::Send(Parent, Index);
		}
		NewPlayer->PlayerController->Player = NULL;
		NewPlayer->PlayerController->NetConnection = NULL;
		NewPlayer->PlayerController = NULL;
	}

	NewPlayer->CurrentNetSpeed = CurrentNetSpeed;

	// Init the new playerpawn.
	PC->Role = ROLE_AutonomousProxy;
	PC->NetConnection = NetConnection;
	PC->SetPlayer(NewPlayer);
	UE_LOG(LogNet, Verbose, TEXT("%s setplayer %s"), *PC->GetName(), *NewPlayer->GetName());
	PlayerController = PC;
	OwningActor = PC;
}

#if DO_ENABLE_NET_TEST

void UNetConnection::UpdatePacketSimulationSettings(void)
{
	check(Driver);
	PacketSimulationSettings.PktLoss = Driver->PacketSimulationSettings.PktLoss;
	PacketSimulationSettings.PktOrder = Driver->PacketSimulationSettings.PktOrder;
	PacketSimulationSettings.PktDup = Driver->PacketSimulationSettings.PktDup;
	PacketSimulationSettings.PktLag = Driver->PacketSimulationSettings.PktLag;
	PacketSimulationSettings.PktLagVariance = Driver->PacketSimulationSettings.PktLagVariance;
}
#endif

/**
 * Called to determine if a voice packet should be replicated to this
 * connection or any of its child connections
 *
 * @param Sender - the sender of the voice packet
 *
 * @return true if it should be sent on this connection, false otherwise
 */
bool UNetConnection::ShouldReplicateVoicePacketFrom(const FUniqueNetId& Sender)
{
	if (PlayerController &&
		// Has the handshaking of the mute list completed?
		PlayerController->MuteList.bHasVoiceHandshakeCompleted)
	{	
		// Check with the owning player controller first.
		if (Sender.IsValid() &&
			// Determine if the server should ignore replication of voice packets that are already handled by a peer connection
			//(!Driver->AllowPeerVoice || !Actor->HasPeerConnection(Sender)) &&
			// Determine if the sender was muted for the local player 
			PlayerController->IsPlayerMuted(Sender) == false)
		{
			// The parent wants to allow, but see if any child connections want to mute
			for (int32 Index = 0; Index < Children.Num(); Index++)
			{
				if (Children[Index]->ShouldReplicateVoicePacketFrom(Sender) == false)
				{
					// A child wants to mute, so skip
					return false;
				}
			}
			// No child wanted to block it so accept
			return true;
		}
	}
	// Not able to handle voice yet or player is muted on this connection
	return false;
}

void UNetConnection::ResetGameWorldState()
{
	//Clear out references and do whatever else so that nothing holds onto references that it doesn't need to.
	DestroyedStartupOrDormantActors.Empty();
	ClientVisibleLevelNames.Empty();
	KeepProcessingActorChannelBunchesMap.Empty();
	DormantReplicatorMap.Empty();
	CleanupDormantActorState();
}

void UNetConnection::CleanupDormantActorState()
{
	DormantReplicatorMap.Empty();
}

void UNetConnection::FlushDormancy(class AActor* Actor)
{
	UE_LOG( LogNetDormancy, Verbose, TEXT( "FlushDormancy: %s. Connection: %s" ), *Actor->GetName(), *GetName() );
	
	if ( Driver->GetNetworkObjectList().MarkActive( Actor, this, Driver->NetDriverName ) )
	{
		FlushDormancyForObject( Actor );

		for ( UActorComponent* ActorComp : Actor->GetReplicatedComponents() )
		{
			if ( ActorComp && ActorComp->GetIsReplicated() )
			{
				FlushDormancyForObject( ActorComp );
			}
		}
	}

	// If channel is pending dormancy, cancel it
			
	// If the close bunch was already sent, that is fine, by reseting the dormant flag
	// here, the server will not add the actor to the dormancy list when he closes the channel 
	// after he gets the client ack. The result is the channel will close but be open again
	// right away
	UActorChannel* Ch = ActorChannels.FindRef(Actor);

	if ( Ch != nullptr )
	{
		UE_LOG( LogNetDormancy, Verbose, TEXT( "    Found Channel[%d] '%s'. Reseting Dormancy. Ch->Closing: %d" ), Ch->ChIndex, *Ch->Describe(), Ch->Closing );

		Ch->Dormant = 0;
		Ch->bPendingDormancy = 0;
	}

}

void UNetConnection::ForcePropertyCompare( AActor* Actor )
{
	UActorChannel* Ch = ActorChannels.FindRef( Actor );

	if ( Ch != nullptr )
	{
		Ch->bForceCompareProperties = true;
	}
}

/** Wrapper for validating an objects dormancy state, and to prepare the object for replication again */
void UNetConnection::FlushDormancyForObject( UObject* Object )
{
	static const auto ValidateCVar = IConsoleManager::Get().FindTConsoleVariableDataInt( TEXT( "net.DormancyValidate" ) );
	const bool ValidateProperties = ( ValidateCVar && ValidateCVar->GetValueOnAnyThread() == 1 );

	TSharedRef< FObjectReplicator > * Replicator = DormantReplicatorMap.Find( Object );

	if ( Replicator != NULL )
	{
		if ( ValidateProperties )
		{
			Replicator->Get().ValidateAgainstState( Object );
		}

		DormantReplicatorMap.Remove( Object );

		// Set to NULL to force a new replicator to be created using the objects current state
		// It's totally possible to let this replicator fall through, and continue on where we left off 
		// which could send all the changed properties since this object went dormant
		Replicator = NULL;	
	}

	if ( Replicator == NULL )
	{
		Replicator = &DormantReplicatorMap.Add( Object, TSharedRef<FObjectReplicator>( new FObjectReplicator() ) );

		Replicator->Get().InitWithObject( Object, this, false );		// Init using the objects current state

		// Flush the must be mapped GUIDs, the initialization may add them, but they're phantom and will be remapped when actually sending
		UPackageMapClient * PackageMapClient = CastChecked< UPackageMapClient >(PackageMap);

		if (PackageMapClient)
		{
			TArray< FNetworkGUID >& MustBeMappedGuidsInLastBunch = PackageMapClient->GetMustBeMappedGuidsInLastBunch();
			MustBeMappedGuidsInLastBunch.Empty();
		}	
	}
}

/** Wrapper for setting the current client login state, so we can trap for debugging, and verbosity purposes. */
void UNetConnection::SetClientLoginState( const EClientLoginState::Type NewState )
{
	if ( ClientLoginState == NewState )
	{
		UE_LOG(LogNet, Verbose, TEXT("UNetConnection::SetClientLoginState: State same: %s"), EClientLoginState::ToString( NewState ) );
		return;
	}

	UE_LOG(LogNet, Verbose, TEXT("UNetConnection::SetClientLoginState: State changing from %s to %s"), EClientLoginState::ToString( ClientLoginState ), EClientLoginState::ToString( NewState ) );
	ClientLoginState = NewState;
}

/** Wrapper for setting the current expected client login msg type. */
void UNetConnection::SetExpectedClientLoginMsgType( const uint8 NewType )
{
	if ( ExpectedClientLoginMsgType == NewType )
	{
		UE_LOG(LogNet, Verbose, TEXT("UNetConnection::SetExpectedClientLoginMsgType: Type same: %i"), NewType );
		return;
	}

	UE_LOG(LogNet, Verbose, TEXT("UNetConnection::SetExpectedClientLoginMsgType: Type changing from %i to %i"), ExpectedClientLoginMsgType, NewType );
	ExpectedClientLoginMsgType = NewType;
}

/** This function validates that ClientMsgType is the next expected msg type. */
bool UNetConnection::IsClientMsgTypeValid( const uint8 ClientMsgType )
{
	if ( ClientLoginState == EClientLoginState::LoggingIn )
	{
		// If client is logging in, we are expecting a certain msg type each step of the way
		if ( ClientMsgType != ExpectedClientLoginMsgType )
		{
			// Not the expected msg type
			UE_LOG(LogNet, Log, TEXT("UNetConnection::IsClientMsgTypeValid FAILED: (ClientMsgType != ExpectedClientLoginMsgType) Remote Address=%s"), *LowLevelGetRemoteAddress());
			return false;
		}
	} 
	else
	{
		// Once a client is logged in, we no longer expect any of the msg types below
		if ( ClientMsgType == NMT_Hello || ClientMsgType == NMT_Login )
		{
			// We don't want to see these msg types once the client is fully logged in
			UE_LOG(LogNet, Log, TEXT("UNetConnection::IsClientMsgTypeValid FAILED: Invalid msg after being logged in - Remote Address=%s"), *LowLevelGetRemoteAddress());
			return false;
		}
	}

	return true;
}

/**
* This function tracks the number of log calls per second for this client, 
* and disconnects the client if it detects too many calls are made per second
*/
bool UNetConnection::TrackLogsPerSecond()
{
	const double NewTime = FPlatformTime::Seconds();

	const double LogCallTotalTime = NewTime - LogCallLastTime;

	LogCallCount++;

	static const double LOG_AVG_THRESHOLD				= 0.5;		// Frequency to check threshold
	static const double	MAX_LOGS_PER_SECOND_INSTANT		= 60;		// If they hit this limit, they will instantly get disconnected
	static const double	MAX_LOGS_PER_SECOND_SUSTAINED	= 5;		// If they sustain this logs/second for a certain count, they get disconnected
	static const double	MAX_SUSTAINED_COUNT				= 10;		// If they sustain MAX_LOGS_PER_SECOND_SUSTAINED for this count, they get disconnected (5 seconds currently)

	if ( LogCallTotalTime > LOG_AVG_THRESHOLD )
	{
		const double LogsPerSecond = (double)LogCallCount / LogCallTotalTime;

		LogCallLastTime = NewTime;
		LogCallCount	= 0;

		if ( LogsPerSecond > MAX_LOGS_PER_SECOND_INSTANT )
		{
			// Hit this instant limit, we instantly disconnect them
			UE_LOG( LogNet, Warning, TEXT( "UNetConnection::TrackLogsPerSecond instant FAILED. LogsPerSecond: %f, RemoteAddr: %s" ), (float)LogsPerSecond, *LowLevelGetRemoteAddress() );
			Close();		// Close the connection

			PerfCountersIncrement(TEXT("ClosedConnectionsDueToMaxBadRPCsLimit"));
			return false;
		}

		if ( LogsPerSecond > MAX_LOGS_PER_SECOND_SUSTAINED )
		{
			// Hit the sustained limit, count how many times we get here
			LogSustainedCount++;

			// Warn that we are approaching getting disconnected (will be useful when going over historical logs)
			UE_LOG( LogNet, Warning, TEXT( "UNetConnection::TrackLogsPerSecond: LogsPerSecond > MAX_LOGS_PER_SECOND_SUSTAINED. LogSustainedCount: %i, LogsPerSecond: %f, RemoteAddr: %s" ), LogSustainedCount, (float)LogsPerSecond, *LowLevelGetRemoteAddress() );

			if ( LogSustainedCount > MAX_SUSTAINED_COUNT )
			{
				// Hit the sustained limit for too long, disconnect them
				UE_LOG( LogNet, Warning, TEXT( "UNetConnection::TrackLogsPerSecond: LogSustainedCount > MAX_SUSTAINED_COUNT. LogsPerSecond: %f, RemoteAddr: %s" ), (float)LogsPerSecond, *LowLevelGetRemoteAddress() );
				Close();		// Close the connection

				PerfCountersIncrement(TEXT("ClosedConnectionsDueToMaxBadRPCsLimit"));
				return false;
			}
		}
		else
		{
			// Reset sustained count since they are not above the threshold
			LogSustainedCount = 0;
		}
	}

	return true;
}

void UNetConnection::ResetPacketBitCounts()
{
	NumPacketIdBits = 0;
	NumBunchBits = 0;
	NumAckBits = 0;
	NumPaddingBits = 0;
}

void UNetConnection::SetPlayerOnlinePlatformName(const FName InPlayerOnlinePlatformName)
{
	PlayerOnlinePlatformName = InPlayerOnlinePlatformName;
}
