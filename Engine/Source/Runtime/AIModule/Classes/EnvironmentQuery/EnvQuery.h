// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Engine/DataAsset.h"
#include "EnvQuery.generated.h"

class UEdGraph;
class UEnvQueryOption;

#if WITH_EDITORONLY_DATA
class UEdGraph;
#endif // WITH_EDITORONLY_DATA

UCLASS(BlueprintType)
class AIMODULE_API UEnvQuery : public UDataAsset
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** Graph for query */
	UPROPERTY()
	UEdGraph*	EdGraph;
#endif

protected:
	friend class UEnvQueryManager;

	UPROPERTY()
	FName QueryName;

	UPROPERTY()
	TArray<UEnvQueryOption*> Options;

public:
	/** Gather all required named params */
	void CollectQueryParams(UObject& QueryOwner, TArray<FAIDynamicParam>& NamedValues) const;

	virtual  void PostInitProperties() override;

	/** QueryName patching up */
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#endif // WITH_EDITOR

	FName GetQueryName() const { return QueryName; }

	TArray<UEnvQueryOption*>& GetOptionsMutable() { return Options; }
	const TArray<UEnvQueryOption*>& GetOptions() const { return Options; }
};
