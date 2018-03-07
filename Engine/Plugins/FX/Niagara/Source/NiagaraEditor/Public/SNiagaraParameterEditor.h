// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SCompoundWidget.h"
#include "StructOnScope.h"

/** Base class for inline parameter editors. These editors are expected to maintain an internal value which
	is populated from a parameter struct. */
class NIAGARAEDITOR_API SNiagaraParameterEditor : public SCompoundWidget
{
public:
	DECLARE_DELEGATE(FOnValueChange);

public:
	/** Updates the internal value of the widget from a struct. */
	virtual void UpdateInternalValueFromStruct(TSharedRef<FStructOnScope> Struct) = 0;

	/** Updates a struct from the internal value of the widget. */
	virtual void UpdateStructFromInternalValue(TSharedRef<FStructOnScope> Struct) = 0;
	
	/** Gets whether this is currently the exclusive editor of this parameter, meaning that the corresponding details view
		should not be updated.  This hack is necessary because the details view closes all color pickers when
		it's changed! */
	bool GetIsEditingExclusively();

	/** Sets the OnBeginValueChange delegate which is run when a continuous internal value change begins. */
	void SetOnBeginValueChange(FOnValueChange InOnBeginValueChange);

	/** Sets the OnBeginValueChange delegate which is run when a continuous internal value change ends. */
	void SetOnEndValueChange(FOnValueChange InOnEndValueChange);

	/** Sets the OnValueChanged delegate which is run when the internal value changes. */
	void SetOnValueChanged(FOnValueChange InOnValueChanged);

protected:
	/** Sets whether this is currently the exclusive editor of this parameter, meaning that the corresponding details view
		should not be updated.  This hack is necessary because the details view closes all color pickers when
		it's changed! */
	void SetIsEditingExclusively(bool bInIsEditingExclusively);

	/** Executes the OnBeginValueChange delegate */
	void ExecuteOnBeginValueChange();

	/** Executes the OnEndValueChange delegate. */
	void ExecuteOnEndValueChange();

	/** Executes the OnValueChanged delegate. */
	void ExecuteOnValueChanged();

private:
	/** Whether this is currently the exclusive editor of this parameter, meaning that the corresponding details view
		should not be updated.  This hack is necessary because the details view closes all color pickers when
		it's changed! */
	bool bIsEditingExclusively;

	/** A delegate which is executed when a continuous change to the internal value begins. */
	FOnValueChange OnBeginValueChange;

	/** A delegate which is executed when a continuous change to the internal value ends. */
	FOnValueChange OnEndValueChange;

	/** A delegate which is executed when the internal value changes. */
	FOnValueChange OnValueChanged;
};