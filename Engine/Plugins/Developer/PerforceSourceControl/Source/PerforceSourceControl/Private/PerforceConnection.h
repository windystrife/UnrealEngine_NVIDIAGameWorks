// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PerforceSourceControlPrivate.h"
#include "PerforceSourceControlCommand.h"

/** A map containing result of running Perforce command */
class FP4Record : public TMap<FString, FString >
{
public:

	virtual ~FP4Record() {}

	FString operator () (const FString& Key) const
	{
		const FString* Found = Find(Key);
		return Found ? *Found : TEXT("");
	}
};

typedef TArray<FP4Record> FP4RecordSet;

class FPerforceConnection
{
public:
	//This constructor is strictly for internal questions to perforce (get client spec list, etc)
	FPerforceConnection(const FPerforceConnectionInfo& InConnectionInfo);
	/** API Specific close of source control project*/
	~FPerforceConnection();

	/** 
	 * Attempts to automatically detect the workspace to use based on the working directory
	 */
	static bool AutoDetectWorkspace(const FPerforceConnectionInfo& InConnectionInfo, FString& OutWorkspaceName);

	/**
	 * Static function in charge of making sure the specified connection is valid or requests that data from the user via dialog
	 * @param InOutPortName			Port name in the inifile.  Out value is the port name from the connection dialog
	 * @param InOutUserName			User name in the inifile.  Out value is the user name from the connection dialog
	 * @param InOutWorkspaceName	Workspace name in the inifile.  Out value is the client spec from the connection dialog
	 * @param InConnectionInfo		Connection credentials
	 * @return - true if the connection, whether via dialog or otherwise, is valid.  False if source control should be disabled
	 */
	static bool EnsureValidConnection(FString& InOutServerName, FString& InOutUserName, FString& InOutWorkspaceName, const FPerforceConnectionInfo& InConnectionInfo);

	/**
	 * Get List of ClientSpecs
	 * @param InConnectionInfo	Connection credentials
	 * @param InOnIsCancelled	Delegate called to check for operation cancellation.
	 * @param OutWorkspaceList	The workspace list output.
	 * @param OutErrorMessages	Any error messages output.
	 * @return - True if successful
	 */
	bool GetWorkspaceList(const FPerforceConnectionInfo& InConnectionInfo, FOnIsCancelled InOnIsCancelled, TArray<FString>& OutWorkspaceList, TArray<FText>& OutErrorMessages);

	/** Returns true if connection is currently active */
	bool IsValidConnection();

	/** If the connection is valid, disconnect from the server */
	void Disconnect();

	/**
	 * Runs internal perforce command, catches exceptions, returns results
	 */
	bool RunCommand(const FString& InCommand, const TArray<FString>& InParameters, FP4RecordSet& OutRecordSet, TArray<FText>&  OutErrorMessage, FOnIsCancelled InIsCancelled, bool& OutConnectionDropped)
	{
		const bool bStandardDebugOutput=true;
		const bool bAllowRetry=true;
		return RunCommand(InCommand, InParameters, OutRecordSet, OutErrorMessage, InIsCancelled, OutConnectionDropped, bStandardDebugOutput, bAllowRetry);
	}

	/**
	 * Runs internal perforce command, catches exceptions, returns results
	 */
	bool RunCommand(const FString& InCommand, const TArray<FString>& InParameters, FP4RecordSet& OutRecordSet, TArray<FText>& OutErrorMessage, FOnIsCancelled InIsCancelled, bool& OutConnectionDropped, const bool bInStandardDebugOutput, const bool bInAllowRetry);

	/**
	 * Creates a changelist with the specified description
	 */
	int32 CreatePendingChangelist(const FText &Description, FOnIsCancelled InIsCancelled, TArray<FText>& OutErrorMessages);

	/**
	 * Attempt to login - some servers will require this 
	 * @param	InConnectionInfo		Credentials to use
	 * @return true if successful
	 */
	bool Login(const FPerforceConnectionInfo& InConnectionInfo);

	/**
	 * Make a valid connection if possible
	 * @param InConnectionInfo		Connection credentials
	 */
	void EstablishConnection(const FPerforceConnectionInfo& InConnectionInfo);

public:
#if USE_P4_API
	/** Perforce API client object */
	ClientApi P4Client;
#endif

	/** The current root for the client workspace */
	FString ClientRoot;

	/** true if connection was successfully established */
	bool bEstablishedConnection;

	/** Is this a connection to a unicode server? */ 
	bool bIsUnicode;
};

/**
 * Connection that is used within specific scope
 */
class FScopedPerforceConnection
{
public:
	/** 
	 * Constructor - establish a connection.
	 * The concurrency of the passed in command determines whether the persistent connection is 
	 * used or another connection is established (connections cannot safely be used across different
	 * threads).
	 */
	FScopedPerforceConnection( class FPerforceSourceControlCommand& InCommand );

	/** 
	 * Constructor - establish a connection.
	 * The concurrency passed in determines whether the persistent connection is used or another 
	 * connection is established (connections cannot safely be used across different threads).
	 */
	FScopedPerforceConnection( EConcurrency::Type InConcurrency, const FPerforceConnectionInfo& InConnectionInfo );

	/**
	 * Destructor - disconnect if this is a temporary connection
	 */
	~FScopedPerforceConnection();

	/** Get the connection wrapped by this class */
	FPerforceConnection& GetConnection() const
	{
		return *Connection;
	}

	/** Check if this connection is valid */
	bool IsValid() const
	{
		return Connection != NULL;
	}

private:
	/** Set up the connection */
	void Initialize( const FPerforceConnectionInfo& InConnectionInfo );

private:
	/** The perforce connection we are using */
	FPerforceConnection* Connection;

	/** The concurrency of this connection */
	EConcurrency::Type Concurrency;
};
