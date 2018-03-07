// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RuntimeAssetCacheBucket.h"

/*
 * Scope lock guarding RuntimeAssetCache bucket metadata critical section.
 **/
class FRuntimeAssetCacheBucketScopeLock
{
public:
	FRuntimeAssetCacheBucketScopeLock(FRuntimeAssetCacheBucket& InBucket)
		: Bucket(InBucket)
	{
		Bucket.MetadataCriticalSection.Lock();
	}

	~FRuntimeAssetCacheBucketScopeLock()
	{
		Bucket.MetadataCriticalSection.Unlock();
	}

private:
	/** Bucket to protect. */
	FRuntimeAssetCacheBucket& Bucket;
};
