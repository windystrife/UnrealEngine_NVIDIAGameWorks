// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "ExportTextContainer.generated.h"

UCLASS(MinimalAPI)
class UExportTextContainer : public UObject
{
	GENERATED_UCLASS_BODY()

	/** ExportText representation of one or more objects */
	UPROPERTY()
	FString ExportText;
};



