// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlState.h"
#include "SubversionSourceControlState.h"
#include "ISourceControlOperation.h"
#include "ISourceControlProvider.h"
#include "ISubversionSourceControlWorker.h"

class FSubversionSourceControlCommand;

DECLARE_DELEGATE_RetVal(FSubversionSourceControlWorkerRef, FGetSubversionSourceControlWorker)

class FSubversionSourceControlProvider : public ISourceControlProvider
{
public:
	/** Constructor */
	FSubversionSourceControlProvider() 
		: bWorkingOffline(true)
	{
	}

	/* ISourceControlProvider implementation */
	virtual void Init(bool bForceConnection = true) override;
	virtual void Close() override;
	virtual FText GetStatusText() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsAvailable() const override;
	virtual const FName& GetName(void) const override;
	virtual ECommandResult::Type GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage ) override;
	virtual TArray<FSourceControlStateRef> GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const override;
	virtual FDelegateHandle RegisterSourceControlStateChanged_Handle( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged ) override;
	virtual void UnregisterSourceControlStateChanged_Handle( FDelegateHandle Handle ) override;
	virtual ECommandResult::Type Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() ) override;
	virtual bool CanCancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) const override;
	virtual void CancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) override;
	virtual bool UsesLocalReadOnlyState() const override;
	virtual bool UsesChangelists() const override;
	virtual bool UsesCheckout() const override;
	virtual void Tick() override;
	virtual TArray< TSharedRef<class ISourceControlLabel> > GetLabels( const FString& InMatchingSpec ) const override;
#if SOURCE_CONTROL_WITH_SLATE
	virtual TSharedRef<class SWidget> MakeSettingsWidget() const override;
#endif

	/** Get the address of the repository */
	const FString& GetRepositoryName() const;

	/** Get the username we use to access the repository */
	const FString& GetUserName() const;

	/** Get the root of our working copy */
	const FString& GetWorkingCopyRoot() const;

	/** Set the root of our working copy */
	void SetWorkingCopyRoot(const FString& InWorkingCopyRoot);

	/** Get the root of our repository */
	const FString& GetRepositoryRoot() const;

	/** Set the root of our repository */
	void SetRepositoryRoot(const FString& InRepositoryRoot);

	/** Helper function used to update state cache */
	TSharedRef<FSubversionSourceControlState, ESPMode::ThreadSafe> GetStateInternal(const FString& Filename);

	/** Remove a named file from the state cache */
	bool RemoveFileFromCache(const FString& Filename);

	/**
	 * Register a worker with the provider.
	 * This is used internally so the provider can maintain a map of all available operations.
	 */
	void RegisterWorker( const FName& InName, const FGetSubversionSourceControlWorker& InDelegate );

private:

	/** Helper function for Execute() */
	TSharedPtr<class ISubversionSourceControlWorker, ESPMode::ThreadSafe> CreateWorker(const FName& InOperationName) const;

	/**
	 * In the case that a non-trusted SSL certificate came from a hostname other than the issuing server
	 * (say you are accessing the server via its IP rather than its DNS entry), we need to do some 
	 * command-line trickery to auto-accept the SSL certificate.

	 * @param	Password	The password to use when attempting to auto-accept the SSL certificate
	 */
	void AutoAcceptNonTrustedSSL(const FString& Password);

	/**
	 * Loads user/SCC information from the INI file.
	 */
	void ParseCommandLineSettings(bool bForceConnection);

	/**
	 * Helper function for running command synchronously.
	 */
	ECommandResult::Type ExecuteSynchronousCommand(class FSubversionSourceControlCommand& InCommand, const FText& Task, bool bSuppressResponseMsg);

	/**
	 * Run a command synchronously or asynchronously.
	 */
	ECommandResult::Type IssueCommand(class FSubversionSourceControlCommand& InCommand, const bool bSynchronous);

	/**
	 * Test the connection to the repository.
	 */
	bool TestConnection(const FString& RepositoryName, const FString& UserName, const FString& Password = FString());

	/**
	 * Output any messages this command holds
	 */
	void OutputCommandMessages(const class FSubversionSourceControlCommand& InCommand) const;

	/**
	 * Update the connection state according to the results of this command.
	 */
	void UpdateConnectionState(class FSubversionSourceControlCommand& InCommand);

private:

	/** Cached working copy root */
	FString WorkingCopyRoot;

	/** Cached repository root */
	FString RepositoryRoot;

	/** Flag for working offline - i.e. we haven't been able to connect to a server yet */
	bool bWorkingOffline;

	/** State cache */
	TMap<FString, TSharedRef<class FSubversionSourceControlState, ESPMode::ThreadSafe> > StateCache;

	/** The currently registered source control operations */
	TMap<FName, FGetSubversionSourceControlWorker> WorkersMap;

	/** Queue for commands given by the main thread */
	TArray < FSubversionSourceControlCommand* > CommandQueue;

	/** For notifying when the source control states in the cache have changed */
	FSourceControlStateChanged OnSourceControlStateChanged;
};
