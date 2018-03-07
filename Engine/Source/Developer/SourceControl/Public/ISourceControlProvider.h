// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "Features/IModularFeature.h"
#include "ISourceControlState.h"
#include "SourceControlHelpers.h"

#ifndef SOURCE_CONTROL_WITH_SLATE
	#error "SOURCE_CONTROL_WITH_SLATE must be defined. Did you forget a dependency on the 'SourceControl' module?"
#endif

class ISourceControlLabel;

/**
 * Hint for how to execute the operation. Note that asynchronous operations require
 * Tick() to be called to manage completed operations.
 */
namespace EConcurrency
{
	enum Type
	{
		/** Force the operation to be issued on the same thread, blocking until complete. */
		Synchronous,
		/** Run the command on another thread, returning immediately. */
		Asynchronous
	};
}

/**
 * Hint to provider when updating state
 */
namespace EStateCacheUsage
{
	enum Type
	{
		/** Force a synchronous update of the state of the file. */
		ForceUpdate,
		/** Use the cached state if possible */
		Use,
	};
}

/**
 * Results of a command execution
 */
namespace ECommandResult
{
	enum Type
	{
		/** Command failed to execute correctly or was not supported by the provider. */
		Failed,
		/** Command executed successfully */
		Succeeded,
		/** Command was canceled before completion */
		Cancelled,
	};
}

/** Delegate used by providers for when operations finish */
DECLARE_DELEGATE_TwoParams(FSourceControlOperationComplete, const FSourceControlOperationRef&, ECommandResult::Type);

/** Delegate used by providers to create source control operations */
typedef TSharedRef<class ISourceControlOperation, ESPMode::ThreadSafe> FOperationSharedRef;
DECLARE_DELEGATE_RetVal(FOperationSharedRef, FGetSourceControlOperation);

/** Delegate called when the state of an item (or group of items) has changed. */
DECLARE_MULTICAST_DELEGATE(FSourceControlStateChanged);

/**
 * Interface to talking with source control providers.
 */
class ISourceControlProvider : public IModularFeature
{
public:
	/**
	 * Virtual destructor
	 */
	virtual ~ISourceControlProvider() {}

	/** 
	 * Initialize source control provider.
	 * @param	bForceConnection	If set, this flag forces the provider to attempt a connection to its server.
	 */
	virtual void Init(bool bForceConnection = true) = 0;

	/** 
	 * Shut down source control provider.
	 */
	virtual void Close() = 0;

	/** Get the source control provider name */
	virtual const FName& GetName() const = 0;

	/** Get the source control status as plain, human-readable text */
	virtual FText GetStatusText() const = 0;

	/** Quick check if source control is enabled */
	virtual bool IsEnabled() const = 0;

	/**
	 * Quick check if source control is available for use (server-based providers can use this
	 * to return whether the server is available or not)
	 *
	 * @return	true if source control is available, false if it is not
	 */
	virtual bool IsAvailable() const = 0;

	/**
	 * Login to the source control server (if any).
	 * This is just a wrapper around Execute().
	 * @param	InPassword						The password (if required) to use when logging in.
	 * @param	InConcurrency					How to execute the operation, blocking or asynchronously on another thread.
	 * @param	InOperationCompleteDelegate		Delegate to call when the operation is completed. This is called back internal to this call when executed on the main thread, or from Tick() when queued for asynchronous execution.
	 * @return the result of the operation.
	 */
	virtual ECommandResult::Type Login( const FString& InPassword = FString(), EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() )
	{
		TSharedRef<FConnect, ESPMode::ThreadSafe> ConnectOperation = ISourceControlOperation::Create<FConnect>();
		ConnectOperation->SetPassword(InPassword);
		return Execute(ConnectOperation, InConcurrency, InOperationCompleteDelegate);
	}

	/**
	 * Get the state of each of the passed-in files. State may be cached for faster queries. Note states can be NULL!
	 * @param	InFiles				The files to retrieve state for.
	 * @param	OutState			The states of the files. This will be empty if the operation fails. Note states can be NULL!
	 * @param	InStateCacheUsage	Whether to use the state cache or to force a (synchronous) state retrieval.
	 * @return the result of the operation.
	 */
	virtual ECommandResult::Type GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage ) = 0;

	/**
	 * Helper overload for state retrieval, see GetState().
	 */
	virtual ECommandResult::Type GetState( const TArray<UPackage*>& InPackages, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage )
	{
		TArray<FString> Files = SourceControlHelpers::PackageFilenames(InPackages);
		return GetState(Files, OutState, InStateCacheUsage);
	}

	/**
	 * Helper overload for state retrieval, see GetState().
	 */
	virtual TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> GetState( const FString& InFile, EStateCacheUsage::Type InStateCacheUsage )
	{
		TArray<FString> Files;
		TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> > States;
		Files.Add(InFile);
		if(GetState(Files, States, InStateCacheUsage) == ECommandResult::Succeeded)
		{
			TSharedRef<ISourceControlState, ESPMode::ThreadSafe> State = States[0];
			return State;
		}
		return NULL;
	}

	/**
	 * Helper overload for state retrieval, see GetState().
	 */
	virtual TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> GetState( const UPackage* InPackage, EStateCacheUsage::Type InStateCacheUsage )
	{
		return GetState(SourceControlHelpers::PackageFilename(InPackage), InStateCacheUsage);
	}

	/**
	 * Get all cached source control state objects for which the supplied predicate returns true
	 */
	virtual TArray<FSourceControlStateRef> GetCachedStateByPredicate(TFunctionRef<bool(const FSourceControlStateRef&)> Predicate) const = 0;

	/**
	 * Register a delegate to be called when source control state(s) change
	 */
	virtual FDelegateHandle RegisterSourceControlStateChanged_Handle( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged ) = 0;

	/**
	 * Unregister a delegate to be called when source control state(s) change
	 */
	virtual void UnregisterSourceControlStateChanged_Handle( FDelegateHandle Handle ) = 0;

	/**
	 * Attempt to execute an operation on the passed-in files (if any are required).
	 * @param	InOperation						The operation to perform.
	 * @param	InFiles							The files to operate on.
	 * @param	InConcurrency					How to execute the operation, blocking or asynchronously on another thread.
	 * @param	InOperationCompleteDelegate		Delegate to call when the operation is completed. This is called back internal to this call when executed on the main thread, or from Tick() when queued for asynchronous execution.
	 * @return the result of the operation.
	 */
	virtual ECommandResult::Type Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() ) = 0;

	/**
	 * Helper overload for operation execution, see Execute().
	 */
	virtual ECommandResult::Type Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() )
	{
		return Execute(InOperation, TArray<FString>(), InConcurrency, InOperationCompleteDelegate);
	}

	/**
	 * Helper overload for operation execution, see Execute().
	 */
	virtual ECommandResult::Type Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const UPackage* InPackage, const EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() )
	{
		return Execute(InOperation, SourceControlHelpers::PackageFilename(InPackage), InConcurrency, InOperationCompleteDelegate);
	}

	/**
	 * Helper overload for operation execution, see Execute().
	 */
	virtual ECommandResult::Type Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const FString& InFile, const EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() )
	{
		TArray<FString> FileArray;
		FileArray.Add(InFile);
		return Execute(InOperation, FileArray, InConcurrency, InOperationCompleteDelegate);
	}

	/**
	 * Helper overload for operation execution, see Execute().
	 */
	virtual ECommandResult::Type Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<UPackage*>& InPackages, const EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() )
	{
		TArray<FString> FileArray = SourceControlHelpers::PackageFilenames(InPackages);
		return Execute(InOperation, FileArray, InConcurrency, InOperationCompleteDelegate);
	}

	/**
	 * Check to see if we can cancel an operation.
	 * @param	InOperation		The operation to check.
	 * @return true if the operation was cancelled.
	 */
	virtual bool CanCancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) const = 0;

	/**
	 * Attempt to cancel an operation in progress.
	 * @param	InOperation		The operation to attempt to cancel.
	 */
	virtual void CancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) = 0;

	/**
	 * Get a label matching the passed-in name.
	 * @param	InLabelName	String specifying the label name
	 * @return the label, if any
	 */
	virtual TSharedPtr<class ISourceControlLabel> GetLabel( const FString& InLabelName ) const
	{
		TArray< TSharedRef<class ISourceControlLabel> > Labels = GetLabels(InLabelName);
		if(Labels.Num() > 0)
		{
			return Labels[0];
		}

		return NULL;
	}

	/**
	 * Get an array of labels matching the passed-in spec.
	 * @param	InMatchingSpec	String specifying the label spec.
	 * @return an array of labels matching the spec.
	 */
	virtual TArray< TSharedRef<class ISourceControlLabel> > GetLabels( const FString& InMatchingSpec ) const = 0;

	/**
	 * Whether the provider uses local read-only state to signal whether a file is editable.
	 */
	virtual bool UsesLocalReadOnlyState() const = 0;

	/**
	 * Whether the provider uses changelists to identify commits/revisions
	 */
	virtual bool UsesChangelists() const = 0;

	/**
	 * Whether the provider uses the checkout workflow
	 */
	virtual bool UsesCheckout() const = 0;

	/**
	 * Called every update.
	 */
	virtual void Tick() = 0;

#if SOURCE_CONTROL_WITH_SLATE
	/** 
	 * Create a settings widget for display in the login window.
	 * @returns a widget used to edit the providers settings required prior to connection.
	 */
	virtual TSharedRef<class SWidget> MakeSettingsWidget() const = 0;
#endif // SOURCE_CONTROL_WITH_SLATE
};


