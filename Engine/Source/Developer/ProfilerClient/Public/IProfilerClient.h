// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

struct FProfilerDataFrame;
struct FStatMetaData;

/** Delegate for passing profiler data to UI */
DECLARE_MULTICAST_DELEGATE_TwoParams(FProfilerClientDataDelegate, const FGuid&, const FProfilerDataFrame&);

/** Delegate for alerting UI a session has been established */
DECLARE_MULTICAST_DELEGATE_TwoParams(FProfilerClientConnectedDelegate, const FGuid&, const FGuid&);

/** Delegate for alerting UI a session has been disconnected */
DECLARE_MULTICAST_DELEGATE_TwoParams(FProfilerClientDisconnectedDelegate, const FGuid&, const FGuid&);

/** Delegate for alerting subscribers the meta data has been updated */
DECLARE_MULTICAST_DELEGATE_TwoParams(FProfilerMetaDataUpdateDelegate, const FGuid&, const FStatMetaData&);

/** Delegate for alerting clients a load has started */
DECLARE_MULTICAST_DELEGATE_OneParam(FProfilerLoadStartedDelegate, const FGuid&);

/** Delegate for alerting clients a load has completed */
DECLARE_MULTICAST_DELEGATE_OneParam(FProfilerLoadCompletedDelegate, const FGuid&);

/** Delegate for alerting clients a load was cancelled */
DECLARE_MULTICAST_DELEGATE_OneParam(FProfilerLoadCancelledDelegate, const FGuid&);

/** Delegate for alerting clients a load has loaded meta data */
DECLARE_MULTICAST_DELEGATE_OneParam(FProfilerLoadedMetaDataDelegate, const FGuid&);

/** Delegate for updating the current progress of file transfer. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FProfilerFileTransferDelegate, const FString& /*Filename*/, int64 /*FileProgress*/, int64 /*FileSize*/);


/**
 * Interface for Profiler Client.
 */
class IProfilerClient
{
public:

	/**
	 * Subscribe to the given profiler session id
	 *
	 * @param Session The session to subscribe to for profiler data
	 */
	virtual void Subscribe(const FGuid& Session) = 0;

	/**
	 * 
	 */
	virtual void Track(const FGuid& Instance) = 0;

	/**
	 * 
	 */
	virtual void Untrack(const FGuid& Instance) = 0;

	/** Unsubscribe from all sessions. */
	virtual void Unsubscribe() = 0;

	/**
	 * Changes the current state of the capturing data service-side.
	 *
	 * @param bRequestedCaptureState Data capture state that should be set.
	 * @param InstanceId If valid, this function will be executed only for the specified instance, otherwise will be executed on all instances.
	 *
	 */
	virtual void SetCaptureState(const bool bRequestedCaptureState, const FGuid& InstanceId = FGuid()) = 0; 

	/**
	 * Changes the current state of the previewing capture data.
	 *
	 * @param bRequestedCaptureState Data preview state that should be set.
	 * @param InstanceId If valid, this function will be executed only for the specified instance, otherwise will be executed on all instances.
	 *
	 */
	virtual void SetPreviewState(const bool bRequestedPreviewState, const FGuid& InstanceId = FGuid()) = 0;

	/**
	 * Loads a Capture file
	 */
	virtual void LoadCapture(const FString& DataFilepath, const FGuid& ProfileId) = 0;

	/**
	 * Cancels a capture file load that is in progress
	 */
	virtual void CancelLoading(const FGuid InstanceId) = 0;

	/**
	 * Requests the last captured file from the service.
	 *
	 * @param InstanceId If valid, this function will be executed only for the specified instance, otherwise will be executed on all instances
	 */
	virtual void RequestLastCapturedFile(const FGuid& InstanceId = FGuid()) = 0; 

	/**
	 * Gets the description for the given stat id
	 *
	 * @param StatId The identifier of the statistic description to retrieve.
	 * @return the FStatMetaData struct with the description.
	 */
	virtual const FStatMetaData& GetStatMetaData(const FGuid& InstanceId) const = 0;

	/**
	 * Retrieves the profiler data delegate.
	 *
	 * @return profiler data delegate.
	 */
	virtual FProfilerClientDataDelegate& OnProfilerData() = 0;

	/**
	 * Retrieves the profiler file transfer delegate.
	 *
	 * @return profiler file transfer delegate.
	 */
	virtual FProfilerFileTransferDelegate& OnProfilerFileTransfer() = 0;

	/**
	 * Retrieves the profiler client connected delegate.
	 *
	 * @return profiler client connected delegate.
	 */
	virtual FProfilerClientConnectedDelegate& OnProfilerClientConnected() = 0;

	/**
	 * Retrieves the profiler client disconnected delegate.
	 *
	 * @return profiler client disconnected delegate.
	 */
	virtual FProfilerClientDisconnectedDelegate& OnProfilerClientDisconnected() = 0;

	/**
	 * Retrieves the profiler meta data update delegate.
	 *
	 * @return profiler meta data delegate.
	 */
	virtual FProfilerMetaDataUpdateDelegate& OnMetaDataUpdated() = 0;

	/**
	 * Retrieves the load started delegate.
	 *
	 * @return profiler load start delegate.
	 */
	virtual FProfilerLoadStartedDelegate& OnLoadStarted() = 0;

	/**
	 * Retrieves the load completed delegate.
	 *
	 * @return profiler load completed delegate.
	 */
	virtual FProfilerLoadCompletedDelegate& OnLoadCompleted() = 0;

	/**
	* Retrieves the load cancelled delegate.
	*
	* @return profiler load cancelled delegate.
	*/
	virtual FProfilerLoadCancelledDelegate& OnLoadCancelled() = 0;

protected:

	/** Virtual destructor. */
	virtual ~IProfilerClient() { }
};
