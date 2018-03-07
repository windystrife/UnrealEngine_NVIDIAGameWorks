// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Channel.h"
#include "NetcodeUnitTest.h"
#include "Engine/NetworkDelegates.h"
#include "Engine/World.h"
#include "Sockets.h"

class APlayerController;
class FInBunch;
class FInternetAddr;
class FOutBunch;
class UMinimalClient;
class UNetConnection;
class UNetDriver;
class UUnitTestChannel;
class UUnitTestNetDriver;
struct FUniqueNetIdRepl;
class UClientUnitTest;



// Delegates

/**
 * Delegate used to control/block receiving of RPC's
 *
 * @param Actor			The actor the RPC will be called on
 * @param Function		The RPC to call
 * @param Parameters	The parameters data blob
 * @param bBlockRPC		Whether or not to block execution of the RPC
*/
DECLARE_DELEGATE_FourParams(FOnProcessNetEvent, AActor* /*Actor*/, UFunction* /*Function*/, void* /*Parameters*/, bool& /*bBlockRPC*/);


/**
 * Class for encapsulating ProcessEvent and general RPC hooks, implemented globally for each UWorld
 */
class FProcessEventHook
{
public:
	/**
	 * Get a reference to the net event hook singular
	 */
	static FProcessEventHook& Get()
	{
		static FProcessEventHook* HookSingular = nullptr;

		if (HookSingular == nullptr)
		{
			HookSingular = new FProcessEventHook();
		}

		return *HookSingular;
	}

public:
	/**
	 * Default constructor
	 */
	FProcessEventHook()
		: NetEventHooks()
	{
	}

	/**
	 * Adds an RPC hook for the specified UWorld.
	 *
	 * @param InWorld	The UWorld whose RPC's should be hooked
	 * @param InHook	The delegate for handling the hooked RPC's
	 */
	void AddRPCHook(UWorld* InWorld, FOnProcessNetEvent InHook);

	/**
	 * Removes the RPC hook for the specified UWorld.
	 *
	 * @param InWorld	The UWorld whose RPC hook should be cleared.
	 */
	void RemoveRPCHook(UWorld* InWorld);

	/**
	 * Adds a non-RPC hook for the specified UWorld.
	 *
	 * @param InWorld	The UWorld whose RPC's should be hooked
	 * @param InHook	The delegate for handling the hooked RPC's
	 */
	void AddEventHook(UWorld* InWorld, FOnProcessNetEvent InHook);

	/**
	 * Removes the hook for the specified UWorld.
	 *
	 * @param InWorld	The UWorld whose RPC hook should be cleared.
	 */
	void RemoveEventHook(UWorld* InWorld);


private:
	/**
	 * Base hook for AActor::ProcessEventDelegate - responsible for filtering based on RPC events and Actor UWorld
	 */
	bool HandleProcessEvent(AActor* Actor, UFunction* Function, void* Parameters);

private:
	/** The global list of RPC hooks, and the UWorld they are associated with */
	TMap<UWorld*, FOnProcessNetEvent> NetEventHooks;

	/** The global list of ProcessEvent hooks, and the UWorld they are associated with */
	TMap<UWorld*, FOnProcessNetEvent> EventHooks;
};


/**
 * A delegate network notify class, to allow for easy inline-hooking.
 * 
 * NOTE: This will memory leak upon level change and re-hooking (if used as a hook),
 * because there is no consistent way to handle deleting it.
 */
class FNetworkNotifyHook : public FNetworkNotify
{
public:
	/**
	 * Base constructor
	 */
	FNetworkNotifyHook()
		: FNetworkNotify()
		, NotifyAcceptingConnectionDelegate()
		, NotifyAcceptedConnectionDelegate()
		, NotifyAcceptingChannelDelegate()
		, NotifyControlMessageDelegate()
		, HookedNotify(NULL)
	{
	}

	/**
	 * Constructor which hooks an existing network notify
	 */
	FNetworkNotifyHook(FNetworkNotify* InHookNotify)
		: FNetworkNotify()
		, HookedNotify(InHookNotify)
	{
	}

	/**
	 * Virtual destructor
	 */
	virtual ~FNetworkNotifyHook()
	{
	}


protected:
	/**
	 * Old/original notifications
	 */

	virtual EAcceptConnection::Type NotifyAcceptingConnection() override;

	virtual void NotifyAcceptedConnection(UNetConnection* Connection) override;

	virtual bool NotifyAcceptingChannel(UChannel* Channel) override;

	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch) override;


	DECLARE_DELEGATE_RetVal(EAcceptConnection::Type, FNotifyAcceptingConnectionDelegate);
	DECLARE_DELEGATE_OneParam(FNotifyAcceptedConnectionDelegate, UNetConnection* /*Connection*/);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FNotifyAcceptingChannelDelegate, UChannel* /*Channel*/);
	DECLARE_DELEGATE_RetVal_ThreeParams(bool, FNotifyControlMessageDelegate, UNetConnection* /*Connection*/, uint8 /*MessageType*/,
										FInBunch& /*Bunch*/);

public:
	FNotifyAcceptingConnectionDelegate	NotifyAcceptingConnectionDelegate;
	FNotifyAcceptedConnectionDelegate	NotifyAcceptedConnectionDelegate;
	FNotifyAcceptingChannelDelegate		NotifyAcceptingChannelDelegate;
	FNotifyControlMessageDelegate		NotifyControlMessageDelegate;

	/** If this is hooking an existing network notify, save a reference */
	FNetworkNotify* HookedNotify;
};


/**
 * Class for hooking world tick events, and setting globals for log hooking
 */
class FWorldTickHook
{
public:
	FWorldTickHook(UWorld* InWorld)
		: AttachedWorld(InWorld)
	{
	}

	void Init()
	{
		if (AttachedWorld != NULL)
		{
#if TARGET_UE4_CL >= CL_DEPRECATEDEL
			TickDispatchDelegateHandle  = AttachedWorld->OnTickDispatch().AddRaw(this, &FWorldTickHook::TickDispatch);
			PostTickFlushDelegateHandle = AttachedWorld->OnPostTickFlush().AddRaw(this, &FWorldTickHook::PostTickFlush);
#else
			AttachedWorld->OnTickDispatch().AddRaw(this, &FWorldTickHook::TickDispatch);
			AttachedWorld->OnPostTickFlush().AddRaw(this, &FWorldTickHook::PostTickFlush);
#endif
		}
	}

	void Cleanup()
	{
		if (AttachedWorld != NULL)
		{
#if TARGET_UE4_CL >= CL_DEPRECATEDEL
			AttachedWorld->OnPostTickFlush().Remove(PostTickFlushDelegateHandle);
			AttachedWorld->OnTickDispatch().Remove(TickDispatchDelegateHandle);
#else
			AttachedWorld->OnPostTickFlush().RemoveRaw(this, &FWorldTickHook::PostTickFlush);
			AttachedWorld->OnTickDispatch().RemoveRaw(this, &FWorldTickHook::TickDispatch);
#endif
		}

		AttachedWorld = NULL;
	}

private:
	void TickDispatch(float DeltaTime)
	{
		GActiveLogWorld = AttachedWorld;
	}

	void PostTickFlush()
	{
		GActiveLogWorld = NULL;
	}

public:
	/** The world this is attached to */
	UWorld* AttachedWorld;

#if TARGET_UE4_CL >= CL_DEPRECATEDEL
private:
	/** Handle for Tick dispatch delegate */
	FDelegateHandle TickDispatchDelegateHandle;

	/** Handle for PostTick dispatch delegate */
	FDelegateHandle PostTickFlushDelegateHandle;
#endif
};


/**
 * Hooks netcode object serialization, in order to replace replication of a specific object, with another specified object,
 * for the lifetime of the scoped instance
 */
class NETCODEUNITTEST_API FScopedNetObjectReplace
{
public:
	FScopedNetObjectReplace(UClientUnitTest* InUnitTest, UObject* InObjToReplace, UObject* InObjReplacement);

	~FScopedNetObjectReplace();


private:
	UClientUnitTest* UnitTest;

	UObject* ObjToReplace;
};


/**
 * Netcode based utility functions
 */
struct NUTNet
{
	/**
	 * Handles setting up the client beacon once it is replicated, so that it can properly send RPC's
	 * (normally the serverside client beacon links up with the pre-existing beacon on the clientside,
	 * but with unit tests there is no pre-existing clientside beacon)
	 *
	 * @param InBeacon		The beacon that should be setup
	 * @param InConnection	The connection associated with the beacon
	 */
	static void HandleBeaconReplicate(AActor* InBeacon, UNetConnection* InConnection);


	/**
	 * Creates a barebones/minimal UWorld, for setting up minimal fake player connections,
	 * and as a container for objects in the unit test commandlet
	 *
	 * @return	Returns the created UWorld object
	 */
	static NETCODEUNITTEST_API UWorld* CreateUnitTestWorld(bool bHookTick=true);

	/**
	 * Marks the specified unit test world for cleanup
	 *
	 * @param CleanupWorld	The unit test world to be marked for cleanup
	 * @param bImmediate	If true, all unit test worlds pending cleanup, are immediately cleaned up
	 */
	static void MarkUnitTestWorldForCleanup(UWorld* CleanupWorld, bool bImmediate=false);

	/**
	 * Cleans up unit test worlds queued for cleanup
	 */
	static void CleanupUnitTestWorlds();

	/**
	 * Returns true, if the specified world is a unit test world
	 */
	static bool IsUnitTestWorld(UWorld* InWorld);
};

