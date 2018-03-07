// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectClusters.h: Unreal UObject Cluster helper functions
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectArray.h"

#ifndef UE_GCCLUSTER_VERBOSE_LOGGING
#define UE_GCCLUSTER_VERBOSE_LOGGING (0 && !UE_BUILD_SHIPPING)
#endif

// Verifies cluster assumptions
COREUOBJECT_API bool VerifyClusterAssumptions(UObject* ClusterRootObject);

#if !UE_BUILD_SHIPPING

// Dumps a single cluster to log
COREUOBJECT_API void DumpClusterToLog(const FUObjectCluster& Cluster, bool bHierarchy, bool bIndexOnly);

// Dumps all clusters to log.
COREUOBJECT_API void ListClusters(const TArray<FString>& Args);

// Attempts to find clusters with no references to them
COREUOBJECT_API void FindStaleClusters(const TArray<FString>& Args);

#endif // !UE_BUILD_SHIPPING

