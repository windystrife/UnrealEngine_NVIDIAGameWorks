// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CurveTableEditorHandle.h"

FRichCurve* FCurveTableEditorHandle::GetCurve() const
{
	if (CurveTable != nullptr && RowName != NAME_None)
	{
		return CurveTable.Get()->FindCurve(RowName, TEXT("CurveTableEditorHandle::GetCurve"));
	}
	return nullptr;
}

TArray<FRichCurveEditInfoConst> FCurveTableEditorHandle::GetCurves() const
{
	TArray<FRichCurveEditInfoConst> Curves;

	const FRichCurve* Curve = GetCurve();
	if (Curve)
	{
		Curves.Add(FRichCurveEditInfoConst(Curve, RowName));
	}

	return Curves;
}

TArray<FRichCurveEditInfo> FCurveTableEditorHandle::GetCurves()
{
	TArray<FRichCurveEditInfo> Curves;

	FRichCurve* Curve = GetCurve();
	if (Curve)
	{
		Curves.Add(FRichCurveEditInfo(Curve, RowName));
	}

	return Curves;
}

void FCurveTableEditorHandle::ModifyOwner()
{
	check(false);	// This handle is read only, so cannot be used to modify curves
}

void FCurveTableEditorHandle::MakeTransactional()
{
	check(false);	// This handle is read only, so cannot be used to modify curves
}

void FCurveTableEditorHandle::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{

}

bool FCurveTableEditorHandle::IsValidCurve(FRichCurveEditInfo CurveInfo)
{
	return CurveInfo.CurveToEdit == GetCurve();
}
