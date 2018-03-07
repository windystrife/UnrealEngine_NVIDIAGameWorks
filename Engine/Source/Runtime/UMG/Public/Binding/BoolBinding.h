// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Binding/PropertyBinding.h"
#include "BoolBinding.generated.h"

UCLASS()
class UMG_API UBoolBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UBoolBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	bool GetValue() const;
};
