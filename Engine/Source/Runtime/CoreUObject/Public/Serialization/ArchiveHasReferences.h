// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArchiveUObject.h"

class COREUOBJECT_API FArchiveHasReferences : private FArchiveUObject
{
public:
	FArchiveHasReferences(UObject* InTarget, const TSet<UObject*>& InPotentiallyReferencedObjects);

	bool HasReferences() const { return Result; }

	static TArray<UObject*> GetAllReferencers(const TArray<UObject*>& Referencees, const TSet<UObject*>* ObjectsToIgnore );
	static TArray<UObject*> GetAllReferencers(const TSet<UObject*>& Referencees, const TSet<UObject*>* ObjectsToIgnore );

private:
	virtual FArchive& operator<<( UObject*& Obj ) override;

	UObject* Target;
	const TSet<UObject*>& PotentiallyReferencedObjects;
	bool Result;
};
