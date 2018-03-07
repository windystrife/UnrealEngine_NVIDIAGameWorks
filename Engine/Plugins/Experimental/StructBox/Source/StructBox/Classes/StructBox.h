// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "StructBox.generated.h"

USTRUCT(BlueprintType)
struct FStructBox
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UScriptStruct* ScriptStruct;

	uint8* StructMemory;

	FStructBox() 
		: StructMemory(nullptr)
	{}

	bool IsValid() const
	{
		return ScriptStruct && StructMemory;
	}

	void Destroy(UScriptStruct* ActualStruct);
	void Create(UScriptStruct* ActualStruct);

	~FStructBox();

	bool Serialize(FArchive& Ar);

	bool Identical(const FStructBox* Other, uint32 PortFlags) const;

	void AddStructReferencedObjects(class FReferenceCollector& Collector) const;

	FStructBox& operator=(const FStructBox& Other);

private:
	FStructBox(const FStructBox&);
};

template<>
struct TStructOpsTypeTraits<FStructBox> : public TStructOpsTypeTraitsBase2<FStructBox>
{
	enum
	{
		WithZeroConstructor = true,
		WithCopy = true,
		WithIdentical = true,
		WithAddStructReferencedObjects = true,
		WithSerializer = true
		// TODO.. WithPostSerialize etc..
	};
};
