// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Styling/SlateColor.h"
#include "Binding/PropertyBinding.h"
#include "ColorBinding.generated.h"

UCLASS()
class UMG_API UColorBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UColorBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	virtual void Bind(UProperty* Property, FScriptDelegate* Delegate) override;

	UFUNCTION()
	FSlateColor GetSlateValue() const;

	UFUNCTION()
	FLinearColor GetLinearValue() const;

private:
	mutable TOptional<bool> bNeedsConversion;
};
