// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Binding/PropertyBinding.h"
#include "WidgetBinding.generated.h"

class UWidget;

UCLASS()
class UMG_API UWidgetBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UWidgetBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	UWidget* GetValue() const;
};
