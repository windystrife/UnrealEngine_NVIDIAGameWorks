// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RichCurveEditorCommands.h"

#define LOCTEXT_NAMESPACE "RichCurveEditorCommands"

void FRichCurveEditorCommands::RegisterCommands()
{
	UI_COMMAND(ZoomToFitHorizontal, "Fit Horizontal", "Zoom to Fit - Horizontal", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ZoomToFitVertical, "Fit Vertical", "Zoom to Fit - Vertical", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ZoomToFit, "Fit", "Zoom to Fit", EUserInterfaceActionType::Button, FInputChord(EKeys::F));
	UI_COMMAND(ZoomToFitAll, "FitAll", "Zoom to Fit All", EUserInterfaceActionType::Button, FInputChord(EKeys::A));

	UI_COMMAND(ToggleInputSnapping, "Input Snapping", "Toggle Input Snapping", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleOutputSnapping, "Output Snapping", "Toggle Output Snapping", EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(InterpolationConstant, "Constant", "Constant interpolation", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::Five));
	UI_COMMAND(InterpolationLinear, "Linear", "Linear interpolation", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::Four));
	UI_COMMAND(InterpolationCubicAuto, "Auto", "Cubic interpolation - Automatic tangents", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::One));
	UI_COMMAND(InterpolationCubicUser, "User", "Cubic interpolation - User flat tangents", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::Two));
	UI_COMMAND(InterpolationCubicBreak, "Break", "Cubic interpolation - User broken tangents", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::Three));

	UI_COMMAND(FlattenTangents, "Flatten", "Flatten tangents", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StraightenTangents, "Straighten", "Straighten tangents", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(BakeCurve, "Bake", "Bake curve", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReduceCurve, "Reduce", "Reduce curve", EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(SetPreInfinityExtrapCycle, "Cycle", "Set the pre-infinity curve extrapolation to cycle.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetPreInfinityExtrapCycleWithOffset, "Cycle with Offset", "Set the pre-infinity curve extrapolation to cycle with offset.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetPreInfinityExtrapOscillate, "Oscillate", "Set the pre-infinity curve extrapolation to oscillate.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetPreInfinityExtrapLinear, "Linear", "Set the pre-infinity curve extrapolation to linear.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetPreInfinityExtrapConstant, "Constant", "Set the pre-infinity curve extrapolation to constant.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetPostInfinityExtrapCycle, "Cycle", "Set the post-infinity curve extrapolation to cycle.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetPostInfinityExtrapCycleWithOffset, "Cycle with Offset", "Set the post-infinity curve extrapolation to cycle with offset.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetPostInfinityExtrapOscillate, "Oscillate", "Set the post-infinity curve extrapolation to oscillate.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetPostInfinityExtrapLinear, "Linear", "Set the post-infinity curve extrapolation to linear.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetPostInfinityExtrapConstant, "Constant", "Set the post-infinity curve extrapolation to constant.", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(SetAllCurveVisibility, "All Curves", "Show all curves in the curve editor.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetSelectedCurveVisibility, "Selected Curves", "Show only selected curves in the curve editor.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetAnimatedCurveVisibility, "Animated Curves", "Show only curves with keyframes in the curve editor.", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(SetAllTangentsVisibility, "All Tangents", "Show all tangents in the curve editor.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetSelectedKeysTangentVisibility, "Selected Keys", "Show tangents for selected keys in the curve editor.", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(SetNoTangentsVisibility, "No Tangents", "Show no tangents in the curve editor.", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(ToggleAutoFrameCurveEditor, "Auto Frame Curves", "Auto frame curves when they are selected.", EUserInterfaceActionType::ToggleButton, FInputChord() );
	UI_COMMAND(ToggleShowCurveEditorCurveToolTips, "Curve Tool Tips", "Show a tool tip with name and values when hovering over a curve.", EUserInterfaceActionType::ToggleButton, FInputChord() );
}

#undef LOCTEXT_NAMESPACE
