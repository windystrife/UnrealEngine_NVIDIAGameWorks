// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InterpolationCurveEd.cpp: Implementation of CurveEdInterface for various track types.
=============================================================================*/

#include "CoreMinimal.h"
#include "Matinee/InterpTrackMove.h"
#include "Matinee/InterpTrackFloatBase.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "Matinee/InterpTrackVectorBase.h"
#include "Matinee/InterpTrackLinearColorBase.h"

/*-----------------------------------------------------------------------------
 UInterpTrackMove
-----------------------------------------------------------------------------*/

int32 UInterpTrackMove::CalcSubIndex(bool bPos, int32 InIndex) const
{
	if(bPos)
	{
		if(bShowTranslationOnCurveEd)
		{
			return InIndex;
		}
		else
		{
			return INDEX_NONE;
		}
	}
	else
	{
		// Only allow showing rotation curve if not using Quaternion interpolation method.
		if(bShowRotationOnCurveEd && !bUseQuatInterpolation)
		{
			if(bShowTranslationOnCurveEd)
			{
				return InIndex + 3;
			}
			else
			{
				return InIndex;
			}
		}
	}
	return INDEX_NONE;
}


int32 UInterpTrackMove::GetNumKeys() const
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	return PosTrack.Points.Num();
}

int32 UInterpTrackMove::GetNumSubCurves() const
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );

	int32 NumSubs = 0;

	if(bShowTranslationOnCurveEd)
	{
		NumSubs += 3;
	}

	if(bShowRotationOnCurveEd && !bUseQuatInterpolation)
	{
		NumSubs += 3;
	}

	return NumSubs;
}


FColor UInterpTrackMove::GetSubCurveButtonColor(int32 SubCurveIndex, bool bIsSubCurveHidden) const
{
	// Check for array out of bounds because it will crash the program
	check(SubCurveIndex >= 0);
	check(SubCurveIndex < GetNumSubCurves());

	FColor ButtonColor;

	switch(SubCurveIndex)
	{
	case 0:
		// Red
		ButtonColor = bIsSubCurveHidden ? FColor(32, 0, 0) : FColor::Red;
		break;
	case 1:
		// Green
		ButtonColor = bIsSubCurveHidden ? FColor(0, 32, 0) : FColor::Green;
		break;
	case 2:
		// Blue
		ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 32) : FColor::Blue;
		break;
	case 3:
		// Dark red
		ButtonColor = bIsSubCurveHidden ? FColor(28, 0, 0) : FColor(196, 0, 0);
		break;
	case 4:
		// Dark green
		ButtonColor = bIsSubCurveHidden ? FColor(0, 28, 0) : FColor(0, 196, 0);
		break;
	case 5:
		// Dark blue
		ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 28) : FColor(0, 0, 196);
		break;
	default:
		// A bad sub-curve index was given. 
		check(false);
		break;
	}

	return ButtonColor;
}

float UInterpTrackMove::GetKeyIn(int32 KeyIndex)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );
	return PosTrack.Points[KeyIndex].InVal;
}

float UInterpTrackMove::GetKeyOut(int32 SubIndex, int32 KeyIndex)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );
	
	if(SubIndex == CalcSubIndex(true,0))
		return PosTrack.Points[KeyIndex].OutVal.X;
	else if(SubIndex == CalcSubIndex(true,1))
		return PosTrack.Points[KeyIndex].OutVal.Y;
	else if(SubIndex == CalcSubIndex(true,2))
		return PosTrack.Points[KeyIndex].OutVal.Z;
	else if(SubIndex == CalcSubIndex(false,0))
		return EulerTrack.Points[KeyIndex].OutVal.X;
	else if(SubIndex == CalcSubIndex(false,1))
		return EulerTrack.Points[KeyIndex].OutVal.Y;
	else if(SubIndex == CalcSubIndex(false,2))
		return EulerTrack.Points[KeyIndex].OutVal.Z;
	else
	{
		check(0);
		return 0.f;
	}
}

void UInterpTrackMove::GetInRange(float& MinIn, float& MaxIn) const
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	if( PosTrack.Points.Num() == 0 )
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		MinIn = PosTrack.Points[ 0 ].InVal;
		MaxIn = PosTrack.Points[ PosTrack.Points.Num()-1 ].InVal;
	}
}

void UInterpTrackMove::GetOutRange(float& MinOut, float& MaxOut) const
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	FVector PosMinVec, PosMaxVec;
	PosTrack.CalcBounds(PosMinVec, PosMaxVec, FVector::ZeroVector);

	FVector EulerMinVec, EulerMaxVec;
	EulerTrack.CalcBounds(EulerMinVec, EulerMaxVec, FVector::ZeroVector);

	// Only output bounds for curve currently being displayed.
	if(bShowTranslationOnCurveEd)
	{
		if(bShowRotationOnCurveEd && !bUseQuatInterpolation)
		{
			MinOut = FMath::Min( PosMinVec.GetMin(), EulerMinVec.GetMin() );
			MaxOut = FMath::Max( PosMaxVec.GetMax(), EulerMaxVec.GetMax() );
		}
		else
		{
			MinOut = PosMinVec.GetMin();
			MaxOut = PosMaxVec.GetMax();
		}
	}
	else
	{
		if(bShowRotationOnCurveEd && !bUseQuatInterpolation)
		{
			MinOut = EulerMinVec.GetMin();
			MaxOut = EulerMaxVec.GetMax();
		}
		else
		{
			MinOut = 0.f;
			MaxOut = 0.f;
		}
	}
}


FColor UInterpTrackMove::GetKeyColor(int32 SubIndex, int32 KeyIndex, const FColor& CurveColor)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	if(SubIndex == CalcSubIndex(true,0))
		return FColor::Red;
	else if(SubIndex == CalcSubIndex(true,1))
		return FColor::Green;
	else if(SubIndex == CalcSubIndex(true,2))
		return FColor::Blue;
	else if(SubIndex == CalcSubIndex(false,0))
		return FColor(255,128,128);
	else if(SubIndex == CalcSubIndex(false,1))
		return FColor(128,255,128);
	else if(SubIndex == CalcSubIndex(false,2))
		return FColor(128,128,255);
	else
	{
		check(0);
		return FColor::Black;
	}
}

EInterpCurveMode UInterpTrackMove::GetKeyInterpMode(int32 KeyIndex) const
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );
	check( PosTrack.Points[KeyIndex].InterpMode == EulerTrack.Points[KeyIndex].InterpMode );

	return PosTrack.Points[KeyIndex].InterpMode;
}

void UInterpTrackMove::GetTangents(int32 SubIndex, int32 KeyIndex, float& ArriveTangent, float& LeaveTangent) const
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	if(SubIndex == CalcSubIndex(true,0))
	{
		ArriveTangent = PosTrack.Points[KeyIndex].ArriveTangent.X;
		LeaveTangent = PosTrack.Points[KeyIndex].LeaveTangent.X;
	}
	else if(SubIndex == CalcSubIndex(true,1))
	{
		ArriveTangent = PosTrack.Points[KeyIndex].ArriveTangent.Y;
		LeaveTangent = PosTrack.Points[KeyIndex].LeaveTangent.Y;
	}
	else if(SubIndex == CalcSubIndex(true,2))
	{
		ArriveTangent = PosTrack.Points[KeyIndex].ArriveTangent.Z;
		LeaveTangent = PosTrack.Points[KeyIndex].LeaveTangent.Z;
	}
	else if(SubIndex == CalcSubIndex(false,0))
	{
		ArriveTangent = EulerTrack.Points[KeyIndex].ArriveTangent.X;
		LeaveTangent = EulerTrack.Points[KeyIndex].LeaveTangent.X;
	}
	else if(SubIndex == CalcSubIndex(false,1))
	{
		ArriveTangent = EulerTrack.Points[KeyIndex].ArriveTangent.Y;
		LeaveTangent = EulerTrack.Points[KeyIndex].LeaveTangent.Y;
	}
	else if(SubIndex == CalcSubIndex(false,2))
	{
		ArriveTangent = EulerTrack.Points[KeyIndex].ArriveTangent.Z;
		LeaveTangent = EulerTrack.Points[KeyIndex].LeaveTangent.Z;
	}
	else
	{
		check(0);
	}
}

float UInterpTrackMove::EvalSub(int32 SubIndex, float InVal)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);

	FVector OutPos = PosTrack.Eval(InVal, FVector::ZeroVector);
	FVector OutEuler = EulerTrack.Eval(InVal, FVector::ZeroVector);

	if(SubIndex == CalcSubIndex(true,0))
		return OutPos.X;
	else if(SubIndex == CalcSubIndex(true,1))
		return OutPos.Y;
	else if(SubIndex == CalcSubIndex(true,2))
		return OutPos.Z;
	else if(SubIndex == CalcSubIndex(false,0))
		return OutEuler.X;
	else if(SubIndex == CalcSubIndex(false,1))
		return OutEuler.Y;
	else if(SubIndex == CalcSubIndex(false,2))
		return OutEuler.Z;
	else
	{
		check(0);
		return 0.f;
	}
}


int32 UInterpTrackMove::CreateNewKey(float KeyIn)
{	
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );

	FVector NewKeyPos = PosTrack.Eval(KeyIn, FVector::ZeroVector);
	int32 NewPosIndex = PosTrack.AddPoint(KeyIn, NewKeyPos);
	PosTrack.AutoSetTangents(LinCurveTension);

	FVector NewKeyEuler = EulerTrack.Eval(KeyIn, FVector::ZeroVector);
	int32 NewEulerIndex = EulerTrack.AddPoint(KeyIn, NewKeyEuler);
	EulerTrack.AutoSetTangents(AngCurveTension);

	FName DefaultName(NAME_None);
	int32 NewLookupKeyIndex = LookupTrack.AddPoint(KeyIn, DefaultName);

	check((NewPosIndex == NewEulerIndex) && (NewEulerIndex == NewLookupKeyIndex));

	return NewPosIndex;
}

void UInterpTrackMove::DeleteKey(int32 KeyIndex)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	PosTrack.Points.RemoveAt(KeyIndex);
	PosTrack.AutoSetTangents(LinCurveTension);

	EulerTrack.Points.RemoveAt(KeyIndex);
	EulerTrack.AutoSetTangents(AngCurveTension);

	LookupTrack.Points.RemoveAt(KeyIndex);
}

int32 UInterpTrackMove::SetKeyIn(int32 KeyIndex, float NewInVal)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	int32 NewPosIndex = PosTrack.MovePoint(KeyIndex, NewInVal);
	PosTrack.AutoSetTangents(LinCurveTension);

	int32 NewEulerIndex = EulerTrack.MovePoint(KeyIndex, NewInVal);
	EulerTrack.AutoSetTangents(AngCurveTension);

	int32 NewLookupKeyIndex = LookupTrack.MovePoint(KeyIndex, NewInVal);

	check((NewPosIndex == NewEulerIndex) && (NewEulerIndex == NewLookupKeyIndex));

	return NewPosIndex;
}

void UInterpTrackMove::SetKeyOut(int32 SubIndex, int32 KeyIndex, float NewOutVal) 
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	if(SubIndex == CalcSubIndex(true,0))
		PosTrack.Points[KeyIndex].OutVal.X = NewOutVal;
	else if(SubIndex == CalcSubIndex(true,1))
		PosTrack.Points[KeyIndex].OutVal.Y = NewOutVal;
	else if(SubIndex == CalcSubIndex(true,2))
		PosTrack.Points[KeyIndex].OutVal.Z = NewOutVal;
	else if(SubIndex == CalcSubIndex(false,0))
		EulerTrack.Points[KeyIndex].OutVal.X = NewOutVal;
	else if(SubIndex == CalcSubIndex(false,1))
		EulerTrack.Points[KeyIndex].OutVal.Y = NewOutVal;
	else  if(SubIndex == CalcSubIndex(false,2))
		EulerTrack.Points[KeyIndex].OutVal.Z = NewOutVal;
	else
		check(0);

	PosTrack.AutoSetTangents(LinCurveTension);
	EulerTrack.AutoSetTangents(AngCurveTension);
}

void UInterpTrackMove::SetKeyInterpMode(int32 KeyIndex, EInterpCurveMode NewMode) 
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );
	
	PosTrack.Points[KeyIndex].InterpMode = NewMode;
	PosTrack.AutoSetTangents(LinCurveTension);

	EulerTrack.Points[KeyIndex].InterpMode = NewMode;
	EulerTrack.AutoSetTangents(AngCurveTension);
}

void UInterpTrackMove::SetTangents(int32 SubIndex, int32 KeyIndex, float ArriveTangent, float LeaveTangent)
{
	check( PosTrack.Points.Num() == EulerTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < 6);
	check( KeyIndex >= 0 && KeyIndex < PosTrack.Points.Num() );

	if(SubIndex == CalcSubIndex(true,0))
	{
		PosTrack.Points[KeyIndex].ArriveTangent.X = ArriveTangent;
		PosTrack.Points[KeyIndex].LeaveTangent.X = LeaveTangent;
	}
	else if(SubIndex == CalcSubIndex(true,1))
	{
		PosTrack.Points[KeyIndex].ArriveTangent.Y = ArriveTangent;
		PosTrack.Points[KeyIndex].LeaveTangent.Y = LeaveTangent;
	}
	else if(SubIndex == CalcSubIndex(true,2))
	{
		PosTrack.Points[KeyIndex].ArriveTangent.Z = ArriveTangent;
		PosTrack.Points[KeyIndex].LeaveTangent.Z = LeaveTangent;
	}
	else if(SubIndex == CalcSubIndex(false,0))
	{
		EulerTrack.Points[KeyIndex].ArriveTangent.X = ArriveTangent;
		EulerTrack.Points[KeyIndex].LeaveTangent.X = LeaveTangent;
	}	
	else if(SubIndex == CalcSubIndex(false,1))
	{
		EulerTrack.Points[KeyIndex].ArriveTangent.Y = ArriveTangent;
		EulerTrack.Points[KeyIndex].LeaveTangent.Y = LeaveTangent;
	}	
	else if(SubIndex == CalcSubIndex(false,2))
	{
		EulerTrack.Points[KeyIndex].ArriveTangent.Z = ArriveTangent;
		EulerTrack.Points[KeyIndex].LeaveTangent.Z = LeaveTangent;
	}
	else
	{
		check(0);
	}
}


/*-----------------------------------------------------------------------------
	UInterpTrackMoveAxis
-----------------------------------------------------------------------------*/

FColor UInterpTrackMoveAxis::GetSubCurveButtonColor( int32 SubCurveIndex, bool bIsSubCurveHidden ) const
{
	check( SubCurveIndex >= 0 && SubCurveIndex < GetNumSubCurves() );

	// Determine the color based on what axis they represent.
	// X = Red, Y = Green, Z = Blue.
	FColor ButtonColor;
	switch( MoveAxis )
	{
	case AXIS_TranslationX:
	case AXIS_RotationX:
		ButtonColor = bIsSubCurveHidden ? FColor(32,0,0) : FColor::Red;
		break;
	case AXIS_TranslationY:
	case AXIS_RotationY:
		ButtonColor = bIsSubCurveHidden ? FColor(0,32,0) : FColor::Green;
		break;
	case AXIS_TranslationZ:
	case AXIS_RotationZ:
		ButtonColor = bIsSubCurveHidden ? FColor(0,0,32) : FColor::Blue;
		break;
	default:
		checkf(false, TEXT("Invalid axis") );
	}

	return ButtonColor;
}

FColor UInterpTrackMoveAxis::GetKeyColor( int32 SubIndex, int32 KeyIndex, const FColor& CurveColor )
{
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	check( SubIndex >= 0 && SubIndex < GetNumSubCurves() );

	// Determine the color based on what axis they represent.
	// X = Red, Y = Green, Z = Blue.
	FColor KeyColor;
	switch( MoveAxis )
	{
	case AXIS_TranslationX:
		KeyColor = FColor::Red;
		break;
	case AXIS_TranslationY:
		KeyColor = FColor::Green;
		break;
	case AXIS_TranslationZ:
		KeyColor = FColor::Blue;
		break;
	case AXIS_RotationX:
		KeyColor = FColor(255,128,128);
		break;
	case AXIS_RotationY:
		KeyColor = FColor(128,255,128);
		break;
	case AXIS_RotationZ:
		KeyColor = FColor(128,128,255);
		break;
	default:
		checkf(false, TEXT("Invalid axis") );
	}

	return KeyColor;
}

int32 UInterpTrackMoveAxis::CreateNewKey(float KeyIn)
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	
	int32 NewKeyIndex = Super::CreateNewKey( KeyIn );

	FName DefaultName(NAME_None);
	int32 NewLookupKeyIndex = LookupTrack.AddPoint( KeyIn, DefaultName );
	check( NewKeyIndex == NewLookupKeyIndex );

	return NewKeyIndex;
}

void UInterpTrackMoveAxis::DeleteKey( int32 KeyIndex )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );

	Super::DeleteKey( KeyIndex );

	LookupTrack.Points.RemoveAt(KeyIndex);
}

int32 UInterpTrackMoveAxis::SetKeyIn( int32 KeyIndex, float NewInVal )
{
	check( FloatTrack.Points.Num() == LookupTrack.Points.Num() );
	
	int32 NewIndex = Super::SetKeyIn( KeyIndex, NewInVal );

	int32 NewLookupKeyIndex = LookupTrack.MovePoint( KeyIndex, NewInVal );

	check( NewIndex == NewLookupKeyIndex );

	return NewIndex;
}

#if WITH_EDITORONLY_DATA
UTexture2D* UInterpTrackMoveAxis::GetTrackIcon() const
{
	return NULL;
}
#endif // WITH_EDITORONLY_DATA

/*-----------------------------------------------------------------------------
	UInterpTrackFloatBase
-----------------------------------------------------------------------------*/

int32 UInterpTrackFloatBase::GetNumKeys() const
{
	return FloatTrack.Points.Num();
}

int32 UInterpTrackFloatBase::GetNumSubCurves() const
{
	return 1;
}

float UInterpTrackFloatBase::GetKeyIn(int32 KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	return FloatTrack.Points[KeyIndex].InVal;
}

float UInterpTrackFloatBase::GetKeyOut(int32 SubIndex, int32 KeyIndex)
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	return FloatTrack.Points[KeyIndex].OutVal;
}

void UInterpTrackFloatBase::GetInRange(float& MinIn, float& MaxIn) const
{
	if(FloatTrack.Points.Num() == 0)
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		MinIn = FloatTrack.Points[ 0 ].InVal;
		MaxIn = FloatTrack.Points[ FloatTrack.Points.Num()-1 ].InVal;
	}
}

void UInterpTrackFloatBase::GetOutRange(float& MinOut, float& MaxOut) const
{
	FloatTrack.CalcBounds(MinOut, MaxOut, 0.f);
}

EInterpCurveMode UInterpTrackFloatBase::GetKeyInterpMode(int32 KeyIndex) const
{
	checkf( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num(), TEXT("Key index out of range %d"), KeyIndex );
	return FloatTrack.Points[KeyIndex].InterpMode;
}

void UInterpTrackFloatBase::GetTangents(int32 SubIndex, int32 KeyIndex, float& ArriveTangent, float& LeaveTangent) const
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	ArriveTangent = FloatTrack.Points[KeyIndex].ArriveTangent;
	LeaveTangent = FloatTrack.Points[KeyIndex].LeaveTangent;
}

float UInterpTrackFloatBase::EvalSub(int32 SubIndex, float InVal)
{
	check(SubIndex == 0);
	return FloatTrack.Eval(InVal, 0.f);
}

int32 UInterpTrackFloatBase::CreateNewKey(float KeyIn)
{
	float NewKeyOut = FloatTrack.Eval(KeyIn, 0.f);
	int32 NewPointIndex = FloatTrack.AddPoint(KeyIn, NewKeyOut);
	FloatTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackFloatBase::DeleteKey(int32 KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	FloatTrack.Points.RemoveAt(KeyIndex);
	FloatTrack.AutoSetTangents(CurveTension);
}

int32 UInterpTrackFloatBase::SetKeyIn(int32 KeyIndex, float NewInVal)
{
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	int32 NewPointIndex = FloatTrack.MovePoint(KeyIndex, NewInVal);
	FloatTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackFloatBase::SetKeyOut(int32 SubIndex, int32 KeyIndex, float NewOutVal) 
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	FloatTrack.Points[KeyIndex].OutVal = NewOutVal;
	FloatTrack.AutoSetTangents(CurveTension);
}

void UInterpTrackFloatBase::SetKeyInterpMode(int32 KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	FloatTrack.Points[KeyIndex].InterpMode = NewMode;
	FloatTrack.AutoSetTangents(CurveTension);
}

void UInterpTrackFloatBase::SetTangents(int32 SubIndex, int32 KeyIndex, float ArriveTangent, float LeaveTangent)
{
	check( SubIndex == 0 );
	check( KeyIndex >= 0 && KeyIndex < FloatTrack.Points.Num() );
	FloatTrack.Points[KeyIndex].ArriveTangent = ArriveTangent;
	FloatTrack.Points[KeyIndex].LeaveTangent = LeaveTangent;
}


/*-----------------------------------------------------------------------------
	UInterpTrackVectorBase
-----------------------------------------------------------------------------*/

int32 UInterpTrackVectorBase::GetNumKeys() const
{
	return VectorTrack.Points.Num();
}

int32 UInterpTrackVectorBase::GetNumSubCurves() const
{
	return 3;
}

FColor UInterpTrackVectorBase::GetSubCurveButtonColor(int32 SubCurveIndex, bool bIsSubCurveHidden) const
{
	// Check for array out of bounds because it will crash the program
	check(SubCurveIndex >= 0);
	check(SubCurveIndex < GetNumSubCurves());

	FColor ButtonColor;

	switch(SubCurveIndex)
	{
	case 0:
		// Red
		ButtonColor = bIsSubCurveHidden ? FColor(32, 0, 0) : FColor::Red;
		break;
	case 1:
		// Green
		ButtonColor = bIsSubCurveHidden ? FColor(0, 32,  0) : FColor::Green;
		break;
	case 2:
		// Blue
		ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 32) : FColor::Blue;
		break;
	default:
		// A bad sub-curve index was given. 
		check(false);
		break;
	}

	return ButtonColor;
}

float UInterpTrackVectorBase::GetKeyIn(int32 KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );
	return VectorTrack.Points[KeyIndex].InVal;
}

float UInterpTrackVectorBase::GetKeyOut(int32 SubIndex, int32 KeyIndex)
{
	check( SubIndex >= 0 && SubIndex < 3 );
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );

	if(SubIndex == 0)
		return VectorTrack.Points[KeyIndex].OutVal.X;
	else if(SubIndex == 1)
		return VectorTrack.Points[KeyIndex].OutVal.Y;
	else
		return VectorTrack.Points[KeyIndex].OutVal.Z;
}

void UInterpTrackVectorBase::GetInRange(float& MinIn, float& MaxIn) const
{
	if(VectorTrack.Points.Num() == 0)
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		MinIn = VectorTrack.Points[ 0 ].InVal;
		MaxIn = VectorTrack.Points[ VectorTrack.Points.Num()-1 ].InVal;
	}
}

void UInterpTrackVectorBase::GetOutRange(float& MinOut, float& MaxOut) const
{
	FVector MinVec, MaxVec;
	VectorTrack.CalcBounds(MinVec, MaxVec, FVector::ZeroVector);

	MinOut = MinVec.GetMin();
	MaxOut = MaxVec.GetMax();
}

FColor UInterpTrackVectorBase::GetKeyColor(int32 SubIndex, int32 KeyIndex, const FColor& CurveColor)
{
	check( SubIndex >= 0 && SubIndex < 3);
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );

	if (SubIndex == 0)
	{
		return FColor::Red;
	}
	else if (SubIndex == 1)
	{
		return FColor::Green;
	}
	else
	{
		return FColor::Blue;
	}
}

EInterpCurveMode UInterpTrackVectorBase::GetKeyInterpMode(int32 KeyIndex) const
{
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );
	return VectorTrack.Points[KeyIndex].InterpMode;
}

void UInterpTrackVectorBase::GetTangents(int32 SubIndex, int32 KeyIndex, float& ArriveTangent, float& LeaveTangent) const
{
	check( SubIndex >= 0 && SubIndex < 3 );
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );

	if(SubIndex == 0)
	{
		ArriveTangent = VectorTrack.Points[KeyIndex].ArriveTangent.X;
		LeaveTangent = VectorTrack.Points[KeyIndex].LeaveTangent.X;
	}
	else if(SubIndex == 1)
	{
		ArriveTangent = VectorTrack.Points[KeyIndex].ArriveTangent.Y;
		LeaveTangent = VectorTrack.Points[KeyIndex].LeaveTangent.Y;
	}
	else if(SubIndex == 2)
	{
		ArriveTangent = VectorTrack.Points[KeyIndex].ArriveTangent.Z;
		LeaveTangent = VectorTrack.Points[KeyIndex].LeaveTangent.Z;
	}
}

float UInterpTrackVectorBase::EvalSub(int32 SubIndex, float InVal)
{
	check( SubIndex >= 0 && SubIndex < 3 );

	FVector OutVal = VectorTrack.Eval(InVal, FVector::ZeroVector);

	if(SubIndex == 0)
		return OutVal.X;
	else if(SubIndex == 1)
		return OutVal.Y;
	else
		return OutVal.Z;
}

int32 UInterpTrackVectorBase::CreateNewKey(float KeyIn)
{
	FVector NewKeyOut = VectorTrack.Eval(KeyIn, FVector::ZeroVector);
	int32 NewPointIndex = VectorTrack.AddPoint(KeyIn, NewKeyOut);
	VectorTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackVectorBase::DeleteKey(int32 KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );
	VectorTrack.Points.RemoveAt(KeyIndex);
	VectorTrack.AutoSetTangents(CurveTension);
}

int32 UInterpTrackVectorBase::SetKeyIn(int32 KeyIndex, float NewInVal)
{
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );
	int32 NewPointIndex = VectorTrack.MovePoint(KeyIndex, NewInVal);
	VectorTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackVectorBase::SetKeyOut(int32 SubIndex, int32 KeyIndex, float NewOutVal) 
{
	check( SubIndex >= 0 && SubIndex < 3 );
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );

	if(SubIndex == 0)
		VectorTrack.Points[KeyIndex].OutVal.X = NewOutVal;
	else if(SubIndex == 1)
		VectorTrack.Points[KeyIndex].OutVal.Y = NewOutVal;
	else 
		VectorTrack.Points[KeyIndex].OutVal.Z = NewOutVal;

	VectorTrack.AutoSetTangents(0.f);
}

void UInterpTrackVectorBase::SetKeyInterpMode(int32 KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );
	VectorTrack.Points[KeyIndex].InterpMode = NewMode;
	VectorTrack.AutoSetTangents(CurveTension);
}

void UInterpTrackVectorBase::SetTangents(int32 SubIndex, int32 KeyIndex, float ArriveTangent, float LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 3 );
	check( KeyIndex >= 0 && KeyIndex < VectorTrack.Points.Num() );

	if(SubIndex == 0)
	{
		VectorTrack.Points[KeyIndex].ArriveTangent.X = ArriveTangent;
		VectorTrack.Points[KeyIndex].LeaveTangent.X = LeaveTangent;
	}
	else if(SubIndex == 1)
	{
		VectorTrack.Points[KeyIndex].ArriveTangent.Y = ArriveTangent;
		VectorTrack.Points[KeyIndex].LeaveTangent.Y = LeaveTangent;
	}
	else if(SubIndex == 2)
	{
		VectorTrack.Points[KeyIndex].ArriveTangent.Z = ArriveTangent;
		VectorTrack.Points[KeyIndex].LeaveTangent.Z = LeaveTangent;
	}
}




/*-----------------------------------------------------------------------------
UInterpTrackLinearColorBase
-----------------------------------------------------------------------------*/

int32 UInterpTrackLinearColorBase::GetNumKeys() const
{
	return LinearColorTrack.Points.Num();
}

int32 UInterpTrackLinearColorBase::GetNumSubCurves() const
{
	return 4;
}

FColor UInterpTrackLinearColorBase::GetSubCurveButtonColor(int32 SubCurveIndex, bool bIsSubCurveHidden) const
{
	// Check for array out of bounds because it will crash the program
	check(SubCurveIndex >= 0);
	check(SubCurveIndex < GetNumSubCurves());

	FColor ButtonColor;

	switch(SubCurveIndex)
	{
	case 0:
		// Red
		ButtonColor = bIsSubCurveHidden ? FColor(32, 0,  0) : FColor::Red;
		break;
	case 1:
		// Green
		ButtonColor = bIsSubCurveHidden ? FColor(0, 32,  0) : FColor::Green;
		break;
	case 2:
		// Blue
		ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 32) : FColor::Blue;
		break;
	case 3:
		ButtonColor = bIsSubCurveHidden ? FColor::Black : FColor::White;
		break;
	default:
		// A bad sub-curve index was given. 
		check(false);
		break;
	}

	return ButtonColor;
}

float UInterpTrackLinearColorBase::GetKeyIn(int32 KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );
	return LinearColorTrack.Points[KeyIndex].InVal;
}

float UInterpTrackLinearColorBase::GetKeyOut(int32 SubIndex, int32 KeyIndex)
{
	check( SubIndex >= 0 && SubIndex < 4 );
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );

	if(SubIndex == 0)
		return LinearColorTrack.Points[KeyIndex].OutVal.R;
	else if(SubIndex == 1)
		return LinearColorTrack.Points[KeyIndex].OutVal.G;
	else if(SubIndex == 2)
		return LinearColorTrack.Points[KeyIndex].OutVal.B;
	else
		return LinearColorTrack.Points[KeyIndex].OutVal.A;
}

void UInterpTrackLinearColorBase::GetInRange(float& MinIn, float& MaxIn) const
{
	if(LinearColorTrack.Points.Num() == 0)
	{
		MinIn = 0.f;
		MaxIn = 0.f;
	}
	else
	{
		MinIn = LinearColorTrack.Points[ 0 ].InVal;
		MaxIn = LinearColorTrack.Points[ LinearColorTrack.Points.Num()-1 ].InVal;
	}
}

void UInterpTrackLinearColorBase::GetOutRange(float& MinOut, float& MaxOut) const
{
	FLinearColor MinVec, MaxVec;
	LinearColorTrack.CalcBounds(MinVec, MaxVec, FLinearColor(0.f, 0.f, 0.f, 0.f));

	MinOut = MinVec.GetMin();
	MaxOut = MaxVec.GetMax();
}

FColor UInterpTrackLinearColorBase::GetKeyColor(int32 SubIndex, int32 KeyIndex, const FColor& CurveColor)
{
	check( SubIndex >= 0 && SubIndex < 4);
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );

	if (SubIndex == 0)
	{
		return FColor::Red;
	}
	else if (SubIndex == 1)
	{
		return FColor::Green;
	}
	else if (SubIndex == 2)
	{
		return FColor::Blue;
	}
	else
	{
		return FColor::White;
	}
}

EInterpCurveMode UInterpTrackLinearColorBase::GetKeyInterpMode(int32 KeyIndex) const
{
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );
	return LinearColorTrack.Points[KeyIndex].InterpMode;
}

void UInterpTrackLinearColorBase::GetTangents(int32 SubIndex, int32 KeyIndex, float& ArriveTangent, float& LeaveTangent) const
{
	check( SubIndex >= 0 && SubIndex < 4 );
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );

	if(SubIndex == 0)
	{
		ArriveTangent = LinearColorTrack.Points[KeyIndex].ArriveTangent.R;
		LeaveTangent = LinearColorTrack.Points[KeyIndex].LeaveTangent.R;
	}
	else if(SubIndex == 1)
	{
		ArriveTangent = LinearColorTrack.Points[KeyIndex].ArriveTangent.G;
		LeaveTangent = LinearColorTrack.Points[KeyIndex].LeaveTangent.G;
	}
	else if(SubIndex == 2)
	{
		ArriveTangent = LinearColorTrack.Points[KeyIndex].ArriveTangent.B;
		LeaveTangent = LinearColorTrack.Points[KeyIndex].LeaveTangent.B;
	}
	else if(SubIndex == 3)
	{
		ArriveTangent = LinearColorTrack.Points[KeyIndex].ArriveTangent.A;
		LeaveTangent = LinearColorTrack.Points[KeyIndex].LeaveTangent.A;
	}
}

float UInterpTrackLinearColorBase::EvalSub(int32 SubIndex, float InVal)
{
	check( SubIndex >= 0 && SubIndex < 4 );

	FLinearColor OutVal = LinearColorTrack.Eval(InVal, FLinearColor(0.f, 0.f, 0.f, 0.f));

	if(SubIndex == 0)
		return OutVal.R;
	else if(SubIndex == 1)
		return OutVal.G;
	else if(SubIndex == 2)
		return OutVal.B;
	else
		return OutVal.A;
}

int32 UInterpTrackLinearColorBase::CreateNewKey(float KeyIn)
{
	FLinearColor NewKeyOut = LinearColorTrack.Eval(KeyIn, FLinearColor(0.f, 0.f, 0.f, 0.f));
	int32 NewPointIndex = LinearColorTrack.AddPoint(KeyIn, NewKeyOut);
	LinearColorTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackLinearColorBase::DeleteKey(int32 KeyIndex)
{
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );
	LinearColorTrack.Points.RemoveAt(KeyIndex);
	LinearColorTrack.AutoSetTangents(CurveTension);
}

int32 UInterpTrackLinearColorBase::SetKeyIn(int32 KeyIndex, float NewInVal)
{
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );
	int32 NewPointIndex = LinearColorTrack.MovePoint(KeyIndex, NewInVal);
	LinearColorTrack.AutoSetTangents(CurveTension);
	return NewPointIndex;
}

void UInterpTrackLinearColorBase::SetKeyOut(int32 SubIndex, int32 KeyIndex, float NewOutVal) 
{
	check( SubIndex >= 0 && SubIndex < 4 );
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );

	if(SubIndex == 0)
		LinearColorTrack.Points[KeyIndex].OutVal.R = NewOutVal;
	else if(SubIndex == 1)
		LinearColorTrack.Points[KeyIndex].OutVal.G = NewOutVal;
	else if(SubIndex == 2)
		LinearColorTrack.Points[KeyIndex].OutVal.B = NewOutVal;
	else 
		LinearColorTrack.Points[KeyIndex].OutVal.A = NewOutVal;

	LinearColorTrack.AutoSetTangents(0.f);
}

void UInterpTrackLinearColorBase::SetKeyInterpMode(int32 KeyIndex, EInterpCurveMode NewMode) 
{
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );
	LinearColorTrack.Points[KeyIndex].InterpMode = NewMode;
	LinearColorTrack.AutoSetTangents(CurveTension);
}

void UInterpTrackLinearColorBase::SetTangents(int32 SubIndex, int32 KeyIndex, float ArriveTangent, float LeaveTangent)
{
	check( SubIndex >= 0 && SubIndex < 4 );
	check( KeyIndex >= 0 && KeyIndex < LinearColorTrack.Points.Num() );

	if(SubIndex == 0)
	{
		LinearColorTrack.Points[KeyIndex].ArriveTangent.R = ArriveTangent;
		LinearColorTrack.Points[KeyIndex].LeaveTangent.R = LeaveTangent;
	}
	else if(SubIndex == 1)
	{
		LinearColorTrack.Points[KeyIndex].ArriveTangent.G = ArriveTangent;
		LinearColorTrack.Points[KeyIndex].LeaveTangent.G = LeaveTangent;
	}
	else if(SubIndex == 2)
	{
		LinearColorTrack.Points[KeyIndex].ArriveTangent.B = ArriveTangent;
		LinearColorTrack.Points[KeyIndex].LeaveTangent.B = LeaveTangent;
	}	
	else if(SubIndex == 3)
	{
		LinearColorTrack.Points[KeyIndex].ArriveTangent.A = ArriveTangent;
		LinearColorTrack.Points[KeyIndex].LeaveTangent.A = LeaveTangent;
	}
}

