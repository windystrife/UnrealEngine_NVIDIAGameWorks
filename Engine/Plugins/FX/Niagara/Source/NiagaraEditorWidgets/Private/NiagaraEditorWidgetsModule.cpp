// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorWidgetsModule.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "NiagaraEditorModule.h"
#include "SNiagaraStack.h"
#include "DetailCustomizations/NiagaraDataInterfaceCurveDetails.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

IMPLEMENT_MODULE(FNiagaraEditorWidgetsModule, NiagaraEditorWidgets);

FNiagaraStackCurveEditorOptions::FNiagaraStackCurveEditorOptions()
	: ViewMinInput(0)
	, ViewMaxInput(1)
	, ViewMinOutput(0)
	, ViewMaxOutput(1)
	, bAreCurvesVisible(true)
	, Height(100)
{
}

float FNiagaraStackCurveEditorOptions::GetViewMinInput() const
{
	return ViewMinInput;
}

float FNiagaraStackCurveEditorOptions::GetViewMaxInput() const
{
	return ViewMaxInput;
}

void FNiagaraStackCurveEditorOptions::SetInputViewRange(float InViewMinInput, float InViewMaxInput)
{
	ViewMinInput = InViewMinInput;
	ViewMaxInput = InViewMaxInput;
}

float FNiagaraStackCurveEditorOptions::GetViewMinOutput() const
{
	return ViewMinOutput;
}

float FNiagaraStackCurveEditorOptions::GetViewMaxOutput() const
{
	return ViewMaxOutput;
}

void FNiagaraStackCurveEditorOptions::SetOutputViewRange(float InViewMinOutput, float InViewMaxOutput)
{
	ViewMinOutput = InViewMinOutput;
	ViewMaxOutput = InViewMaxOutput;
}

float FNiagaraStackCurveEditorOptions::GetTimelineLength() const
{
	return ViewMaxInput - ViewMinInput;
}

float FNiagaraStackCurveEditorOptions::GetHeight() const
{
	return Height;
}

void FNiagaraStackCurveEditorOptions::SetHeight(float InHeight)
{
	Height = InHeight;
}

bool FNiagaraStackCurveEditorOptions::GetAreCurvesVisible() const
{
	return bAreCurvesVisible;
}

void FNiagaraStackCurveEditorOptions::SetAreCurvesVisible(bool bInAreCurvesVisible)
{
	bAreCurvesVisible = bInAreCurvesVisible;
}

void FNiagaraEditorWidgetsModule::StartupModule()
{
	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	OnCreateStackWidgetHandle = NiagaraEditorModule.SetOnCreateStackWidget(FNiagaraEditorModule::FOnCreateStackWidget::CreateLambda([](UNiagaraStackViewModel* ViewModel)
	{
		return SNew(SNiagaraStack, ViewModel);
	}));
	FNiagaraEditorWidgetsStyle::Initialize();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("NiagaraDataInterfaceCurve", FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraDataInterfaceCurveDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("NiagaraDataInterfaceVector2DCurve", FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraDataInterfaceVector2DCurveDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("NiagaraDataInterfaceVectorCurve", FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraDataInterfaceVectorCurveDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("NiagaraDataInterfaceVector4Curve", FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraDataInterfaceVector4CurveDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("NiagaraDataInterfaceColorCurve", FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraDataInterfaceColorCurveDetails::MakeInstance));
}

void FNiagaraEditorWidgetsModule::ShutdownModule()
{
	FNiagaraEditorModule* NiagaraEditorModule = FModuleManager::GetModulePtr<FNiagaraEditorModule>("NiagaraEditor");
	if (NiagaraEditorModule != nullptr)
	{
		NiagaraEditorModule->ResetOnCreateStackWidget(OnCreateStackWidgetHandle);
	}

	FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyModule != nullptr)
	{
		PropertyModule->UnregisterCustomClassLayout("NiagaraDataInterfaceCurve");
		PropertyModule->UnregisterCustomClassLayout("NiagaraDataInterfaceVector2DCurve");
		PropertyModule->UnregisterCustomClassLayout("NiagaraDataInterfaceVectorCurve");
		PropertyModule->UnregisterCustomClassLayout("NiagaraDataInterfaceVector4Curve");
		PropertyModule->UnregisterCustomClassLayout("NiagaraDataInterfaceColorCurve");
	}

	FNiagaraEditorWidgetsStyle::Shutdown();
}

TSharedRef<FNiagaraStackCurveEditorOptions> FNiagaraEditorWidgetsModule::GetOrCreateStackCurveEditorOptionsForObject(UObject* Object, bool bDefaultAreCurvesVisible, float DefaultHeight)
{
	TSharedRef<FNiagaraStackCurveEditorOptions>* StackCurveEditorOptions = ObjectToStackCurveEditorOptionsMap.Find(FObjectKey(Object));
	if (StackCurveEditorOptions == nullptr)
	{
		StackCurveEditorOptions = &ObjectToStackCurveEditorOptionsMap.Add(FObjectKey(Object), MakeShared<FNiagaraStackCurveEditorOptions>());
		(*StackCurveEditorOptions)->SetAreCurvesVisible(bDefaultAreCurvesVisible);
		(*StackCurveEditorOptions)->SetHeight(DefaultHeight);
	}
	return *StackCurveEditorOptions;
}