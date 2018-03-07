// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InterpFilter.h
=============================================================================*/ 

#include "AnimInterpFilter.h"

////////////////////////////
// FFIRFilter

float FFIRFilter::GetInterpolationCoefficient (EFilterInterpolationType InterpolationType, int32 CoefficientIndex) const
{
	float Step = GetStep();

	switch (InterpolationType)
	{
	case BSIT_Average:
		return Step;	
	case BSIT_Linear:
		return Step*CoefficientIndex;
	case BSIT_Cubic:
		return Step*Step*Step*CoefficientIndex;
	}

	return 0.f;
}

void FFIRFilter::CalculateCoefficient(EFilterInterpolationType InterpolationType)
{
	if ( IsValid() )
	{
		float Sum=0.f;
		for (int32 I=0; I<Coefficients.Num(); ++I)
		{
			Coefficients[I] = GetInterpolationCoefficient(InterpolationType, I);
			Sum += Coefficients[I];
		}

		// now normalize it, if not 1
		if ( fabs(Sum-1.f) > ZERO_ANIMWEIGHT_THRESH )
		{
			for (int32 I=0; I<Coefficients.Num(); ++I)
			{
				Coefficients[I]/=Sum;
			}
		}
	}
}

float FFIRFilter::GetFilteredData(float Input)
{
	if ( IsValid() )
	{
		FilterWindow[CurrentStack] = Input;
		float Result = CalculateFilteredOutput();

		CurrentStack = CurrentStack+1;
		if ( CurrentStack > FilterWindow.Num()-1 ) 
		{
			CurrentStack = 0;
		}

		LastOutput = Result;
		return Result;
	}

	LastOutput = Input;
	return Input;
}

float FFIRFilter::CalculateFilteredOutput() const
{
	float Output = 0.f;
	int32 StackIndex = CurrentStack;

	for ( int32 I=Coefficients.Num()-1; I>=0; --I )
	{
		Output += FilterWindow[StackIndex]*Coefficients[I];
		StackIndex = StackIndex-1;
		if (StackIndex < 0)
		{
			StackIndex = FilterWindow.Num()-1;
		}
	}

	return Output;
}

int32 FFIRFilterTimeBased::GetSafeCurrentStackIndex()
{
	// if valid range
	check ( CurrentStackIndex < FilterWindow.Num() );

	// see if it's expired yet
	if ( !FilterWindow[CurrentStackIndex].IsValid() )
	{
		return CurrentStackIndex;
	}

	// else see any other index is available
	// when you do this, go to forward, (oldest)
	// this should not be the case because most of times
	// current one should be the oldest one, but since 
	// we jumps when reallocation happens, we still do this
	for (int32 I=0; I<FilterWindow.Num(); ++I)
	{
		int32 NewIndex = CurrentStackIndex + I;
		if (NewIndex >= FilterWindow.Num())
		{
			NewIndex = NewIndex - FilterWindow.Num();
		}

		if ( !FilterWindow[NewIndex].IsValid() )
		{
			return NewIndex;
		}
	}

	// if current one isn't available anymore 
	// that means we need more stack
	int32 NewIndex = FilterWindow.Num();
	FilterWindow.AddZeroed(5);
	return NewIndex;
}

void FFIRFilterTimeBased::RefreshValidFilters()
{
	NumValidFilter = 0;

	if ( TimeDuration > 0.f )
	{
		// run validation test
		for (int32 I=0; I<FilterWindow.Num(); ++I)
		{
			FilterWindow[I].CheckValidation(CurrentTime, TimeDuration);
			if ( FilterWindow[I].IsValid() )
			{
				++NumValidFilter;
			}
		}
	}
}

float FFIRFilterTimeBased::GetFilteredData(float Input, float DeltaTime)
{
	float Result;

	CurrentTime += DeltaTime;

	if ( IsValid() )
	{
		RefreshValidFilters();

		CurrentStackIndex = GetSafeCurrentStackIndex();
		FilterWindow[CurrentStackIndex].SetInput(Input, CurrentTime);
		Result = CalculateFilteredOutput();
		CurrentStackIndex = CurrentStackIndex+1;
		if ( CurrentStackIndex > FilterWindow.Num()-1 ) 
		{
			CurrentStackIndex = 0;
		}
	}
	else
	{
		Result = Input;
	}

	LastOutput = Result;
	return Result;
}

float FFIRFilterTimeBased::GetInterpolationCoefficient (FFilterData & Data)
{
	if (Data.IsValid())
	{
		float Diff = Data.Diff(CurrentTime);
		if (Diff<=TimeDuration)
		{
			switch(InterpolationType)
			{
			case BSIT_Average:
				return 1.f;
			case BSIT_Linear:
				return 1.f - Diff/TimeDuration;
			case BSIT_Cubic:
				return 1.f - Diff*Diff*Diff/TimeDuration;
			}
		}
	}

	return 0.f;
}

float FFIRFilterTimeBased::CalculateFilteredOutput()
{
	check ( IsValid() );
	float SumCoefficient = 0.f;
	float SumInputs = 0.f;
	int32 NumValidSamples = 0;
	for (int32 I=0; I<FilterWindow.Num(); ++I)
	{
		float Coefficient = GetInterpolationCoefficient(FilterWindow[I]);
		if (Coefficient > 0.f)
		{
			SumCoefficient += Coefficient;
			SumInputs += Coefficient * FilterWindow[I].Input;
			++NumValidSamples;
		}
	}

	// if number of samples are 1, do not normalize
// 	if (NumValidSamples == 1)
// 	{
// 		return SumInputs;
// 	}

	return (SumCoefficient>0.f)?SumInputs/SumCoefficient : 0.f;
}

