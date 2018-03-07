// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FLazyObjectPtr;
struct FSoftObjectPtr;
struct FSoftObjectPath;
struct FWeakObjectPtr;

/**
 * Base FArchive for serializing UObjects. Supports FLazyObjectPtr and FSoftObjectPtr serialization.
 */
class COREUOBJECT_API FArchiveUObject : public FArchive
{
public:

	using FArchive::operator<<; // For visibility of the overloads we don't override

	//~ Begin FArchive Interface
	virtual FArchive& operator<<(FLazyObjectPtr& Value) override;
	virtual FArchive& operator<<(FSoftObjectPtr& Value) override;
	virtual FArchive& operator<<(FSoftObjectPath& Value) override;
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override;
	//~ End FArchive Interface
};
