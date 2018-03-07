// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "XmppConnection.h"
#include "Modules/ModuleInterface.h"
#include "XmppMultiUserChat.h"

class Error;

/**
 * Module for Xmpp connections
 * Use CreateConnection to create a new Xmpp connection
 */
class XMPP_API FXmppModule :
	public IModuleInterface, public FSelfRegisteringExec
{

public:

	// FSelfRegisteringExec

	/**
	 * Handle exec commands starting with "XMPP"
	 *
	 * @param InWorld	the world context
	 * @param Cmd		the exec command being executed
	 * @param Ar		the archive to log results to
	 *
	 * @return true if the handler consumed the input, false to continue searching handlers
	 */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/** 
	 * Exec command handlers
	 */
	bool HandleXmppCommand( const TCHAR* Cmd, FOutputDevice& Ar );

	// FXmppModule

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static FXmppModule& Get();

	/**
	 * Checks to see if this module is loaded and ready.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static bool IsAvailable();

	/**
	 * Creates a new Xmpp connection for the current platform and associated it with the user
	 *
	 * @return new Xmpp connection instance
	 */
	TSharedRef<class IXmppConnection> CreateConnection(const FString& UserId);

	/**
	 * Return an existing Xmpp connection associated with a user
	 *
	 * @return new Xmpp connection instance
	 */
	TSharedPtr<class IXmppConnection> GetConnection(const FString& UserId) const;

	/**
	 * Remove an existing Xmpp connection associated with a user
	 *
	 * @param UserId user to find connection for
	 */
	void RemoveConnection(const FString& UserId);

	/**
	 * Clean up any pending connection removals
	 */
	void ProcessPendingRemovals();

	/**
	 * Remove an existing Xmpp connection
	 *
	 * @param Connection reference to find/remove
	 */
	void RemoveConnection(const TSharedRef<class IXmppConnection>& Connection);

	/**
	 * @return true if Xmpp requests are globally enabled
	 */
	inline bool IsXmppEnabled() const
	{
		return bEnabled;
	}

private:

	// IModuleInterface

	void OnXmppRoomCreated(const TSharedRef<IXmppConnection>& Connection, bool bSuccess, const FXmppRoomId& RoomId, const FString& Error);
	void OnXmppRoomConfigured(const TSharedRef<IXmppConnection>& Connection, bool bSuccess, const FXmppRoomId& RoomId, const FString& Error);

	/**
	 * Called when Xmpp module is loaded
	 * Initialize platform specific parts of Xmpp handling
	 */
	virtual void StartupModule() override;
	
	/**
	 * Called when Xmpp module is unloaded
	 * Shutdown platform specific parts of Xmpp handling
	 */
	virtual void ShutdownModule() override;

	/**
	 * Connection cleanup before removal
	 */
	void CleanupConnection(const TSharedRef<class IXmppConnection>& Connection);

	/** toggles Xmpp requests */
	bool bEnabled;
	/** singleton for the module while loaded and available */
	static FXmppModule* Singleton;

	/** Active Xmpp server connections mapped by user id */
	TMap<FString, TSharedRef<class IXmppConnection>> ActiveConnections;
	/** Xmpp connections pending removal on next tick */
	TSet<TSharedPtr<IXmppConnection>> PendingRemovals;
	/** Keep track of removed connections pending cleanup */
	TArray<TSharedRef<class IXmppConnection>> PendingDeleteConnections;
};
