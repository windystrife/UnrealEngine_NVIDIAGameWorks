// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

/*-----------------------------------------------------------------------------
   FCurveEditorCommands
-----------------------------------------------------------------------------*/

class FCurveEditorCommands : public TCommands<FCurveEditorCommands>
{
public:
	/** Constructor */
	FCurveEditorCommands() 
		: TCommands<FCurveEditorCommands>("CurveEditor", NSLOCTEXT("Contexts", "CurveEditor", "CurveEditor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	/** See tooltips in cpp for documentation */
	
	TSharedPtr<FUICommandInfo> RemoveCurve;
	TSharedPtr<FUICommandInfo> RemoveAllCurves;
	TSharedPtr<FUICommandInfo> SetTime;
	TSharedPtr<FUICommandInfo> SetValue;
	TSharedPtr<FUICommandInfo> SetColor;
	TSharedPtr<FUICommandInfo> DeleteKeys;
	TSharedPtr<FUICommandInfo> ScaleTimes;
	TSharedPtr<FUICommandInfo> ScaleValues;
	TSharedPtr<FUICommandInfo> ScaleSingleCurveTimes;
	TSharedPtr<FUICommandInfo> ScaleSingleCurveValues;
	TSharedPtr<FUICommandInfo> ScaleSingleSubCurveValues;
	TSharedPtr<FUICommandInfo> FitHorizontally;
	TSharedPtr<FUICommandInfo> FitVertically;
	TSharedPtr<FUICommandInfo> Fit;
	TSharedPtr<FUICommandInfo> PanMode;
	TSharedPtr<FUICommandInfo> ZoomMode;
	TSharedPtr<FUICommandInfo> CurveAuto;
	TSharedPtr<FUICommandInfo> CurveAutoClamped;
	TSharedPtr<FUICommandInfo> CurveUser;
	TSharedPtr<FUICommandInfo> CurveBreak;
	TSharedPtr<FUICommandInfo> Linear;
	TSharedPtr<FUICommandInfo> Constant;
	TSharedPtr<FUICommandInfo> FlattenTangents;
	TSharedPtr<FUICommandInfo> StraightenTangents;
	TSharedPtr<FUICommandInfo> ShowAllTangents;
	TSharedPtr<FUICommandInfo> CreateTab;
	TSharedPtr<FUICommandInfo> DeleteTab;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
