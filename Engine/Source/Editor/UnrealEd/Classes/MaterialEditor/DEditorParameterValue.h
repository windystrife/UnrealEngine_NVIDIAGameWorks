// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"

#include "DEditorParameterValue.generated.h"

UCLASS(hidecategories=Object, collapsecategories, editinlinenew)
class UNREALED_API UDEditorParameterValue : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DEditorParameterValue)
	uint32 bOverride:1;

	UPROPERTY(EditAnywhere, Category=DEditorParameterValue)
	FName ParameterName;

	UPROPERTY()
	FGuid ExpressionId;

#if WITH_EDITORONLY_DATA
	/** Controls where this parameter is displayed in a material instance parameter list.  The lower the number the higher up in the parameter list. */
	UPROPERTY()
	int32 SortPriority;
#endif
};

