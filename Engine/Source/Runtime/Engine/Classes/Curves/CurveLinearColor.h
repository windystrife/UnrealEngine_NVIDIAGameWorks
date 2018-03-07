// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Curves/CurveBase.h"
#include "CurveLinearColor.generated.h"


USTRUCT()
struct ENGINE_API FRuntimeCurveLinearColor
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY()
	FRichCurve ColorCurves[4];

	UPROPERTY(EditAnywhere, Category = RuntimeFloatCurve)
	class UCurveLinearColor* ExternalCurve;

	FLinearColor GetLinearColorValue(float InTime) const;
};

UCLASS(BlueprintType)
class ENGINE_API UCurveLinearColor : public UCurveBase
{
	GENERATED_UCLASS_BODY()

	/** Keyframe data, one curve for red, green, blue, and alpha */
	UPROPERTY()
	FRichCurve FloatCurves[4];

	// Begin FCurveOwnerInterface
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;
	virtual bool IsLinearColorCurve() const override { return true; }

	UFUNCTION(BlueprintCallable, Category="Math|Curves")
	virtual FLinearColor GetLinearColorValue(float InTime) const override;

	bool HasAnyAlphaKeys() const override { return FloatCurves[3].GetNumKeys() > 0; }

	virtual bool IsValidCurve( FRichCurveEditInfo CurveInfo ) override;
	// End FCurveOwnerInterface

	/** Determine if Curve is the same */
	bool operator == (const UCurveLinearColor& Curve) const;
};

