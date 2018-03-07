// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackItemGroup.h"
#include "NiagaraCommon.h"
#include "NiagaraStackScriptItemGroup.generated.h"

class FNiagaraScriptViewModel;
class UNiagaraStackEditorData;
class UNiagaraStackAddScriptModuleItem;
class UNiagaraStackSpacer;
class UNiagaraNodeOutput;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackScriptItemGroup : public UNiagaraStackItemGroup
{
	GENERATED_BODY()

public:
	UNiagaraStackScriptItemGroup();

	void Initialize(
		TSharedRef<FNiagaraSystemViewModel> InSystemViewModel,
		TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel,
		UNiagaraStackEditorData& InStackEditorData,
		TSharedRef<FNiagaraScriptViewModel> InScriptViewModel,
		ENiagaraScriptUsage InScriptUsage,
		int32 InScriptOccurrence = 0);

	ENiagaraScriptUsage GetScriptUsage() const { return ScriptUsage; }
	int32 GetScriptOccurrence() const { return ScriptOccurrence; }

	virtual FText GetDisplayName() const override;
	void SetDisplayName(FText InDisplayName);

	virtual int32 GetErrorCount() const override;
	virtual bool GetErrorFixable(int32 ErrorIdx) const override;
	virtual bool TryFixError(int32 ErrorIdx) override;
	virtual FText GetErrorText(int32 ErrorIdx) const override;
	virtual FText GetErrorSummaryText(int32 ErrorIdx) const override;

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;
	TSharedPtr<FNiagaraScriptViewModel> ScriptViewModel;

private:
	void ItemAdded();

	void ChildModifiedGroupItems();

private:
	ENiagaraScriptUsage ScriptUsage;

	int32 ScriptOccurrence;

	FText DisplayName;

	UPROPERTY()
	UNiagaraStackAddScriptModuleItem* AddModuleItem;

	UPROPERTY()
	UNiagaraStackSpacer* BottomSpacer;

	DECLARE_DELEGATE(FFixError);
	struct FError
	{
		FText ErrorText;
		FText ErrorSummaryText;
		FFixError Fix;
	};

	TSharedPtr<FError> Error;
};