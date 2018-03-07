// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Modules/ModuleInterface.h"
#include "Containers/Queue.h"

PACKETHANDLER_API DECLARE_LOG_CATEGORY_EXTERN(PacketHandlerLog, Log, All);


class HandlerComponent;
class FEncryptionComponent;
class ReliabilityHandlerComponent;


/**
 * Delegates
 */

// Delegate for allowing access to LowLevelSend, without a dependency upon Engine
DECLARE_DELEGATE_ThreeParams(FPacketHandlerLowLevelSend, void* /* Data */, int32 /* CountBytes */, int32 /* CountBits */);

/**
 * Callback for notifying higher-level code that handshaking has completed, and that packets are now ready to send without buffering
 */
DECLARE_DELEGATE(FPacketHandlerHandshakeComplete);


/**
 * Enums related to the PacketHandler
 */

namespace Handler
{
	/**
	 * State of PacketHandler
	 */
	enum State
	{
		Uninitialized,			// PacketHandler is uninitialized
		InitializingComponents,	// PacketHandler is initializing HandlerComponents
		Initialized				// PacketHandler and all HandlerComponents (if any) are initialized
	};

	/**
	 * Mode of Packet Handler
	 */
	enum Mode
	{
		Client,					// Clientside PacketHandler
		Server					// Serverside PacketHandler
	};

	namespace Component
	{
		/**
		 * HandlerComponent State
		 */
		enum State
		{
			UnInitialized,		// HandlerComponent not yet initialized
			InitializedOnLocal, // Initialized on local instance
			InitializeOnRemote, // Initialized on remote instance, not on local instance
			Initialized         // Initialized on both local and remote instances
		};
	}
}

/**
 * The result of calling Incoming and Outgoing in the PacketHandler
 */
struct PACKETHANDLER_API ProcessedPacket
{
	/** Pointer to the returned packet data */
	uint8* Data;

	/** Size of the returned packet data in bits */
	int32 CountBits;

	/** Whether or not there was an error processing the packet */
	bool bError;

public:
	ProcessedPacket()
		: Data()
		, CountBits(0)
		, bError(false)
	{
	}

	/**
	 * Base constructor
	 */
	ProcessedPacket(uint8* InData, int32 InCountBits, bool bInError=false)
		: Data(InData)
		, CountBits(InCountBits)
		, bError(bInError)
	{
	}
};

/**
 * PacketHandler will buffer packets, this struct is used to buffer such packets while handler components are initialized
 */
struct PACKETHANDLER_API BufferedPacket
{
	/** Buffered packet data */
	uint8* Data;

	/** Size of buffered packet in bits */
	uint32 CountBits;

	/** Used by ReliabilityHandlerComponent, to mark a packet for resending */
	float ResendTime;

	/** Used by ReliabilityHandlerComponent, to track packet id's */
	uint32 Id;

	/** For connectionless packets, the address to send to (format is abstract, determined by active net driver) */
	FString Address;

	/** If buffering a packet through 'SendHandlerPacket', track the originating component */
	HandlerComponent* FromComponent;


public:
	/**
	 * Base constructor
	 */
	BufferedPacket()
		: Data(nullptr)
		, CountBits(0)
		, ResendTime(0.f)
		, Id(0)
		, Address()
		, FromComponent(nullptr)
	{
	}

	BufferedPacket(uint8* InCopyData, uint32 InCountBits, float InResendTime=0.f, uint32 InId=0)
		: CountBits(InCountBits)
		, ResendTime(InResendTime)
		, Id(InId)
		, Address()
		, FromComponent(nullptr)
	{
		check(InCopyData != nullptr);

		Data = new uint8[FMath::DivideAndRoundUp(InCountBits, 8u)];
		FMemory::Memcpy(Data, InCopyData, FMath::DivideAndRoundUp(InCountBits, 8u));
	}

	BufferedPacket(FString InAddress, uint8* InCopyData, uint32 InCountBits, float InResendTime=0.f, uint32 InId=0)
		: BufferedPacket(InCopyData, InCountBits, InResendTime, InId)
	{
		Address = InAddress;
	}

	/**
	 * Base destructor
	 */
	~BufferedPacket()
	{
		if (Data != nullptr)
		{
			delete Data;
		}
	}
};

/**
 * This class maintains an array of all PacketHandler Components and forwards incoming and outgoing packets the each component
 */
class PACKETHANDLER_API PacketHandler
{
public:
	/**
	 * Base constructor
	 */
	PacketHandler();

	/**
	 * Base Destructor
	 */
	virtual ~PacketHandler()
	{
	};

	/**
	 * Handles initialization of manager
	 *
	 * @param Mode					The mode the manager should be initialized in
	 * @param InMaxPacketBits		The maximum supported packet size
	 * @param bConnectionlessOnly	Whether or not this is a connectionless-only manager (ignores .ini components)
	 */
	void Initialize(Handler::Mode Mode, uint32 InMaxPacketBits, bool bConnectionlessOnly=false);

	/**
	 * Used for external initialization of delegates
	 *
	 * @param InLowLevelSendDel		The delegate the PacketHandler should use for triggering packet sends
	 */
	void InitializeDelegates(FPacketHandlerLowLevelSend InLowLevelSendDel)
	{
		LowLevelSendDel = InLowLevelSendDel;
	}

	/**
	 * Triggers initialization of HandlerComponents.
	 */
	void InitializeComponents();


	/**
	 * Triggered by the higher level netcode, to begin any required HandlerComponent handshakes
	 */
	void BeginHandshaking(FPacketHandlerHandshakeComplete InHandshakeDel=FPacketHandlerHandshakeComplete());


	void Tick(float DeltaTime);

	/**
	 * Adds a HandlerComponent to the pipeline, prior to initialization (none can be added after initialization)
	 *
	 * @param NewHandler		The HandlerComponent to add
	 * @param bDeferInitialize	Whether or not to defer triggering Initialize (for batch-adds - code calling this, triggers it instead)
	 */
	void AddHandler(TSharedPtr<HandlerComponent> NewHandler, bool bDeferInitialize=false);

	/**
	 * As above, but initializes from a string specifying the component module, and (optionally) additional options
	 *
	 * @param ComponentStr		The handler component to load
	 * @param bDeferInitialize	Whether or not to defer triggering Initialize (for batch-adds - code calling this, triggers it instead)
	 */
	TSharedPtr<HandlerComponent> AddHandler(FString ComponentStr, bool bDeferInitialize=false);


	// @todo #JohnB: Add runtime-calculated arrays for each packet pipeline type, to reduce redundant iterations,
	//				(there are 3x iterations now, 1 for each packet pipeline type), and to ignore inactive HandlerComponent's

	// @todo #JohnB: The reserved packet bits needs to be handled differently for the 'High' functions, as they use SendBuffer,
	//					which normally is reduced in size by reserved packet bits.


	/**
	 * @todo #JohnB: Work in progress, don't use yet.
	 *
	 * Processes incoming packets at the UNetConnection level, after uncapping the packet into an FBitReader.
	 *
	 * Use this for simple data additions to packets, and for maximum compatibility with other HandlerComponent's.
	 *
	 * @param Reader	The FBitReader for the incoming packet
	 */
	void IncomingHigh(FBitReader& Reader);

	/**
	 * @todo #JohnB: Work in progress, don't use yet.
	 *
	 * Processes outgoing packets at the UNetConnection level, after game data is written, and just before the packet is capped.
	 *
	 * Use this for simple data additions to packets, and for maximum compatibility with other HandlerComponent's.
	 *
	 * @param Writer	The FBitWriter for the outgoing packet
	 */
	void OutgoingHigh(FBitWriter& Writer);

	/**
	 * Processes incoming packets at the PacketHandler level, before any UNetConnection processing takes place on the packet.
	 *
	 * Use this for more complex changes to packets, such as compression/encryption,
	 * but be aware that compatibility problems with other HandlerComponent's are more likely.
	 *
	 * @param Packet		The packet data to be processed
	 * @param CountBytes	The size of the packet data in bytes
	 * @return				Returns the final packet
	 */
	FORCEINLINE const ProcessedPacket Incoming(uint8* Packet, int32 CountBytes)
	{
		return Incoming_Internal(Packet, CountBytes);
	}

	/**
	 * Processes outgoing packets at the PacketHandler level, after all UNetConnection processing.
	 *
	 * Use this for more complex changes to packets, such as compression/encryption,
	 * but be aware that compatibility problems with other HandlerComponent's are more likely.
	 *
	 * @param Packet		The packet data to be processed
	 * @param CountBits		The size of the packet data in bits
	 * @return				Returns the final packet
	 */
	FORCEINLINE const ProcessedPacket Outgoing(uint8* Packet, int32 CountBits)
	{
		return Outgoing_Internal(Packet, CountBits);
	}

	/**
	 * Processes incoming packets without a UNetConnection, in the same manner as 'Incoming' above
	 * IMPORTANT: Net drivers triggering this, should call 'UNetDriver::FlushHandler' shortly afterwards, to minimize packet buffering
	 * NOTE: Connectionless packets are unreliable.
	 *
	 * @param Address		The address the packet was received from (format is abstract, determined by active net driver)
	 * @param Packet		The packet data to be processed
	 * @param CountBytes	The size of the packet data in bytes
	 * @return				Returns the final packet
	 */
	FORCEINLINE const ProcessedPacket IncomingConnectionless(FString Address, uint8* Packet, int32 CountBytes)
	{
		return Incoming_Internal(Packet, CountBytes, true, Address);
	}

	/**
	 * Processes outgoing packets without a UNetConnection, in the same manner as 'Outgoing' above
	 * NOTE: Connectionless packets are unreliable.
	 *
	 * @param Address		The address the packet is being sent to (format is abstract, determined by active net driver)
	 * @param Packet		The packet data to be processed
	 * @param CountBits		The size of the packet data in bits
	 * @return				Returns the final packet
	 */
	FORCEINLINE const ProcessedPacket OutgoingConnectionless(FString Address, uint8* Packet, int32 CountBits)
	{
		return Outgoing_Internal(Packet, CountBits, true, Address);
	}

	/** Returns a pointer to the component set as the encryption handler, if any. */
	TSharedPtr<FEncryptionComponent> GetEncryptionComponent();

protected:
	/**
	 * Internal handling for Incoming/IncomingConnectionless
	 *
	 * @param Packet			The packet data to be processed
	 * @param CountBytes		The size of the packet data in bytes
	 * @param bConnectionless	Whether or not this should be processed as a connectionless packet
	 * @param Address			The address the packet was received from (format is abstract, determined by active net driver)
	 * @return					Returns the final packet
	 */
	const ProcessedPacket Incoming_Internal(uint8* Packet, int32 CountBytes, bool bConnectionless=false,
													FString Address=TEXT(""));

	/**
	 * Internal handling for Outgoing/OutgoingConnectionless
	 *
	 * @param Packet			The packet data to be processed
	 * @param CountBits			The size of the packet data in bits
	 * @param bConnectionless	Whether or not this should be sent as a connectionless packet
	 * @param Address			The address the packet is being sent to (format is abstract, determined by active net driver)
	 * @return					Returns the final packet
	 */
	const ProcessedPacket Outgoing_Internal(uint8* Packet, int32 CountBits, bool bConnectionless=false,
													FString Address=TEXT(""));

public:
	/**
	 * Send a packet originating from a HandlerComponent - will process through the HandlerComponents chain,
	 * starting after the triggering component.
	 * NOTE: Requires that InitializeDelegates is called, with a valid LowLevelSend delegate.
	 *
	 * @param Component		The HandlerComponent sending the packet
	 * @param Writer		The packet being sent
	 */
	void SendHandlerPacket(HandlerComponent* InComponent, FBitWriter& Writer);


	/**
	 * Triggered when a child HandlerComponent has been initialized
	 */
	void HandlerComponentInitialized(HandlerComponent* InComponent);

	/**
	 * Queue's a packet to be sent when the handler is ticked (as a raw packet, since it's already been processed)
	 *
	 * @param PacketToQueue		The packet to be queued
	 */
	FORCEINLINE void QueuePacketForRawSending(BufferedPacket* PacketToQueue)
	{
		QueuedRawPackets.Enqueue(PacketToQueue);
	}

	/**
	 * Queue's a packet to be sent through 'SendHandlerPacket'
	 *
	 * @param PacketToQueue		The packet to be queued
	 */
	FORCEINLINE void QueueHandlerPacketForSending(BufferedPacket* PacketToQueue)
	{
		QueuedHandlerPackets.Enqueue(PacketToQueue);
	}

	/**
	 * Gets a packet from the buffered packet queue for sending
	 *
	 * @return		The packet to be sent, or nullptr if none are to be sent
	 */
	BufferedPacket* GetQueuedPacket();

	/**
	* Gets a packet from the buffered packet queue for sending (as a raw packet)
	*
	* @return		The packet to be sent, or nullptr if none are to be sent
	*/
	BufferedPacket* GetQueuedRawPacket();

	/**
	 * Gets a packet from the buffered connectionless packet queue for sending
	 *
	 * @return		The packet to be sent, or nullptr if none are to be sent
	 */
	BufferedPacket* GetQueuedConnectionlessPacket();

	/**
	 * Gets the combined reserved packet/protocol bits from all handlers, for reserving space in the parent connections packets
	 *
	 * @return	The combined reserved packet/protocol bits
	 */
	int32 GetTotalReservedPacketBits();


	/**
	 * Sets whether or not outgoing packets should bypass this handler - used when raw packet sends are necessary
	 * (such as for the stateless handshake)
	 *
	 * @param bInEnabled	Whether or not raw sends are enabled
	 */
	FORCEINLINE void SetRawSend(bool bInEnabled)
	{
		bRawSend = bInEnabled;
	}

	/**
	 * Whether or not raw packet sends are enabled
	 */
	FORCEINLINE bool GetRawSend()
	{
		return bRawSend;
	}

	/**
	 * Whether or not the packet handler is fully initialized, post-handshake etc.
	 */
	FORCEINLINE bool IsFullyInitialized()
	{
		return State == Handler::State::Initialized;
	}


private:
	/**
	 * Set state of handler
	 *
	 * @param InState	The new state for the handler
	 */
	void SetState(Handler::State InState);

	/**
	 * Called when net send/receive functions are triggered, when the handler is still uninitialized - to set a valid initial state
	 */
	void UpdateInitialState();

	/**
	 * Called when handler is finished initializing
	 */
	void HandlerInitialized();

	/**
	 * Replaces IncomingPacket with all unread data from ReplacementPacket
	 *
	 * @param ReplacementPacket		The packet whose unread data should replace IncomingPacket
	 */
	void ReplaceIncomingPacket(FBitReader& ReplacementPacket);

	/**
	 * Takes a Packet whose position is not at bit 0, and shifts/aligns all packet data to place the current bit at position 0
	 *
	 * @param Packet	The packet to realign
	 */
	void RealignPacket(FBitReader& Packet);


public:
	/** Mode of the handler, Client or Server */
	Handler::Mode Mode;

	/** Time, updated by Tick */
	float Time;

	/** Whether or not this PacketHandler handles connectionless (i.e. non-UNetConnection) data */
	bool bConnectionlessHandler;

private:
	/** Delegate used for triggering PacketHandler/HandlerComponent-sourced sends */
	FPacketHandlerLowLevelSend LowLevelSendDel;

	/** Delegate used for notifying that handshaking has completed */
	FPacketHandlerHandshakeComplete HandshakeCompleteDel;


	/** Used for packing outgoing packets */
	FBitWriter OutgoingPacket;

	/** Used for unpacking incoming packets */
	FBitReader IncomingPacket;


	/** The HandlerComponent pipeline, for processing incoming/outgoing packets */
	TArray<TSharedPtr<HandlerComponent>> HandlerComponents;

	/** A direct pointer to the component configured as the encryption component. Will also be present in the HandlerComponents array. */
	TSharedPtr<FEncryptionComponent> EncryptionComponent;

	/** The maximum supported packet size (reflects UNetConnection::MaxPacket) */
	uint32 MaxPacketBits;


	/** State of the handler */
	Handler::State State;
	

	/** Packets that are buffered while HandlerComponents are being initialized */
	TArray<BufferedPacket*> BufferedPackets;

	/** Packets that are queued to be sent when handler is ticked */
	TQueue<BufferedPacket*> QueuedPackets;

	/** Packets that are queued to be sent when handler is ticked (as a raw packet) */
	TQueue<BufferedPacket*> QueuedRawPackets;

	/** Packets that are queued to be sent through 'SendHandlerPacket' */
	TQueue<BufferedPacket*> QueuedHandlerPackets;

	/** Packets that are buffered while HandlerComponents are being initialized */
	TArray<BufferedPacket*> BufferedConnectionlessPackets;

	/** Packets that are queued to be sent when handler is ticked */
	TQueue<BufferedPacket*> QueuedConnectionlessPackets;

	/** Reliability Handler Component */
	TSharedPtr<ReliabilityHandlerComponent> ReliabilityComponent;

	/** Whether or not outgoing packets bypass the handler */
	bool bRawSend;

	/** Whether or not component handshaking has begun */
	bool bBeganHandshaking;
};

/**
 * This class appends or modifies incoming and outgoing packets on a connection
 */
class PACKETHANDLER_API HandlerComponent
{
	friend class PacketHandler;

public:
	/**
	 * Base constructor
	 */
	HandlerComponent();

	/**
	 * Base destructor
	 */
	virtual ~HandlerComponent()
	{
	}

	/**
	 * Returns whether this handler is currently active
	 */
	virtual bool IsActive() const;

	/**
	 * Return whether this handler is valid
	 */
	virtual bool IsValid() const PURE_VIRTUAL(Handler::Component::IsValid, return false;);

	/**
	 * Returns whether this handler is initialized
	 */
	bool IsInitialized() const;

	/**
	 * Returns whether this handler requires a tick every frame
	 */
	bool DoesTick() const;


	/**
	 * Handles incoming packets
	 *
	 * @param Packet	The packet to be handled
	 */
	virtual void Incoming(FBitReader& Packet) PURE_VIRTUAL(HandlerComponent::Incoming,);

	/**
	 * Handles any outgoing packets
	 *
	 * @param Packet	The packet to be handled
	 */
	virtual void Outgoing(FBitWriter& Packet) PURE_VIRTUAL(HandlerComponent::Outgoing,);

	/**
	 * Handles incoming packets not associated with a UNetConnection
	 *
	 * @param Address	The address the packet was received from (format is abstract, determined by active net driver)
	 * @param Packet	The packet to be handled
	 */
	virtual void IncomingConnectionless(FString Address, FBitReader& Packet) PURE_VIRTUAL(HandlerComponent::IncomingConnectionless,);

	/**
	 * Handles any outgoing packets not associated with a UNetConnection
	 *
	 * @param Address	The address the packet is being sent to (format is abstract, determined by active net driver)
	 * @param Packet	The packet to be handled
	 */
	virtual void OutgoingConnectionless(FString Address, FBitWriter& Packet) PURE_VIRTUAL(HandlerComponent::OutgoingConnectionless,);


	/**
	 * Whether or not the Incoming/IncomingConnectionless implementations, support reading Packets that aren't aligned at bit position 0
	 * (i.e. whether or not this handler supports bit-level, rather than byte-level, reads)
	 *
	 * @return	Whether or not the above is supported
	 */
	virtual bool CanReadUnaligned()
	{
		return false;
	}


	/**
	 * Initialization functionality should be placed here
	 */
	virtual void Initialize() PURE_VIRTUAL(HandlerComponent::Initialize,);

	/**
	 * Notification to this component that it is ready to begin handshaking
	 */
	virtual void NotifyHandshakeBegin()
	{
	}

	/**
	 * Tick functionality should be placed here
	 */
	virtual void Tick(float DeltaTime)
	{
	}

	/**
	 * Sets whether this handler is currently active
	 *
	 * @param Active	Whether or not the handled should be active
	 */
	virtual void SetActive(bool Active);

	/**
	 * Returns the amount of reserved packet/protocol bits expected from this component.
	 *
	 * IMPORTANT: This MUST be accurate, and should represent the worst-case number of reserved bits expected from the component.
	 *				If this is inaccurate, packets will randomly fail to send, in rare cases which are extremely hard to trace.
	 *
	 * @return	The worst-case reserved packet bits for the component
	 */
	virtual int32 GetReservedPacketBits() PURE_VIRTUAL(Handler::Component::GetReservedPacketBits, return -1;);

protected:
	/**
	 * Sets the state of the handler
	 *
	 * @param State		The new state for the handler
	 */
	void SetState(Handler::Component::State State);

	/**
	 * Should be called when the handler is fully initialized on both remote and local
	 */
	void Initialized();


public:
	/** The manager of the handler, set in initialization */
	PacketHandler* Handler; 

protected:
	/** The state of this handler */
	Handler::Component::State State;

	/** Maximum number of Outgoing packet bits supported (automatically calculated to factor in other HandlerComponent reserved bits) */
	uint32 MaxOutgoingBits;

	/** Whether this handler has to perform a network handshake during initialization (requires waiting on other HandlerComponent's) */
	bool bRequiresHandshake;

	/** Whether this handler depends upon the ReliabilityHandlerComponent being enabled */
	bool bRequiresReliability;

private:
	/** Whether this handler is active, which dictates whether it will receive incoming and outgoing packets. */
	bool bActive;

	/** Whether this handler is fully initialized on both remote and local */
	bool bInitialized;
};

/**
 * PacketHandler Module Interface
 */
class PACKETHANDLER_API FPacketHandlerComponentModuleInterface : public IModuleInterface
{
public:
	/* Creates an instance of this component */
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options)
		PURE_VIRTUAL(FPacketHandlerModuleInterface::CreateComponentInstance, return TSharedPtr<HandlerComponent>(NULL););

	virtual void StartupModule() override;

	virtual void ShutdownModule() override;
};
