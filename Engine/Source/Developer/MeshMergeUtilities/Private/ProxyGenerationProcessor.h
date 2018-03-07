// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "RawMesh.h"
#include "MaterialUtilities.h"

struct FMergeCompleteData;
struct FProxyGenerationData;

DECLARE_LOG_CATEGORY_EXTERN(LogMeshMerging, Verbose, All);

class FProxyGenerationProcessor : FTickerObjectBase
{
public:
	FProxyGenerationProcessor();
	~FProxyGenerationProcessor();

	void AddProxyJob(FGuid InJobGuid, FMergeCompleteData* InCompleteData);
	virtual bool Tick(float DeltaTime) override;
	void ProxyGenerationComplete(FRawMesh& OutProxyMesh, struct FFlattenMaterial& OutMaterial, const FGuid OutJobGUID);

	//@third party BEGIN SIMPLYGON
	void ProxyGenerationFailed(const FGuid OutJobGUID, const FString& ErrorMessage);
	//@third party END SIMPLYGON

protected:
	/** Called when the map has changed*/
	void OnMapChange(uint32 MapFlags);

	/** Called when the current level has changed */
	void OnNewCurrentLevel();

	/** Clears the processing data array/map */
	void ClearProcessingData();

protected:
	/** Structure storing the data required during processing */
	struct FProxyGenerationData
	{
		FRawMesh RawMesh;
		FFlattenMaterial Material;
		FMergeCompleteData* MergeData;
	};

	void ProcessJob(const FGuid& JobGuid, FProxyGenerationData* Data);
protected:
	/** Holds Proxy mesh job data together with the job Guid */
	TMap<FGuid, FMergeCompleteData*> ProxyMeshJobs;
	/** Holds Proxy generation data together with the job Guid */
	TMap<FGuid, FProxyGenerationData*> ToProcessJobDataMap;
	/** Critical section to keep ProxyMeshJobs/ToProcessJobDataMap access thread-safe */
	FCriticalSection StateLock;
};

