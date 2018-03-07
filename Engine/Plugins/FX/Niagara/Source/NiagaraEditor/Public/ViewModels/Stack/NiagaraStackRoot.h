// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.h"
#include "NiagaraStackRoot.generated.h"

class FNiagaraEmitterViewModel;
class UNiagaraStackEmitterSpawnScriptItemGroup;
class UNiagaraStackScriptItemGroup;
class UNiagaraStackScriptItemGroup;
class UNiagaraStackScriptItemGroup;
class UNiagaraStackEventHandlerGroup;
class UNiagaraStackRenderItemGroup;
class UNiagaraStackParameterStoreGroup;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackRoot : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	UNiagaraStackRoot();

	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel);

	virtual bool GetShouldShowInStack() const override;

protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren) override;

private:
	void EmitterEventArraysChanged();

private:
	UPROPERTY()
	UNiagaraStackScriptItemGroup* SystemSpawnGroup;

	UPROPERTY()
	UNiagaraStackScriptItemGroup* SystemUpdateGroup;

	UPROPERTY()
	UNiagaraStackParameterStoreGroup* SystemExposedVariablesGroup;

	UPROPERTY()
	UNiagaraStackEmitterSpawnScriptItemGroup* EmitterSpawnGroup;

	UPROPERTY()
	UNiagaraStackScriptItemGroup* EmitterUpdateGroup;

	UPROPERTY()
	UNiagaraStackScriptItemGroup* ParticleSpawnGroup;

	UPROPERTY()
	UNiagaraStackScriptItemGroup* ParticleUpdateGroup;

	UPROPERTY()
	UNiagaraStackEventHandlerGroup* AddEventHandlerGroup;

	UPROPERTY()
	UNiagaraStackRenderItemGroup* RenderGroup;
};