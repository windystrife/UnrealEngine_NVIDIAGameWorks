// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialEditor/DEditorParameterValue.h"

#include "DEditorFontParameterValue.generated.h"

USTRUCT()
struct UNREALED_API FDFontParameters
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = DFontParameter)
	class UFont* FontValue;

	UPROPERTY(EditAnywhere, Category = DFontParameter)
	int32 FontPage;
};

UCLASS(hidecategories = Object, collapsecategories)
class UNREALED_API UDEditorFontParameterValue : public UDEditorParameterValue
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = DEditorFontParameterValue)
	struct FDFontParameters ParameterValue;
};

