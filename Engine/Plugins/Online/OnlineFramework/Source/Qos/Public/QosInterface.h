// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "QosRegionManager.h"
#include "UObject/GCObject.h"

class IAnalyticsProvider;

#define NO_REGION TEXT("NONE")

/**
 * Main Qos interface for actions related to server quality of service
 */
class QOS_API FQosInterface : public TSharedFromThis<FQosInterface>, public FGCObject
{
public:

	/**
	 * Get the interface singleton
	 */
	static TSharedRef<FQosInterface> Get();

	/**
	 * Start running the async QoS evaluation
	 */
	void BeginQosEvaluation(UWorld* World, const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, const FSimpleDelegate& OnComplete);

	/**
	 * Get the region ID for this instance, checking ini and commandline overrides.
	 *
	 * Dedicated servers will have this value specified on the commandline
	 *
	 * Clients pull this value from the settings (or command line) and do a ping test to determine if the setting is viable.
	 *
	 * @return the default region identifier
	 */
	FString GetRegionId() const;

	/** @return true if a reasonable enough number of results were returned from all known regions, false otherwise */
	bool AllRegionsFound() const;

	/**
	 * Get the list of regions that the client can choose from (returned from search and must meet min ping requirements)
	 *
	 * If this list is empty, the client cannot play.
	 */
	const TArray<FQosRegionInfo>& GetRegionOptions() const;

	/**
	 * Try to set the selected region ID (must be present in GetRegionOptions)
	 */
	bool SetSelectedRegion(const FString& RegionId);

	/**
	 * Force the selected region creating a fake RegionOption if necessary
	 */
	void ForceSelectRegion(const FString& RegionId);

	/**
	 * Get the datacenter id for this instance, checking ini and commandline overrides
	 * This is only relevant for dedicated servers (so they can advertise). Client does
	 * not search on this (but may choose to prioritize results later)
	 *
	 * @return the default datacenter identifier
	 */
	static FString GetDatacenterId();

	/**
	 * Debug output for current region / datacenter information
	 */
	void DumpRegionStats();

protected:

	friend class FQosModule;
	FQosInterface();
	
	bool Init();

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:

	/** Reference to the evaluator for making datacenter determinations */
	UQosRegionManager* RegionManager;
};

