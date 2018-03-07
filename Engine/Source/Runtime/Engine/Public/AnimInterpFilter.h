// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InterpFilter.h
=============================================================================*/ 

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

struct FFIRFilter
{
public:
	FFIRFilter()
	{
	}

	FFIRFilter(int32 WindowLen)
	{
		Initialize(WindowLen);
	}

	void Initialize(int32 WindowLen)
	{
		if ( WindowLen > 0 )
		{
			FilterWindow.AddZeroed(WindowLen);
			Coefficients.AddZeroed(WindowLen);
			CurrentStack = 0;
		}
		else
		{
			FilterWindow.Reset();
			Coefficients.Reset();
			CurrentStack = 0;		
		}
	}

	void CalculateCoefficient(EFilterInterpolationType InterpolationType);
	float GetFilteredData(float Input);
	bool IsValid() const { return FilterWindow.Num() > 0; }
	float				LastOutput;
private:

	// CurrentStack is latest till CurrentStack + 1 is oldest
	// note that this works in reverse order with Coefficient
	TArray<float>		FilterWindow;
	// n-1 is latest till 0 is oldest
	TArray<float>		Coefficients;
	int32					CurrentStack;

	float GetStep() const
	{
		check( IsValid() );
		return 1.f/(float)Coefficients.Num();
	}

	float GetInterpolationCoefficient (EFilterInterpolationType InterpolationType, int32 CoefficientIndex) const;
	float CalculateFilteredOutput() const;
};


struct FFilterData
{
	float Input;
	float Time;

	FFilterData()
	{
	}
	void CheckValidation(float CurrentTime, float ValidationWindow)
	{
		if ( Diff(CurrentTime) > ValidationWindow )
		{
			Time = 0.f;
		}
	}

	bool IsValid()
	{
		return (Time > 0.f);
	}

	float Diff(float InTime)
	{
		return (InTime - Time);
	}

	void SetInput(float InData, float InTime)
	{
		Input = InData;
		Time = InTime;
	}
};

struct ENGINE_API FFIRFilterTimeBased
{
public:
	FFIRFilterTimeBased()
	{
	}

	FFIRFilterTimeBased(float Duration, EFilterInterpolationType InInterpolationType)
	{
		Initialize(Duration, InInterpolationType);
	}

	void Initialize(float WindowDuration, EFilterInterpolationType BlendType)
	{
		FilterWindow.Empty(10);
		FilterWindow.AddZeroed(10);
		InterpolationType = BlendType;
		NumValidFilter = 0;
		CurrentStackIndex = 0;
		TimeDuration = WindowDuration;
		CurrentTime = 0.f;
		LastOutput = 0.f;
	}

	float GetFilteredData(float Input, float DeltaTime);
	bool IsValid() const { return TimeDuration > 0.f; }
	float				LastOutput;

	void SetWindowDuration(float WindowDuration)
	{
		TimeDuration = WindowDuration;
	}

#if WITH_EDITOR
	bool NeedsUpdate(const EFilterInterpolationType InType, const float InTime)
	{
		return InterpolationType != InType || TimeDuration != InTime;
	}
#endif // WITH_EDITOR
private:
	TArray<FFilterData>			FilterWindow;
	EFilterInterpolationType	InterpolationType;
	int32						CurrentStackIndex;
	float						TimeDuration;
	int32						NumValidFilter;
	float						CurrentTime;

	float GetInterpolationCoefficient (FFilterData & Data);
	float CalculateFilteredOutput();
	int32 GetSafeCurrentStackIndex();
	void RefreshValidFilters();
};
