// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Containers/Ticker.h"
#include "GameFramework/WorldSettings.h"
#include "MeshUtilities.h"
#include "IMeshReductionInterfaces.h"

class ALODActor;

class HIERARCHICALLODUTILITIES_API FHierarchicalLODProxyProcessor : public FTickerObjectBase
{
public:		
	FHierarchicalLODProxyProcessor();
	~FHierarchicalLODProxyProcessor();
	
	/** Begin FTickerObjectBase */
	virtual bool Tick(float DeltaTime) override;
	/** End FTickerObjectBase */

	/**
	* AddProxyJob
	* @param InLODActor - LODActor for which the mesh will be generated
	* @param LODSetup - Simplification settings structure
	* @return FGuid - Guid for the job
	*/
	FGuid AddProxyJob(ALODActor* InLODActor, const FHierarchicalSimplification& LODSetup);
	
	/** Callback function used for processing finished mesh generation jobs	
	* @param InGuid - Guid of the finished job
	* @param InAssetsToSync - Assets data created by the job
	*/
	void ProcessProxy(const FGuid InGuid, TArray<UObject*>& InAssetsToSync);
	
	/** Returns the callback delegate which will be passed onto ProxyLOD function */
	FCreateProxyDelegate& GetCallbackDelegate();
protected:
	/** Called when the map has changed*/
	void OnMapChange(uint32 MapFlags);

	/** Called when the current level has changed */
	void OnNewCurrentLevel();

	/** Clears the processing data array/map */
	void ClearProcessingData();
protected:
	/** Structure storing the data required during processing */
	struct FProcessData
	{
		/** LODActor instance for which a proxy is generated */
		ALODActor* LODActor;
		/** Array with resulting asset objects from proxy generation (StaticMesh/Materials/Textures) */
		TArray<class UObject*> AssetObjects;
		/** HLOD settings structure used for creating the proxy */
		FHierarchicalSimplification LODSetup;
	};
private:
	/** Map and array used to store job data */
	TMap<FGuid, FProcessData*> JobActorMap;
	TArray<FProcessData*> ToProcessJobs;
	/** Delegate to pass onto */
	FCreateProxyDelegate CallbackDelegate;	
	/** Critical section to keep JobActorMap/ToProcessJobs access thread-safe */
	FCriticalSection StateLock;	
};
