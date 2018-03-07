// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Delegates/IDelegateInstance.h"
#include "UObject/ObjectKey.h"
#include "Templates/SharedPointer.h"

class FNiagaraStackCurveEditorOptions
{
public:
	FNiagaraStackCurveEditorOptions();

	float GetViewMinInput() const;
	float GetViewMaxInput() const;
	void SetInputViewRange(float InViewMinInput, float InViewMaxInput);

	float GetViewMinOutput() const;
	float GetViewMaxOutput() const;
	void SetOutputViewRange(float InViewMinOutput, float InViewMaxOutput);

	bool GetAreCurvesVisible() const;
	void SetAreCurvesVisible(bool bInAreCurvesVisible);

	float GetTimelineLength() const;

	float GetHeight() const;
	void SetHeight(float InHeight);

private:
	float ViewMinInput;
	float ViewMaxInput;
	float ViewMinOutput;
	float ViewMaxOutput;
	bool bAreCurvesVisible;
	float Height;
};

class UObject;

/** A module containing widgets for editing niagara data. */
class FNiagaraEditorWidgetsModule : public IModuleInterface
{

public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedRef<FNiagaraStackCurveEditorOptions> GetOrCreateStackCurveEditorOptionsForObject(UObject* Object, bool bDefaultAreCurvesVisible, float DefaultHeight);

private:
	FDelegateHandle OnCreateStackWidgetHandle;

	TMap<FObjectKey, TSharedRef<FNiagaraStackCurveEditorOptions>> ObjectToStackCurveEditorOptionsMap;
};