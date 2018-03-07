// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FNiagaraSystemViewModel;
class SCurveEditor;
class FUICommandList;
struct FNiagaraSimulation;
class FNiagaraEffectInstance;
class UNiagaraEffect;
class UCurveBase;

/** A curve editor control for curves in a niagara System. */
class SNiagaraCurveEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraCurveEditor) { }
	SLATE_END_ARGS()

	virtual void Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel);

	~SNiagaraCurveEditor();

private:
	TSharedRef<SWidget> ConstructToolBar(TSharedPtr<FUICommandList> CurveEditorCommandList);

	/** Makes the curve editor view options menu for the toolbar. */
	TSharedRef<SWidget> MakeCurveEditorViewOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList);

	/** Makes the curve editor curve options menu for the toolbar. */
	TSharedRef<SWidget> MakeCurveEditorCurveOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList);

	float GetInputSnap() const;
	void SetInputSnap(float Value);

	float GetOutputSnap() const;
	void SetOutputSnap(float Value);

	void OnCurveOwnerChanged();

private:
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel;
	TSharedPtr<SCurveEditor>CurveEditor;
	float InputSnap;
	float OutputSnap;
};