// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/NameAsStringProxyArchive.h"

class UObject;

/**
 * Implements a proxy archive that serializes UObjects and FNames as string data.
 *
 * Expected use is:
 *    FArchive* SomeAr = CreateAnAr();
 *    FObjectAndNameAsStringProxyArchive Ar(*SomeAr);
 *    SomeObject->Serialize(Ar);
 *    FinalizeAr(SomeAr);
 * 
 * @param InInnerArchive The actual FArchive object to serialize normal data types (FStrings, INTs, etc)
 */
struct FObjectAndNameAsStringProxyArchive : public FNameAsStringProxyArchive
{
	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InInnerArchive - The inner archive to proxy.
	 * @param bInLoadIfFindFails - Indicates whether to try and load a ref'd object if we don't find it
	 */
	FObjectAndNameAsStringProxyArchive(FArchive& InInnerArchive, bool bInLoadIfFindFails)
		:	FNameAsStringProxyArchive(InInnerArchive)
		,	bLoadIfFindFails(bInLoadIfFindFails)
	{ }

	/** If we fail to find an object during loading, try and load it. */
	bool bLoadIfFindFails;

	COREUOBJECT_API virtual FArchive& operator<<(UObject*& Obj) override;
	COREUOBJECT_API virtual FArchive& operator<<(FWeakObjectPtr& Obj) override;
};

