// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"
#include "HAL/FileManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/SlowTask.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "Serialization/ArrayWriter.h"
#include "Containers/Queue.h"
#include "SimplygonSwarmCommon.h"

#include "MeshUtilities.h"

#include "Runtime/Online/HTTP/Public/Interfaces/IHttpResponse.h"
#include "Runtime/Online/HTTP/Public/Interfaces/IHttpRequest.h"
#include "Runtime/Online/HTTP/Public/HttpModule.h"

/** Enum representing state used by Simplygon Grid Server */
enum SimplygonRESTState
{
	SRS_UNKNOWN,
	SRS_FAILED,
	SRS_ASSETUPLOADED_PENDING,
	SRS_ASSETUPLOADED,
	SRS_JOBCREATED_PENDING,
	SRS_JOBCREATED,
	SRS_JOBSETTINGSUPLOADED_PENDING,
	SRS_JOBSETTINGSUPLOADED,
	SRS_JOBPROCESSING_PENDING,
	SRS_JOBPROCESSING,
	SRS_JOBPROCESSED,
	SRS_ASSETDOWNLOADED_PENDING,
	SRS_ASSETDOWNLOADED
};

/** Enum representing state used internally by REST Client to manage multi-part asset uploading to Simplygon Grid */
enum EUploadPartState
{
	UPS_BEGIN,			// start uploading (hand shake)
	UPS_UPLOADING_PART,	// upload part
	UPS_END				// upload transaction completed
};

/**
Intermediate struct to hold upload file chunks for multi-part upload
Note : Multi part upload are required as the Simplygon Grid Server has a 2GB file upload limitation
*/
struct FSwarmUploadPart
{
	/** Upload part binary chunck */
	TArray<uint8> Data;

	/** Part number */
	int32 PartNumber;

	/** Bool if part has been uploaded */
	FThreadSafeBool PartUploaded;

	~FSwarmUploadPart()
	{
		Data.Empty();
	}

};

/** Struct holding essential task data for task management. */
struct FSwarmTaskkData
{
	/** Path to the zip file that needs to be uploaded */
	FString ZipFilePath;

	/** Path to spl file that needs to be uploaded */
	FString SplFilePath;

	/** Path to zip file containing resulting geometry */
	FString OutputZipFilePath;

	/** Swarm Job Directory */
	FString JobDirectory;

	/** Swarm Job Name - Can be used to track jobs using the Admin Utility*/
	FString JobName;

	/** Lock for synchornization between threads */
	FCriticalSection* StateLock;

	/** Unique Job Id */
	FGuid ProcessorJobID;

	/** Set if the task upload has been completed */
	FThreadSafeBool TaskUploadComplete;

	/** Supports Dithered Transition */
	bool bDitheredTransition;
    // Whether or not emissive should be outputted  
	bool bEmissive;
};

/** Struct that hold intermediate data used to communicate next state to Simplygon Grid Server */
struct FSwarmJsonResponse
{
	/** Unique JobId */
	FString JobId;

	/** Unique asset id returned from server.
	Note : This can be used to check if asset already is available on the server to save network bandwidth
	*/
	FString AssetId;

	/** Supports Dithered Transition */
	FString ErrorMessage;

	/** Supports Dithered Transition */
	uint32 Progress;

	/** Supports Dithered Transition */
	FString Status;

	/** Supports Dithered Transition */
	FString OutputAssetId;

	/** Supports Dithered Transition */
	FString UploadId;
};

/** Simplygon Swarm Task. Responsible for communicating with the Grid Server  */
class FSimplygonSwarmTask
{

public:
	DECLARE_DELEGATE_OneParam(FSimplygonSwarmTaskDelegate, const FSimplygonSwarmTask&);

	FSimplygonSwarmTask(const FSwarmTaskkData& InTaskData);
	~FSimplygonSwarmTask();

	/* Method to get/set Task Sate */
	SimplygonRESTState GetState() const;
	void SetState(SimplygonRESTState InState);

	bool IsFinished() const;

	/*~ Events */
	FSimplygonSwarmTaskDelegate& OnAssetDownloaded() { return OnAssetDownloadedDelegate; }
	FSimplygonSwarmTaskDelegate& OnAssetUploaded() { return OnAssetUploadedDelegate; }
	FSimplygonSwarmTaskDelegate& OnSwarmTaskFailed() { return OnTaskFailedDelegate; }

	void EnableDebugLogging();	

	/**
	Method to setup the server the task should use.
	Currently only Single host is supported.
	*/
	void SetHost(FString InHostAddress);

	/*
	The following method is used to split the asset into multiple parts for a multi part upload.
	*/
	void CreateUploadParts(const int32 MaxUploadFileSize);

	/*
	Tells if the current task needs to be uploaded as seperate part due to data size limits
	*/
	bool NeedsMultiPartUpload();

	//~Being Rest methods
	void AccountInfo();
	void CreateJob();
	void UploadJobSettings();
	void ProcessJob();
	void GetJob();
	void UploadAsset();
	void DownloadAsset();

	/*
	Note for multi part upload the following need to happen
	Call upload being to setup the multi part upload
	upload parts using MultiPartUplaodPart
	Call MultiPartupload end
	*/
	void MultiPartUploadBegin();
	void MultiPartUploadPart(const uint32 partNo);
	void MultiPartUploadEnd();
	void MultiPartUploadGet();

	//~Being Response methods
	void AccountInfo_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void CreateJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void UploadJobSettings_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void ProcessJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void GetJob_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void UploadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void DownloadAsset_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void AddAuthenticationHeader(TSharedRef<IHttpRequest> request);

	//Multi part upload responses
	void MultiPartUploadBegin_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void MultiPartUploadPart_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void MultiPartUploadEnd_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void MultiPartUploadGet_Response(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	//~End Rest methods

	/** Helper method use to deserialze json respone from SimplygonGRid and popupate the FSwarmJsonRepose Struct*/
	bool ParseJsonMessage(FString InJsonMessage, FSwarmJsonResponse& OutResponseData);

	/** Essential Task Data */
	FSwarmTaskkData TaskData;

private:

	/** Task State */
	SimplygonRESTState State;

	/** Job Id */
	FString JobId;

	/** Asset Id returned from the server */
	FString InputAssetId;

	/** Output Asset id */
	FString OutputAssetId;

	/** Is Completed */
	FThreadSafeBool IsCompleted;

	/** Enable Debug Logging */
	bool bEnableDebugLogging;

	/** Parts left to upload (Multi-part Uploading) */
	FThreadSafeCounter RemainingPartsToUpload;

	/** Debug HttpRequestCounter
	Note: This was added to track issues when two resposnes came for a completed job.
	Since the job was completed before the object is partially destoreyd when a new response came in.
	The import file method failed. This was added for debugging. The most likely cause if that the respose delegate is never cleaned up.
	(This must be zero else, Sometime two responses for job completed arrive which caused issue) */
	FThreadSafeCounter DebugHttpRequestCounter;

	/** Multipart upload has been initalized*/
	bool bMultiPartUploadInitialized;

	/** Multi part upload data*/
	TIndirectArray<FSwarmUploadPart> UploadParts;

	/** Total number of parts to upload*/
	uint32 TotalParts;

	/** Simplygon Grid Server IP address */
	FString HostName;

	/** API Key used to communicate with the Grid Server */
	FString APIKey;

	/** Upload Id used for multipart upload */
	FString UploadId;

	/** Total upload size */
	int32 UploadSize;

	//~ Delegates Begin
	FSimplygonSwarmTaskDelegate OnAssetDownloadedDelegate;
	FSimplygonSwarmTaskDelegate OnAssetUploadedDelegate;
	FSimplygonSwarmTaskDelegate OnProgressUpdated;
	FSimplygonSwarmTaskDelegate OnTaskFailedDelegate;
	//~ Delegates End

	/** Map that stores pending request. They need to be cleaned up when destroying the instance. Especially if job has completed*/
	TMap<TSharedPtr<IHttpRequest>, FString> PendingRequests;

};

/*
Simplygon REST Based Clinet. Responsible for managing/controlling task. Runs on its own thread.
*/
class FSimplygonRESTClient : public FRunnable
{
public:

	/* Add a swarm task to the Queue*/
	void AddSwarmTask(TSharedPtr<FSimplygonSwarmTask>& InTask);

	void SetMaxUploadSizeInBytes(int32 InMaxUploadSizeInBytes);

	/*Get pointer to the runnable*/
	static FSimplygonRESTClient* Get();

	static void Shutdown();

private:


	FSimplygonRESTClient();

	~FSimplygonRESTClient();

	//~ Begin FRunnable Interface
	virtual bool Init();

	virtual uint32 Run();

	/* Method that goes through all task and checks their status*/
	void UpdateTaskStates();

	/* Moves jobs into the Jobs Buffer*/
	void MoveItemsToBoundedArray();

	virtual void Stop();

	virtual void Exit();
	//~ End FRunnable Interface


	void Wait(const float InSeconds, const float InSleepTime = 0.1f);
	
	void EnusureCompletion();

	/** Checks if there's been any Stop requests */
	FORCEINLINE bool ShouldStop() const
	{
		return StopTaskCounter.GetValue() > 0;
	}

private:

	/*Static instance*/
	static FSimplygonRESTClient* Runnable;

	/*Critical Section*/
	FCriticalSection CriticalSectionData;	

	FThreadSafeCounter StopTaskCounter;	

	/* a local buffer as to limit the number of concurrent jobs. */
	TArray<TSharedPtr< class FSimplygonSwarmTask>> JobsBuffer;

	/* Pending Jobs Queue */
	TQueue<TSharedPtr<class FSimplygonSwarmTask>, EQueueMode::Mpsc> PendingJobs;

	/* Thread*/
	FRunnableThread* Thread;

	/* Simplygon Grid Server IP Address*/
	FString HostName;

	/* API Key*/
	FString APIKey;

	/* Add a swarm task to the Queue*/
	bool bEnableDebugging;

	/* Sleep time between status updates*/
	float DelayBetweenRuns;

	/* Number of Simultaneous Jobs to Manage*/
	int32 JobLimit;

	/* Max Upload size in bytes . Should not be more than the 2GB data limit for the Grid Server*/
	int32 MaxUploadSizeInBytes;
};