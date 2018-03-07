// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "QosInterface.h"
#include "QosModule.h"

//static 
TSharedRef<FQosInterface> FQosInterface::Get()
{
	return FQosModule::Get().GetQosInterface();
}

FQosInterface::FQosInterface()
	: RegionManager(nullptr)
{
}

bool FQosInterface::Init()
{
	RegionManager = NewObject<UQosRegionManager>();
	return RegionManager != nullptr;
}

void FQosInterface::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (RegionManager)
	{
		Collector.AddReferencedObject(RegionManager);
	}
}

// static
FString FQosInterface::GetDatacenterId()
{
	return UQosRegionManager::GetDatacenterId();
}

void FQosInterface::BeginQosEvaluation(UWorld* World, const TSharedPtr<IAnalyticsProvider>& AnalyticsProvider, const FSimpleDelegate& OnComplete)
{
	check(RegionManager);
	RegionManager->BeginQosEvaluation(World, AnalyticsProvider, OnComplete);
}

FString FQosInterface::GetRegionId() const
{
	check(RegionManager);
	return RegionManager->GetRegionId();
}

bool FQosInterface::AllRegionsFound() const
{
	check(RegionManager);
	return RegionManager->AllRegionsFound();
}

const TArray<FQosRegionInfo>& FQosInterface::GetRegionOptions() const
{
	check(RegionManager);
	return RegionManager->GetRegionOptions();
}

void FQosInterface::ForceSelectRegion(const FString& InRegionId)
{
	check(RegionManager);
	RegionManager->ForceSelectRegion(InRegionId);
}

bool FQosInterface::SetSelectedRegion(const FString& InRegionId)
{
	check(RegionManager);
	return RegionManager->SetSelectedRegion(InRegionId);
}

void FQosInterface::DumpRegionStats()
{
	check(RegionManager);
	return RegionManager->DumpRegionStats();
}
