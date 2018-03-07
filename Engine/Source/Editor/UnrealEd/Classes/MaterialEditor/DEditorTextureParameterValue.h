// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialEditor/DEditorParameterValue.h"

#include "DEditorTextureParameterValue.generated.h"

UCLASS(hidecategories=Object, collapsecategories)
class UNREALED_API UDEditorTextureParameterValue : public UDEditorParameterValue
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DEditorTextureParameterValue)
	class UTexture* ParameterValue;

};

