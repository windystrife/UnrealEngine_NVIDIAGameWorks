// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialEditor/DEditorParameterValue.h"

#include "DEditorStaticSwitchParameterValue.generated.h"

UCLASS(hidecategories=Object, collapsecategories)
class UNREALED_API UDEditorStaticSwitchParameterValue : public UDEditorParameterValue
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DEditorStaticSwitchParameterValue)
	uint32 ParameterValue:1;

};

