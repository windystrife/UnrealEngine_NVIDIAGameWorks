// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "LiveLinkRefSkeleton.h"
#include "LiveLinkTypes.generated.h"

USTRUCT()
struct FLiveLinkCurveElement
{
public:
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName CurveName;
	
	UPROPERTY()
	float CurveValue;
};

struct FOptionalCurveElement
{
	/** Curve Value */
	float					Value;
	/** Whether this value is set or not */
	bool					bValid;

	FOptionalCurveElement(float InValue)
		: Value(InValue)
		, bValid(true)
	{}

	FOptionalCurveElement()
		: Value(0.f)
		, bValid(false)
	{}

	bool IsValid() const
	{
		return bValid;
	}

	void SetValue(float InValue)
	{
		Value = InValue;
		bValid = true;
	}
};

//Helper struct for updating curve data across multiple frames of live link data
struct FLiveLinkCurveIntegrationData
{
public:

	// Number of new curves that need to be added to existing frames
	int32 NumNewCurves;

	// Built curve buffer for current frame in existing curve key format
	TArray<FOptionalCurveElement> CurveValues;
};

struct FLiveLinkCurveKey
{
	TArray<FName> CurveNames;

	FLiveLinkCurveIntegrationData UpdateCurveKey(const TArray<FLiveLinkCurveElement>& CurveElements);
};

struct FLiveLinkTimeCode
{
	// Time for this frame. Used during interpolation. If this goes backwards we will dump already stored frames. 
	double Time;

	// Frame number for this data.
	int32 FrameNum;

	// Value calculated on create to represent the different between the source time and client time
	double Offset;
};

struct FLiveLinkSubjectFrame
{
	// Ref Skeleton for transforms
	FLiveLinkRefSkeleton RefSkeleton;

	// Key for storing curve data (Names)
	FLiveLinkCurveKey	 CurveKeyData;

	// Transforms for this frame
	TArray<FTransform>		Transforms;
	
	// Curve data for this frame
	TArray<FOptionalCurveElement>	Curves;
};