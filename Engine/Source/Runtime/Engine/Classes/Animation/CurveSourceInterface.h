// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "UObject/Interface.h"
#include "CurveSourceInterface.generated.h"

UINTERFACE()
class ENGINE_API UCurveSourceInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

/** Name/value pair for retrieving curve values */
USTRUCT(BlueprintType)
struct ENGINE_API FNamedCurveValue
{
	GENERATED_BODY()

	/** The name of the curve */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Curve")
	FName Name;

	/** The value of the curve */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Curve")
	float Value;
};

/** A source for curves */
class ENGINE_API ICurveSourceInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	/** The default binding, for clients to opt-in to */
	static const FName DefaultBinding;

	/** 
	 * Get the name that this curve source can be bound to by.
	 * Clients of this curve source will use this name to identify this source.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Curves")
	FName GetBindingName() const;

	/** Get the value for a specified curve */
	UFUNCTION(BlueprintNativeEvent, Category = "Curves")
	float GetCurveValue(FName CurveName) const;

	/** Evaluate all curves that this source provides */
	UFUNCTION(BlueprintNativeEvent, Category = "Curves")
	void GetCurves(TArray<FNamedCurveValue>& OutValues) const;
};
