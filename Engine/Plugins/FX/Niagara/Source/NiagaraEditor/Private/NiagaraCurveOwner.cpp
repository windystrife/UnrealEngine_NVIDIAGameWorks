// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraCurveOwner.h"

FNiagaraCurveOwner::FNiagaraCurveOwner()
	: bIsColorCurve(false)
{
}

void FNiagaraCurveOwner::EmptyCurves()
{
	ConstCurves.Empty();
	Curves.Empty();
	EditInfoToOwnerMap.Empty();
	EditInfoToColorMap.Empty();
	EditInfoToNotifyCurveChangedMap.Empty();
	bIsColorCurve = false;
}

void FNiagaraCurveOwner::AddCurve(FRichCurve& Curve, FName Name, FLinearColor Color, UObject& Owner, FNotifyCurveChanged CurveChangedHandler)
{
	FRichCurveEditInfo EditInfo(&Curve, Name);
	Curves.Add(EditInfo);
	ConstCurves.Add(FRichCurveEditInfoConst(&Curve, Name));
	EditInfoToOwnerMap.Add(EditInfo, &Owner);
	EditInfoToColorMap.Add(EditInfo, Color);
	EditInfoToNotifyCurveChangedMap.Add(EditInfo, CurveChangedHandler);
	bIsColorCurve = false;
}

void FNiagaraCurveOwner::SetColorCurves(FRichCurve& RedCurve, FRichCurve& GreenCurve, FRichCurve& BlueCurve, FRichCurve& AlphaCurve, FName Name, UObject& Owner, FNotifyCurveChanged CurveChangedHandler)
{
	EmptyCurves();
	FString NamePrefix = Name != NAME_None ? Name.ToString() + TEXT(".") : FString();
	AddCurve(RedCurve, *(NamePrefix + TEXT("Red")), FLinearColor::Red, Owner, CurveChangedHandler);
	AddCurve(GreenCurve, *(NamePrefix + TEXT("Green")), FLinearColor::Green, Owner, CurveChangedHandler);
	AddCurve(BlueCurve, *(NamePrefix + TEXT("Blue")), FLinearColor::Blue, Owner, CurveChangedHandler);
	AddCurve(AlphaCurve, *(NamePrefix + TEXT("Alpha")), FLinearColor::White, Owner, CurveChangedHandler);
	bIsColorCurve = true;
}

TArray<FRichCurveEditInfoConst> FNiagaraCurveOwner::GetCurves() const
{
	return ConstCurves;
}

TArray<FRichCurveEditInfo> FNiagaraCurveOwner::GetCurves()
{
	return Curves;
};

void FNiagaraCurveOwner::ModifyOwner()
{
	TArray<UObject*> Owners;
	EditInfoToOwnerMap.GenerateValueArray(Owners);
	for (UObject* Owner : Owners)
	{
		Owner->Modify();
	}
}

TArray<const UObject*> FNiagaraCurveOwner::GetOwners() const
{
	TArray<UObject*> Owners;
	EditInfoToOwnerMap.GenerateValueArray(Owners);
	TArray<const UObject*> ConstOwners;
	ConstOwners.Append(Owners);
	return ConstOwners;
}

void FNiagaraCurveOwner::MakeTransactional()
{
	TArray<UObject*> Owners;
	EditInfoToOwnerMap.GenerateValueArray(Owners);
	for (UObject* Owner : Owners)
	{
		Owner->SetFlags(RF_Transactional);
	}
}

void FNiagaraCurveOwner::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{
	for (const FRichCurveEditInfo& ChangedCurveEditInfo : ChangedCurveEditInfos)
	{
		FNotifyCurveChanged* CurveChanged = EditInfoToNotifyCurveChangedMap.Find(ChangedCurveEditInfo);
		UObject** CurveOwner = EditInfoToOwnerMap.Find(ChangedCurveEditInfo);
		if (CurveChanged != nullptr && CurveOwner != nullptr)
		{
			CurveChanged->Execute(ChangedCurveEditInfo.CurveToEdit, *CurveOwner);
		}
	}
}

bool FNiagaraCurveOwner::IsLinearColorCurve() const
{
	return bIsColorCurve;
}

FLinearColor FNiagaraCurveOwner::GetLinearColorValue(float InTime) const
{
	return FLinearColor(
		Curves[0].CurveToEdit->Eval(InTime),
		Curves[1].CurveToEdit->Eval(InTime),
		Curves[2].CurveToEdit->Eval(InTime),
		Curves[3].CurveToEdit->Eval(InTime));
}

bool FNiagaraCurveOwner::HasAnyAlphaKeys() const
{
	return bIsColorCurve && Curves[3].CurveToEdit->GetNumKeys() > 0;
}

bool FNiagaraCurveOwner::IsValidCurve(FRichCurveEditInfo CurveInfo)
{
	return EditInfoToOwnerMap.Contains(CurveInfo);
}

FLinearColor FNiagaraCurveOwner::GetCurveColor(FRichCurveEditInfo CurveInfo) const
{
	const FLinearColor* Color = EditInfoToColorMap.Find(CurveInfo);
	if (Color != nullptr)
	{
		return *Color;
	}
	return FCurveOwnerInterface::GetCurveColor(CurveInfo);
}