// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
 
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "ConfigPropertyHelper.generated.h"

UENUM()
enum EConfigFileSourceControlStatus
{
	CFSCS_Unknown UMETA(DisplayName=Unknown),
	CFSCS_Writable UMETA(DisplayName = "Available to edit"),
	CFSCS_Locked UMETA(DisplayName = "File is locked"),
};

UCLASS(MinimalAPI)
class UPropertyConfigFileDisplayRow : public UObject
{
	GENERATED_BODY()
public:

	void InitWithConfigAndProperty(const FString& InConfigFileName, UProperty* InEditProperty);

public:
	UPROPERTY(Transient, Category = Helper, VisibleAnywhere)
	FString ConfigFileName;

	UPROPERTY(Transient, Category = Helper, EditAnywhere, meta=(EditCondition="bIsFileWritable"))
	UProperty* ExternalProperty;

	UPROPERTY(Transient, Category = Helper, VisibleAnywhere)
	bool bIsFileWritable;
};


UCLASS(MinimalAPI)
class UConfigHierarchyPropertyView : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient, Category=Helper, EditAnywhere)
	TWeakObjectPtr<UProperty> EditProperty;

	UPROPERTY(Transient, Category = Helper, EditAnywhere)
	TArray<UPropertyConfigFileDisplayRow*> ConfigFilePropertyObjects;
};
