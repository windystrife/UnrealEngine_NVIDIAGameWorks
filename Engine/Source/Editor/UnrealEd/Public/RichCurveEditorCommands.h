// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

/**
 * Defines commands for the SCurveEditor.
 */
class UNREALED_API FRichCurveEditorCommands : public TCommands<FRichCurveEditorCommands>
{
public:
	FRichCurveEditorCommands()
		: TCommands<FRichCurveEditorCommands>
		(
			TEXT("RichCurveEditor"),
			NSLOCTEXT("Contexts", "RichCurveEditor", "Curve Editor"),
			NAME_None,
			FEditorStyle::GetStyleSetName()
		)
	{
	}

	TSharedPtr<FUICommandInfo> ZoomToFitHorizontal;
	TSharedPtr<FUICommandInfo> ZoomToFitVertical;
	TSharedPtr<FUICommandInfo> ZoomToFit;
	TSharedPtr<FUICommandInfo> ZoomToFitAll;
	TSharedPtr<FUICommandInfo> ToggleInputSnapping;
	TSharedPtr<FUICommandInfo> ToggleOutputSnapping;

	TSharedPtr<FUICommandInfo> InterpolationConstant;
	TSharedPtr<FUICommandInfo> InterpolationLinear;
	TSharedPtr<FUICommandInfo> InterpolationCubicAuto;
	TSharedPtr<FUICommandInfo> InterpolationCubicUser;
	TSharedPtr<FUICommandInfo> InterpolationCubicBreak;

	TSharedPtr<FUICommandInfo> FlattenTangents;
	TSharedPtr<FUICommandInfo> StraightenTangents;

	TSharedPtr<FUICommandInfo> BakeCurve;
	TSharedPtr<FUICommandInfo> ReduceCurve;

	TSharedPtr<FUICommandInfo> SetPreInfinityExtrapCycle;
	TSharedPtr<FUICommandInfo> SetPreInfinityExtrapCycleWithOffset;
	TSharedPtr<FUICommandInfo> SetPreInfinityExtrapOscillate;
	TSharedPtr<FUICommandInfo> SetPreInfinityExtrapLinear;
	TSharedPtr<FUICommandInfo> SetPreInfinityExtrapConstant;

	TSharedPtr<FUICommandInfo> SetPostInfinityExtrapCycle;
	TSharedPtr<FUICommandInfo> SetPostInfinityExtrapCycleWithOffset;
	TSharedPtr<FUICommandInfo> SetPostInfinityExtrapOscillate;
	TSharedPtr<FUICommandInfo> SetPostInfinityExtrapLinear;
	TSharedPtr<FUICommandInfo> SetPostInfinityExtrapConstant;

	TSharedPtr<FUICommandInfo> SetAllCurveVisibility;
	TSharedPtr<FUICommandInfo> SetSelectedCurveVisibility;
	TSharedPtr<FUICommandInfo> SetAnimatedCurveVisibility;

	TSharedPtr<FUICommandInfo> SetAllTangentsVisibility;
	TSharedPtr<FUICommandInfo> SetSelectedKeysTangentVisibility;
	TSharedPtr<FUICommandInfo> SetNoTangentsVisibility;

	TSharedPtr<FUICommandInfo> ToggleAutoFrameCurveEditor;
	TSharedPtr<FUICommandInfo> ToggleShowCurveEditorCurveToolTips;

public:
	virtual void RegisterCommands() override;
};
