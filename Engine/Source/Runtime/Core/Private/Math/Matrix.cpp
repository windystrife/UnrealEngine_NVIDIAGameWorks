// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Math/Matrix.h"

void FMatrix::ErrorEnsure(const TCHAR* Message)
{
	UE_LOG(LogUnrealMath, Error, TEXT("FMatrix::InverseFast(), trying to invert a NIL matrix, this results in NaNs! Use Inverse() instead."));
	ensureMsgf(false, TEXT("FMatrix::InverseFast(), trying to invert a NIL matrix, this results in NaNs! Use Inverse() instead."));
}
