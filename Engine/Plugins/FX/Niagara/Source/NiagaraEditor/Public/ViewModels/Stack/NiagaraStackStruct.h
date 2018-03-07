// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "StructOnScope.h"
#include "PropertyEditorDelegates.h"
#include "NiagaraStackStruct.generated.h"

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackStruct : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	UNiagaraStackStruct();

	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, UObject* InOwningObject, const UStruct* StructClass, uint8* StructData);

	UObject* GetOwningObject();
	TSharedPtr<FStructOnScope> GetStructOnScope();

	//~ UNiagaraStackEntry interface
	virtual int32 GetItemIndentLevel() const override;

	/** Sets the item indent level for this stack entry. */
	void SetItemIndentLevel(int32 InItemIndentLevel);

	bool HasDetailCustomization() const;
	
	FOnGetDetailCustomizationInstance GetDetailsCustomization() const;
	void SetDetailsCustomization(FOnGetDetailCustomizationInstance Customization);

private:
	UObject* OwningObject;
	TSharedPtr<FStructOnScope> StructData;
	int32 ItemIndentLevel;

	FOnGetDetailCustomizationInstance DetailCustomization;
};