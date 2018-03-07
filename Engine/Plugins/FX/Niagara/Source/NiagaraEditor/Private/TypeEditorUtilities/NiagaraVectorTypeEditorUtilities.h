// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "INiagaraEditorTypeUtilities.h"

class SNiagaraParameterEditor;

/** Niagara editor utilities for the FVector2D type. */
class FNiagaraEditorVector2TypeUtilities : public FNiagaraEditorTypeUtilities
{
public:
	//~ INiagaraEditorTypeUtilities interface.
	virtual bool CanCreateParameterEditor() const override { return true; }
	virtual TSharedPtr<SNiagaraParameterEditor> CreateParameterEditor() const override;
	virtual bool CanHandlePinDefaults() const override;
	virtual FString GetPinDefaultStringFromValue(const FNiagaraVariable& Variable) const override;
	virtual void SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const override;
};

/** Niagara editor utilities for the FVector type. */
class FNiagaraEditorVector3TypeUtilities : public FNiagaraEditorTypeUtilities
{
public:
	//~ INiagaraEditorTypeUtilities interface.
	virtual bool CanCreateParameterEditor() const override { return true; }
	virtual TSharedPtr<SNiagaraParameterEditor> CreateParameterEditor() const override;
	virtual bool CanHandlePinDefaults() const override;
	virtual FString GetPinDefaultStringFromValue(const FNiagaraVariable& Variable) const override;
	virtual void SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const override;
};

/** Niagara editor utilities for the FVector4 type. */
class FNiagaraEditorVector4TypeUtilities : public FNiagaraEditorTypeUtilities
{
public:
	//~ INiagaraEditorTypeUtilities interface.
	virtual bool CanCreateParameterEditor() const override { return true; }
	virtual TSharedPtr<SNiagaraParameterEditor> CreateParameterEditor() const override;
	virtual bool CanHandlePinDefaults() const override;
	virtual FString GetPinDefaultStringFromValue(const FNiagaraVariable& Variable) const override;
	virtual void SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const override;
};