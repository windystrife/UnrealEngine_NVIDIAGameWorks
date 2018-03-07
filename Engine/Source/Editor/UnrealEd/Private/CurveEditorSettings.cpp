// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CurveEditorSettings.h"

UCurveEditorSettings::UCurveEditorSettings( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	bAutoFrameCurveEditor = true;
	bShowCurveEditorCurveToolTips = true;
	CurveVisibility = ECurveEditorCurveVisibility::SelectedCurves;
	TangentVisibility = ECurveEditorTangentVisibility::SelectedKeys;
}

bool UCurveEditorSettings::GetAutoFrameCurveEditor() const
{
	return bAutoFrameCurveEditor;
}

void UCurveEditorSettings::SetAutoFrameCurveEditor(bool InbAutoFrameCurveEditor)
{
	if (bAutoFrameCurveEditor != InbAutoFrameCurveEditor)
	{
		bAutoFrameCurveEditor = InbAutoFrameCurveEditor;
		SaveConfig();
	}
}

bool UCurveEditorSettings::GetShowCurveEditorCurveToolTips() const
{
	return bShowCurveEditorCurveToolTips;
}

void UCurveEditorSettings::SetShowCurveEditorCurveToolTips(bool InbShowCurveEditorCurveToolTips)
{
	if (bShowCurveEditorCurveToolTips != InbShowCurveEditorCurveToolTips)
	{
		bShowCurveEditorCurveToolTips = InbShowCurveEditorCurveToolTips;
		SaveConfig();
	}
}

ECurveEditorCurveVisibility::Type UCurveEditorSettings::GetCurveVisibility() const
{
	return CurveVisibility;
}

void UCurveEditorSettings::SetCurveVisibility(ECurveEditorCurveVisibility::Type InCurveVisibility)
{
	if (CurveVisibility != InCurveVisibility)
	{
		CurveVisibility = InCurveVisibility;
		OnCurveEditorCurveVisibilityChanged.Broadcast();
		SaveConfig();
	}
}

ECurveEditorTangentVisibility::Type UCurveEditorSettings::GetTangentVisibility() const
{
	return TangentVisibility;
}

void UCurveEditorSettings::SetTangentVisibility(ECurveEditorTangentVisibility::Type InTangentVisibility)
{
	if (TangentVisibility != InTangentVisibility)
	{
		TangentVisibility = InTangentVisibility;
		SaveConfig();
	}
}

UCurveEditorSettings::FOnCurveEditorCurveVisibilityChanged& UCurveEditorSettings::GetOnCurveEditorCurveVisibilityChanged()
{
	return OnCurveEditorCurveVisibilityChanged;
}
