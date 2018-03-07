// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialEditor/DEditorParameterValue.h"

#include "DEditorStaticComponentMaskParameterValue.generated.h"

USTRUCT()
struct UNREALED_API FDComponentMaskParameter
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=DComponentMaskParameter)
	uint32 R:1;

	UPROPERTY(EditAnywhere, Category=DComponentMaskParameter)
	uint32 G:1;

	UPROPERTY(EditAnywhere, Category=DComponentMaskParameter)
	uint32 B:1;

	UPROPERTY(EditAnywhere, Category=DComponentMaskParameter)
	uint32 A:1;



		/** Constructor */
		FDComponentMaskParameter(bool InR, bool InG, bool InB, bool InA) :
			R(InR),
			G(InG),
			B(InB),
			A(InA)
		{
		};
		FDComponentMaskParameter() :
			R(false),
			G(false),
			B(false),
			A(false)
		{
		};
	
};

UCLASS(hidecategories=Object, collapsecategories)
class UNREALED_API UDEditorStaticComponentMaskParameterValue : public UDEditorParameterValue
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=DEditorStaticComponentMaskParameterValue)
	struct FDComponentMaskParameter ParameterValue;

};

