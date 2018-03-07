// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialEditor/DEditorParameterValue.h"

#include "DEditorVectorParameterValue.generated.h"

UCLASS(hidecategories=Object, collapsecategories, editinlinenew)
class UNREALED_API UDEditorVectorParameterValue : public UDEditorParameterValue
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DEditorVectorParameterValue)
	FLinearColor ParameterValue;

};

