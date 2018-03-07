// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraNode.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.generated.h"

class UNiagaraScript;

UCLASS(MinimalAPI)
class UNiagaraNodeAssignment : public UNiagaraNodeFunctionCall
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Assignment)
	FNiagaraVariable AssignmentTarget;

	UPROPERTY(EditAnywhere, Category = Assignment)
	FString AssignmentDefaultValue;

	//~ Begin EdGraphNode Interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AllocateDefaultPins() override;

	//~ UNiagaraNodeFunctionCall interface
	virtual bool RefreshFromExternalChanges() override;

private:
	void GenerateScript();

	void InitializeScript(UNiagaraScript* NewScript);
};

