// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "GenericPlatform/ICursor.h"
#include "Binding/PropertyBinding.h"
#include "MouseCursorBinding.generated.h"

UCLASS()
class UMG_API UMouseCursorBinding : public UPropertyBinding
{
	GENERATED_BODY()

public:

	UMouseCursorBinding();

	virtual bool IsSupportedSource(UProperty* Property) const override;
	virtual bool IsSupportedDestination(UProperty* Property) const override;

	UFUNCTION()
	EMouseCursor::Type GetValue() const;
};
