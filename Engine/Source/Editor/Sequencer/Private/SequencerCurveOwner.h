// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveOwnerInterface.h"
#include "CurveEditorSettings.h"

class FSequencerNodeTree;
class UMovieSceneSection;

/** A curve owner interface for displaying animation curves in sequencer. */
class FSequencerCurveOwner : public FCurveOwnerInterface
{
public:
	FSequencerCurveOwner(TSharedPtr<FSequencerNodeTree> InSequencerNodeTree, ECurveEditorCurveVisibility::Type CurveVisibility);

	/** Return the set of selected curves */
	TArray<FRichCurve*> GetSelectedCurves() const;

	/** FCurveOwnerInterface */
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;
	virtual void ModifyOwner() override;
	virtual TArray<const UObject*> GetOwners() const override;
	virtual void MakeTransactional() override;
	virtual void OnCurveChanged( const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos ) override;
	virtual bool IsValidCurve( FRichCurveEditInfo CurveInfo ) override;
	virtual FLinearColor GetCurveColor( FRichCurveEditInfo CurveInfo) const override;

private:
	/** The FSequencerNodeTree used to build the curve owner. */
	TSharedPtr<FSequencerNodeTree> SequencerNodeTree;

	/** The ordered array of const curves used to implement the curve owner interface. */
	TArray<FRichCurveEditInfoConst> ConstCurves;

	/** The ordered array or curves used to implement the curve owner interface. */
	TArray<FRichCurveEditInfo> Curves;

	/** A map of curve edit infos to their corresponding sections. */
	TMap<FRichCurveEditInfo, UMovieSceneSection*> EditInfoToSectionMap;
};
