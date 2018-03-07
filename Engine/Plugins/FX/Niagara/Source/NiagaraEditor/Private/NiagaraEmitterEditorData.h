// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEmitterEditorData.generated.h"

class UNiagaraStackEditorData;

/** Editor only UI data for emitters. */
UCLASS()
class UNiagaraEmitterEditorData : public UObject
{
	GENERATED_BODY()

public:
	UNiagaraEmitterEditorData(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoad() override;

	UNiagaraStackEditorData& GetStackEditorData() const;

private:
	UPROPERTY(Instanced)
	UNiagaraStackEditorData* StackEditorData;
};