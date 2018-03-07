// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Styling/SlateBrush.h"
#include "Binding/PropertyBinding.h"
#include "BrushBinding.generated.h"

UCLASS()
class UMG_API UBrushBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UBrushBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	FSlateBrush GetValue() const;

private:
	enum class EConversion : uint8
	{
		None,
		Texture,
		//Material,
	};

	mutable TOptional<EConversion> bConversion;
};
