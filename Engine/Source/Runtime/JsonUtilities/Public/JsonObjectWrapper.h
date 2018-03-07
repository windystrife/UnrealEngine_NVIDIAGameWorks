// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"

#include "JsonObjectWrapper.generated.h"

class FJsonObject;

/** UStruct that holds a JsonObject, can be used by structs passed to JsonObjectConverter to pass through JsonObjects directly */
USTRUCT(BlueprintType)
struct JSONUTILITIES_API FJsonObjectWrapper
{
	GENERATED_USTRUCT_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "JSON")
	FString JsonString;

	TSharedPtr<FJsonObject> JsonObject;

	bool ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText);
	bool ExportTextItem(FString& ValueStr, FJsonObjectWrapper const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const;
	void PostSerialize(const FArchive& Ar);

	explicit operator bool() const
	{
		return JsonObject.IsValid();
	}
};

template<>
struct TStructOpsTypeTraits<FJsonObjectWrapper> : public TStructOpsTypeTraitsBase2<FJsonObjectWrapper>
{
	enum
	{
		WithImportTextItem = true,
		WithExportTextItem = true,
		WithPostSerialize = true,
	};
};
UCLASS()
class UJsonUtilitiesDummyObject : public UObject
{
	GENERATED_BODY()
};
