// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SharedPointer.h"
#include "Delegate.h"

class FStructOnScope;
class SNiagaraParameterEditor;
class SWidget;
struct FNiagaraVariable;

class INiagaraEditorTypeUtilities
{
public:
	DECLARE_DELEGATE(FNotifyValueChanged);
public:
	virtual ~INiagaraEditorTypeUtilities() {}

	virtual bool CanProvideDefaultValue() const = 0;

	virtual void UpdateStructWithDefaultValue(TSharedRef<FStructOnScope> Struct) const = 0;

	virtual bool CanCreateParameterEditor() const = 0;

	virtual TSharedPtr<SNiagaraParameterEditor> CreateParameterEditor() const = 0;

	virtual bool CanCreateDataInterfaceEditor() const = 0;

	virtual TSharedPtr<SWidget> CreateDataInterfaceEditor(UObject* DataInterface, FNotifyValueChanged DataInterfaceChangedHandler) const = 0;

	virtual bool CanHandlePinDefaults() const = 0;

	virtual FString GetPinDefaultStringFromValue(const FNiagaraVariable& Variable) const = 0;

	virtual void SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const = 0;
};

class FNiagaraEditorTypeUtilities : public INiagaraEditorTypeUtilities, public TSharedFromThis<FNiagaraEditorTypeUtilities>
{
public:
	DECLARE_DELEGATE(FNotifyValueChanged);
public:
	//~ INiagaraEditorTypeUtilities
	virtual bool CanProvideDefaultValue() const override { return false; }
	virtual void UpdateStructWithDefaultValue(TSharedRef<FStructOnScope> Struct) const override { }
	virtual bool CanCreateParameterEditor() const override { return false; }
	virtual TSharedPtr<SNiagaraParameterEditor> CreateParameterEditor() const override { return TSharedPtr<SNiagaraParameterEditor>(); }
	virtual bool CanCreateDataInterfaceEditor() const override { return false; };
	virtual TSharedPtr<SWidget> CreateDataInterfaceEditor(UObject* DataInterface, FNotifyValueChanged DataInterfaceChangedHandler) const override { return TSharedPtr<SWidget>(); }
	virtual bool CanHandlePinDefaults() const override { return false; }
	virtual FString GetPinDefaultStringFromValue(const FNiagaraVariable& Variable) const override { return FString(); }
	virtual void SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const override { }
};