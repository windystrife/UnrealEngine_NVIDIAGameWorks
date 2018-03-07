// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialEditor/DEditorParameterValue.h"

#include "DEditorScalarParameterValue.generated.h"

UCLASS(hidecategories=Object, collapsecategories, editinlinenew)
class UNREALED_API UDEditorScalarParameterValue : public UDEditorParameterValue
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DEditorScalarParameterValue)
	float ParameterValue;

	float SliderMin;
	float SliderMax;
};

