// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GCObject.h"

/**
 * Specific implementation of FGCObject that prevents a single UObject-based pointer from being GC'd while this guard is in scope.
 * @note This is the lean version of TStrongObjectPtr which uses an inline FGCObject so *can't* safely be used with containers that treat types as trivially relocatable.
 */
class FGCObjectScopeGuard : public FGCObject
{
public:
	explicit FGCObjectScopeGuard(const UObject* InObject)
		: Object(InObject)
	{
	}

	virtual ~FGCObjectScopeGuard()
	{
	}

	/** Non-copyable */
	FGCObjectScopeGuard(const FGCObjectScopeGuard&) = delete;
	FGCObjectScopeGuard& operator=(const FGCObjectScopeGuard&) = delete;

	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(Object);
	}

private:
	const UObject* Object;
};
