// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "AI/NavDataGenerator.h"

class ANavigationGraph;

/**
 * Class that handles generation of the ANavigationGraph data
 */
class FNavGraphGenerator : public FNavDataGenerator
{
public:
	FNavGraphGenerator(ANavigationGraph* InDestNavGraph);
	virtual ~FNavGraphGenerator();

private:
	/** Prevent copying. */
	FNavGraphGenerator(FNavGraphGenerator const& NoCopy) { check(0); };
	FNavGraphGenerator& operator=(FNavGraphGenerator const& NoCopy) { check(0); return *this; }

public:
	//----------------------------------------------------------------------//
	// FNavDataGenerator overrides
	//----------------------------------------------------------------------//
	virtual bool IsBuildInProgress(bool bCheckDirtyToo = false) const override;

private:
	// Performs initial setup of member variables so that generator is ready to do its thing from this point on
	void Init();
	void CleanUpIntermediateData();
	void UpdateBuilding();

private:
	/** Bounding geometry definition. */
	TArray<AVolume const*> InclusionVolumes;

	FCriticalSection GraphChangingLock;

	ANavigationGraph* DestNavGraph;

	uint32 bInitialized:1;
};
