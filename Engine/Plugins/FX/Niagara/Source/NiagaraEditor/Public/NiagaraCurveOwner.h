// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/CurveOwnerInterface.h"
#include "NiagaraDataInterface.h"

/** A curve owner for curves in a niagara System. */
class NIAGARAEDITOR_API FNiagaraCurveOwner : public FCurveOwnerInterface
{
public:
	DECLARE_DELEGATE_TwoParams(FNotifyCurveChanged, FRichCurve*, UObject*);

public:
	FNiagaraCurveOwner();

	/** Removes all of the curves from the curve owner. */
	void EmptyCurves();

	/** Adds a curve to this curve owner. */
	void AddCurve(FRichCurve& Curve, FName Name, FLinearColor Color, UObject& Owner, FNotifyCurveChanged CurveChangedHandler);

	void SetColorCurves(FRichCurve& RedCurve, FRichCurve& GreenCurve, FRichCurve& BlueCurve, FRichCurve& AlphaCurve, FName Name, UObject& Owner, FNotifyCurveChanged CurveChangedHandler);

	/** FCurveOwnerInterface */
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;
	virtual void ModifyOwner() override;
	virtual TArray<const UObject*> GetOwners() const override;
	virtual void MakeTransactional() override;
	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override;
	virtual bool IsLinearColorCurve() const override;
	virtual FLinearColor GetLinearColorValue(float InTime) const override;
	virtual bool HasAnyAlphaKeys() const override;
	virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) override;
	virtual FLinearColor GetCurveColor(FRichCurveEditInfo CurveInfo) const override;

private:
	/** The ordered array of const curves used to implement the curve owner interface. */
	TArray<FRichCurveEditInfoConst> ConstCurves;

	/** The ordered array or curves used to implement the curve owner interface. */
	TArray<FRichCurveEditInfo> Curves;

	/** A map of curve edit infos to their corresponding owners. */
	TMap<FRichCurveEditInfo, UObject*> EditInfoToOwnerMap;

	/** A map of curve edit infos to their colors. */
	TMap<FRichCurveEditInfo, FLinearColor> EditInfoToColorMap;

	/** A map of curve edit infos to change handler delegates. */
	TMap<FRichCurveEditInfo, FNotifyCurveChanged> EditInfoToNotifyCurveChangedMap;

	/** Whether or not this set of curves should be treated as a color curve. */
	bool bIsColorCurve;
};