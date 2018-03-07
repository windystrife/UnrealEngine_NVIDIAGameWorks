// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Binding/PropertyBinding.h"

#include "VisibilityBinding.generated.h"

enum class ESlateVisibility : uint8;

UCLASS()
class UMG_API UVisibilityBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UVisibilityBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	ESlateVisibility GetValue() const;
};
