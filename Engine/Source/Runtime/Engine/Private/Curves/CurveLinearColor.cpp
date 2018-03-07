// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UCurveLinearColor.cpp
=============================================================================*/

#include "Curves/CurveLinearColor.h"

FLinearColor FRuntimeCurveLinearColor::GetLinearColorValue(float InTime) const
{
	FLinearColor Result;

	Result.R = ColorCurves[0].Eval(InTime);
	Result.G = ColorCurves[1].Eval(InTime);
	Result.B = ColorCurves[2].Eval(InTime);

	// No alpha keys means alpha should be 1
	if (ColorCurves[3].GetNumKeys() == 0)
	{
		Result.A = 1.0f;
	}
	else
	{
		Result.A = ColorCurves[3].Eval(InTime);
	}

	return Result;
}

UCurveLinearColor::UCurveLinearColor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UCurveLinearColor::GetLinearColorValue( float InTime ) const
{
	FLinearColor Result;

	Result.R = FloatCurves[0].Eval(InTime);
	Result.G = FloatCurves[1].Eval(InTime);
	Result.B = FloatCurves[2].Eval(InTime);

	// No alpha keys means alpha should be 1
	if( FloatCurves[3].GetNumKeys() == 0 )
	{
		Result.A = 1.0f;
	}
	else
	{
		Result.A = FloatCurves[3].Eval(InTime);
	}

	return Result;
}

static const FName RedCurveName(TEXT("R"));
static const FName GreenCurveName(TEXT("G"));
static const FName BlueCurveName(TEXT("B"));
static const FName AlphaCurveName(TEXT("A"));

TArray<FRichCurveEditInfoConst> UCurveLinearColor::GetCurves() const 
{
	TArray<FRichCurveEditInfoConst> Curves;
	Curves.Add(FRichCurveEditInfoConst(&FloatCurves[0], RedCurveName));
	Curves.Add(FRichCurveEditInfoConst(&FloatCurves[1], GreenCurveName));
	Curves.Add(FRichCurveEditInfoConst(&FloatCurves[2], BlueCurveName));
	Curves.Add(FRichCurveEditInfoConst(&FloatCurves[3], AlphaCurveName));
	return Curves;
}

TArray<FRichCurveEditInfo> UCurveLinearColor::GetCurves()
{
	TArray<FRichCurveEditInfo> Curves;
	Curves.Add(FRichCurveEditInfo(&FloatCurves[0], RedCurveName));
	Curves.Add(FRichCurveEditInfo(&FloatCurves[1], GreenCurveName));
	Curves.Add(FRichCurveEditInfo(&FloatCurves[2], BlueCurveName));
	Curves.Add(FRichCurveEditInfo(&FloatCurves[3], AlphaCurveName));
	return Curves;
}

bool UCurveLinearColor::operator==( const UCurveLinearColor& Curve ) const
{
	return (FloatCurves[0] == Curve.FloatCurves[0]) && (FloatCurves[1] == Curve.FloatCurves[1]) && (FloatCurves[2] == Curve.FloatCurves[2]) && (FloatCurves[3] == Curve.FloatCurves[3]) ;
}

bool UCurveLinearColor::IsValidCurve( FRichCurveEditInfo CurveInfo )
{
	return CurveInfo.CurveToEdit == &FloatCurves[0] ||
		CurveInfo.CurveToEdit == &FloatCurves[1] ||
		CurveInfo.CurveToEdit == &FloatCurves[2] ||
		CurveInfo.CurveToEdit == &FloatCurves[3];
}

